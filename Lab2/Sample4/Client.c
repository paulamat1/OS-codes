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

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
volatile sig_atomic_t last_signal=0;

typedef struct
{
    char* name;
    mqd_t client_q;
}client_info;

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

void handle_messages(client_info* info)
{
    static struct sigevent noti;
    noti.sigev_notify = SIGEV_SIGNAL;
    noti.sigev_signo = SIGRTMIN;
    noti.sigev_value.sival_ptr = info;
    if (mq_notify(info->client_q, &noti) < 0)
        ERR("mq_notify");
}

void mqnotifHandler(int sig,siginfo_t *info, void *p)
{
    client_info* client_info = info->si_value.sival_ptr;
    handle_messages(client_info);

    unsigned int msg_prio;
    char message[MSG_SIZE];
    while(1)
    {
        if (mq_receive(client_info->client_q, message, MSG_SIZE, &msg_prio) < 1)
        {
            if (errno == EAGAIN)
                break;
            else
                ERR("mq_receive");
        }
        if (msg_prio==2)
        {
            printf("%s\n",message);
        }
        if(msg_prio==1)
        {
            printf("Server closed the connection\n");
            mq_close(client_info->client_q);
            mq_unlink(client_info->name);
            free(client_info);
            exit(EXIT_SUCCESS);
        }
    }
}

void client_work(char* client_name, char* server_name)
{
    struct mq_attr attr = {};
    attr.mq_maxmsg = MAX_MSG;
    attr.mq_msgsize=MSG_SIZE;

    char client_queue_name[MSG_SIZE];
    snprintf(client_queue_name,MSG_SIZE,"/chat_%s",client_name);
    mqd_t client_q = mq_open(client_queue_name,O_RDWR|O_CREAT|O_NONBLOCK,0600,&attr);
    if(client_q<0)
        ERR("mq_open");


    char server_queue_name[MSG_SIZE];
    snprintf(server_queue_name,MSG_SIZE,"/chat_%s",server_name);
    mqd_t server_q;
    while(1)
    {
        server_q = mq_open(server_queue_name,O_WRONLY);
        if(server_q<0 && errno == ENOENT)
            continue;
        else if(server_q<0)
            ERR("mq_open");
        else
            break;
    }

    char message[MSG_SIZE];
    memset(message,0,MSG_SIZE);
    memcpy(message,client_name,MSG_SIZE);

    if(mq_send(server_q,message,MSG_SIZE,0)==-1)
        ERR("mq_send");

    client_info* info = (client_info*)malloc(sizeof(client_info));
    info->client_q = client_q;
    info->name = client_name;
    handle_messages(info);

    while(1)
    {
        if(last_signal==SIGINT)
        {
            char goodbye_message = ' ';
            int res;
            while((res = mq_send(server_q,&goodbye_message,1,1))==-1)
            {
                if(errno ==EAGAIN)
                    continue;
                ERR("mq_send");
            }
            free(info);
            mq_close(server_q);
            mq_close(client_q);
            mq_unlink(client_queue_name);
            exit(EXIT_SUCCESS);
        }
        char input[MSG_SIZE];
        while(fgets(input,MSG_SIZE,stdin)!=NULL)
        {
            int res = mq_send(server_q,input,MSG_SIZE,2);
            if(res<0 && errno == EAGAIN)
                continue;
            else if(res<0)
                ERR("mq_send");
        }
    }

    free(info);
    mq_close(server_q);
    mq_close(client_q);
    mq_unlink(client_queue_name);
}

int main(int argc, char** argv)
{
    if(argc!=3)
        usage(argv[0]);
    char* server_name = argv[1];
    char* client_name = argv[2]; 
    
    if(sethandler(mqnotifHandler,SIGRTMIN)==-1)
        ERR("sethandler");
    if(sethandler(sigintHandler,SIGINT)==-1)
        ERR("sethandler");

    client_work(client_name,server_name);
    return EXIT_SUCCESS;
}