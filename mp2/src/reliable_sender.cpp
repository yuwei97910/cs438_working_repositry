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

#include <time.h>
#include <sys/time.h>
#include <iostream>

// #define MAXDATASIZE 1472
#define MAXDATASIZE 1400
#define MAXWINDOWSIZE 1000
#define MAXTIME 80000

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
    long long int seq_num;
    int content_size;
    int packet_type;
    char content[MAXDATASIZE];
} packet;
packet packet_buffer[MAXWINDOWSIZE];
long packet_buffer_check[MAXWINDOWSIZE];
long long int accumulated_bytes = 0;


void diep(char *s) {
    perror(s);
    exit(1);
}

void set_timeout(int sockfd, int timeout_value=MAXTIME){
    struct timeval timer;
    timer.tv_sec = 0;
    timer.tv_usec = timeout_value;
    int ret = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timer, sizeof(timer));
    if (ret==-1){
        perror("SET TIMEOUT");
    }
}


int send_packet(FILE *file_ptr, long long int base_packet_id, float cw, long long int bytesToTransfer, bool timeout=false){
    int sent_bytes = 0;
    // std::cout << "based id: "<< base_packet_id << " cw: "<< int(cw) << " bytesToTransfer: "<< bytesToTransfer <<" Max packet least: " << int((bytesToTransfer-accumulated_bytes)/MAXWINDOWSIZE)+1 << "\n";
    long packets_to_sent = base_packet_id + int(cw);
    // if ((bytesToTransfer-accumulated_bytes) < MAXWINDOWSIZE){
    //     packets_to_sent = 2;
    // }
    if (bytesToTransfer-accumulated_bytes <= 0) {
        std::cout << "(bytesToTransfer-accumulated_bytes == 0) Base packet: " << base_packet_id << " No packet to send\n";
        packets_to_sent = 0;
        return 0;
    }
    
    // std::cout << "PACKET TO SEND: "<< packets_to_sent << "\n";

    if (timeout) {
        // std::cout << "TIMEOUT SENDING "<< base_packet_id <<" PACKET AGAIN; In QUEUE:"<< base_packet_id % MAXWINDOWSIZE << " BUFFER SEQ: "<< packet_buffer[base_packet_id % MAXWINDOWSIZE].seq_num << " BUFFER SIZE: " << packet_buffer[base_packet_id % MAXWINDOWSIZE].content_size << "\n";
        // std::cout << "packet_buffer_check[i % MAXWINDOWSIZE]: "<<packet_buffer_check[base_packet_id % MAXWINDOWSIZE] <<"\n";
        // std::cout << "packet_buffer_check[i-1 % MAXWINDOWSIZE]: "<<packet_buffer_check[(base_packet_id-1) % MAXWINDOWSIZE] <<"\n";
        // std::cout << "packet_buffer_check[i+1 % MAXWINDOWSIZE]: "<<packet_buffer_check[(base_packet_id+1) % MAXWINDOWSIZE] <<"\n";
        sent_bytes = sendto(s, &(packet_buffer[base_packet_id % MAXWINDOWSIZE]), sizeof(packet), 0, (struct sockaddr *)&si_other, slen);
        return sent_bytes;
    }
    // std::cout <<"Base:" << base_packet_id << " packets to send: "<< packets_to_sent << "\n";
    for (long i=base_packet_id; i<packets_to_sent; i++){
        if (packet_buffer_check[i % MAXWINDOWSIZE] == i){
            // std::cout << "PACKET: " << i << " ;in buffer - " << i % MAXWINDOWSIZE << " is already sent.\n";
            continue;
        }
        // std::cout << "ACCUMULATED READ: " << accumulated_bytes << " (bytesToTransfer-accumulated_bytes): " << bytesToTransfer-accumulated_bytes <<"\n";
        long read_bytes = min(MAXDATASIZE, bytesToTransfer-accumulated_bytes);
        
        if (read_bytes < 0){
            read_bytes = 0;
            // return 0;
        }
        else {
            fread(packet_buffer[i % MAXWINDOWSIZE].content, 1, read_bytes, file_ptr);
        }
        // std::cout << "READ BYTES: " << read_bytes <<"\n";

        packet_buffer[i % MAXWINDOWSIZE].seq_num = i;
        packet_buffer[i % MAXWINDOWSIZE].packet_type = PACKET_TYPE_DATA;
        packet_buffer[i % MAXWINDOWSIZE].content_size = read_bytes;
        accumulated_bytes = accumulated_bytes + MAXDATASIZE;

        sent_bytes = sendto(s, &(packet_buffer[i % MAXWINDOWSIZE]), sizeof(packet), 0, (struct sockaddr *)&si_other, slen);
        packet_buffer_check[i % MAXWINDOWSIZE] = i;

        // std::cout << "### -> Sent packet Seq: " << i << " type " << packet_buffer[i % MAXWINDOWSIZE].packet_type << " sent size:" << packet_buffer[i % MAXWINDOWSIZE].content_size << "\n";
        // std::cout << "SNED CONTENT:\n<start>\n" << packet_buffer[i % MAXWINDOWSIZE].content<<"\n<end>\n";
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
    double sst = 100;
    double cw = 1;

    packet received_packet;
    // int base_sent_packet = 0;
    long long int expected_ack_num = 0;
    int dup_ack_cnt = 0;
    int timeout_cnt = 0;
    int running_phase = PHASE_SLOWSTART;
    bool all_packet_sent = false;
    // *** A Timer (timer extend the time when receive new ack)
    // struct timeval timer;
    // timer.tv_sec = 0;
    // timer.tv_usec = MAXTIME;
    // int ret = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timer, sizeof(timer));
    // if (ret==-1){
    //     perror("SET TIMEOUT");
    // }
    set_timeout(s);
    for (int i=0; i<MAXWINDOWSIZE; i++){
        packet_buffer_check[i] = -1;

    }
    
    int send_bytes = 0;
    send_bytes = send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);
    while (true)
    {
        // *** Receive Ack
        bool timeout = false;
        bool is_new_ack = false;
        long long int received_ack_num = -1;

        int receive_numbytes = recvfrom(s, &received_packet, sizeof(packet), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen);
        std::cout << "\n========================================\nReceived bytes: " << receive_numbytes <<" Type: "<< received_packet.packet_type << " (PACKET_TYPE_ACK id 2) \n";
        if (receive_numbytes == -1){
            timeout = true;
            timeout_cnt ++;

            dup_ack_cnt = 0;
            // std::cout<<"TIME OUT Count: "<<timeout_cnt<<" state: "<<running_phase <<"\n";
            // if(timeout_cnt>5 and all_packet_sent) break;
        }
        
        received_ack_num = received_packet.seq_num;
        std::cout << "Received ack: " << received_ack_num << "; Expected ack: "<<  expected_ack_num << " TOTAL PACK SHOULD SEND: " << (bytesToTransfer / MAXDATASIZE) << "\n";
        // std::cout << "CW: " << cw <<" SST: "<< sst << " STATE: "<<running_phase<<"\n";
        if (received_ack_num >= (bytesToTransfer / MAXDATASIZE) & all_packet_sent) {
            // std::cout << "received all acks\n\n";
            break;
        }

        if (received_ack_num >= expected_ack_num){
            for (int i = expected_ack_num; i <= received_ack_num; i++){
                    packet_buffer_check[i % MAXWINDOWSIZE] = -1;
                    packet_buffer[i % MAXWINDOWSIZE].seq_num = -1;
                    packet_buffer[i % MAXWINDOWSIZE].content_size = 0;
                    strcpy(packet_buffer[i % MAXWINDOWSIZE].content, "");
                }
            is_new_ack = true;
            timeout_cnt = 0;
            expected_ack_num = received_ack_num + 1;

            set_timeout(s);
        }
        if ((received_ack_num < (expected_ack_num-1)) & timeout==false){
            // std::cout << "SMALL ACK: " << received_ack_num << " Expected - 1: "<<(expected_ack_num-1) <<"\n";
            continue;
        }
        if (received_ack_num == (expected_ack_num-1)){
            dup_ack_cnt ++;
        }
        // std::cout << "----PHASE: " << running_phase << "\n";
        if (running_phase == PHASE_SLOWSTART){
            // **** IF TIMEOUT: Resend
            if (timeout){
                // std::cout << "----PHASE: " << running_phase << " TIMEOUT.\n";
                sst = cw/2;
                cw = 1;
                dup_ack_cnt = 0;
                send_bytes = send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer, timeout=true);
            }
            else if (is_new_ack){
                // std::cout << "----PHASE: " << running_phase << " IS NEW ACK.\n";
                if (cw < MAXWINDOWSIZE){
                    cw ++;
                }
                dup_ack_cnt = 0;
                send_bytes = send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);
            }
            else if ((is_new_ack == false) & (dup_ack_cnt > 0)){
                // std::cout << "----PHASE: " << running_phase << " IS DUP.\n";
                if (dup_ack_cnt >= 3){
                    sst = cw/2;
                    cw = sst;
                    cw = cw + 3;
                    running_phase = PHASE_FAST_RECOVERY;
                    send_bytes = send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);
                }
            }
            // *** IF CW is larger than SST -> go into the CONGESTION_AVOIDANCE STAGE
            if (cw > sst){
                running_phase = PHASE_CONGESTION_AVOIDANCE;
            }
        }
        else if (running_phase == PHASE_CONGESTION_AVOIDANCE){
            // **** IF TIMEOUT: Resend
            if (timeout){
                // std::cout << "----PHASE: " << running_phase << " TIMEOUT.\n";
                sst = cw/2;
                cw = 1;
                dup_ack_cnt = 0;
                send_bytes = send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer, timeout=true);
                running_phase = PHASE_SLOWSTART;
            }
            // **** IF New Ack
            else if (is_new_ack){
                // std::cout << "----PHASE: " << running_phase << " IS NEW ACK.\n";
                cw = cw + 1/cw;
                dup_ack_cnt = 0;
                send_bytes = send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);
            }
            // **** IF Dup Ack - >= 3: Resend oder(Ack+1) packet
            else if ((is_new_ack == false) & (dup_ack_cnt > 0)){
                // std::cout << "----PHASE: " << running_phase << " IS NEW DUP.\n";
                if (dup_ack_cnt >= 3){
                    sst = cw/2;
                    cw = sst;
                    cw = cw + 3;
                    send_bytes = send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);

                    running_phase = PHASE_FAST_RECOVERY;
                }
            }
        }
        else if (running_phase == PHASE_FAST_RECOVERY){
            // **** IF TIMEOUT: Resend
            if (timeout){
                // std::cout << "----PHASE: " << running_phase << " TIMEOUT.\n";
                sst = cw/2;
                cw = 1;
                dup_ack_cnt = 0;
                send_bytes = send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer, timeout=true);
                running_phase = PHASE_SLOWSTART;
            }
            // **** IF New Ack
            else if (is_new_ack){
                cw = sst;
                dup_ack_cnt = 0;
                send_bytes = send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);
                running_phase = PHASE_CONGESTION_AVOIDANCE;
            }
            // **** IF Dup Ack - >= 3: Resend oder(Ack+1) packet
            else if (dup_ack_cnt > 0){
                cw = cw + 1;
                send_bytes = send_packet(file_ptr, expected_ack_num, cw, bytesToTransfer);
            }
        }
        if (send_bytes == 0) {
            expected_ack_num += -1;
            all_packet_sent = true;
        }
    }
    // *** Sent the ending packet
    packet packet_end;
    packet_end.packet_type = PACKET_TYPE_FINISH;
    packet_end.seq_num = -1;
    // std::cout << "\n\nSENDING THE END POCKET: " << packet_end.packet_type << "\n";
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


