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
#define MAXDATASIZE 100
#define MAXWINDOWSIZE 100
#define PHASE_SLOWSTART 0
#define PHASE_CONGESTION_AVOIDANCE 1
#define PHASE_FAST_RECOVERY 2
#define min(x, y) x < y ? x : y

struct sockaddr_in si_other;
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
packet packet_queue[MAXWINDOWSIZE];
packet packet_buffer[MAXWINDOWSIZE];
bool packet_buffer_check[MAXWINDOWSIZE];

// int total_packet_cnt = 0;
int accumulated_bytes = 0;
// packet packet_buffer[MAXWINDOWSIZE];
// int sent_buffer[MAXWINDOWSIZE];

void diep(char *s) {
    perror(s);
    exit(1);
}

// void pack_packet(FILE *file_ptr, int packet_pos, unsigned long long int bytesToTransfer){
//     int next_pos = packet_pos + MAXDATASIZE;
//     int read_bytes = min(MAXDATASIZE, bytesToTransfer-next_pos);
//     if (read_bytes <= 0) return;

//     fread(packet_buffer[packet_pos].content, 1, read_bytes, file_ptr);
//     packet_buffer[packet_pos].seq_num = packet_pos;
//     packet_buffer[packet_pos].packet_type = PACKET_TYPE_DATA;
//     packet_buffer[packet_pos].content_size = read_bytes;
// }

int send_packet(FILE *file_ptr, int expected_ack, float cw, unsigned long long int bytesToTransfer, bool timeout=false){
    int sent_bytes = 0;
    int packets_to_sent = min(int(expected_ack + int(cw)), int(bytesToTransfer/MAXWINDOWSIZE));
    if (timeout) {
        sendto(s, &(packet_buffer[expected_ack % MAXWINDOWSIZE]), sizeof(packet), 0, (struct sockaddr *)&si_other, slen);
    }
    else {
        for (int i = expected_ack; i < packets_to_sent; i++){
            int read_bytes = min(MAXDATASIZE, bytesToTransfer-accumulated_bytes);
            if (read_bytes <= 0) break;

            fread(packet_buffer[i % MAXWINDOWSIZE].content, 1, read_bytes, file_ptr);
            packet_buffer[i % MAXWINDOWSIZE].seq_num = i;
            packet_buffer[i % MAXWINDOWSIZE].packet_type = PACKET_TYPE_DATA;
            packet_buffer[i % MAXWINDOWSIZE].content_size = read_bytes;
            accumulated_bytes = accumulated_bytes + MAXDATASIZE;

            sent_bytes = sendto(s, &(packet_buffer[i % MAXWINDOWSIZE]), sizeof(packet), 0, (struct sockaddr *)&si_other, slen);
            std::cout << "### -> Sent packet Seq: " << i << " type " << packet_buffer[i % MAXWINDOWSIZE].packet_type << " sent size:" << packet_buffer[i % MAXWINDOWSIZE].content_size << "\n";
            //std::cout << "content:\n<start>\n" << packet_queue[i].content << "\n<end>\n\n";
        }
    }
    return sent_bytes;
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    FILE *file_ptr;
    file_ptr = fopen(filename, "rb");
    if (file_ptr == NULL) {
        printf("Could not open file to send.\n");
        exit(1);
    }
    std::cout << "Open file: " << filename << "\n";

    // *** pack the file into packets
    // total_packet_cnt = pack_packet(file_ptr, bytesToTransfer);
    // std::cout << "\nTotal Packet Count: " << total_packet_cnt << "\n";

	/* Determine how many bytes to transfer */
    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *)&si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0){
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    /* Send data and receive acknowledgements on s*/
    float sst = 100;
    float cw = 1;

    packet received_packet;
    // int base_sent_packet = 0;
    int expected_ack_num = 0;
    int received_ack_num = 0;
    int dup_ack_cnt = 0;
    int running_phase = PHASE_SLOWSTART;

    // *** A Timer (timer extend the time when receive new ack)
    int start_time = 0;
    bool transfer_done = false;
    int send_bytes = 0;

    packet start_packet;
    start_packet.packet_type = PACKET_TYPE_START;
    start_packet.seq_num = 0;
    send_bytes = sendto(s, &start_packet, sizeof(packet), 0, (struct sockaddr *)&si_other, slen);
    
    std::cout << "TRY SENDING: " << send_bytes << "bytes of start message\n\n";

    while (true)
    {
        // *** Receive Ack
        bool timeout = false;
        bool is_new_ack = false;

        int receive_numbytes = recvfrom(s, &received_packet, sizeof(packet), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen);
        std::cout << "\n========================================\nReceived bytes: " << receive_numbytes <<" Type: "<< received_packet.packet_type<<" (PACKET_TYPE_ACK id 2) \n";

        if (receive_numbytes == -1){
            printf("Connection Error.\n");
            break;
        }
        
        if (received_packet.packet_type == PACKET_TYPE_START){
            send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);
            expected_ack_num = 1;
            continue;
        }

        received_ack_num = received_packet.seq_num;
        std::cout << "Received ack: " << received_ack_num << "; Expected ack: "<<  expected_ack_num << "\n";
        if (received_ack_num >= int(bytesToTransfer / MAXWINDOWSIZE)) break;

        if (received_ack_num >= expected_ack_num){
            is_new_ack = true;
            expected_ack_num = received_ack_num;
        }
        else if (received_ack_num == -1){
            timeout == true;
            // Transfer end 
        }
        else if (received_ack_num < expected_ack_num-1){
            continue;
        }
        else if (received_ack_num == expected_ack_num-1)
        {
            dup_ack_cnt ++;
        }

        // if (received_ack_num >= total_packet_cnt){
        //     transfer_done = true;
        //     break;
        // }
        
        if (running_phase == PHASE_SLOWSTART){
            // **** IF New Ack
            if (is_new_ack){
                cw ++;
                dup_ack_cnt = 0;

                // ***** Send Packet
                send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);

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
                    send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);
                }
            }

            // **** IF TIMEOUT: Resend
            else if (timeout){
                sst = cw/2;
                cw = 1;
                dup_ack_cnt = 0;
                send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer, timeout);
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
                send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);
            }
            // **** IF Dup Ack - >= 3: Resend oder(Ack+1) packet
            else if (is_new_ack == false & dup_ack_cnt > 0){
                if (dup_ack_cnt >= 3){
                    sst = cw/2;
                    cw = sst;
                    cw = cw + 3;
                    send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);

                    running_phase = PHASE_FAST_RECOVERY;
                }
            }
            // **** IF TIMEOUT: Resend
            else if (timeout){
                sst = cw/2;
                cw = 1;
                dup_ack_cnt = 0;
                send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer, timeout);
            }
        }
        else if (running_phase == PHASE_FAST_RECOVERY){
            // **** IF New Ack
            if (is_new_ack){
                cw = sst;
                dup_ack_cnt = 0;
                send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);
            }
            // **** IF Dup Ack - >= 3: Resend oder(Ack+1) packet
            else if (dup_ack_cnt > 0){
                cw = cw + 1.0;
                send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);
            }
            // **** IF TIMEOUT: Resend
            else if (timeout){
                sst = cw/2;
                cw = 1;
                dup_ack_cnt = 0;
                send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer, timeout);
                running_phase = PHASE_SLOWSTART;

                printf("PHASE_FAST_RECOVERY: TIMEOUT");
            }
        }
    }
    // *** Sent the ending packet
    packet packet_end;
    packet_end.packet_type = PACKET_TYPE_FINISH;
    packet_end.seq_num = -1;
    std::cout << "SENDING THE END POCKET: " << packet_end.packet_type << "\n";
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


