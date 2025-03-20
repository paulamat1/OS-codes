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

void usage(char* pname)
{
    fprintf(stderr,"USAGE: %s\n", pname);
    exit(EXIT_FAILURE);
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

void parent_work(int read_end, int write_end)
{
    srand(getpid());
    char buffer[PIPE_BUF];
    buffer[0] = 3;
    buffer[1] = 1;
    buffer[2] = '\0';
    int status;
    
    while(1)
    {
        status=TEMP_FAILURE_RETRY(write(write_end, buffer, buffer[0]));
        if(status<0 && errno == EPIPE)
            return;
        if(status<0)
            ERR("write");

        char c;
        status=TEMP_FAILURE_RETRY(read(read_end,&c,1));
        if(status<0 && errno == EINTR)
            continue;
        if(status<0)
            ERR("read");
        if(status==0)
            break;

        buffer[0] = c;
        if(TEMP_FAILURE_RETRY(read(read_end, buffer+1, c-1))<c-1)
            ERR("read");


        printf("PID: %d, %d\n", getpid(), buffer[1]);
        if(buffer[1]==0)
            exit(EXIT_SUCCESS);
        buffer[1] = -10 + buffer[1] + rand()%21;
    }
}

void child_work(int read_end,int write_end)
{
    srand(getpid());
    char buffer[PIPE_BUF];
    int status;

    while(1)
    {
        char c;
        status=TEMP_FAILURE_RETRY(read(read_end,&c,1));
        if(status<0 && errno == EINTR)
            continue;
        if(status<0)
            ERR("read");
        if(status==0)
            break;
        
        buffer[0] =c;
        if(TEMP_FAILURE_RETRY(read(read_end, buffer+1, c-1))<c-1)
            ERR("read");
        printf("PID: %d, %d\n", getpid(), buffer[1]);

        if(buffer[1]==0)
            exit(EXIT_SUCCESS);

        buffer[1] = -10 + buffer[1] + rand()%21;

        status = TEMP_FAILURE_RETRY(write(write_end, buffer, buffer[0]));
        if(status<0 && errno==EPIPE)
            break;
        if(status<0)
            ERR("write");
    }
}

void create_children(int R[2])
{
    pid_t pid;
    int P[2],S[2], n=0;
    if(pipe(P) || pipe(S))
        ERR("pipe");

    while(n<2)
    {
        if((pid=fork())==-1)
            ERR("fork");
        if(pid == 0 && n==0)
        {
            if(close(P[0]) || close(S[0]) || close(S[1]) || close(R[1]))
                ERR("close");
            child_work(R[0], P[1]);
            if(close(R[0]) || close(P[1]))
                ERR("close");
            exit(EXIT_SUCCESS);
        }
        if(pid==0 && n==1)
        {
            if(close(P[1]) || close(S[0]) || close(R[1]) || close(R[0]))
                ERR("close");
            child_work(P[0], S[1]);
            if(close(S[1]) || close(P[0]))
                ERR("close");
            exit(EXIT_SUCCESS);
        }
        n++;
    }
    if(close(P[0]) || close(P[1]) || close(S[1]) || close(R[0]))
        ERR("close");
    parent_work(S[0], R[1]);
    if(close(R[1]) || close(S[0]))
        ERR("close");
}

int main(int argc, char** argv)
{
    if(argc!=1)
        usage(argv[0]);
    if(sethandler(sigchldhandler,SIGCHLD)==-1)
        ERR("sethandler");
    int R[2];
    if(pipe(R))
        ERR("pipe");
    create_children(R);
    return EXIT_SUCCESS;
}
