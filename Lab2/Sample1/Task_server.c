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
#define MAX_NAME_LENGTH 20

typedef struct 
{
    pid_t pid;
    int num1;
    int num2;
}message;

#define MSG_SIZE sizeof(message)

volatile sig_atomic_t last_signal=0;

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
    last_signal=sig;
}

void server_process()
{
    struct mq_attr attr = {};
    attr.mq_maxmsg = MAX_MSG;
    attr.mq_msgsize = MSG_SIZE;

    char name1[MAX_NAME_LENGTH];
    char name2[MAX_NAME_LENGTH];
    char name3[MAX_NAME_LENGTH];
    snprintf(name1,MAX_NAME_LENGTH,"/%d_s", getpid());
    snprintf(name2,MAX_NAME_LENGTH,"/%d_d", getpid());
    snprintf(name3,MAX_NAME_LENGTH,"/%d_m", getpid());

    mqd_t server_q1, server_q2, server_q3;

    if((server_q1=mq_open(name1, O_RDWR | O_CREAT | O_NONBLOCK, 0600, &attr))==-1)
        ERR("mq_open");
    if((server_q2=mq_open(name2, O_RDWR | O_CREAT | O_NONBLOCK, 0600, &attr))==-1)
        ERR("mq_open");
    if((server_q3=mq_open(name3, O_RDWR | O_CREAT | O_NONBLOCK, 0600, &attr))==-1)
        ERR("mq_open");
    
    printf("%s\n", name1);
    printf("%s\n",name2);
    printf("%s\n",name3);

    sleep(1);

    while(1)
    {
        if(last_signal==SIGINT)
        {
            printf("[Server]: Received SIGINT\n");
            mq_close(server_q1);
            mq_close(server_q2);
            mq_close(server_q3);
            mq_unlink(name1);
            mq_unlink(name2);
            mq_unlink(name3);
            exit(EXIT_SUCCESS);
        }

        int num, result;

        message msg_add = {};
        num = mq_receive(server_q1, (char*)&msg_add,sizeof(message),0);
        if(num<0)
        {
            if(errno != EAGAIN)
                ERR("mq_receive");
        }
        else
        {
            result = msg_add.num1+msg_add.num2;
            char client_name[MAX_NAME_LENGTH];
            snprintf(client_name,MAX_NAME_LENGTH,"/%d",msg_add.pid);
            mqd_t client_q;
            if((client_q=mq_open(client_name,O_WRONLY))==-1)
                ERR("mq_open");
            if((mq_send(client_q,(char*)&result,sizeof(int), 0))==-1)
                ERR("mq_send");
            mq_close(client_q);
        }

        message msg_div = {};
        num = mq_receive(server_q2, (char*)&msg_div,sizeof(message),0);
        if(num<0)
        {
            if(errno != EAGAIN)
                ERR("mq_receive");
        }
        else
        {
            result = msg_div.num1/msg_div.num2;
            char client_name[MAX_NAME_LENGTH];
            snprintf(client_name,MAX_NAME_LENGTH,"/%d",msg_div.pid);
            mqd_t client_q;
            if((client_q=mq_open(client_name,O_WRONLY))==-1)
                ERR("mq_open");
            if((mq_send(client_q,(char*)&result,sizeof(int), 0))==-1)
                ERR("mq_send");
            mq_close(client_q);
        }

        message msg_mod = {};
        num = mq_receive(server_q3, (char*)&msg_mod,sizeof(message),0);
        if(num<0)
        {
            if(errno != EAGAIN)
                ERR("mq_receive");
        }
        else 
        {
            result = msg_mod.num1%msg_mod.num2;
            char client_name[MAX_NAME_LENGTH];
            snprintf(client_name,MAX_NAME_LENGTH,"/%d",msg_mod.pid);
            mqd_t client_q;
            if((client_q=mq_open(client_name,O_WRONLY))==-1)
                ERR("mq_open");
            if((mq_send(client_q,(char*)&result,sizeof(int), 0))==-1)
                ERR("mq_send");
            mq_close(client_q);
        }
    }

    mq_close(server_q1);
    mq_close(server_q2);
    mq_close(server_q3);
    mq_unlink(name1);
    mq_unlink(name2);
    mq_unlink(name3);
}

int main(int argc, char** argv)
{
    if(sethandler(sigintHandler,SIGINT)==-1)
        ERR("sighandler");
    server_process();
    return EXIT_SUCCESS;
}