/*
CS438 MP1
Sep 16, 2022
Yu-Wei Lai

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

#include <arpa/inet.h>

#include <iostream>
using namespace std;

// #define PORT "80" // the port client will be connecting to 
#define MAXDATASIZE 1000 // max number of bytes we can get at once 

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

	if (argc != 2) {
	    fprintf(stderr, "usage: client hostname\n");
	    exit(1);
	}
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* --------------------------- */
	/* Deal with the input request */
	string request_host_name;
	string request_port_num;
	string request_file_dir;

	// Split the input string
	string input_text = string(argv[1]);
	int input_len = int(input_text.length());

	int i = 7; // exclude "http://"
	int loc_first_slash = -1;
	int colon_loc = -1;
	while (i < input_len) {
		if(char(input_text[i]) == '/' && loc_first_slash == -1) {
			loc_first_slash = i;
		}
		else if (char(input_text[i]) == ':' && colon_loc == -1) {
			colon_loc = i;
		}
		i++;
	}

	if(colon_loc == -1) {
		request_host_name = input_text.substr(7, loc_first_slash-7);
		request_port_num = "3490"; // give it a default port
		request_file_dir = input_text.substr(loc_first_slash+1, input_len-loc_first_slash-1);
	}
	else {
		cout << ("with colon\n");
		request_host_name = input_text.substr(7, colon_loc-7);
		request_port_num = input_text.substr(colon_loc+1, loc_first_slash-colon_loc-1);
		request_file_dir = input_text.substr(loc_first_slash+1, input_len-loc_first_slash-1);
	}

	cout << "\nHOST " << (request_host_name) << "\n";
	cout << "PORT " << (request_port_num) << "\n";
	cout << "DIR " << (request_file_dir) << "\n";

	/* --------------------------- */
	// Build Connection: Use the input address and port
	if ((rv = getaddrinfo(request_host_name.c_str(), request_port_num.c_str(), &hints, &servinfo)) != 0) {
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
	
	/* --------------------------- */
	/* Send message to the server */
	string request = "GET /" + request_file_dir + " HTTP/1.1\r\n" +
					"USER-Agent: Wget/1.12 (linux-gnu)\r\n" +
					"Host: " + request_host_name + ":" + request_port_num + "\r\n" +
					"Connection: Keep-Alive\r\n\r\n";
	cout << "\nRequest: " << (request);
	cout << "\nRequest length: " << (request.length()) << "\n";

	// *** Send the get request
	send(sockfd, request.c_str(), request.length(), 0); // send something

	// *** Receive from the client
	string receive_text = "";
	int word_cnt = 0;
	int line_cnt = 0;

	int receive_numbytes = 0;
	char header[MAXDATASIZE];

	// ***** Receive the header
	receive_numbytes = recv(sockfd, header, MAXDATASIZE, 0);
	cout << "\nHeader Received Bytes: " << receive_numbytes;
	cout << "\nHeader Received Char:\n" << header << "\n";

	// ***** Save the received content to the given path
	string output_dir = "./output";
	FILE *output_ptr = fopen(output_dir.c_str(), "w");
	char receive_buff[MAXDATASIZE];
	while (true) {
		// ***** Receive the content
		receive_numbytes = recv(sockfd, receive_buff, MAXDATASIZE, 0);
		cout << "Received Bytes: " << receive_numbytes << "\n";
		if (receive_numbytes <= 0) {
			break;
		}

		fwrite(receive_buff, receive_numbytes, 1, output_ptr);
	}
	fclose(output_ptr);

	// close
	close(sockfd);
	return 0;
}

