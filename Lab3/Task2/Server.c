#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
#define SHM_SIZE 1024

typedef struct 
{
    int running;
    pthread_mutex_t mutex;
    sigset_t oldmask, newmask;
}signalhandling_args_t;

void usage(char* pname)
{
    fprintf(stderr, "USAGE: %s\n", pname);
}

void* signalHandler(void* args)
{
    signalhandling_args_t* signalhandling_args = (signalhandling_args_t*)args;

    int signo;
    if(sigwait(&signalhandling_args->newmask, &signo))
        ERR("sigwait");
    if(signo != SIGINT)
        ERR("unexpected signal");

    pthread_mutex_lock(&signalhandling_args->mutex);
    signalhandling_args->running = 0;
    pthread_mutex_unlock(&signalhandling_args->mutex);

    return NULL;
}

int main(int argc, char** argv)
{
    if(argc!=2)
        usage(argv[0]);

    int n = atoi(argv[1]);
    if(n<3 || n>100)
        usage(argv[0]);
    
    pid_t pid = getpid();
    srand(getpid());

    printf("My PID is %d\n", pid);

    int shm_fd;
    char shm_name[16];
    snprintf(shm_name, 16, "/%d-board", pid);

    if((shm_fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0666))==-1)
        ERR("shm_open");
    if(ftruncate(shm_fd, SHM_SIZE)==-1)
        ERR("ftruncate");
    
    char* shm_ptr;
    if((shm_ptr = (char*)mmap(NULL, SHM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED,shm_fd, 0))==MAP_FAILED)
        ERR("mmap");
    
    pthread_mutex_t* mutex = (pthread_mutex_t*)shm_ptr;
    char* Shared_N = (shm_ptr + sizeof(pthread_mutex_t));
    char* board = (shm_ptr + sizeof(pthread_mutex_t) + 1);
    Shared_N[0] = n;

    for(int i = 0; i<n;i++)
    {
        for(int j = 0; j<n;j++)
        {
            board[i*n + j] = 1 + rand()%9;
        }
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(mutex, &attr);

    signalhandling_args_t signalhandling_args = {1,PTHREAD_MUTEX_INITIALIZER};
    sigemptyset(&signalhandling_args.newmask);
    sigaddset(&signalhandling_args.newmask,SIGINT);
    if(pthread_sigmask(SIG_BLOCK, &signalhandling_args.newmask,&signalhandling_args.oldmask)==-1)
        ERR("pthread_sigmask");
    
    pthread_t signaling_thread;
    pthread_create(&signaling_thread,NULL, signalHandler,&signalhandling_args);

    while(1)
    {
        pthread_mutex_lock(&signalhandling_args.mutex);
        if(!signalhandling_args.running)
        {
            pthread_mutex_unlock(&signalhandling_args.mutex);
            break;
        }
        pthread_mutex_unlock(&signalhandling_args.mutex);

        int error;
        if((error = pthread_mutex_lock(mutex))!=0)
        {
            if(error == EOWNERDEAD)
            {
                pthread_mutex_consistent(mutex);
            }
            else
                ERR("pthread_mutex_lock");
        }
        for(int i = 0; i<n;i++)
        {
            for(int j = 0 ; j<n;j++)
            {
                printf("%d", board[i*n+j]);
            }
            printf("\n");
        }
        printf("\n");
        pthread_mutex_unlock(mutex);
        struct timespec t = {3,0};
        nanosleep(&t, &t);
    }

    pthread_join(signaling_thread, NULL);

    pthread_mutexattr_destroy(&attr);
    pthread_mutex_destroy(mutex);

    
    munmap(shm_ptr, SHM_SIZE);
    shm_unlink(shm_name);

    return EXIT_SUCCESS;
}