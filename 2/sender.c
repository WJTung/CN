#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h> 
#include <sys/types.h>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/select.h>
#include "protocol.h"

#define TIMEOUT_INTERVAL 1E5
#define EMPTY 0
#define WAITING 1
#define ACKED 2
#define OUT_OF_ORDER 3
#define TIMEOUT 4

struct segment *SEGs[MAX_SEG_NUM];
int status[MAX_SEG_NUM];

double time_difference(struct timeval *start_time)
{
    double start_us = (double)start_time->tv_sec * 1E6 + start_time->tv_usec;
    struct timeval now_time;
    gettimeofday(&now_time, NULL);
    double now_us = (double)now_time.tv_sec * 1E6 + now_time.tv_usec;
    return now_us - start_us;
}

int ACK_ready(int sockfd, int sec, int usec)
{
    fd_set rfds;
    struct timeval tv;
    
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);
    tv.tv_sec = sec;
    tv.tv_usec = usec;
    
    if(select(sockfd + 1, &rfds, NULL, NULL, &tv))
        return 1;
    
    return 0;
}

int main(int argc, char **argv) // IP, PORT, source file
{
    int AGENT_SENDER_PORT = atoi(argv[2]);
    
    struct sockaddr_in agent_sender;
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    agent_sender.sin_family = AF_INET;
    agent_sender.sin_port = htons(AGENT_SENDER_PORT);
    inet_aton(argv[1], &agent_sender.sin_addr);
    
    int agent_sender_len = sizeof(agent_sender);
    struct segment SEG_SYN, SEG_SYNACK;
    SEG_SYN.HDR.SYN = 1; 
    SEG_SYN.HDR.ACK = 0;
    sendto(sockfd, &SEG_SYN, sizeof(struct segment), 0, &agent_sender, sizeof(agent_sender));
    puts("send\tSYN");
    recvfrom(sockfd, &SEG_SYNACK, sizeof(struct segment), 0, &agent_sender, &agent_sender_len);
    puts("receive\tSYNACK");
    int sender_port = SEG_SYNACK.HDR.dest_port;

    FILE *input = fopen(argv[3], "rb");
    clock_t start_time;

    int cwnd = 1;
    int ssthresh = 16;
    int base = 1;
    int Seq_FIN = MAX_SEG_NUM + 1;

    while(base != Seq_FIN)
    {
        puts("****************************************");
        printf("base = %d cwnd = %d\n", base, cwnd);
        int i;
        for(i = base; i < base + cwnd && i < Seq_FIN; i++)
        {
            if(status[i] == EMPTY)
            {
                struct segment *SEG = (struct segment *)malloc(sizeof(struct segment));
                SEG->HDR.source_port = sender_port;
                SEG->HDR.dest_port = AGENT_SENDER_PORT;
                SEG->HDR.data_length = fread(SEG->data, sizeof(char), MAX_DATA_SIZE, input);
                if(SEG->HDR.data_length == 0)
                    Seq_FIN = i;
                else
                {
                    if(feof(input))
                        Seq_FIN = i + 1;
                    // make a new segment
                    SEG->HDR.Seq = i;
                    SEG->HDR.ACK = 0;
                    SEGs[i] = SEG;
                    printf("send\tdata\t#%d\twinSize = %d\n", i, cwnd);
                    sendto(sockfd, SEG, sizeof(struct segment), 0, &agent_sender, sizeof(agent_sender));
                }
            }
            else // retransmit because of timeout or out of order
            {
                printf("resnd\tdata\t#%d\twinSize = %d\n", i, cwnd);
                sendto(sockfd, SEGs[i], sizeof(struct segment), 0, &agent_sender, sizeof(agent_sender)); 
            }
            status[i] = WAITING;
        }
    
        // monitor if sockfd become ready for recv 
        struct timeval start_time;
        gettimeofday(&start_time, NULL);
        int ACK_num = 0;
        while(time_difference(&start_time) <= TIMEOUT_INTERVAL && ACK_num != cwnd)
        {
            if(ACK_ready(sockfd, 0, 1000))
            {
                struct segment SEG_ACK;
                recvfrom(sockfd, &SEG_ACK, sizeof(struct segment), 0, &agent_sender, &agent_sender_len);
                printf("recv\tack\t#%d\n", SEG_ACK.HDR.ACK);
                status[SEG_ACK.HDR.ACK] = ACKED;
                ACK_num++;
            }
        }
        
        if(ACK_num == cwnd || ACK_num == (Seq_FIN - base))
        {
            base = base + ACK_num;
            if(cwnd >= ssthresh)
                cwnd = cwnd + 1;
            else
                cwnd *= 2;
        }
        else
        {
            // timeout
            int gap = 0;
            for(i = base; i < base + cwnd && i < Seq_FIN; i++)
            {
                if(status[i] == WAITING)
                {
                    status[i] = TIMEOUT;
                    if(gap == 0)
                        gap = i;
                }
                if(status[i] == ACKED)
                {
                    if(gap == 0)
                        free(SEGs[i]); // This segment has been sent successfully
                    else
                        status[i] = OUT_OF_ORDER;
                }
            }
            base = gap;
            if(cwnd > 2)
                ssthresh = cwnd / 2;
            else
                ssthresh = 1;
            cwnd = 1;
            printf("time out\tthreshold = %d\n", ssthresh);
        }
    }
    fclose(input);
    
    // send FIN
    struct segment SEG_FIN;
    SEG_FIN.HDR.source_port = sender_port;
    SEG_FIN.HDR.dest_port = AGENT_SENDER_PORT;
    SEG_FIN.HDR.FIN = 1;
    SEG_FIN.HDR.ACK = 0;
    printf("send\tfin\n");
    sendto(sockfd, &SEG_FIN, sizeof(struct segment), 0, &agent_sender, sizeof(agent_sender));
    while(!ACK_ready(sockfd, 1, 0))
        sendto(sockfd, &SEG_FIN, sizeof(struct segment), 0, &agent_sender, sizeof(agent_sender));
    struct segment SEG_FINACK;
    recvfrom(sockfd, &SEG_FINACK, sizeof(struct segment), 0, &agent_sender, &agent_sender_len);
    if(SEG_FINACK.HDR.FIN == 1 && SEG_FINACK.HDR.ACK == 1)
        printf("recv\tfinack\n");
    close(sockfd);
    
    return 0;
}

