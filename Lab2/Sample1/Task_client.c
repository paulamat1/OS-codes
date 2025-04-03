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
#define MSG_SIZE sizeof(int)

typedef struct {
    pid_t pid;
    int num1;
    int num2;
} message;

void usage(char* pname)
{
    fprintf(stderr, "USAGE: %s\n",pname);
    exit(EXIT_SUCCESS);
}

void client_process(char* server_queue1, char* server_queue2,char* server_queue3)
{
    struct mq_attr attr = {};
    attr.mq_maxmsg = MAX_MSG;
    attr.mq_msgsize = MSG_SIZE;

    char name[MAX_NAME_LENGTH];
    snprintf(name,MAX_NAME_LENGTH,"/%d", getpid());
    mqd_t mqdes;

    if((mqdes=mq_open(name, O_RDWR | O_CREAT, 0600, &attr))==-1)
        ERR("mq_open");
    
    printf("%s\n",name);
    sleep(1);

    mqd_t server_q1,server_q2,server_q3;
    if((server_q1=mq_open(server_queue1, O_WRONLY))==-1)
        ERR("mq_open");
    if((server_q2=mq_open(server_queue2, O_WRONLY))==-1)
        ERR("mq_open");
    if((server_q3=mq_open(server_queue3, O_WRONLY))==-1)
        ERR("mq_open");
    
    char line[256];
    while(fgets(line,sizeof(line),stdin)!=NULL)
    {
        int num1, num2;
        if(sscanf(line, "%d %d", &num1,&num2)!=2)
            usage("invalid input on stdin");

        message msg;
        msg.num1 = num1;
        msg.num2 = num2;
        msg.pid = getpid();

        switch(rand()%3)
        {
            case 0: 
            {
                if(mq_send(server_q1, (char*)&msg, sizeof(message), 0)==-1)
                    ERR("mq_send");
                printf("Addition\n");
                break;
            }
            case 1:
            {
                if(num2 == 0)
                    printf("Cannot do this operation\n");continue;
                if(mq_send(server_q2, (char*)&msg, sizeof(message), 0)==-1)
                    ERR("mq_send");
                printf("Division\n");
                break;
            }
            case 2:
            {
                if(num2 == 0)
                    printf("Cannot do this operation\n");continue;
                if(mq_send(server_q3, (char*)&msg, sizeof(message), 0)==-1)
                    ERR("mq_send");
                printf("Modulo\n");
                break;
            }
            default: 
                break;
        }

        struct timespec ts;
        if(clock_gettime(CLOCK_REALTIME,&ts)==-1)
            ERR("clock_gettime");
        
        ts.tv_nsec += 100000000; 

        if (ts.tv_nsec >= 1000000000) {  //tv_nsec holds nanoseconds (0–999,999,999) so we have to check if it overflowed
            ts.tv_sec += ts.tv_nsec / 1000000000;
            ts.tv_nsec = ts.tv_nsec % 1000000000;
        }

        int result;
        int num;
        num=mq_timedreceive(mqdes, (char*)&result, sizeof(int), NULL,&ts);
        if(num<0)
        {
            if(errno == ETIMEDOUT)
            {
                printf("[Client]: Timed out\n");
                mq_close(server_q1);
                mq_close(server_q2);
                mq_close(server_q3);
                mq_close(mqdes);
                mq_unlink(name);
                exit(EXIT_FAILURE);
            }
            ERR("mq_receive");
        }
        printf("[Client]: Received %d\n", result);
    }

    mq_close(server_q1);
    mq_close(server_q2);
    mq_close(server_q3);
    mq_close(mqdes);
    mq_unlink(name);
}

int main(int argc, char** argv)
{
    if(argc!=4)
        usage(argv[0]);
    char* server_queue1 = argv[1];
    char* server_queue2 = argv[2];
    char* server_queue3 = argv[3];

    client_process(server_queue1, server_queue2,server_queue3);
    return EXIT_SUCCESS;
}