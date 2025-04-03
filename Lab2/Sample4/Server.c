#define _GNU_SOURCE
#include <errno.h>
#include <mqueue.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_MSG 10
#define MSG_SIZE 255
#define NEW_FORMAT_SPACE 20

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
volatile sig_atomic_t last_signal=0;

typedef struct 
{
    mqd_t* client_qs;
    int* client_count;
    pid_t* client_pids;
    char** client_names;
    mqd_t server_q;
}server_info;


void usage(char* pname)
{
    fprintf(stderr,"USAGE:%s\n",pname);
    exit(EXIT_FAILURE);
}

int sethandler(void(*f)(int, siginfo_t *, void *), int sig)
{
    struct sigaction act;
    memset(&act,0,sizeof(struct sigaction));
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = f;
    if(sigaction(sig,&act,NULL)==-1)
        return -1;
    return 0;
}

void sigintHandler(int sig,siginfo_t *info, void *p)
{
    last_signal=sig;
}

void handle_messages(server_info* info)
{
    static struct sigevent noti;
    noti.sigev_notify = SIGEV_SIGNAL;
    noti.sigev_signo = SIGRTMIN;
    noti.sigev_value.sival_ptr = info;
    if (mq_notify(info->server_q, &noti) < 0)
        ERR("mq_notify");
}

void mqnotifHandler(int sig,siginfo_t *info, void *p)
{
    server_info* server_i = info->si_value.sival_ptr;
    mqd_t* client_qs = server_i->client_qs;
    int* client_count = server_i->client_count;
    mqd_t server_q = server_i->server_q;
    pid_t* client_pids = server_i->client_pids;
    char** client_names = server_i->client_names;

    handle_messages(server_i);

    pid_t sender_pid = info->si_pid;

    unsigned int msg_prio;
    char message[MSG_SIZE];  //substracted so that the new format of the sent message ([name] message) fits
    while(1)
    {
        if (mq_receive(server_q, message, MSG_SIZE, &msg_prio) == -1)
        {
            if (errno == EAGAIN)
                break;
            else
                ERR("mq_receive");
        }
        if (msg_prio==2)
        {
            message[strcspn(message, "\n")] = '\0';
            char* name;
            char Message[MSG_SIZE];
            for(int i = 0; i<*(client_count);i++)
            {
                if(client_qs[i] == -1)
                    continue;
                if(client_pids[i]==sender_pid)
                {
                    name = client_names[i];
                    break;
                }
            }
            snprintf(Message, MSG_SIZE, "[%s] %s", name,message); 
            printf("%s\n" ,Message);
            for(int i = 0; i<*(client_count);i++)
            {
                if(client_qs[i] == -1)
                    continue;
                if(mq_send(client_qs[i],Message,MSG_SIZE,2)==-1)
                    ERR("mq_send");
            }
        }
        if(msg_prio==0)
        {
            printf("Client %s has connected!\n",message);
            char client_queue_name[MSG_SIZE];
            snprintf(client_queue_name,MSG_SIZE,"/chat_%s",message);  //What to do with this??? Also can I assume that the client
            client_qs[*(client_count)] = mq_open(client_queue_name,O_WRONLY);//can connect only after the server is up? Otherwise there
            if(client_qs[*(client_count)]<0)//is a problem with the messages - the servers queue is full before he sets up the notification
                ERR("mq_open");//resulting in it never getting notified about messages from this client.
            client_pids[*(client_count)] = sender_pid;
            client_names[*(client_count)] = strdup(message);
            (*client_count)++;
        }
        if(msg_prio==1)
        {
            char* name;
            for(int i = 0; i<*(client_count);i++)
            {
                if(client_pids[i]==sender_pid)
                {
                    name = client_names[i];
                    client_qs[i] = -1;
                    break;
                }
            }
            printf("Client %s disconnected!\n",name);
        }
    }
}

void server_work(char* server_name)
{
    struct mq_attr attr = {};
    attr.mq_maxmsg = MAX_MSG;
    attr.mq_msgsize=MSG_SIZE;

    char server_queue_name[MSG_SIZE];
    snprintf(server_queue_name,MSG_SIZE,"/chat_%s",server_name);
    mqd_t server_q = mq_open(server_queue_name,O_RDONLY|O_CREAT|O_NONBLOCK,0600,&attr);
    if(server_q==(mqd_t)-1)
    {
        printf("%d %s\n",errno,server_queue_name);
        ERR("mq_open");
    }
    
    pid_t* client_pids = (pid_t*)malloc(sizeof(pid_t)*8);
    char** client_names = (char**)malloc(sizeof(char*)*8);
    mqd_t* client_q = (mqd_t*)calloc(8,sizeof(mqd_t));
    int client_count=0;

    server_info* info = (server_info*)malloc(sizeof(server_info));
    info->server_q = server_q;
    info->client_qs=client_q;
    info->client_count = &client_count;
    info->client_pids = client_pids;
    info->client_names = client_names;

    handle_messages(info);

    while(1)
    {
        if(last_signal==SIGINT)
        {
            for(int i = 0; i<client_count;i++)
            {
                if(client_q[i] == -1)
                    continue;
                char goodbye_message = ' ';
                int res;
                while((res = mq_send(client_q[i],&goodbye_message,1,1))==-1)
                {
                    if(errno ==EAGAIN)
                        continue;
                    ERR("mq_send");
                }
            }
            for(int i = 0; i<client_count;i++)
            {
                free(client_names[i]);
            }
            free(client_names);
            free(client_q);
            free(info);
            free(client_pids);
            mq_close(server_q);
            mq_unlink(server_queue_name);
            return;
        }
        
        char input[MSG_SIZE];
        while(fgets(input,MSG_SIZE,stdin)!=NULL)
        {
            input[strcspn(input, "\n")] = '\0';
            char Message[MSG_SIZE];
            snprintf(Message, MSG_SIZE, "[SERVER] %s",input); 

            for(int i = 0; i<client_count;i++)
            {
                int res = mq_send(client_q[i],Message,MSG_SIZE,2);
                if(res<0 && errno ==EAGAIN)
                    continue;
                else if(res<0)
                    ERR("mq_send");
            }
        }
    }

    for(int i = 0; i<client_count;i++)
    {
        free(client_names[i]);
    }
    free(client_names);
    free(client_q);
    free(info);
    free(client_pids);
    mq_close(server_q);
    mq_unlink(server_queue_name);
}

int main(int argc, char** argv)
{
    if(argc!=2)
        usage(argv[0]);   
    char* server_name = argv[1];

    if(sethandler(mqnotifHandler,SIGRTMIN)==-1)
        ERR("sethandler");
    if(sethandler(sigintHandler,SIGINT)==-1)
        ERR("sethandler");

    server_work(server_name);
    return EXIT_SUCCESS;
}