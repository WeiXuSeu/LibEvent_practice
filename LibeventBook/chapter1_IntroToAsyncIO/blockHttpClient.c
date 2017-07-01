/*For sockadd in*/
#include<netinet/in.h>
/*For socket function*/
#include<sys/socket.h>
/*For gethostbyname*/
#include<netdb.h>

#include<unistd.h>
#include<string.h>
#include<stdio.h>

int main(int c, char **v)
{
    const char query[]=
        "GET /HTTP/1.1/\r\n"
        "Host:www.baidu.com\r\n"
        "\r\n \r\n";
    const char hostname[]="www.baidu.com";
    struct sockaddr_in sockAddrIn;
    struct hostent *host;
    const char *cp;
    int fd;
    ssize_t n_written, remaining;
    char buf[7200];
    char str[32];
    char **pptr;    

    /*Look up the ip address for the hostname.
     Watch out: this isn't threadsafe on most platforms */
    host = gethostbyname(hostname);
    if(!host)
    {
        fprintf(stderr,"Couldn't lookup %s", hostname);
        return 1;
    }
    else
    {
        fprintf(stdout,"Get host of:%s \n",hostname);
        printf("Host length is:%d\n",host->h_length);
 	pptr = host->h_addr_list;
        inet_ntop(host->h_addrtype,*pptr,str,sizeof(str));
        printf("the first address is: %s\n",str);
    }

    /*Allow a new socket*/
    printf("Allow a new socket\n");
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0)
    {
        perror("Wrong socket");
        close(fd);
 	return 1;
    }

    /*Connect to the remote host*/
    printf("Connect to the remote host\n");
    memset(&sockAddrIn,0,sizeof(sockAddrIn));
    sockAddrIn.sin_family = AF_INET;
    sockAddrIn.sin_port = htons(80);
    sockAddrIn.sin_addr = *(struct in_addr*)host->h_addr;
    inet_ntop(host->h_addrtype,&(sockAddrIn.sin_addr),str,sizeof(str));
    printf("Connect to address:%s\n",str);
    if(connect(fd, (struct sockaddr*)&sockAddrIn, sizeof(sockAddrIn)))
    {
        perror("connect");
        close(fd);
	return 1;
    }
 
    /*Write the query*/
    cp = query;
    remaining = strlen(query);
    while(remaining)
    {
        n_written = send(fd, cp, remaining, 0);
    	fprintf(stdout, "n_writen:%d, remaining:%d\n",n_written, remaining);
        if(n_written <= 0)
       	{
	    perror("send");
	    return 1;
	}
    	remaining -= n_written;
        cp += n_written;
    }
 
    /*Get an answer back*/
    while(1)
    {
        ssize_t result = recv(fd, buf, sizeof(buf), 0);
  	fprintf(stdout,"recv result:%d\n",result);
        if(0 == result)
    	    break;
    	else if(result<0)
    	{
     	    perror("recv");
   	    close(fd);
 	    return 1;
    	}
	fwrite(buf, 1, result, stdout);
    }
    
    close(fd);
    return 0;
}
