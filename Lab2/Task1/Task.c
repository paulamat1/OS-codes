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
#define MAX_NUM 10
#define LIFE_SPAN 10

volatile sig_atomic_t children_left = 0;

void usage(char* pname)
{
    fprintf(stderr, "USAGE:%s\n", pname);
    exit(EXIT_FAILURE);
}

int sethandler(void (*f)(int, siginfo_t *, void *), int sig)
{
    struct sigaction act;
    memset(&act,0,sizeof(struct sigaction));
    act.sa_sigaction = f;  //instead of sa_handler because weuse SA_SIGINFO which expects a 3 parameter f function
    act.sa_flags = SA_SIGINFO;
    if(sigaction(sig,&act,NULL)==-1)
        return -1;
    return 0;
}

void sigchldhandler(int sig, siginfo_t* info, void* context)
{
    pid_t pid;
    while(1)
    {
        pid = waitpid(0,NULL,WNOHANG);
        if(pid==0)
        {
            return;
        }
        if(pid < 0)
        {
            if(errno == ECHILD)
                return;
            ERR("waitpid");
        }
        children_left--;
    }
}

void mq_handler(int sig, siginfo_t* info, void* context)
{
    mqd_t* pin;
    uint8_t num; //stores one byte integer in range 0-255
    unsigned int msg_prio;

    pin = (mqd_t*)info->si_value.sival_ptr;
    static struct sigevent notif;
    notif.sigev_notify=SIGEV_SIGNAL;
    notif.sigev_signo= SIGRTMIN;
    notif.sigev_value.sival_ptr=pin;

    if(mq_notify(*pin,&notif)==-1)
        ERR("mq_notify");
    
    while(1)
    {
        if(mq_receive(*pin,(char*)&num,1,&msg_prio)<1)
        {
            if(errno == EAGAIN)
                break;
            ERR("mq_receive");
        }
        if(msg_prio==0)
            printf("Got timeout from %d\n", num);
        else
            printf("%d is a Bingo number\n", num);
    }
}

void child_work(int n, mqd_t pin, mqd_t pout)
{
    srand(getpid());
    uint8_t my_bingo = (uint8_t)(rand()%MAX_NUM);
    uint8_t num;
    int life = 1 + rand()%LIFE_SPAN;

    for(int i = 0; i<life;i++)
    {
        if(TEMP_FAILURE_RETRY(mq_receive(pout,(char*)&num,1,NULL))<1)
            ERR("mq_receive");
        printf("[%d] received %d\n", getpid(), num);
        if(my_bingo==num)
        {
            while(1)
            {
                if(TEMP_FAILURE_RETRY(mq_send(pin, (char*)&n,1,1))!=0)
                {
                    if(errno == EAGAIN)
                        continue;
                    ERR("mq_send");
                }
                break;
            }
            return;
        }
    }
    while(1)
    {
        if(TEMP_FAILURE_RETRY(mq_send(pin, (char*)&n,1,0))!=0)
        {
            if(errno == EAGAIN)
                continue;
            ERR("mq_send");
        }
        break;
    }
    printf("GOODBYE %d\n",n);
}

void create_children(int n, mqd_t pin, mqd_t pout)
{
    while(n--)
    {
        switch(fork())
        {
            case 0:
                child_work(n,pin,pout);
                exit(EXIT_SUCCESS);
            case -1:
                ERR("fork");
        } 
        children_left++;  
    }
}

void parent_work(mqd_t pout)
{
    while(children_left)
    {
        uint8_t num = (uint8_t)(rand()%MAX_NUM);
        if(mq_send(pout, (char*)&num,1,0)!=0)
            ERR("mq_send");
        sleep(1);
    }
    printf("Parent terminates\n");
}

int main(int argc, char** argv)
{
    if(argc!=2)
        usage(argv[0]);
    int n = atoi(argv[1]);
    if(n<=0 || n>=100)
        usage(argv[0]);
    
    mqd_t pin, pout;
    struct mq_attr attr = {};
    attr.mq_maxmsg=10;
    attr.mq_msgsize=1;

    if((pin = TEMP_FAILURE_RETRY(mq_open("/bingo_in", O_RDWR | O_NONBLOCK | O_CREAT, 0600,&attr)))==-1)
        ERR("mq_open");
    if((pout = TEMP_FAILURE_RETRY(mq_open("/bingo_out", O_RDWR | O_CREAT, 0600, &attr)))==-1)
        ERR("mq_open");
    
    if(sethandler(sigchldhandler, SIGCHLD)==-1)
        ERR("sethandler");
    if(sethandler(mq_handler, SIGRTMIN)==-1)
        ERR("sethandler");
    create_children(n, pin,pout);
    
    static struct sigevent noti;
    noti.sigev_notify=SIGEV_SIGNAL;
    noti.sigev_signo=SIGRTMIN;
    noti.sigev_value.sival_ptr=&pin;
    if(mq_notify(pin,&noti))
        ERR("mq_notify");
    
    parent_work(pout);

    mq_close(pin);
    mq_close(pout);
    mq_unlink("/bingo_in");
    mq_unlink("/bingo_out");
    return EXIT_SUCCESS;
}