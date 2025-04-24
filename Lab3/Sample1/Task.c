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

typedef struct
{
    char* file_memory;
    int* output_data;
    pthread_mutex_t* mxdata;
    int start;
    int end;
}thread_data;

void usage(char* pname)
{
    fprintf(stderr, "Usage%s\n", pname);
    exit(EXIT_FAILURE);
}

void* thread_work(void* args)
{
    thread_data* data = (thread_data*)args;
    for(int i = data->start; i<data->end;i++)
    {
        unsigned char c = (unsigned char)data->file_memory[i];
        pthread_mutex_lock(&data->mxdata[c]);
        data->output_data[c]++;
        pthread_mutex_unlock(&data->mxdata[c]);
    }
    return NULL;
}

void parent_work(int* output_data)
{
    for(int i = 0; i<256;i++)
    {
        if(output_data[i]>0)
            printf("%c: %d\n", i, output_data[i]);
    }
}

int main(int argc, char** argv)
{
    if(argc!=3)
        usage(argv[0]);
    int n = atoi(argv[1]);
    char* filename = argv[2];

    int fd;
    if((fd = open(filename, O_CREAT | O_RDWR, 0666))==-1)
        ERR("open");
    
    int fd2;
    if((fd2=shm_open("/output_data", O_CREAT | O_RDWR | O_TRUNC, 0666))==-1)
        ERR("shm_open");
    if (ftruncate(fd2, sizeof(int)*256) == -1)
        ERR("ftruncate");

    struct stat st;
    fstat(fd, &st);

    char* file_memory;
    if((file_memory = mmap(NULL,st.st_size,PROT_READ,MAP_SHARED,fd,0))==MAP_FAILED)
        ERR("mmap");
    
    int* output_data;
    if((output_data = mmap(NULL, sizeof(int)*256,PROT_WRITE | PROT_READ, MAP_SHARED,fd2,0))==MAP_FAILED)
        ERR("mmap");
    
    for(int i = 0; i<st.st_size;i++)
        write(STDOUT_FILENO, &file_memory[i],1);

    thread_data* data = (thread_data*)malloc(sizeof(thread_data)*n);
    pthread_t* tids = (pthread_t*)malloc(sizeof(pthread_t)*n);
    pthread_mutex_t* mxdata = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)*256);
    pthread_mutexattr_t*mxattr = (pthread_mutexattr_t*)malloc(sizeof(pthread_mutexattr_t)*256);
    if(!tids || !data || !mxdata || !mxattr)
        ERR("malloc");

    for(int i =0; i<256;i++)
    {
        pthread_mutexattr_init(&mxattr[i]);
        pthread_mutexattr_setpshared(&mxattr[i], PTHREAD_PROCESS_SHARED);
        pthread_mutexattr_setrobust(&mxattr[i], PTHREAD_MUTEX_ROBUST);
        pthread_mutex_init(&mxdata[i], &mxattr[i]);
    }

    int chunk_size = st.st_size/n;

    for(int i = 0; i<n;i++)
    {
        if(i == n-1)
            data[i].end = st.st_size;
        else
            data[i].end = (i+1)*chunk_size;

        data[i].start = i*chunk_size;
        data[i].file_memory = file_memory;
        data[i].output_data = output_data;
        data[i].mxdata = mxdata;

        if(pthread_create(&tids[i],NULL, thread_work,&data[i])==-1)
            ERR("pthread_create");
    }

    for(int i = 0; i<n;i++)
    {
        pthread_join(tids[i],NULL);
    }

    parent_work(output_data);

    for(int i = 0; i<256;i++)
    {
        pthread_mutex_destroy(&mxdata[i]);
        pthread_mutexattr_destroy(&mxattr[i]);
    }

    free(tids);
    free(data);
    free(mxdata);
    free(mxattr);
    close(fd);
    close(fd2);
    munmap(file_memory,st.st_size);
    munmap(output_data,sizeof(int)*256);
    return EXIT_SUCCESS;
}