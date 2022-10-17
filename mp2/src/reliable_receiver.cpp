/* 
 * File:   receiver_main.c
 * Author: YuWei Lai
 *
 * Created on Oct. 10, 2022
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include <iostream>
// using namespace std;

#define MAXDATASIZE 100
#define MAXWINDOWSIZE 100

struct sockaddr_in si_me, si_other;
int s;
int slen;

// content packet packed by sender
#define PACKET_TYPE_START 0
#define PACKET_TYPE_FINISH -1
#define PACKET_TYPE_DATA 1
#define PACKET_TYPE_ACK 2
typedef struct packet{
    int seq_num;
    int content_size;
    int packet_type;
    char content[MAXDATASIZE];
} packet;
packet packet_buffer[MAXWINDOWSIZE];
int received_buffer[MAXWINDOWSIZE];

void diep(char *s) {
    perror(s);
    exit(1);
}

void send_ack(int ack_num){
    packet ack_packet;
    if (ack_num == -1){
        ack_packet.packet_type = PACKET_TYPE_FINISH;
    }
    else{
        ack_packet.packet_type = PACKET_TYPE_ACK;
        while (received_buffer[ack_num] == 1){
            std::cout << ack_num <<"\n";
            ack_num = ack_num + 1;
        }
    }
    // strcpy(ack_packet.content, "ACK");
    std::cout << "!!!!! ACK NUM: "<<ack_num<<"\n";
    ack_packet.seq_num = ack_num;
    sendto(s, &ack_packet, sizeof(packet), 0, (struct sockaddr *) &si_other, slen);
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof(si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    printf("Now binding\n");
    if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
        diep("bind");

	/* Now receive data and send acknowledgements */
    // *** Send a handshake
    int send_bytes = 0;

    // *** buffer for the receiver
    FILE *output_ptr = fopen(destinationFile, "wb");
    packet received_packet;
    int last_ack = 0;
    while (true)
    {   
        std::cout << "\nSTART RECEIVEING...\n\n";

        char received_buff[MAXDATASIZE];
        int receive_numbytes = recvfrom(s, &received_buff, MAXDATASIZE, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen); // UDP: recvfrom
        // std::cout << "CONTENT:\n" << received_packet.content << "\n";
        if (receive_numbytes <= 0){
            std::cout << "No New Bytes Receving; Connection End.\n";
            break;
        }
        std::cout << "RECEIVING:" << receive_numbytes <<" Bytes\n";
        memcpy(&received_packet, received_buff, receive_numbytes);
        std::cout << "RECEIVED SEQ: "<< received_packet.seq_num <<" expected seq num: "<< last_ack << " TYPE: "<< received_packet.packet_type <<"\n";// << "; RECEIVED CONTENT: " << received_packet.content;
        
        // **** If the sender inform to build a connection
        if (received_packet.packet_type == PACKET_TYPE_START){
            packet start_packet;
            start_packet.packet_type = PACKET_TYPE_START;
            start_packet.seq_num = 0;
            send_bytes = sendto(s, &start_packet, sizeof(packet), 0, (struct sockaddr *)&si_other, slen);
            continue;
        }
        // **** If the sender infrom the file is ending
        if (received_packet.packet_type == PACKET_TYPE_FINISH){
            send_ack(-1);
            break;
        }

        // *** received_seq_num >= last_ack: write the content and send back the ack; update the highest ack as this ack
        if (received_packet.seq_num >= last_ack){
            received_buffer[received_packet.seq_num] = 1;
            packet_buffer[received_packet.seq_num] = received_packet;

            // send ack
            send_ack(last_ack);

            // write into the output file
            // std::cout << "Writing the content:\n<start>\n"<<received_packet.content <<"\n<end>\n\n";
            while (received_buffer[last_ack] == 1){
                last_ack = last_ack + 1;
                fwrite(packet_buffer[last_ack].content, 1, packet_buffer[last_ack].content_size, output_ptr);
                // received_buffer[last_ack] = 0;
            }
        }
        // *** received_seq_num < last_ack: already received, send the highest ack
        else if (received_packet.seq_num < last_ack){
            // send ack
            send_ack(last_ack);
        }
    }
    fclose(output_ptr);

    // close the socket
    close(s);
	printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

