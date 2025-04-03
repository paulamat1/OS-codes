#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define MAX_BUF 256

volatile sig_atomic_t lastsignal = 0;

void usage(char* pname)
{
    fprintf(stderr, "USAGE:%s\n", pname);
    exit(EXIT_FAILURE);
}

int sethandler(void(*f)(int), int sig)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler=f;
    if(sigaction(sig, &act,NULL)==-1)
        return -1;
    return 0;
}

void sigalrmhandler(int sig)
{
    lastsignal=sig;
}

void sigchldhandler(int sig)
{
    pid_t pid;
    while(1)
    {
        pid = waitpid(0,NULL,WNOHANG);
        if(pid==0)
            return;
        if(pid<0)
        {
            if(errno == ECHILD)
                return;
            ERR("waitpid");
        }
    }
}

void parent_work(int n, int* fds, pid_t* pids,int R)
{
    srand(getpid());
    char buffer[MAX_BUF];
    int attendance[n];
    int difficulty[4] = {3,6,7,5};
    for(int i = 0; i<4;i++)
        difficulty[i]+=(1 + rand()%20);

    for(int i = 0; i<n;i++)
    {
        char message[MAX_BUF];
        snprintf(message, sizeof(message), "Teacher: Is %d here?", pids[i]);
        memset(buffer,0, MAX_BUF);
        memcpy(buffer, message, strlen(message));
        buffer[MAX_BUF-1]='\0';
        printf("Teacher: Is %d here?\n", pids[i]);
        

        if(TEMP_FAILURE_RETRY(write(fds[i], buffer, MAX_BUF))!=MAX_BUF)
        {
            if(errno == EPIPE)
            {
                if(close(fds[i]))
                    ERR("close");
                fds[i]=0;
                continue;
            }
            ERR("write");
        }
    }
    int status;
    pid_t pid;
    for(int i = 0; i<n;i++)
    {
        memset(buffer,0,MAX_BUF);
        if((status=TEMP_FAILURE_RETRY(read(R, buffer, MAX_BUF)))!=MAX_BUF)
        {
            if(status<0 && errno ==EINTR)
                break;
            if(status<0)
                ERR("read");
            if(status==0)
            {
                if(close(fds[i]))
                    ERR("close");
                fds[i] = 0;
                continue;
            }
        }
        sscanf(buffer, "Student %d: HERE", &pid);
        for(int i = 0; i<n;i++)
        {
            if(pids[i] == pid)
            {
                attendance[i]=1;
                break;
            }
        }
    }

    alarm(1);
    int scores[n];
    for(int i = 0;i<n;i++)
        scores[i]=0;

    for(int j = 0; j<4;j++)
    {
        for(int i = 0; i<n;i++)
        {
            if(lastsignal==SIGALRM)
            {
                printf("Teacher: END OF TIME!\n");
                for(int i = 0; i<n;i++)
                {
                    printf("Teacher: %d scores %d\n", pids[i], scores[i]);
                }
                for(int i = 0; i<n;i++)
                {
                    if(fds[i]!=0)
                    {
                        if(close(fds[i]))
                            ERR("close");
                        fds[i]=0;
                    }
                }
                if(close(R))
                    ERR("close");
                return;
            }

            int score;
            memset(buffer,0,MAX_BUF);
            if((status=TEMP_FAILURE_RETRY(read(R, buffer, MAX_BUF)))!=MAX_BUF)
            {
                if(status<0 && errno ==EINTR)
                    break;
                if(status<0)
                    ERR("read");
                if(status==0)
                {
                    if(close(fds[i]))
                        ERR("close");
                    fds[i] = 0;
                    continue;
                }
            }
            sscanf(buffer, "%d %d", &pid, &score);
            if(score>=difficulty[j])
            {
                scores[i]++;
                printf("Teacher: Student %d finished stage %d (on success)\n", pid ,j);
                char message[MAX_BUF];
                snprintf(message, sizeof(message), "Success");
                memset(buffer,0, MAX_BUF);
                memcpy(buffer, message, strlen(message));
                buffer[MAX_BUF-1]='\0';
            }
            else
            {
                printf("Teacher: Student %d needs to fix stage %d (on failure)\n", pid,j);
                char message[MAX_BUF];
                snprintf(message, sizeof(message), "Failure");
                memset(buffer,0, MAX_BUF);
                memcpy(buffer, message, strlen(message));
                buffer[MAX_BUF-1]='\0';
            } 
            if(TEMP_FAILURE_RETRY(write(fds[i], buffer, MAX_BUF))!=MAX_BUF)
            {
                if(errno == EPIPE)
                {
                    if(close(fds[i]))
                        ERR("close");
                    fds[i]=0;
                    continue;
                }
                ERR("write");
            }
        }
    }
    if(close(R))
        ERR("close");
}

void child_work(int read_end, int write_end)
{
    srand(getpid());
    char buffer[MAX_BUF];
    int status, k = 3 + rand()%7;
    pid_t pid = getpid(), pid2;

    if((status=TEMP_FAILURE_RETRY(read(read_end, buffer, MAX_BUF)))!=MAX_BUF)
    {
        if(status<0 && errno ==EINTR){}
        if(status<0)
            ERR("read");
        if(status==0)
        {
            if(close(read_end))
                ERR("close");
            exit(EXIT_SUCCESS);
        }
    }
    sscanf(buffer, "Teacher: Is %d here?\n", &pid2);
    if(pid==pid2)
    {
        char message[MAX_BUF];
        snprintf(message, sizeof(message), "Student %d: HERE", pid);
        memset(buffer, 0, MAX_BUF);
        memcpy(buffer, message, strlen(message));
        buffer[MAX_BUF-1]='\0';
        printf("Student %d: HERE\n", pid);

        if(TEMP_FAILURE_RETRY(write(write_end, buffer, MAX_BUF))!=MAX_BUF)
        {
            if(errno == EPIPE)
            {
                if(close(read_end))
                    ERR("close");
                return;
            }
            ERR("write");
        }
    }
    int success_count=0;
    for(int i = 0; i<4;i++)
    {
        int t = 100 + rand() % 401;
        int q = 1 + rand() % 20;
        int score = k+q;
        struct timespec time = {0,t*1000000};
        while(nanosleep(&time,&time)>0 && errno==EINTR){}

        char message[MAX_BUF];
        snprintf(message, sizeof(message), "%d %d", pid, score);
        memset(buffer, 0, MAX_BUF);
        memcpy(buffer, message, strlen(message));
        buffer[MAX_BUF-1]='\0';

        if(TEMP_FAILURE_RETRY(write(write_end, buffer, MAX_BUF))!=MAX_BUF)
        {
            if(errno == EPIPE)
            {
                if(close(read_end))
                    ERR("close");
                printf("Student %d: Oh no, I haven't finished stage %d. I need more time.\n", pid,i+1);
                exit(EXIT_SUCCESS);
            }
            ERR("write");
        }
        
        memset(buffer, 0, MAX_BUF);
        if((status=TEMP_FAILURE_RETRY(read(read_end, buffer, MAX_BUF)))!=MAX_BUF)
        {
            if(status<0 && errno ==EINTR){}
            if(status<0)
                ERR("read");
            if(status==0)
            {
                if(close(read_end))
                    ERR("close");
                printf("Student %d: Oh no, I haven't finished stage %d. I need more time.\n", pid,i+1);
                exit(EXIT_SUCCESS);
            }
        }
        if(strncmp(buffer,"Success", 7)==0)
            success_count++;
    }
    if(success_count==4)
        printf("Student %d: I NAILED IT\n", pid);
}

void create_children_pipes(int n, int*fds, pid_t* pids,int R)
{
    pid_t pid;

    for(int i = 0; i<n;i++)
    {
        int S[2];
        if(pipe(S))
            ERR("pipe");

        if((pid=fork())<0)
            ERR("fork");
        if(pid==0)
        {
            int j = 0;
            while(j<i)
            {
                if(close(fds[j]))
                    ERR("close");
                j++;
            }
            if(close(S[1]))
                ERR("close");
            child_work(S[0], R);
            if(close(S[0])||close(R))
                ERR("close");
            exit(EXIT_SUCCESS);
        }
        else
        {
            if(close(S[0]))
                ERR("close");
            pids[i] = pid;
            fds[i] = S[1];
        }
    }
}

int main(int argc, char**argv)
{
    if(argc!=2)
        usage(argv[0]);
    int n = atoi(argv[1]), R[2];
    if(n<3 || n>20)
        usage(argv[0]);
    int* fds = malloc(sizeof(int)*n);
    pid_t* pids = malloc(sizeof(pid_t)*n);

    if(sethandler(sigchldhandler, SIGCHLD)==-1)
        ERR("sethandler");
    if(sethandler(sigalrmhandler,SIGALRM)==-1)
        ERR("sethandler");

    if(pipe(R)<0)
        ERR("pipe");
    
    create_children_pipes(n,fds,pids,R[1]);

    if(close(R[1]))
        ERR("close");
    parent_work(n, fds, pids,R[0]);
    
    pid_t pid;
    while((pid=waitpid(0,NULL,0))>0){}

    for(int i = 0; i<n;i++)
    {
        if(fds[i]!=0)
        {
            if(close(fds[i]))
                ERR("close");
        }
    }
    printf("Teacher: IT'S FINALLY OVER!\n");
    free(fds);
    free(pids);
    return EXIT_SUCCESS;
}