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

#define ERR(source)  (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
#define MAX_NAME_LENGTH 100
#define MAX_QUEUE_NAME_LENGTH 101
#define MSG_SIZE 256
#define MAX_MSG_COUNT 4
#define LIFE_SPAN 10

typedef struct child_data
{
    char* name;
    mqd_t queue;
}child_data;

void usage(char* pname)
{
    fprintf(stderr,"USAGE:%s", pname);
    exit(EXIT_FAILURE);
}

void create_queues(mqd_t* queues,char** queue_names,int n)
{
    for(int i = 0; i<n;i++)
    {
        struct mq_attr attr = {};
        attr.mq_maxmsg = MAX_MSG_COUNT;
        attr.mq_msgsize = MSG_SIZE;

        queues[i] = mq_open(queue_names[i],O_RDWR|O_NONBLOCK|O_CREAT, 0600,&attr);
        if(queues[i]==-1)
            ERR("mq_open");
    }
}
void handle_messages(union sigval sv)
{
    child_data* data = sv.sival_ptr;
    char message[MSG_SIZE];

    struct sigevent notification = {};
    notification.sigev_notify = SIGEV_THREAD;
    notification.sigev_value.sival_ptr = data;
    notification.sigev_notify_function = handle_messages;

    int res = mq_notify(data->queue, &notification);
    if (res == -1)
        ERR("mq_notify");
    
    while(1)
    {
        int res=mq_receive(data->queue,message,MSG_SIZE,0);
        if(res==-1)
        {
            if(errno == EAGAIN)
                break;
            ERR("mq_receive");
        }
        printf("[%s]: %s\n",data->name,message);
    }
}

void child_work(mqd_t* queues,char** names,int n,int child_index)
{
    srand(getpid());
    printf("[%d] I am here\n", getpid());

    child_data data = {};
    data.name = names[child_index];
    data.queue=queues[child_index];

    union sigval sv;
    sv.sival_ptr=&data;
    handle_messages(sv);

    for(int i = 0; i<LIFE_SPAN;i++)
    {
        int receiver = rand()%n;
        char message[MSG_SIZE];
        switch(rand()%3)
        {
            case 0:
                snprintf(message, MSG_SIZE, "Salve %s!", names[receiver]);
                break;
            case 1:
                snprintf(message, MSG_SIZE, "Visne garum emere, %s?", names[receiver]);
                break;
            case 2:
                snprintf(message, MSG_SIZE, "Fuistine hodie in thermis, %s?", names[receiver]);
                break;
        }
        while(1)
        {
            int res=mq_send(queues[receiver],message,MSG_SIZE,0);
            if(res==-1)
            {
                if(errno==EAGAIN)
                    continue;
                ERR("mq_send");
            }
            break;
        }
        sleep(1);
    }
    printf("%s: Disceo.\n", names[child_index]);
    exit(EXIT_SUCCESS);
}

void create_children(mqd_t* queues,char** names,int n)
{
    for(int i = 0; i<n;i++)
    {
        switch(fork())
        {
            case 0:
            {
                child_work(queues,names,n,i);
            }
            case -1:
                ERR("fork");
        }
    }
}

int main(int argc, char** argv)
{
    if(argc!=2)
        usage(argv[0]);
    int n = atoi(argv[1]);
    if(n<=0 || n>100)
        usage(argv[0]);
    
    mqd_t* queues = (mqd_t*)malloc(sizeof(mqd_t)*n);
    char** queue_names = malloc(sizeof(char*)*n);
    char** names = malloc(sizeof(char*)*n);
    for(int i = 0; i<n;i++)
    {
        queue_names[i] = malloc(sizeof(char)*MAX_NAME_LENGTH);
        names[i] = malloc(sizeof(char)*MAX_QUEUE_NAME_LENGTH);
        snprintf(queue_names[i],MAX_QUEUE_NAME_LENGTH,"/child_%d", i+1);
        snprintf(names[i],MAX_NAME_LENGTH,"child %d", i+1);
    }
    if(!queues || !queue_names || !names)
        ERR("malloc");
    create_queues(queues,queue_names,n);
    create_children(queues,names,n);

    while (wait(NULL) > 0){}
    
    for(int i = 0;i<n;i++)
    {
        mq_close(queues[i]);
        mq_unlink(queue_names[i]);
    }
    free(queues);
    for(int i = 0; i<n;i++)
    {
        free(queue_names[i]);
        free(names[i]);
    }
    free(queue_names);
    printf("Parens: Disceo.\n");
    return EXIT_SUCCESS;
}




