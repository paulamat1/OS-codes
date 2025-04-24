#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>       
#include <fcntl.h> 
#include <semaphore.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
volatile __sig_atomic_t last_signal = 0;
typedef struct
{
    float a;
    float b;
    int total_samples;
    int successful_hits;
    int num;
    pthread_mutex_t mxnum;
}shm_data;

// Values of this function are in range (0,1]
double func(double x)
{
    usleep(2000);
    return exp(-x * x);
}

/**
 * It counts hit points by Monte Carlo method.
 * Use it to process one batch of computation.
 * @param N Number of points to randomize
 * @param a Lower bound of integration
 * @param b Upper bound of integration
 * @return Number of points which was hit.
 */
int randomize_points(int N, float a, float b)
{
    int result = 0;
    for (int i = 0; i < N; ++i)
    {
        double rand_x = ((double)rand() / RAND_MAX) * (b - a) + a;
        double rand_y = ((double)rand() / RAND_MAX);
        double real_y = func(rand_x);

        if (rand_y <= real_y)
            result++;
    }
    return result;
}

/**
 * This function calculates approximation of integral from counters of hit and total points.
 * @param total_randomized_points Number of total randomized points.
 * @param hit_points Number of hit points.
 * @param a Lower bound of integration
 * @param b Upper bound of integration
 * @return The approximation of integral
 */
double summarize_calculations(uint64_t total_randomized_points, uint64_t hit_points, float a, float b)
{
    return (b - a) * ((double)hit_points / (double)total_randomized_points);
}

/**
 * This function locks mutex and can sometime die (it has 2% chance to die).
 * It cannot die if lock would return an error.
 * It doesn't handle any errors. It's users responsibility.
 * Use it only in STAGE 4.
 *
 * @param mtx Mutex to lock
 * @return Value returned from pthread_mutex_lock.
 */
int random_death_lock(pthread_mutex_t* mtx)
{
    int ret = pthread_mutex_lock(mtx);
    if (ret)
        return ret;

    if (rand() % 50 == 0)
        abort();
    return ret;
}

int sethandler(void(*f)(int), int sig)
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = f;
    if(sigaction(sig, &act,NULL)==-1)
        return -1;
    return 1;
}

void sigint_handler(int sig)
{
    last_signal = sig;
}

void usage(char* argv[])
{
    printf("%s a b N - calculating integral with multiple processes\n", argv[0]);
    printf("a - Start of segment for integral (default: -1)\n");
    printf("b - End of segment for integral (default: 1)\n");
    printf("N - Size of batch to calculate before reporting to shared memory (default: 1000)\n");
}

int main(int argc, char* argv[])
{
    if(argc != 4)
        usage(argv);
    
    float a = atof(argv[1]);
    float b = atof(argv[2]);
    int N = atoi(argv[3]);

    if(sethandler(sigint_handler, SIGINT)==-1)
        ERR("sethandler");

    int fd;
    if((fd = shm_open("/output_data", O_RDWR | O_CREAT, 0666))==-1)
        ERR("shm_open");
    if(ftruncate(fd, sizeof(shm_data))==-1)
        ERR("ftruncate");
    shm_data* data;
    if((data = mmap(NULL, sizeof(shm_data), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0))==MAP_FAILED)
        ERR("mmap");
    
    sem_t* sem;
    if((sem = sem_open("/semaphore", O_CREAT, 0666, 1))==NULL)
        ERR("sem");

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);

    if(sem_wait(sem) == -1)
        ERR("sem_wait");
    
    if(data->num == 0)
    {
        data->a = a;
        data->b = b;
        data->total_samples=0;
        data->successful_hits = 0;
        pthread_mutex_init(&data->mxnum, &attr);
    }
    
    data->num++;
    printf("Process %d joined. There are %d active processes.\n", getpid(), data->num);
    if (sem_post(sem) == -1)
        ERR("sem_post");

    for(int i = 0; i<3;i++)
    {
        if(last_signal == SIGINT)
            break;
        int num_hits = randomize_points(N,data->a,data->b);

        int error;
        if((error = random_death_lock(&data->mxnum))!=0)
        {
            
            if (error == EOWNERDEAD)
            {
                pthread_mutex_consistent(&data->mxnum);
                data->num--;
            }
            else
            {
                ERR("pthread_mutex_lock");
            }
        }
        data->successful_hits += num_hits;
        data->total_samples +=N;
        printf("PID %d: batch %d, total samples:%d, total hits: %d\n", getpid(), i+1, data->total_samples, data->successful_hits);
        if(pthread_mutex_unlock(&data->mxnum)!=0)
            ERR("pthread_mutex_unlock");
        sleep(2);
    }

    if(sem_wait(sem) == -1)
        ERR("sem_wait");
    sleep(2);
    if(data->num == 1)
    {
        double result = summarize_calculations(data->total_samples, data->successful_hits, data->a,data->b);
            printf("Result: %f\n", result);
        if(shm_unlink("/output_data")==-1)
            ERR("shm_unlink");
        if(sem_unlink("/semaphore")==-1)
            ERR("sem_unlink");
        if(pthread_mutexattr_destroy(&attr)==-1)
            ERR("pthread_mutexattr_destroy");
    }
    data->num--;
    if(sem_post(sem)==-1)
        ERR("sem_post");

    munmap(data, sizeof(shm_data));
    close(fd);
    sem_close(sem);

    return EXIT_SUCCESS;
}