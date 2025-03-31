#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h> 
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PRINTF(str)printf("[%s-%d]"#str"=%s\n",__func__,__LINE__,str);

//socket configuration
int starup(unsigned short *port)
{
   int sockfd =socket(PF_INET,SOCK_STREAM,0);
   if (sockfd<0)
   {
    perror("socket error");
    exit(1);
   }
   //port reuse
   int  opt =1;
   int ret =setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
   if (ret <0)
   {
    perror("setsockopt error");
    exit(1);
   }
   
    struct sockaddr_in serveraddr;

    memset(&serveraddr,0,sizeof(serveraddr));
    serveraddr.sin_family =AF_INET;
    serveraddr.sin_port = htons((*port));
    serveraddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd,(struct sockaddr*)&serveraddr,sizeof(serveraddr))<0)
    {
        perror("bind error");
        exit(1);
    }
    
    int namelen =sizeof(serveraddr);
    if(*port == 0)
    {
      if (getsockname(sockfd,(struct sockaddr*)&serveraddr,&namelen)<0)
      {
        perror("getsockname error");
        exit(1);
      }
      *port =serveraddr.sin_port;
    }

    if (listen(sockfd,10)<0)
    {
        perror("listen error");
        exit(1);
    }

    return sockfd;   
}

// algorithm remember
int get_line(int sock,char *buff,int size)
{
    char c = 0; //'\0'
    int i = 0;

    //\r\n
    while ( i<size-1 && c !='\n')
    {
        int  n =recv(sock,&c,1,0);
        if (n>0)
        {
            if (c == '\r')
            {
               //peek
               n = recv(sock,&c,1,MSG_PEEK);
               if (n>0 && c=='\n')
               {
                   recv(sock,&c,1,0);
               }else{
                  c ='\n';
               }
            }
            buff[i++] = c;
        }else{
            c = '\n';
        }
    }
    buff[i] = 0; //'\0'
    return i;
}

void unimplement(int client)
{

}

void not_found(int client)
{
    //404 response
    char buff[1024];

    strcpy(buff,"HTTP/1.0 404 OK\r\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"Server: TestHttpd/0.1\r\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"Content-type:text/html\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"\r\n");
    send(client,buff,strlen(buff),0);

    //send 404 webpage
}


void headers(int client,const char*type)
{
     char buff[1024];
     //send in socket model
     strcpy(buff,"HTTP/1.0 200 OK\r\n");
     send(client,buff,strlen(buff),0);

     strcpy(buff,"Server: TestHttpd/0.1\r\n");
     send(client,buff,strlen(buff),0);

    //  strcpy(buff,"Content-type:text/html\n");
    //  send(client,buff,strlen(buff),0);
     char typebuf[1024];
     sprintf(typebuf,"Content-type:%s\r\n",type);
     send(client,typebuf,strlen(typebuf),0);

     strcpy(buff,"\r\n");
     send(client,buff,strlen(buff),0);

}

void cat(int client, FILE *resource)
{
   //send data to the client
   char buff[4096];
   int count = 0;
 
   while (1)
   {
    //read file from the source directory
    int ret = fread(buff,sizeof(char),sizeof(buff),resource);
     if (ret <=0)
     {
        break;
     }
     // send html file to the browser
     send(client,buff,ret,0);
     count +=ret;
   }
   printf("count=%d\n",count);
   
}



void server_file(int client, const char* fileName)
{
   int numchars = 1;
   char buff[1024];

  //read the rest data 
   while (numchars>0 && strcmp(buff,"\n"))
   {
      numchars = get_line(client,buff,sizeof(buff));
      PRINTF(buff);
   }

   //open html file
   FILE *resoure = fopen(fileName,"r");

   if (resoure ==NULL)
   {
       not_found(client);
   }else{
        //send headers to browser
        headers(client,getHeadType(fileName));
        //send data to browser
        cat(client,resoure);
        printf("resource sent\n");
   }

   fclose(resoure);
}

const char* getHeadType(const char *fileName)
{
      const char * ret = "text/html";
      const char * p =strrchr(fileName,'.');

      if(!p) return ret;

      p++;
      if (!strcmp(p,"css"))ret ="text/css";
      else if(!strcmp(p,"jpg"))ret ="image/jpg";
      else if(!strcmp(p,"png"))ret ="image/png";
      else if(!strcmp(p,"js"))ret ="applicaion/x-javascript";

      return ret;
}


void* th_fn(void *arg)
{
   // void* change to int* ,decode * get int type value 
   char buff[1024];
   int fd = *(int*) arg;
   //receive data from client
   int numchar = get_line(fd,buff,sizeof(buff));
   PRINTF(buff);
   //GET or POST method
   char method[255];
   int j = 0,i =0;
   while (  !isspace(buff[j])  && i <sizeof(method)-1)
   {
     method[i++] = buff[j++];
   }

   method[i] =0;
   PRINTF(method);

   //decode 
   //wwww.google.com/abc/test.html
   //GET./abc/test.html HTTP/1.1\n
   if ( strcasecmp(method,"GET") && strcasecmp(method,"POST"))
   {
       unimplement(fd);
       return (void*)0;
   }
   

   //GET./HTTP/1.1\n
   char url[255];
   i = 0;
   while (isspace(buff[j]) && j <sizeof(buff))
   {
     j++;
   }

   while (!isspace(buff[j]) && i<sizeof(url)-1 && j <sizeof(buff))
   {
      url[i++] = buff[j++];
   }

   url[i] =0;
   PRINTF(url);
   
   //www.test.com
   //127.0.0.1:8000/test.html
   // url /test.html
   //htdos/test.html

   //define a char
   char path[512]="";
   //static resources in the server
   //url:/abc/  directory:/abc
   //save url "/" or "/about/" to path:htdos/about/
   sprintf(path,"htdos%s",url);
   if (path[strlen(path)-1] == '/')
   {
       //combine path "/"+"index.html"
       strcat(path,"index.html");
   }
   //htdos/index.html
   PRINTF(path);
   //file data struct 
   struct stat status;
   if (stat(path,&status)==-1)
   {
       //request data read finish
       while (numchar>0 && strcmp(buff,"\n"))
       {
         numchar =get_line(fd,buff,sizeof(buff));
       }
       not_found(fd);
       printf("Not Found!!!\n");
   }else{
     
       // st.mode:file type and permission
       //__S_IFDIR :directory,__S_IREG :regular file,if p"htdos" is the directoty,add /index.html
       //127.0.0.1:8000/abc
       if ((status.st_mode & __S_IFMT)==__S_IFDIR)
       {
          strcat(path,"/index.html");
      
       }
       server_file(fd,path);  
   }



   
   close(fd);
   return (void*)0;
}



int main(int argc, char const *argv[])
{
    unsigned short port = 8000;
    int server_sock = starup(&port);
    printf("Http server is running, listening %d port\n",port);


    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);
    //Setting thread seperate attr.
     pthread_attr_t attr;
     pthread_attr_init(&attr);
     pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    
    while (1)
    {
        int fd = accept(server_sock,(struct sockaddr*)&clientaddr,&len);
      
        if (fd<0)
        {
          perror("accept error");
          continue;
        }

         pthread_t th;
         int err;
         if ((err= pthread_create(&th,&attr,th_fn,(void*)&fd))!=0)
         {
            perror("pthread create error");
         }

         pthread_attr_destroy(&attr);

    }


    close(server_sock);
    return 0;
}
