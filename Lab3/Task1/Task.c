#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
#define ITERATIONS 1000
#define NUM 8

void usage(char* pname)
{
    fprintf(stderr, "USAGE%s\n", pname);
    exit(EXIT_FAILURE);
}

void child_work(int n, float* data, char* log)
{
    srand(getpid());
    int num = 0;
    for(int i = 0; i<ITERATIONS;i++)
    {
        double x = (double)rand() / RAND_MAX;
        double y = (double)rand() / RAND_MAX;
        if(x*x + y*y<=1.0)
            num++;
    }
    data[n] = (float)num / ITERATIONS;
    char buf[NUM+1];

    snprintf(buf, NUM + 1, "%7.5f\n", data[n]*4.0f); //formats data into 7-width line + \n
    memcpy(log + n*NUM, buf, NUM); //copies the buf content into the log memory location at the specified offset
    exit(EXIT_SUCCESS);
}

void parent_work(int n, float* data)
{
    pid_t pid;
    double sum = 0.0;

    for(int i = 0; i<n;i++)
    {
        pid = waitpid(0,NULL,WNOHANG);
        if(pid==-1)
        {
            if(errno == ECHILD)
                break;
            ERR("waitpid");
        }
    }
    for(int i = 0; i<n;i++)
        sum+=data[i];
    sum = sum/n;

    printf("PI is approximately %f\n", sum*4);
}

void create_children(int n, float* data, char* log)
{
    for(int i = 0; i<n;i++)
    {
        pid_t pid;
        if((pid = fork())==-1)
            ERR("fork");
        if(pid == 0)
            child_work(i, data, log);
    }
}

int main(int argc, char** argv)
{
    if(argc!=2)
        usage(argv[0]);
    int n = atoi(argv[1]);
    if(n<=0 || n>30)
        usage(argv[0]);

    int log_fd;
    if((log_fd = open("./log.txt", O_CREAT | O_RDWR | O_TRUNC, 0666))==-1)
        ERR("open");
    if(ftruncate(log_fd, n*NUM)==-1)
        ERR("ftruncate");
    //Without it the log.txt file would be size 0, because it’s opened with the O_TRUNC flag, 
    //which causes the function to remove the file’s contents. Otherwise, if the flag wouldn’t 
    //have been added, the size would have been dependent on the earlier contents, which would have been unpredictable.

    char* log;
    if((log = (char*)mmap(NULL, n*NUM, PROT_WRITE | PROT_READ, MAP_SHARED, log_fd, 0))==MAP_FAILED)
        ERR("mmap");
    
    float* data;
    if((data = (float*)mmap(NULL, n*sizeof(float), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1,0))==MAP_FAILED)
        ERR("mmap");

    create_children(n, data, log);
    parent_work(n, data);

    if(munmap(data, n*sizeof(float)))
        ERR("munmap");
    if(msync(log, n*NUM, MS_SYNC))
        ERR("msync");
    if(munmap(log, n*NUM))
        ERR("munmap");
    
    return EXIT_SUCCESS;
}