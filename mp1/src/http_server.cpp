/*
CS438 MP1
Sep 16, 2022
Yu-Wei Lai

** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <string>
#include <sstream>
#include <vector>

#include <iostream>
using namespace std;

// #define PORT "3490"  // the port users will be connecting to
#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 1000 // max number of bytes we can get at once 

void sigchld_handler(int s) {
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// *** Function for bad request
void send_400_bad_request(int sockfd) {
	string message = "HTTP/1.1 400 Bad Request\r\n\r\n";
	send(sockfd, message.c_str(), message.length(), 0);
	return;
}

// *** Function for dealing the GET request
string dir_from_request(string request) {
	string file_dir = "";
	string item;

	stringstream ss(request);
	vector<string> lines;
	while (getline(ss, item)) {
		lines.push_back(item);
	}

	stringstream line_0(lines[0]);
	vector<string> parts;
	while (getline(line_0, item, ' ')) {
		parts.push_back(item);
	}

	file_dir = string(parts[1]).substr(1);
	return file_dir;
}

// *** Function for sending the file back to the client
void send_file(int sockfd, string dir) {
	// get the file
	FILE *file_ptr = fopen(dir.c_str(), "rb");
	int numbytes = 0;
	
	// file not exists
	if (file_ptr == NULL) {
		string message = "HTTP/1.1 404 Not Found\r\n\r\n";
		cout << message;
		send(sockfd, message.c_str(), message.length(), 0);
		return;
	}

	// file exists
	string message = "HTTP/1.0 200 OK\r\n\r\n";
	send(sockfd, message.c_str(), message.length(), 0);
	cout << message;

	char buff[MAXDATASIZE];
	int read_size = 0;
	while (!feof(file_ptr)) {
		
		read_size = fread(buff, 1, MAXDATASIZE-1, file_ptr);
		cout << "\nSize: " << read_size << "\r\n";

		printf("sending... %i bytes", read_size);
		send(sockfd, buff, read_size, 0);
		if (read_size < MAXDATASIZE-1)
		{
			printf("\nsending {%s} end.", dir.c_str());
			break;
		}
	}
	fclose(file_ptr);
}

int main(int argc, char *argv[]) {

	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if (argc != 2) {
	    fprintf(stderr,"PORT not specified\n");
	    exit(1);
	}
	string PORT = argv[1];

	if ((rv = getaddrinfo(NULL, PORT.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
	printf("server: waiting for connections...\r\n");

	// *** Accepting the GET request and deal with the request in the loop
	while(true) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size); // accept a connection from a client
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		// Check the connection from ...
		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		printf("server: got connection from %s\n", s);

		// this is the child process -- which allow multiple clients to connect
		if (!fork()) { 
			close(sockfd); // child doesn't need the listener

			/* --- Receive the request and send the file back --- */
			// !!! use new_fd here
			// ***** Receive from the client
			char request_buff[MAXDATASIZE];
			int numbytes = 0;
			numbytes = recv(new_fd, request_buff, MAXDATASIZE, 0);
			cout << "\n\n\nRECEVIVED MESSAGE:\n" << request_buff << "\n";

			// ***** Deal with the header -> get the file directory
			string file_dir = "";
			string request_string = string(request_buff);
			file_dir = dir_from_request(request_string);
			cout << "\n\nThe directory: " << file_dir <<"\n";
			
			// ***** Call the function send_file() to check if file exists and send it back
			send_file(new_fd, file_dir);
			
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

