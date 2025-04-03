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
#define MAX_BUF 16

void usage(char* pname)
{
    fprintf(stderr, "USAGE: %s\n", pname);
    exit(EXIT_SUCCESS);
}

int sethandler(void (*f)(int), int sig)
{
    struct sigaction act;
    act.sa_handler = f;
    if(sigaction(sig, &act, NULL)==-1)
       return -1;
    return 0;
}

void sigchld_handler(int sig)
{
    pid_t pid;

    while(1)
    {
        pid = waitpid(0, NULL, WNOHANG);
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

void premature_termination()
{
   srand(getpid());
   if(rand()%20 == 0)
      exit(EXIT_FAILURE);
}

void parent_work(int* fdr, int* fds, int n, int m)
{
   int status;

   char buffer[MAX_BUF], results[MAX_BUF];
   memset(buffer, 0, MAX_BUF);
   memset(results, 0, MAX_BUF);

   char* message = "New Round";
   strncpy(buffer, message, MAX_BUF - 1);
   buffer[MAX_BUF-1] = '\0';

   int* scores = calloc(n,sizeof(int));
   if(!scores)
      ERR("malloc");

   for(int j = 0; j<m;j++)
   {
      int* roundResults = calloc(n,sizeof(int));
      if(!roundResults)
         ERR("malloc");

      for(int i = 0; i<n; i++)
      {
         if(fdr[i]==0 && fds[i]==0)
            continue;
         if((status=TEMP_FAILURE_RETRY(write(fds[i],buffer, sizeof(buffer))))!=sizeof(buffer))
         {
            if(errno == EPIPE)
            {
               if(close(fdr[i]) || close(fds[i]))
                  ERR("close");
               fdr[i] = 0, fds[i]=0;
               continue;
            }
            ERR("write");
         }
      }
      for(int i = 0; i<n; i++)
      {
         if (fdr[i] == 0 && fds[i] == 0) continue;  
         if((status = TEMP_FAILURE_RETRY(read(fdr[i], results, sizeof(results))))!=sizeof(results))
         {
            if(errno ==EINTR)
               continue;
            if(status==0)
            {
               if(close(fdr[i]) || close(fds[i]))
                  ERR("close");
               fdr[i] = 0, fds[i]=0;
               continue;
            }
         }
         roundResults[i] = *(int*)results;
      }

      int result=0, count=0;
      for(int i = 0; i<n;i++)
      {
         if(fdr[i]==0 && fds[i]==0)
            continue;
         if(roundResults[i]>result)
         {
            count=1;
            result = roundResults[i];
         }
         else if(roundResults[i]==result)
            count++;
      }
      memset(results, 0, MAX_BUF);
      int win = result/count;

      for(int i = 0; i<n;i++)
      {
         if(fdr[i]==0 && fds[i]==0)
            continue;
         if(roundResults[i]==result)
         {
            win = result/count;
            memcpy(results, &win, sizeof(int));
            results[MAX_BUF-1]='\0';
            scores[i]++;
            if((status=TEMP_FAILURE_RETRY(write(fds[i], results, sizeof(results))))!=sizeof(results))
            {
               if(errno == EPIPE)
               {
                  if(close(fds[i])||close(fdr[i]))
                     ERR("close");
                  fds[i]=fdr[i]=0;
                  continue;
               }
               ERR("write");
            }
         }
         else
         {  
            win=0;
            memcpy(results,&win, sizeof(int));
            results[MAX_BUF-1]='\0';
            if((status=TEMP_FAILURE_RETRY(write(fds[i], results, sizeof(results))))!=sizeof(results))
            {
               if(errno == EPIPE)
               {
                  if(close(fds[i])||close(fdr[i]))
                     ERR("close");
                  fds[i]=fdr[i]=0;
                  continue;
               }
               ERR("write");
            }
         }
      }

      free(roundResults);
   }

   free(scores);
}

void child_work(int read_end, int write_end, int m)
{
   srand(getpid());
   int* cards = (int*)malloc(sizeof(int)*m);
   for(int i = 0; i<m;i++)
      cards[i] = i+1;
   
   char buffer[MAX_BUF];
   char message[MAX_BUF];
   memset(message, 0, sizeof(message));
   int status, card;

   for(int i = 0; i<m;i++)
   {
      if((status =TEMP_FAILURE_RETRY(read(read_end,buffer, sizeof(buffer))))!=sizeof(buffer))
      {
         if(errno==EINTR)
            continue;
         if(status<0)
            ERR("read");
         if(status==0)
         {
            free(cards);
            return;
         }
      }
      premature_termination();
      while(1)
      {
         card = rand()%m;
         if(cards[card]!=0)
            break;
      }
      memcpy(message, &cards[card], sizeof(int));
      message[MAX_BUF-1]='\0';
      printf("PID: %d Sending %d\n", getpid(),*(int*)message);
      if((status = TEMP_FAILURE_RETRY(write(write_end, message, sizeof(message))))!=sizeof(message))
      {
         if(errno == EPIPE)
         {
            free(cards);
            return;
         }
         ERR("write");
      }
      cards[card]=0;
      memset(message, 0, sizeof(message));
      if((status=TEMP_FAILURE_RETRY(read(read_end,message, sizeof(message))))!=sizeof(message))
      {
         if(errno==EINTR)
            continue;
         if(status<0)
            ERR("read");
         if(status==0)
         {
            free(cards);
            return;
         }
      }
      printf("WON %d points\n",*(int*)message);
   }
   free(cards);
}

void create_children_pipes(int* fds, int* fdr, int n,int m)
{
   pid_t pid;
   for(int i = 0; i<n;i++)
   {
      int R[2],S[2], j=0;
      if(pipe(R) || pipe(S))
         ERR("pipe");

      if((pid=fork())==-1)
         ERR("fork");

      if(pid==0)
      {
         while(j<i)
         {
            if(close(fds[j]) || close(fdr[j]))
               j++;
         }
         free(fds), free(fdr);
         if(close(R[1])|| close(S[0]))
            ERR("close");

         child_work(R[0], S[1],m);

         if(close(R[0])||close(S[1]))
            ERR("close");
         exit(EXIT_SUCCESS);
      }
      if(close(R[0]) || close(S[1]))
         ERR("close");
      fdr[i] = S[0], fds[i] = R[1];
   }
}

int main(int argc, char** argv)
{
   if(argc!=3)
       usage(argv[0]);
   int n = atoi(argv[1]);
   int m = atoi(argv[2]);

   if(n<2 || n>5 || m<5 || m>10)
      usage(argv[0]);
    
   if(sethandler(sigchld_handler, SIGCHLD)==-1)
      ERR("sethandler");
      
   int* fds = (int*)malloc(sizeof(int) * n);
   int* fdr = (int*)malloc(sizeof(int) * n);
   if(!fds || !fdr)
      ERR("malloc");
   create_children_pipes(fds, fdr,n,m);

   parent_work(fdr, fds,n,m);

   for(int i = 0; i<n; i++)
   {
      if(fds[i]!=0 && fdr[i]!=0)
      {
         if(close(fds[i]) || close(fdr[i]))
         ERR("close");
      }
   }
   free(fds);
   free(fdr);
   return EXIT_SUCCESS;
}