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
#include <poll.h>
  
#define TRUE   1
#define FALSE  0
#define PORT 8888
#define CLIENT_NUM 5
#define BLOCK_NUM 2
#define BUFFER_SIZE 1024
 
int main(int argc , char *argv[])
{
    int opt = TRUE;
    int master_socket , addrlen , new_socket , activity, i , valread , sd;
    struct pollfd clientfds[CLIENT_NUM];
    int nfds;
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE+1];  //data buffer of 1K
    int timeout = 5000;          //unit: ms
    char quit[7]="quit\r\n";     //for data from telnet client, the data end with "\r\n"
   
      
    //a message retuan to client when it connect to the server. 
    char *message = "ECHO Daemon v1.0 \r\n";
  
    //initialise all clientfds[i].fd to -1 so not checked
    for (i = 0; i < CLIENT_NUM; i++) 
    {
        clientfds[i].fd = -1;
	clientfds[i].events = 0;
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
    
    clientfds[0].fd = master_socket;
    clientfds[0].events = POLLIN; 
    nfds = 0;
     
    //try to specify maximum of BLOCK_NUM pending connections for the master socket
    if (listen(master_socket, BLOCK_NUM) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
      
    //accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...\n");
    
    /********************************call poll in a infinite loop*******************************/ 
    while(TRUE) 
    {   
        printf("\nlinux poll begin, timeout: 5000ms.\n");
	timeout = 5000;
        //wait for an activity on one of the sockets, timeout is tiemout; 
        activity = poll(clientfds, nfds+1, timeout);
   	
        //return value < 0, error happend; 
        if ((activity < 0) && (errno!=EINTR)) 
        {
            printf("poll error");
        }
	//return value = 0, nothing happend, timeout;
	else if(activity == 0){
	    //no event happend in specific time, timeout;
	    printf("poll return, nothing happend, timeout!\n");
	    continue;
	}
          
        printf("poll return, activity:%d, nfds:%d\n", activity, nfds);
	//If something happened on the master socket , then its an incoming connection
        if (clientfds[0].revents & POLLIN) 
        {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept error");
                exit(EXIT_FAILURE);
            }
          
            //inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
        
            //send new connection greeting message
            if( send(new_socket, message, strlen(message), 0) != strlen(message) ) 
            {
                perror("send error");
            }
              
            puts("Welcome message sent successfully");
              
            //add new socket to array of sockets
            for (i = 1; i < CLIENT_NUM; i++) 
            {
                //if position is empty
                if( clientfds[i].fd == -1 )
                {
                    clientfds[i].fd = new_socket;
                    clientfds[i].events = POLLIN;
                    nfds = nfds > i ? nfds : i;
                    printf("Adding to list of sockets as clientfds[%d].fd = %d\n", i, new_socket);
                    break;
                }
            }
	    if (i == CLIENT_NUM) {
	        perror("Too many clients");
		exit(EXIT_FAILURE);
	    }
	    if(--activity <= 0) {
	        continue;
	    }
        }
          
        //else there is some IO operation on some other socket :)
        for (i = 1; i <= nfds; i++) 
        {
            sd = clientfds[i].fd;
	    printf("check client--%d:fd--%d\n", i, clientfds[i].fd);
            
	    if(clientfds[i].fd < 0){
	        printf("continue: current clientfds[%d]-->fd:%d/\n", i, clientfds[i].fd);
	    }  
            if (clientfds[i].revents & POLLIN)
            {
	      	printf("event pollin happend, waitting to read, clientfds[%d]-->fd:%d\n", i, clientfds[i].fd);
                //Check if it was for closing , and also read the incoming message
                valread = read( sd , buffer, 1024);
		printf("read form client,size:%d\n",valread);
                if (valread <= 0)
                {
                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                    clientfds[i].fd = -1;
                    clientfds[i].events = 0;  
                    //Close the socket and mark as -1 in list for reuse
                    close( sd );
                }
                //get message "quit", close the connection;
                else if(valread == strlen(quit) && memcmp(buffer,quit,strlen(quit))==0) {
		    printf("client of clientfds[%d]-->fd:%d will be closed.\n", i, sd);
		    clientfds[i].fd = -1;
                    clientfds[i].events = 0;
		    memset(buffer, 0, valread);
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
