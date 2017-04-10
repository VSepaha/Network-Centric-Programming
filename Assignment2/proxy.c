/*
 * proxy.c - A Simple Sequential Web proxy
 *
 * Course Name: 14:332:456-Network Centric Programming
 * Assignment 2
 * Student Name: Vineet Sepaha
 * 
 */

 /*
High Level Description:
    The server side of the code works by first taking input in from the command prompt of which 
port on localhost to listen to. We then create a socket and assign it to the port entered. 
Next we bind the socket address struct to the socket. Then we wait and listen for connections 
on the passed in port. When a request is passed in, the connection is accepted on the socket 
and a connection file descriptor is returned. The connection file descriptor contains the 
request information, therefore since this is important information it is read into a buffer.
Once this is complete, we take the first line from the request and parse it to get the type
of request, the host, the port and the protocol. When this is complete, we use the host to get
the IP address and use it all of the information to create a log entry. This completes the work
for the server.
    On the client side, we also create a socket to listen to the port (either port 80 or a port
specified in the uri). After this is done, we use the connect function to connect with the specified
socket. If this is successful, we send the uri to the client socket. Once this is done, we use the
recv function to constantly write to a buffer and from the buffer, we write to the connection 
file descriptor mentioned in the previous paragraph. If every functions correctly, we can see a
webpage in our browser.

*/
#include "csapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <strings.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>

#define maxTokens 5
#define bufferSize 512
#define bigBufferSize 10000

/*This is a structure that I have create to keep track of the client information
as it is being passed from function to function so there is less overhead.*/
struct Client{
    int sockfd;
    struct sockaddr_in servaddr;

} typedef Client;

/*
 * Function prototype
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size, char* hostIP);


/*This is the connect function that I have created. The purpose of this function is
to first create a socket for the client, assign an IP address and a port to it. Then
secondly, to connect the client socket to the specified address.*/
int conn(char* IPaddress, int port, Client *client){

    client->sockfd = Socket(AF_INET, SOCK_STREAM, 0);

    client->servaddr.sin_addr.s_addr = inet_addr(IPaddress);
    client->servaddr.sin_family = AF_INET;
    client->servaddr.sin_port = htons(port);

    Connect(client->sockfd, (struct sockaddr *) &(client->servaddr), sizeof(client->servaddr));
    return 1;

}

/*This is a small send function. It is simply used to send data to the client socket
file descriptor and return an error if there is a problem.*/
int sendData(char* data, Client* client){
    if(send(client->sockfd, data, strlen(data), 0) < 0){
        perror("Send failed: ");
        return 0;
    }
    return 1;
}

/*This is the function that I created to parse the first line of the request.
It returns the values of the host name, request and protocol into a tokens array.
Then the port is returned when the function completes.*/
int getTokens(char uri[], char* tokens[]){
    strtok(uri, " ");
    int port = 80;

    //If the string contains http or https, we remove it to get the hostname
    tokens[0] = strtok(NULL, " ");
    if(strstr(tokens[0], "http://") || strstr(tokens[0], "https://")){
        strsep(&tokens[0], "/");
        tokens[0][strlen(tokens[0])] = 0;
        tokens[0]++;
    }
    /*If the string of the host name contains a colon, we get that value and
    save as the port number*/
    if(strstr(tokens[0], ":")){
        tokens[2] = tokens[0];
        strsep(&tokens[2], ":");
        while(strstr(tokens[2], ":") != NULL){
            tokens[2]++;
        }
        port = atoi(tokens[2]);
    }
    /*If the hostname contains a '/' then there is a request which is saved
    into tokens[3].*/
    if(strstr(tokens[0], "/")){
        tokens[3] = tokens[0];
        strsep(&tokens[3], "/");
        while(strstr(tokens[3], "/") != NULL){
            tokens[3]++;
        }
    } else {
        tokens[3] = tokens[0];
    }

    //Lastly we take the protocol since it is the last token in the request
    tokens[1] = strtok(NULL, " \n");

    return port;

}
/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv) {

    /* Check arguments */
    if (argc != 2) {
	   fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
	   exit(0);
    }
    struct sockaddr_in servaddr;
    int port = atoi(argv[1]); //Convert the input to an int
    Client client;

    //Get the socket file descriptor for the server
    int sockfd = Socket(AF_INET, SOCK_STREAM, 0);
  
    /*Assign values to the socket address struct so we will be able 
    to bind the socket file descriptor to it*/ 
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    Bind(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

    //Listen for any connections on the socket
    Listen(sockfd, LISTENQ);
    int connfd;
    for(;;){
        //accept
        connfd = Accept(sockfd, NULL, 0);

        //read request
        char reqbuf[bigBufferSize];
        bzero(&reqbuf, sizeof(reqbuf));
        Read(connfd, reqbuf, sizeof(reqbuf));

        //Printing out the request buffer
        printf("\n%s\n", reqbuf);

        if(strncmp(reqbuf,"GET",3) != 0) {
            fprintf(stderr,"Not a GET request\n");
            //exit(2);
            continue;
        } 

        /*This is where the string parsing occurs, mostly in the function
        called getTokens()*/
        char* request;
        char* tokens[maxTokens];
        int clientPort = getTokens(reqbuf, tokens);
        if(strstr(tokens[0], tokens[3]) || strlen(tokens[3]) == 0){
            request = "/";
        } else {
            request = tokens[3];
        }
        char* host = tokens[0];
        char hostIP[100];
        
        //printf("Host = %s on port %d\n", host, clientPort);

        //Call the function to format the log entry
        char logstring[bigBufferSize];
        bzero(&logstring, sizeof(logstring));
        format_log_entry(logstring, &servaddr, host, 0, hostIP);

        //Open and write to the proxy.c file
        FILE* fp = Fopen("proxy.log","ab+");
        fprintf(fp, "%s\r\n", logstring);
        printf("Host IP = %s\n\n", hostIP);
                
        /* This is where the client socket is created (in the conn function). After that
        it will send the request stored in dataBuffer to send to the client socket which
        is done in the sendData function*/
        conn(hostIP, clientPort, &client);
        char dataBuffer[bigBufferSize];
        sprintf(dataBuffer, "GET %s %s\nHost: %s\n\n", request, tokens[1], host);
        sendData(dataBuffer, &client);
        printf("%s\n", dataBuffer);

        
        /*Here as we receive data from the client socket, we store it into the buffer
        and write it to the connection file desciptor*/
        char buffer[bufferSize];
        bzero(&buffer, sizeof(buffer));
        while(recv(client.sockfd, buffer, sizeof(buffer), 0) > 0){
            //puts(buffer);
            Write(connfd, buffer, sizeof(buffer));
        }
        Fclose(fp);
        Close(connfd);    
    }
    //Close(connfd);
    Close(client.sockfd);
    Close(sockfd);

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size, char* hostIP)
{
    time_t now;
    char time_str[MAXLINE];
    struct addrinfo hints, *infoptr;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /*These next couple of lines are used to extract the ip address from the 
    hostname*/
    hints.ai_family = AF_INET;
    int result = getaddrinfo(uri, NULL, &hints, &infoptr);
    if (result) {
       fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
       exit(1);
    }
    char hostaddr[256],service[256];
    getnameinfo(infoptr->ai_addr, infoptr->ai_addrlen, hostaddr, sizeof(hostaddr), service, sizeof(service), NI_NUMERICHOST);
    freeaddrinfo(infoptr);
    strcpy(hostIP, hostaddr);

    printf("%s: %s %s\n", time_str, hostaddr, uri);

    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %s %s", time_str, hostaddr, uri);
}


