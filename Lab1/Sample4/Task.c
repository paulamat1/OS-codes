#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
#define MAX_BUF 256

void usage(char* pname)
{
    fprintf(stderr, "USAGE:%s\n", pname);
}

int sethandler(void (*f)(int), int sig)
{
    struct sigaction act;
    act.sa_handler = f;
    if(sigaction(sig,&act,NULL)==-1)
        return -1;
    return 0;
}

void sigchldhandler(int sig)
{
    pid_t pid;

    while(1)
    {
        pid=waitpid(0,NULL,WNOHANG);
        if(pid==0)
            return;
        if(pid<=0)
        {
            if(errno == ECHILD)
                return;
            ERR("waitpid");
        }
    }
}

void premature_termination(pid_t pid, int money)
{
    srand(getpid());
    if(rand()%10 == 0)
    {
        printf("%d: I saved %d\n", pid, money);
        exit(EXIT_SUCCESS);
    }
}

void parent_work(int n, int* fdr, int*fdw)
{
    srand(getpid());
    char buffer[MAX_BUF];
    int active_players=n;

    while(active_players>0)
    {
        int winner = rand()%37;
        printf("Dealer: %d is the lucky number.\n", winner);
        for(int i = 0; i<n;i++)
        {
            if(fdr[i] == -1 && fdw[i]==-1)
                continue;
            memset(buffer,0,MAX_BUF);
            int status,bet,number;
            pid_t pid;

            if((status=TEMP_FAILURE_RETRY(read(fdr[i],buffer,MAX_BUF)))!=MAX_BUF)
            {
                if(status<0 && errno == EINTR)
                    continue;
                if(status==0)
                {
                    if(close(fdr[i])|| close(fdw[i]))
                        ERR("close");
                    fdr[i]=-1, fdw[i]=-1;
                    active_players--;
                    continue;
                }
                ERR("write");
            }

            sscanf(buffer,"%d: I bet: %d on number:%d",&pid,&bet,&number);
            printf("Dealer: %d placed %d on %d\n",pid,bet,number);

            if(number==winner)
            {
                memset(buffer,0,MAX_BUF);
                char message[MAX_BUF];
                snprintf(message, sizeof(message),"WON\n");
                memcpy(buffer,message,strlen(message));
            }
            else
            {
                memset(buffer,0,MAX_BUF);
                char message[MAX_BUF];
                snprintf(message, sizeof(message),"LOST\n");
                memcpy(buffer,message,strlen(message));
            }

            if(TEMP_FAILURE_RETRY(write(fdw[i],buffer,MAX_BUF))!=MAX_BUF)
            {
                if(errno == EPIPE)
                {
                    if(close(fdr[i]) || close(fdw[i]))
                        ERR("close");
                    fdr[i]=-1, fdw[i]=-1;
                    active_players--;
                    continue;
                }
                ERR("write");
            }
        }
    }
}

void child_work(int m, int read_end, int write_end)
{
    srand(time(NULL)*getpid());
    pid_t pid = getpid();
    printf("%d: I have %d and I'm going to play roulette\n", pid, m);
    char buffer[MAX_BUF];

    while(m>0)
    {
        premature_termination(pid,m);
        int bet = 1 + rand()%m;
        int number = rand()%37;

        memset(buffer, 0, sizeof(buffer));
        char message[MAX_BUF];
        snprintf(message,sizeof(message), "%d: I bet: %d on number:%d",pid,bet,number);
        memcpy(buffer, message, strlen(message));

        if(TEMP_FAILURE_RETRY(write(write_end,buffer,MAX_BUF))!=MAX_BUF)
        {
            if(errno == EPIPE)
            {
                if(close(read_end) || close(write_end))
                    ERR("close");
                exit(EXIT_SUCCESS);
            }
            ERR("write");
        }

        memset(buffer,0,MAX_BUF);
        int status;
        if((status=TEMP_FAILURE_RETRY(read(read_end,buffer,MAX_BUF)))!=MAX_BUF)
        {
            if(status<0 && errno == EINTR)
                continue;
            if(status==0)
            {
                if(close(read_end)|| close(write_end))
                    ERR("close");
                continue;
            }
            ERR("write");
        }

        if(strncmp(buffer,"WON", 3)==0)
        {
            int amount = bet*35;
            m+=amount;
            printf("%d: I won %d\n", pid, amount);
        }
        else
            m-=bet;
    }

    printf("%d: I'm broke\n", pid);
    exit(EXIT_SUCCESS);
}

void create_children_pipes(int n, int m,int* fdr, int* fdw)
{
    pid_t pid;

    for(int i = 0; i<n;i++)
    {
        int R[2],W[2];
        if(pipe(R) || pipe(W))
            ERR("pipe");

        if((pid=fork())==-1)
            ERR("fork");
        if(pid==0)
        {
            int j = 0;
            while(j<i)
            {
                if(close(fdr[j])||close(fdw[j]))
                    ERR("close");
                j++;
            }
            free(fdr), free(fdw);
            if(close(W[1])|| close(R[0]))
                ERR("close");
            child_work(m, W[0], R[1]);
            if(close(R[1])||close(W[0]))
                ERR("close");
            exit(EXIT_SUCCESS);
        }
        if(close(R[1])||close(W[0]))
            ERR("close");
        fdr[i] = R[0];
        fdw[i] = W[1];
    }
}

int main(int argc, char** argv)
{
    if(argc!=3)
        usage(argv[0]);
    int n = atoi(argv[1]), m = atoi(argv[2]);
    if(n<1 || m<100)
        usage(argv[0]);
    
    if(sethandler(sigchldhandler, SIGCHLD)==-1)
        ERR("sethandler");

    int* fdw = (int*)malloc(sizeof(int)*n);
    int* fdr = (int*)malloc(sizeof(int)*n);
    if(!fdw || !fdr)
        ERR("malloc");

    create_children_pipes(n,m,fdr, fdw);
    parent_work(n,fdr, fdw);

    pid_t pid;
    while((pid=waitpid(0,NULL,0))>0){}
    printf("Dealer: Casino always wins\n");
    
    free(fdw);
    free(fdr);
    return EXIT_SUCCESS;
}