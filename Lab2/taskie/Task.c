#include <asm-generic/errno.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_MSG_COUNT 4
#define MAX_ITEMS 8
#define MIN_ITEMS 2
#define SHOP_QUEUE_NAME "/shop"
#define MSG_SIZE 128
#define TIMEOUT 2
#define CLIENT_COUNT 8
#define OPEN_FOR 8
#define START_TIME 8
#define MAX_AMOUNT 16

static const char* const UNITS[] = {"kg", "l", "dkg", "g"};
static const char* const PRODUCTS[] = {"mięsa", "śledzi", "octu", "wódki stołowej", "żelatyny"};

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

void msleep(unsigned int milisec)
{
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    struct timespec req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if (nanosleep(&req, &req))
        ERR("nanosleep");
}

void create_queue()
{
    struct mq_attr attr = {};
    attr.mq_maxmsg = MAX_MSG_COUNT;
    attr.mq_msgsize = MSG_SIZE;

    mqd_t queue_d;
    if((queue_d = mq_open(SHOP_QUEUE_NAME,O_RDWR | O_CREAT, 0600, &attr))==-1)
        ERR("mq_open");
    mq_close(queue_d);
}

void handle_messages(union sigval data);

void receive_notifications(mqd_t queue_d)
{
    struct sigevent notification = {};
    notification.sigev_value.sival_ptr = &queue_d;
    notification.sigev_notify = SIGEV_THREAD;
    notification.sigev_notify_function = handle_messages;

    int res = mq_notify(queue_d, &notification);
    if (res == -1 && errno != EBADF)
        ERR("mq_notify");
}

void handle_messages(union sigval data)
{
    mqd_t* queue_d = data.sival_ptr;
    receive_notifications(*queue_d);

    unsigned int msg_prio;
    char message[MSG_SIZE];
    while(1)
    {
        int res = mq_receive(*queue_d,message, MSG_SIZE,&msg_prio);
        if(res<0 && (errno == EAGAIN || errno == EBADF)) //EBADF necessary for notifications using threads - 
            break;//the process could already close the descriptor while the thread is still running and trying
        else if(res<0)//to receive messages from a closed fd.
            ERR("mq_receive");

        msleep(100);
        if(msg_prio!=0)
            printf("Please go to the end of the line!\n");
        else
        {
            switch(rand()%3)
            {
                case 0: {printf("Come back tomorrow.\n"); break;}
                case 1: {printf("Out of stock.\n");break;}
                case 2: {printf("There is an item in the packing zone that shouldn’t be there.\n");break;}
                default: break;
            }
        }
    }
}

void checkout_work()
{
    srand(getpid());

    mqd_t queue_d;
    if((queue_d = mq_open(SHOP_QUEUE_NAME,O_RDONLY|O_NONBLOCK))==-1)
        ERR("mq_open");

    int n = rand();
    if(n%4 == 0)
    {
        printf("Closed today\n");
        mq_close(queue_d);
        exit(EXIT_SUCCESS);
    }
    printf("Open today\n");
    
    union sigval data;
    data.sival_ptr = &queue_d;
    handle_messages(data);

    for(int i =0; i<OPEN_FOR;i++)
    {
        printf("Hour %d:00\n", START_TIME+i);
        msleep(200);
    }

    mq_close(queue_d);
    printf("Store closing.\n");
    sleep(1);
    exit(EXIT_SUCCESS);
}

void client_work()
{
    srand(getpid());

    mqd_t queue_d;
    if((queue_d = mq_open(SHOP_QUEUE_NAME,O_WRONLY))==-1)
        ERR("mq_open");
    
    int n = MIN_ITEMS + rand()% (MAX_ITEMS-MIN_ITEMS+1);

    for(int i = 0; i<n;i++)
    {
        int u = rand()%4;
        int p = rand()%5;
        int a = 1 + rand()%(MAX_AMOUNT+1);
        int P = rand()%2;

        char message[MSG_SIZE];
        snprintf(message, MSG_SIZE, "%d%s %s\n", a, UNITS[u],PRODUCTS[p]);

        struct timespec ts;
        if(clock_gettime(CLOCK_REALTIME,&ts)==-1)
            ERR("clock_gettime");
        ts.tv_sec+=1;

        int res = mq_timedsend(queue_d, message,MSG_SIZE,P, &ts);
        if(res<0 && errno == ETIMEDOUT)
        {
            printf("I will never come here...\n");
            break;
        }
        else if(res<0)
            ERR("mq_timed_send");
        sleep(1);
    }
    mq_close(queue_d);
    exit(EXIT_SUCCESS);
}

void create_clients()
{
    for(int i = CLIENT_COUNT; i>=0;i--)
    {
        pid_t pid;
        if((pid=fork())==-1)
            ERR("fork");
        if(pid==0)
        {
            if(i == CLIENT_COUNT)
            {
                checkout_work();
                msleep(200); 
            }
            else
            {
                client_work();
            }
        }
    }
}

int main(void)
{
    create_queue();
    create_clients();

    while(waitpid(0,NULL,0)>0){}
    printf("END\n");
    mq_unlink(SHOP_QUEUE_NAME);
    return EXIT_SUCCESS;
}
