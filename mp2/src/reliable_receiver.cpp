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
#define MAXDATASIZE 1400
#define MAXWINDOWSIZE 1000

struct sockaddr_in si_me, si_other;
int s;
int slen;

// content packet packed by sender
#define PACKET_TYPE_START 0
#define PACKET_TYPE_FINISH -1
#define PACKET_TYPE_DATA 1
#define PACKET_TYPE_ACK 2
typedef struct packet{
    long long int seq_num;
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

int send_ack(long long int ack_num){
    packet ack_packet;
    if (ack_num == -1){
        ack_packet.packet_type = PACKET_TYPE_FINISH;
    }
    else{
        ack_packet.packet_type = PACKET_TYPE_ACK;
    }
    // strcpy(ack_packet.content, "ACK");
    // std::cout << "::-> SEND ACK NUM: "<< ack_num <<"\n";
    ack_packet.seq_num = ack_num;
    int send_bytes = sendto(s, &ack_packet, sizeof(packet), 0, (struct sockaddr *) &si_other, slen);

    return send_bytes;
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof(si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *)&si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    printf("Now binding\n");
    if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
        diep("bind");

	/* Now receive data and send acknowledgements */
    int send_bytes = 0;
    for(int i=0; i < MAXWINDOWSIZE; i++){
        received_buffer[i] = -1;
    }

    // *** buffer for the receiver
    FILE *output_ptr = fopen(destinationFile, "wb");
    packet received_packet;
    long long int last_ack = -1;
    while (true)
    {   
        std::cout << "\n\n===========================\nSTART RECEIVEING...\n";

        int receive_numbytes = recvfrom(s, &received_packet, sizeof(packet), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen);
        int null_content_cnt = 0;

        // std::cout << "CONTENT:\n" << received_packet.content << "\n";
        if (receive_numbytes <= 0){
            // std::cout << "No New Bytes Receving; Connection End.\n";
            break;
        }
        std::cout << "RECEIVING:" << receive_numbytes <<" Bytes\n";
        std::cout << "RECEIVED SEQ: "<< received_packet.seq_num <<" expected seq num: "<< last_ack+1 << " TYPE: "<< received_packet.packet_type << " RECEIVED SIZE: "<< received_packet.content_size <<"\n"; 
        

        // **** If the sender infrom the file is ending
        if (received_packet.packet_type == PACKET_TYPE_FINISH or null_content_cnt > 10){
            send_bytes = send_ack(-1);
            break;
        }
        
        // *** received_seq_num >= last_ack: write the content and send back the ack; update the highest ack as this ack
        if (received_packet.seq_num > last_ack){
            received_buffer[received_packet.seq_num%MAXWINDOWSIZE] = received_packet.seq_num;
            packet_buffer[received_packet.seq_num%MAXWINDOWSIZE] = received_packet;

            // std::cout << "(last_ack+1) % MAXWINDOWSIZE+1 IS " << (last_ack+1)%MAXWINDOWSIZE + 1 << ": " << received_buffer[(last_ack+1)%MAXWINDOWSIZE]<<"\n";

            // *** last_ack is the received_packet.seq_num for the previous round; (last_ack + 1) is expected to be the base (received_packet.seq_num)
            while (received_buffer[(last_ack+1)%MAXWINDOWSIZE] >= 0){
                last_ack++; // now last ack is the received pack this round
                if (packet_buffer[last_ack%MAXWINDOWSIZE].content_size <= 0) {
                    received_buffer[last_ack%MAXWINDOWSIZE] = -1;
                    null_content_cnt ++;
                    continue;
                }
                else{
                    null_content_cnt = 0;
                    long write_bytes = fwrite(packet_buffer[last_ack%MAXWINDOWSIZE].content, 1, packet_buffer[last_ack%MAXWINDOWSIZE].content_size, output_ptr);
                    received_buffer[last_ack%MAXWINDOWSIZE] = -1;
                    std::cout << "\n---------- Writing... "<< write_bytes <<"bytes\n\n";
                }
            }
            std::cout << "::ACKING... " << last_ack <<"\n";
            send_bytes = send_ack(last_ack);
        }
        // *** received_seq_num < last_ack: already received, send the highest ack
        else {
            send_bytes = send_ack(last_ack);
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

