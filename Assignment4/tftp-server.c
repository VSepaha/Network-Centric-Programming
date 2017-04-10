/*
 * tftp-server.c - A simple tftp server
 *
 * Course Name: 14:332:456-Network Centric Programming
 * Assignment 4
 * Student Name: Vineet Sepaha
 * 
 * Resource: Beej's Guide to Network Programming
 */

/*
High Level Description
	This implementation is similar to the implementation that we did for the TCP connection. We
first opened the socket and used bind with the port number. Once this is done, we wait for the 
receive (similar to the accept in TCP) until a client has made a request. The recvfrom function 
also takes the string passed to it. The information is then parsed and printed out to get in the 
form of REQUEST FILENAME MODE IPADDRESS:PORT. 
	Once this is complete the only thing left to do is return an error packet with the file not found
code to the client. This is done through the sendto function. In this function we pass the created
struct err_packet once the values have been assigned to it. This causes the program to work successfully.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "10000"
#define MAXBUF 2000

#define PORT_LENGTH 5

#define RRQ 1
#define WRQ 2
#define ERROR 5

#define FILE_NOT_FOUND 1

//This is the struct for the error packet
struct err_packet{
	uint16_t opcode;
	uint16_t errorcode;
	char errmsg[MAXBUF+1];
} typedef err_packet;

//This function is used to 
void errpacket(err_packet* packet, uint16_t op){
	packet->opcode = htons(op);
	packet->errorcode = htons(FILE_NOT_FOUND);
	strcpy(packet->errmsg, "File not found");
}

int main(){
	int sockfd, rv, numbytes;

	struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in their_addr;
    socklen_t addr_len;

    char buf[MAXBUF];
    char s[INET6_ADDRSTRLEN];
    char ps[PORT_LENGTH];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

	//get the address information    
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first possible port
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("Socket Error");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Bind Error");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    //Iterative 
    for(;;){
		printf("Waiting for Connection...\n");

		addr_len = sizeof their_addr;
		//receive a connection and request from the socket
		if ((numbytes = recvfrom(sockfd, buf, MAXBUF-1 , 0,
		    (struct sockaddr *)&their_addr, &addr_len)) == -1) {
		    perror("recvfrom");
		    exit(1);
		}

		//get the address that we have a conncetion from
		char* connection_address = inet_ntoa(their_addr.sin_addr);
		//get the port that we have a connection from
		int connection_port = ntohs(their_addr.sin_port);

		//add a terminating character
		buf[numbytes] = '\0';

		//print out the request
		int i;
		for(i = 1; i < numbytes; i++){
			if (i == 1){
				if((int)buf[i] == RRQ){
					printf("RRQ ");
				} else if((int)buf[i] == WRQ){
					printf("WRQ ");
				}
			} else {
				if((int)buf[i] == 0)
					printf("%c", ' ');
				else
					printf("%c", buf[i]);
			}
		}
		//standard output
		printf("from %s:%d\n", connection_address, connection_port);

		//create the error packet struct
		err_packet sendPacket;
		//assign values to 
		errpacket(&sendPacket, ERROR);
		
		//send a packet to the client, in this case, it is an error packet
		if ((numbytes = sendto(sockfd, (err_packet*)&sendPacket, MAXBUF-1 , 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
		    perror("sendto");
		    exit(1);
		}
	}
    close(sockfd);


}