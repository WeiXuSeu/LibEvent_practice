/* Handle multiple socket connections with select and fd_set on Linux */
  
#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
  
#define TRUE   1
#define FALSE  0
#define PORT 8888
#define CLIENT_NUM 2
#define BLOCK_NUM 2
#define BUFFER_SIZE 1024
 
int main(int argc , char *argv[])
{
    int opt = TRUE;
    int master_socket , addrlen , new_socket , client_socket[CLIENT_NUM] , activity, i , valread , sd;
    int max_sd;
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE+1];  //data buffer of 1K
    struct timeval tv;
    char quit[7]="quit\r\n";     //for data from telnet client, the data end with "\r\n"
   
    //set of socket descriptors
    fd_set readfds;
      
    //a message retuan to client when it connect to the server. 
    char *message = "ECHO Daemon v1.0 \r\n";
  
    //initialise all client_socket[] to 0 so not checked
    for (i = 0; i < CLIENT_NUM; i++) 
    {
        client_socket[i] = 0;
    }
    printf("size of buffer:%lu\n",sizeof(buffer));
    memset(buffer,0,sizeof(buffer)*sizeof(buffer[0]));
      
    //create a master socket for listening
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) 
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
  
    /*一个端口释放后会等待两分钟之后才能再被使用，SO_REUSEADDR是让端口释放后立即就可以被再次使用。*/
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
  
    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
      
    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);
     
    //try to specify maximum of BLOCK_NUM pending connections for the master socket
    if (listen(master_socket, BLOCK_NUM) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
      
    //accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");
    
    /********************************call select in a infinite loop*******************************/ 
    while(TRUE) 
    {
        /*每次调用select前都要重新设置文件描述符和时间，因为事件发生后，文件描述符和时间都被内核修改啦*/
        FD_ZERO(&readfds);
  
        /*添加监听套接字*/
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;
		
		tv.tv_sec = 30;
		tv.tv_usec = 0;
         
        /*添加客户端套接字*/
        for ( i = 0 ; i < CLIENT_NUM ; i++) 
        {
            //socket descriptor
            sd = client_socket[i];
             
            //if valid socket descriptor then add to read list
            if(sd > 0)
                FD_SET( sd , &readfds);
             
            //highest file descriptor number, need it for the select function
            if(sd > max_sd)
                max_sd = sd;
        }
  
	printf("\nselect begin, timeout left:%lds-%ldms.\n",tv.tv_sec,tv.tv_usec);
        //wait for an activity on one of the sockets, timeout is tv; 
        activity = select( max_sd + 1 , &readfds , NULL , NULL , &tv);
   	
        //return value < 0, error happend; 
        if ((activity < 0) && (errno!=EINTR)) 
        {
            printf("select error");
        }
	//return value = 0, timeout;
	else if(activity == 0){
	    //no event happend in specific time, timeout;
	    printf("select return, activity:%d,  timeout left:%lds-%ldms.\n", activity, tv.tv_sec, tv.tv_usec);
	    continue;
	}
          
        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) 
        {
	    printf("select return, timeout left:%lds-%ldms.\n",tv.tv_sec,tv.tv_usec);
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }
          
            //inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
        
            //send new connection greeting message
            if( send(new_socket, message, strlen(message), 0) != strlen(message) ) 
            {
                perror("send");
            }
              
            puts("Welcome message sent successfully");
              
            //add new socket to array of sockets
            for (i = 0; i < CLIENT_NUM; i++) 
            {
                //if position is empty
                if( client_socket[i] == 0 )
                {
                    client_socket[i] = new_socket;
                    printf("Adding to list of sockets as %d\n" , i);
                     
                    break;
                }
            }
        }
          
        //else there is some IO operation on some other socket :)
        for (i = 0; i < CLIENT_NUM; i++) 
        {
            sd = client_socket[i];
	    printf("check client--%d:fd--%d\n",i,sd);
              
            if (FD_ISSET( sd , &readfds)) 
            {
		printf("select return, timeout left:%lds-%ldms.\n",tv.tv_sec,tv.tv_usec);
                //Check if it was for closing , and also read the incoming message
                valread = read( sd , buffer, 1024);
		printf("read form client,size:%d\n",valread);
                if (valread <= 0)
                {
                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                      
                    //Close the socket and mark as 0 in list for reuse
                    close( sd );
                    client_socket[i] = 0;
                }
                //get message "quit", close the connection;
                else if(valread == strlen(quit) && memcmp(buffer,quit,strlen(quit))==0) {
		    printf("client of fd:%d will be closed.\n", sd);
		    client_socket[i] = 0;
		    close(sd);
		}
                //Echo back the message that came in
		else
                {   
		    		 
                    printf("data come from client:%s",buffer);
                    send(sd , buffer , strlen(buffer) , 0 );
                    memset(buffer,0,valread);
		    //printf("sleep 5s\n");
		    //sleep(5);
                }
            }
        }
    }
      
    return 0;
} 
