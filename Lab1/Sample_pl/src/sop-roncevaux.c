#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define MAX_KNIGHT_NAME_LENGTH 20

typedef struct
{
    char name[MAX_KNIGHT_NAME_LENGTH];
    int HP;
    int attack;
}knight_info;


int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void msleep(int millisec)
{
    struct timespec tt;
    tt.tv_sec = millisec / 1000;
    tt.tv_nsec = (millisec % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

int count_descriptors()
{
    int count = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat stats;
    if ((dir = opendir("/proc/self/fd")) == NULL)
        ERR("opendir");
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    chdir("/proc/self/fd");
    do
    {
        errno = 0;
        if ((entry = readdir(dir)) != NULL)
        {
            if (lstat(entry->d_name, &stats))
                ERR("lstat");
            if (!S_ISDIR(stats.st_mode))
                count++;
        }
    } while (entry != NULL);
    if (chdir(path))
        ERR("chdir");
    if (closedir(dir))
        ERR("closedir");
    return count - 1;  // one descriptor for open directory
}

void create_pipes(int * fdSR,int* fdSW,int nS, int* fdFR,int*fdFW, int nF)
{
    for(int i = 0; i<nS;i++)
    {
        int P[2];
        if(pipe(P))
            ERR("pipe");
        fdSR[i] = P[0];
        fdSW[i] = P[1];
    }
    for(int i = 0; i<nF;i++)
    {
        int P[2];
        if(pipe(P))
            ERR("pipe");
        fdFR[i] = P[0];
        fdFW[i] = P[1];
    }
}

void child_work_S(int fdR,knight_info* kS,int* fdW, int nF)
{
    srand(getpid());
    printf("I am Spanish knight %s. I will serve my king with my %d HP and %d attack.\n", kS->name,kS->HP,kS->attack);
    
    int flags = fcntl(fdR, F_GETFL, 0);
    if (flags == -1)
        ERR("fcntl");
    if (fcntl(fdR, F_SETFL, flags | O_NONBLOCK) == -1)
        ERR("fcntl F_SETFL");

    int aliveEnemies=nF;
    char n;
    int status;
    while(kS->HP>0)
    {
        if((status=read(fdR,&n, 1))!=1)
        {
            if(status==-1 && errno!=EAGAIN)
                ERR("read");
            if(status==0)
            {
                if(close(fdR))
                    ERR("close");
                break;
            }
        }
        kS->HP -= n;
        int enemy = rand()%nF;
        char strength = rand() % (kS->attack + 1);
        if((status=write(fdW[enemy],&strength, 1))!=1)
        {
            if(errno!=EPIPE)
                ERR("read");
            else
            {
                if(close(fdW[enemy]))
                    ERR("close");
            }
        }
        if(strength==0)
            printf("%s attacks his enemy, however he deflected\n",kS->name);
        if(strength>=1 && strength<=5)
            printf("%s goes to strike, he hit right and well\n", kS->name);
        if(strength>=6)
            printf("%s strikes powerful blow, the shield he breaks and inflicts a big wound\n", kS->name);
        int t = 1 + rand()% 10;
        msleep(t);
    }
    if(close(fdR))
        ERR("close");
    printf("%s dies\n", kS->name);
    exit(EXIT_SUCCESS);
}

void child_work_F(int fdR,knight_info* kF,int* fdW,int nS)
{
    printf("I am Frankish knight %s. I will serve my king with my %d HP and %d attack.\n", kF->name,kF->HP,kF->attack);
    srand(getpid());
    
    int flags = fcntl(fdR, F_GETFL, 0);
    if (flags == -1)
        ERR("fcntl");
    if (fcntl(fdR, F_SETFL, flags | O_NONBLOCK) == -1)
        ERR("fcntl F_SETFL");

    int aliveEnemies = nS;
    char n;
    int status;
    while(kF->HP>0)
    {
        if((status=read(fdR,&n, 1))!=1)
        {
            if(status==-1 && errno!=EAGAIN)
                ERR("read");
            if(status==0)
            {
                if(close(fdR))
                    ERR("close");
                break;
            }
        }
        kF->HP -= n;
        int enemy = rand()%nS;
        char strength = rand() % (kF->attack+1);
        if((status=write(fdW[enemy],&strength, 1))!=1)
        {
            if(errno!=EPIPE)
                ERR("read");
            else
            {
                if(close(fdW[enemy]))
                    ERR("close");
            }
        }
        if(strength==0)
            printf("%s attacks his enemy, however he deflected\n",kF->name);
        if(strength>='1' && strength<=5)
            printf("%s goes to strike, he hit right and well\n", kF->name);
        if(strength>=6)
            printf("%s strikes powerful blow, the shield he breaks and inflicts a big wound\n", kF->name);
        int t = 1 + rand()% 10;
        msleep(t*1000000);
    }
    if(close(fdR))
        ERR("close");
    printf("%s dies\n", kF->name);
    exit(EXIT_SUCCESS);
}

void create_children(int * fdSR,int* fdSW,int nS,knight_info* kS,int* fdFR,int*fdFW, int nF, knight_info *kF)
{
    pid_t pid;
    for(int i = 0; i<nS;i++)
    {
        if((pid = fork())<0)
            ERR("fork");
        if(pid==0)
        {
            for(int j=0;j<nS;j++)
            {
                if(j!=i)
                {
                    if(close(fdSR[j]))
                        ERR("close");
                }
            }
            child_work_S(fdSR[i],&kS[i],fdFW,nS);
        }
    }
    for(int i = 0; i<nF;i++)
    {
        if((pid = fork())<0)
            ERR("fork");
        if(pid==0)
        {
            for(int j=0;j<nF;j++)
            {
                if(j!=i)
                {
                    if(close(fdFR[j]))
                        ERR("close");
                }
            }
            child_work_F(fdFR[i],&kF[i], fdSW, nS);
        }
    }
}

int main(int argc, char* argv[])
{
    srand(time(NULL));
    printf("Opened descriptors: %d\n", count_descriptors());

    FILE* saraceni, *franci;
    if((saraceni = fopen("saraceni.txt","r"))==NULL)
    {
        printf("Saracens have not arrived on the battlefield");
        exit(EXIT_SUCCESS);
    }
    if((franci = fopen("franci.txt","r"))==NULL)
    {
        printf("Franks have not arrived on the battlefield");
        exit(EXIT_SUCCESS);
    }
    int nS, nF;
    if(fscanf(franci,"%d\n", &nF)<0)
        ERR("fscanf");
    if(fscanf(saraceni,"%d\n", &nS)<0)
        ERR("fscanf");
    
    knight_info* knightS = (knight_info*)malloc(sizeof(knight_info)*nS);
    knight_info* knightF = (knight_info*)malloc(sizeof(knight_info)*nF);
    if(!knightF || !knightS)
        ERR("malloc");
    
    for(int i = 0; i<nS;i++)
    {
        if(fscanf(saraceni,"%s %d %d\n",knightS[i].name,&knightS[i].HP,&knightS[i].attack)<0)
            ERR("fscanf");
       // printf("I am Spanish knight %s. I will serve my king with my %d HP and %d attack.\n",knightS[i].name, knightS[i].HP, knightS[i].attack);
    }

    for(int i = 0; i<nF;i++)
    {
        if(fscanf(franci,"%s %d %d\n",knightF[i].name,&knightF[i].HP,&knightF[i].attack)<0)
            ERR("fscanf");
       // printf("I am Frakish knight %s. I will serve my king with my %d HP and %d attack.\n",knightF[i].name, knightF[i].HP, knightF[i].attack);
    }

    if(fclose(saraceni) || fclose(franci))
        ERR("fclose");

    int* fdSR = (int*)malloc(sizeof(int)*nS);
    int* fdFR = (int*)malloc(sizeof(int)*nF);
    int* fdSW = (int*)malloc(sizeof(int)*nS);
    int* fdFW = (int*)malloc(sizeof(int)*nF);
    if(!fdSR || !fdSW || !fdFR || !fdFW)
        ERR("malloc");
    create_pipes(fdSR,fdSW,nS,fdFR,fdFW,nF);
    create_children(fdSR,fdSW,nS,knightS,fdFR,fdFW,nF,knightF);

    pid_t pid;
    while((pid=waitpid(0,NULL,0))>0){}
    for(int i = 0; i<nS;i++)
    {
        if(close(fdSR[i])|| close(fdSW[i]))
            ERR("close");
    }
    for(int i = 0; i<nF;i++)
    {
        if(close(fdFR[i])|| close(fdFW[i]))
            ERR("close");
    }

    free(knightF);
    free(knightS);
    free(fdSR);
    free(fdSW);
    free(fdFR);
    free(fdFW);
}
