/* 
 * File:   sender_main.c
 * Author: YuWei Lai
 *
 * Created on: Oct.10, 2022
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include <iostream>

// #define MAXDATASIZE 1472
#define MAXDATASIZE 1000
#define MAXWINDOWSIZE 1000
#define PHASE_SLOWSTART 0
#define PHASE_CONGESTION_AVOIDANCE 1
#define PHASE_FAST_RECOVERY 2
#define min(x, y) x < y ? x : y

struct sockaddr_in si_other;
int s;
socklen_t slen;

// content packet packed by sender
#define PACKET_TYPE_START 0
#define PACKET_TYPE_FINISH -1
#define PACKET_TYPE_DATA 1
#define PACKET_TYPE_ACK 2
typedef struct {
    int seq_num;
    int content_size;
    int packet_type;
    char content[MAXDATASIZE];
} packet;
packet packet_buffer[MAXWINDOWSIZE];

void diep(char *s) {
    perror(s);
    exit(1);
}

int pack_packet(FILE *file_ptr, unsigned long long int bytesToTransfer){
    int next_bytes = 0;
    int i = 0;
    while (!feof(file_ptr) & next_bytes < bytesToTransfer){
        if (next_bytes >= bytesToTransfer){
            return 0;
        }
        fread(packet_buffer[i].content, 1, min(MAXDATASIZE, bytesToTransfer - next_bytes), file_ptr);
        packet_buffer[i].seq_num = i;
        packet_buffer[i].packet_type = PACKET_TYPE_DATA;
        packet_buffer[i].content_size = min(MAXDATASIZE, bytesToTransfer - next_bytes);

        next_bytes += MAXDATASIZE;
        i ++;
    }
    return i;
}

void send_packet(int expected_ack, float cw, bool timeout=false){
    if (timeout) {
        sendto(s, &(packet_buffer[expected_ack % MAXWINDOWSIZE]), sizeof(packet), 0, (struct sockaddr *)&si_other, slen);
    }
    else {
        for (int i = expected_ack; i < (expected_ack + cw); i++)
        {
            sendto(s, &(packet_buffer[i % MAXWINDOWSIZE]), sizeof(packet), 0, (struct sockaddr *)&si_other, slen);
        }
    }
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    FILE *file_ptr;
    file_ptr = fopen(filename, "rb");
    if (file_ptr == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    // *** pack the file into packets
    int total_packet_cnt = pack_packet(file_ptr, bytesToTransfer);

	/* Determine how many bytes to transfer */
    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    /* Send data and receive acknowledgements on s*/
    int max_sst = 0;
    float sst = 0;
    float cw = 0;

    int base_sent_packet = 0;
    int expected_ack_num = 0;
    int received_ack_num = 0;
    int dup_ack_cnt = 0;

    packet received_packet;
    int running_phase = PHASE_SLOWSTART;

    // *** A Timer (timer extend the time when receive new ack)
    int start_time = 0;
    bool transfer_done = true;
    while (true)
    {
        bool timeout = true;
        bool is_new_ack = false;

        // *** Receive Ack
        int receive_numbytes = recvfrom(s, &received_packet, sizeof(packet), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen);
        if (receive_numbytes <= 0){
            printf("Connection Error.\n");
            break;
        }

        timeout = false;
        received_ack_num = received_packet.seq_num;
        printf("received ack: %d; expected ack: %d\n", received_ack_num, expected_ack_num);
        
        if (received_ack_num >= expected_ack_num){
            is_new_ack = true;
            expected_ack_num = received_ack_num + 1;
        }
        else if (received_ack_num == -1){
            // timeout == true;
            // Transfer end 
            break;
        }
        else if (received_ack_num < expected_ack_num - 1){
            continue;
        }
        else if (received_ack_num == expected_ack_num - 1)
        {
            dup_ack_cnt ++;
        }
        
        
        
        if (running_phase == PHASE_SLOWSTART){
            // **** IF New Ack
            if (is_new_ack){
                cw ++;
                dup_ack_cnt = 0;

                // ***** Send Packet
                send_packet(expected_ack_num, cw);

                // ***** Extend the timer
            }
            
            // **** IF Dup Ack
            else if (is_new_ack == false & dup_ack_cnt > 0){
                if (dup_ack_cnt >= 3){
                    sst = cw/2;
                    cw = sst;
                    cw = cw + 3;
                    running_phase = PHASE_FAST_RECOVERY;

                    // ***** Send Packet
                    send_packet(expected_ack_num, cw);
                }
            }

            // **** IF TIMEOUT: Resend
            else if (timeout){
                sst = cw/2;
                cw = 1;
                dup_ack_cnt = 0;
                send_packet(expected_ack_num, cw), timeout;
            }
            
            // *** IF CW is larger than SST -> go into the CONGESTION_AVOIDANCE STAGE
            if (cw > sst){
                running_phase = PHASE_CONGESTION_AVOIDANCE;
            }
        }
        else if (running_phase == PHASE_CONGESTION_AVOIDANCE){
            // **** IF New Ack
            if (is_new_ack){
                cw = cw + 1/cw;
                dup_ack_cnt = 0;
                send_packet(expected_ack_num, cw);
            }
            // **** IF Dup Ack - >= 3: Resend oder(Ack+1) packet
            else if (is_new_ack == false & dup_ack_cnt > 0){
                if (dup_ack_cnt >= 3){
                    sst = cw/2;
                    cw = sst;
                    cw = cw + 3;
                    send_packet(expected_ack_num, cw);

                    running_phase = PHASE_FAST_RECOVERY;
                }
            }
            // **** IF TIMEOUT: Resend
            else if (timeout){
                sst = cw/2;
                cw = 1;
                dup_ack_cnt = 0;
                send_packet(expected_ack_num, cw, timeout);
            }
        }
        else if (running_phase == PHASE_FAST_RECOVERY){
            // **** IF New Ack
            if (is_new_ack){
                cw = sst;
                dup_ack_cnt = 0;
                send_packet(expected_ack_num, cw);
            }
            // **** IF Dup Ack - >= 3: Resend oder(Ack+1) packet
            else if (dup_ack_cnt > 0){
                cw = cw + 1.0;
                send_packet(expected_ack_num, cw);
            }
            // **** IF TIMEOUT: Resend
            else if (timeout){
                sst = cw/2;
                cw = 1;
                dup_ack_cnt = 0;
                send_packet(expected_ack_num, cw, timeout);
                running_phase = PHASE_SLOWSTART;

                printf("PHASE_FAST_RECOVERY: TIMEOUT");
            }
        }

        if (transfer_done){
            break;
        }
    }
    // *** Sent the ending packet
    packet packet_end;
    packet_end.packet_type = PACKET_TYPE_FINISH;
    packet_end.seq_num = -1;
    
    sendto(s, &(packet_end), sizeof(packet), 0, (struct sockaddr *)&si_other, slen);

    // *** End the connection
    printf("Closing the socket\n");
    close(s);
    return;

}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}


