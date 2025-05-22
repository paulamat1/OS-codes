#include<Sources.h>

#define BACKLOG 3
#define MAXBUF 576
#define MAXADDR 5

struct connections
{
    int free;
    int32_t chunkNo;
    struct sockaddr_in addr;
};

void usage(char *name) 
{ 
    fprintf(stderr, "USAGE: %s port\n", name); 
    exit(EXIT_FAILURE);
}

int make_socket(int domain, int type)
{
    int sock;
    if((sock = socket(domain, type, 0))<0)
        ERR("socket");
    return sock;
}

int bind_inet_socket(uint16_t port, int type)
{
    struct sockaddr_in addr;
    int socketfd, t=1;

    socketfd = make_socket(PF_INET, type);
    memset(&addr, 0, sizeof(struct sockaddr_in));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(setsockopt(socketfd, SOL_SOCKET,SO_REUSEADDR,&t, sizeof(t)))
        ERR("setsockopt");
    if(bind(socketfd,(struct sockaddr*)&addr, sizeof(addr))<0)
        ERR("bind");
    if(SOCK_STREAM == type)
    {
        if(listen(socketfd, BACKLOG)<0)
            ERR("listen");
    }
    return socketfd;
}

int findIndex(struct sockaddr_in addr, struct connections conn[MAXADDR])
{
    int empty=-1, pos=-1;
    for(int i = 0; i<MAXADDR;i++)
    {
        if(conn[i].free)
            empty = i;
        else if(memcmp(&addr, &(conn[i].addr), sizeof(struct sockaddr_in))==0)
        {
            pos = i;
            break;
        }
        if(pos==-1 && empty!=-1)
        {
            conn[empty].free = 0;
            conn[empty].chunkNo = 0;
            conn[empty].addr = addr;
            pos = empty;
        }
    }
    return pos;
}

void ServerWork(int fd)
{
    struct sockaddr_in addr;
    struct connections conn[MAXADDR];

    char buf[MAXBUF];
    socklen_t size = sizeof(addr);
    
    int32_t chunkNo, last;
    for(int i = 0;  i<MAXADDR; i++)
        conn[i].free=1;

    while(1)
    {
        if(TEMP_FAILURE_RETRY(recvfrom(fd, buf, MAXBUF, 0, &addr,&size))<0)
            ERR("recvfrom");
        int i;
        if((i = findIndex(addr, conn))>=0)
        {
            chunkNo = ntohl(*((int32_t*)buf));
            last = ntohl(*(((int32_t*)buf)+1));
            if(chunkNo>conn[i].chunkNo + 1)
                continue;
            else if(chunkNo == conn[i].chunkNo + 1)
            {
                if(last)
                {
                    printf("Last part %d\n%s\n", chunkNo, buf + 2 * sizeof(int32_t));
                    conn[i].free=1;
                }
                else
                    printf("Part %d\n%s\n", chunkNo, buf + 2 * sizeof(int32_t));
                conn[i].chunkNo++;
            }
            if(TEMP_FAILURE_RETRY(sendto(fd, buf, MAXBUF, 0, &addr, size))<0)
            {
                if(errno == EPIPE)
                    conn[i].free=1;
                else    
                    ERR("sendto");
            }
        }   
    }

}

int main(int argc, char** argv)
{
    if(argc!=2)
        usage(argv[0]);
    if(sethandler(SIG_IGN,SIGPIPE)==-1)
        ERR("sethandler");
    
    int fd = bind_inet_socket(atio(argv[1]),SOCK_DGRAM);
    ServerWork(fd);

    if(TEMP_FAILURE_RETRY(close(fd))<0)
        ERR("close");
    fprintf(stdout, "Server has terminated\n");

    return EXIT_SUCCESS;
}