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

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
#define MAX_MSG 4
#define MAX_NAME_LENGTH 64
volatile sig_atomic_t last_signal=0;

typedef struct 
{
    float num1;
    float num2;
}task;

#define MSG_SIZE_TASK sizeof(task) 
#define MSG_SIZE_CHILD sizeof(float)

void usage(char* pname)
{
    fprintf(stderr, "USAGE: %s\n",pname);
    exit(EXIT_SUCCESS);
}

int sethandler(void (*f)(int), int sig)
{
    struct sigaction act;
    memset(&act,0,sizeof(struct sigaction));
    act.sa_handler=f;
    if(sigaction(sig,&act,NULL)==-1)
        return -1;
    return 0;
}

void sigintHandler(int sig)
{
    last_signal = SIGINT;
}

void child_work(char* task_queue_name, mqd_t child_mq)
{
    srand(getpid());
    printf("[%d] Worker ready!\n",getpid());

    if(sethandler(sigintHandler,SIGINT)==-1)
        ERR("sethandler");

    mqd_t server_q;
    if((server_q = mq_open(task_queue_name, O_RDONLY))==-1)
        ERR("mq_open");

    while(1)
    {
        unsigned int msg_prio;
        task tk;
        int num = mq_receive(server_q,(char*)&tk, sizeof(task), &msg_prio);
        if(num<0)
        {
            if(errno == EAGAIN || errno == EINTR)
                continue;
            ERR("mq_receive");
        }
        if(msg_prio==1)
        {
            mq_close(child_mq);
            printf("[%d] Exits!\n", getpid());
            exit(EXIT_SUCCESS);
        }
        printf("[%d] Received task [%f, %f]\n", getpid(), tk.num1, tk.num2);

        int tm = 500 + rand()% 1501;
        struct timespec t = {0,tm*6000000};
        while(nanosleep(&t, &t)>0){}

        float result = tk.num1+tk.num2;
        num = mq_send(child_mq,(char*)&result,sizeof(float),0);

        if(num<0)
        {
            if(errno == EAGAIN || errno == EINTR)
                continue;
            ERR("mq_send");
        }
        printf("[%d] Result [%f]\n",getpid(),result);
    }
    mq_close(child_mq);
    printf("[%d] Exits!\n", getpid());
    exit(EXIT_SUCCESS);
}

void create_children(int n, char* task_queue_name, mqd_t* child_mq,pid_t* child_pids)
{
    for(int i = 0;i<n;i++)
    {
        pid_t pid;
        if((pid=fork())==-1)
            ERR("fork");
        if(pid==0)
            child_work(task_queue_name,child_mq[i]);
        else
            child_pids[i] = pid;
    }
}

void parent_work(int n,mqd_t server_q,char** child_mq_names, pid_t* child_pids)
{
    srand(getpid());
    printf("Server is starting...\n");

    mqd_t* child_mq = (mqd_t*)malloc(sizeof(mqd_t)*n);
    if(!child_mq)
        ERR("malloc");

    for(int i = 0; i<n;i++)
    {
        if((child_mq[i]=mq_open(child_mq_names[i],O_RDONLY|O_NONBLOCK))==-1)
            ERR("mq_open");
    }

    while(1)
    {
        if(last_signal==SIGINT)
        {
            for(int i = 0; i<n;i++)
            {
                while(1)
                {
                    task tk;
                    int num = mq_send(server_q,(char*)&tk, sizeof(task), 1);
                    if(num<0)
                    {
                        if(errno == EAGAIN)
                        {
                            printf("Queue is full!\n");
                            continue;
                        }
                        if(errno ==EINTR)
                            continue;
                        ERR("mq_send");
                    }
                    break;
                }
            }
            printf("Server KILLED!\n");
            free(child_mq);
            return;
        }
        int t = 100 + rand()%4901;
        struct timespec ts = {0,t*1000000};
        while(nanosleep(&ts,&ts)>0){}

        task tk;
        tk.num1 = (float)rand()/(float)RAND_MAX * 100.0;
        tk.num2 = (float)rand()/(float)RAND_MAX * 100.0;

        int num = mq_send(server_q,(char*)&tk, sizeof(task), 0);

        if(num<0)
        {
            if(errno == EAGAIN)
            {
                printf("Queue is full!\n");
                continue;
            }
            if(errno == EINTR)
                continue;
            ERR("mq_send");
        }
        printf("New task queued: [%f, %f]\n", tk.num1, tk.num2);
        
        float result;
        for(int i = 0; i<n;i++)
        {
            int num = mq_receive(child_mq[i],(char*)&result,sizeof(float),0);
            if(num<0)
            {
                if(errno==EAGAIN)
                    continue;
                ERR("mq_receive");
            }
            printf("Result from worker [%d]: [%f]\n",child_pids[i],result);
        }
    }
    free(child_mq);
}

void create_child_mq(int n,char** child_mq_names,mqd_t* child_mq)
{
    for(int i = 0; i<n;i++)
    {
        struct mq_attr attr = {};
        attr.mq_maxmsg=MAX_MSG;
        attr.mq_msgsize = MSG_SIZE_CHILD;
        if((child_mq[i]=mq_open(child_mq_names[i], O_RDWR| O_CREAT| O_NONBLOCK, 0600, &attr))==-1)
            ERR("mq_open");
    }
}

void unlink_child_mq(int n, char** child_mq_names)
{
    for(int i = 0; i<n;i++)
    {
        mq_unlink(child_mq_names[i]);
    }
}

int main(int argc, char**argv)
{
    if(argc!=2)
        usage(argv[0]);
    
    int n = atoi(argv[1]);
    if(n<2 || n>20)
        usage(argv[0]);

    if(sethandler(sigintHandler,SIGINT)==-1)
        ERR("sethandler");

    char task_queue_name[256];
    snprintf(task_queue_name, 256,"/task_queue_%d",getpid());

    mqd_t server_q;
    struct mq_attr attr = {};
    attr.mq_maxmsg = MAX_MSG;
    attr.mq_msgsize = MSG_SIZE_TASK;
    if((server_q=mq_open(task_queue_name, O_RDWR| O_CREAT| O_NONBLOCK, 0600, &attr))==-1)
        ERR("mq_open");

    char** child_mq_names = (char**)malloc(sizeof(char*)*n);
    for(int i = 0; i<n;i++)
    {
        child_mq_names[i] = (char*)malloc(sizeof(char)*MAX_NAME_LENGTH);
        snprintf(child_mq_names[i], MAX_NAME_LENGTH, "/result_queue_%d_%d",getpid(),i+1);
    }

    mqd_t* child_mq = (mqd_t*)malloc(sizeof(mqd_t)*n);
    if(!child_mq)
        ERR("malloc");
    create_child_mq(n,child_mq_names,child_mq);

    pid_t* child_pids = (pid_t*)malloc(sizeof(pid_t)*n);
    if(!child_pids) 
        ERR("malloc");

    create_children(n, task_queue_name,child_mq,child_pids);
    parent_work(n,server_q,child_mq_names,child_pids);
    
    while(waitpid(0,NULL,0)>0){}

    printf("All child processes have finished.\n");

    for(int i = 0; i<n;i++)
    {
        free(child_mq_names[i]);
        mq_close(child_mq[i]);
    }

    unlink_child_mq(n,child_mq_names);
    mq_close(server_q);
    mq_unlink(task_queue_name);
    free(child_mq);
    free(child_mq_names);
    free(child_pids);
    return EXIT_SUCCESS;
}