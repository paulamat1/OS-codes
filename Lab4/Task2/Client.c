#include<Sources.h>

#define MAXBUF 576
volatile sig_atomic_t last_signal = 0;

void usage(char *name) 
{ 
    fprintf(stderr, "USAGE: %s port\n", name); 
    exit(EXIT_FAILURE);
}

void sigalrm_handler(int sig) 
{ 
    last_signal = sig; 
}

int make_socket()
{
    int sock;
    if((sock = socket(PF_INET, SOCK_DGRAM, 0))<0)
        ERR("socket");
    return sock;
}

void sendAndConfirm(int fd, struct sockaddr_in addr, char *buf1, char *buf2, ssize_t size)
{
    struct itimerval ts;
    if (TEMP_FAILURE_RETRY(sendto(fd, buf1, size, 0, &addr, sizeof(addr))) < 0)
        ERR("sendto:");

    memset(&ts, 0, sizeof(struct itimerval));
    setitimer(ITIMER_REAL, &ts, NULL);
    last_signal = 0;

    struct sockaddr_in addr2;

    while (recvfrom(fd, buf2, size, 0, &addr2, sizeof(struct sockaddr)) < 0)
    {
        if (EINTR != errno)
            ERR("recv");
        if (SIGALRM == last_signal)
            break;
    }

}

void ClientWork(int fd, struct sockaddr_in addr, int file)
{
    char buf[MAXBUF];
    char buf2[MAXBUF];
    int offset = 2 * sizeof(int32_t);

    int32_t chunkNo = 0;
    int32_t last = 0;
    ssize_t size;

    while(!last)
    {
        size = bulk_read(file, buf+offset, MAXBUF-offset);
        if(size<0)
            ERR("bulk_read");
        chunkNo++;
        *((int32_t*)buf) = htonl(chunkNo);

        if(size< MAXBUF-offset)
        {
            last=1;
            memset(buf+offset+size, 0, MAXBUF-offset-size); 
            //To avoid sending random leftover data in the buffer to the receiver, you zero out the unused part.
        }
        *(((int32_t*)buf)+1) = htonl(last);
        
        memset(buf2, 0, MAXBUF);
        int counter = 0;
        int confirmed = 0;
        while(!confirmed && counter<5)
        {
            counter++;
            sendAndConfirm(fd, addr, buf, buf2, MAXBUF);

            if((*(int32_t*)buf2) == (int32_t)htonl(chunkNo))
                confirmed = 1;
        }

        if(!confirmed)
            break;
    }
}

int main(int argc, char** argv)
{
    if(argc!=4)
        usage(argv[0]);

    int fd, file;
    struct sockaddr_in addr;

    if(sethandler(SIG_IGN, SIGPIPE)==-1)
        ERR("sethandler");
    if(sethandler(sigalrm_handler, SIGALRM)==-1)
        ERR("sethandler");
    if((file = TEMP_FAILURE_RETRY(open(argv[3], O_RDONLY)))<0)
        ERR("open");

    fd = make_socket();
    addr = make_address(argv[1], argv[2]);
    ClientWork(fd,addr, file);

    if(TEMP_FAILURE_RETRY(close(fd))<0)
        ERR("close");
    if(TEMP_FAILURE_RETRY(close(file))<0)
        ERR("close");

    return EXIT_SUCCESS;
}
