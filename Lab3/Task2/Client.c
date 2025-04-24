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
#define SHM_SIZE 1024

void usage(char* pname)
{
    fprintf(stderr, "USAGE: %s\n", pname);
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    if(argc!=2)
        usage(argv[0]);
    
    int sPid = atoi(argv[1]);
    printf("%d]n", sPid);
    if(sPid == 0)
        usage(argv[0]);
    
    srand(getpid());

    int shm_fd;
    char shm_name[16];
    snprintf(shm_name, 16, "/%d-board", sPid);

    if((shm_fd = shm_open(shm_name, O_RDWR, 0666))==-1)
        ERR("open");
    
    char* shm_ptr;
    if((shm_ptr = (char*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0))==MAP_FAILED)
        ERR("mmap");

    pthread_mutex_t* mutex = (pthread_mutex_t*)shm_ptr;
    char* sharedN = (shm_ptr + sizeof(pthread_mutex_t));
    char* board = (shm_ptr + sizeof(pthread_mutex_t) + 1);
    int n = sharedN[0];

    int score = 0;

    while(1)
    {
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
        int d = 1 + rand()%9;
        if(d == 1)
        {
            printf("Oops...\n");
        }

        int x = rand()%n;
        int y = rand()%n;
        printf("Trying to search field (%d, %d)\n", x, y);

        int num = board[n*y + x];
        if(num==0)
        {
            printf("GAME OVER: score %d\n", score);
            pthread_mutex_unlock(mutex);
            break;
        }
        else
        {
            printf("found %d points\n", num);
            score += num;
            board[n*y + x] = 0;
        }

        pthread_mutex_unlock(mutex);
        struct timespec t = {1,0};
        nanosleep(&t, &t);
    }
    close(shm_fd);
    munmap(shm_ptr, SHM_SIZE);
    return EXIT_SUCCESS;
}