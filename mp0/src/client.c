/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <assert.h>

#include <arpa/inet.h>

#define LISTENPORT "4950"	// the port users will be connecting to
#define MAXBUFLEN 100
#define PORT "3490" // the port client will be connecting to 
#define MAXDATASIZE 100 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];


	// from listener.c

	//struct sockaddr_storage their_addr;
	//socklen_t addr_len;

	//int sockfd_listener, numbytes_listener;  
	//char buf_listener[MAXDATASIZE];
	//struct addrinfo hints_listener, *servinfo_listener, *p_listener;
	//int rv_listener;
	//char s_listener[INET6_ADDRSTRLEN];



	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
		
	if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		//printf("%d\n",sockfd);l
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}

	numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0);
	printf("\n%s\n", numbytes);

	buf[numbytes] = '\0';

	char * pos;
	pos=strchr(buf,'\n'); // 
	//printf("found at %zu\n",pos-buf+1);	
	unsigned int str_position = pos-buf;
	//printf("%zu \n\n",str_position);
	printf("client: received %.*s bytes",str_position,buf);
	printf("%s",buf+str_position+2);


	close(sockfd);

	return 0;
}

