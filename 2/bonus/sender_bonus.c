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
#include <pthread.h>
#include "protocol.h"

#define TIMEOUT_INTERVAL 2E5
#define EMPTY 0
#define WAITING 1
#define ACKED 2
#define OUT_OF_ORDER 3
#define TIMEOUT 4
#define THREAD_NUM 3
#define IP_LENGTH 20

int send_stack[MAX_SEG_NUM];
int send_stack_size = 0;
pthread_mutex_t send_mutex;
int sockfd[THREAD_NUM];
char IP[IP_LENGTH];
struct sockaddr_in agent_sender[THREAD_NUM];
int agent_sender_len[THREAD_NUM];

struct segment *SEGs[MAX_SEG_NUM];
int status[MAX_SEG_NUM];

int finish = 0;
int ready[THREAD_NUM];

double time_difference(struct timeval *start_time)
{
    double start_us = (double)start_time->tv_sec * 1E6 + start_time->tv_usec;
    struct timeval now_time;
    gettimeofday(&now_time, NULL);
    double now_us = (double)now_time.tv_sec * 1E6 + now_time.tv_usec;
    return now_us - start_us;
}

void sender(void *arg)
{ 
    int thread_id = *((int *)arg);
    int AGENT_SENDER_PORT = *((int *)arg + 1);
    
    sockfd[thread_id] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    agent_sender[thread_id].sin_family = AF_INET;
    agent_sender[thread_id].sin_port = htons(AGENT_SENDER_PORT);
    inet_aton(IP, &agent_sender[thread_id].sin_addr);
    
    agent_sender_len[thread_id] = sizeof(agent_sender[thread_id]);
    struct segment SEG_SYN, SEG_SYNACK;
    SEG_SYN.HDR.SYN = 1; 
    SEG_SYN.HDR.ACK = 0;
    sendto(sockfd[thread_id], &SEG_SYN, sizeof(struct segment), 0, &agent_sender[thread_id], sizeof(agent_sender[thread_id]));
    printf("path %d\tsend\tSYN\n", thread_id);
    recvfrom(sockfd[thread_id], &SEG_SYNACK, sizeof(struct segment), 0, &agent_sender[thread_id], &agent_sender_len[thread_id]);
    printf("path %d\treceive\tSYNACK\n", thread_id);
    int sender_port = SEG_SYNACK.HDR.dest_port;
    ready[thread_id] = 1;

    int target;
    while(!finish)
    {
        target = -1;
        pthread_mutex_lock(&send_mutex);
        if(send_stack_size > 0)
        {
            target = send_stack[send_stack_size];
            send_stack_size--;
        }
        pthread_mutex_unlock(&send_mutex);
        if(target != -1)
        {
            printf("path %d\tsend\tdata\t#%d\n", thread_id, target);
            SEGs[target]->HDR.source_port = sender_port;
            sendto(sockfd[thread_id], SEGs[target], sizeof(struct segment), 0, &agent_sender[thread_id], sizeof(agent_sender[thread_id]));
            usleep(1000);
        }
    }
}   

int main(int argc, char **argv) // IP, PORT * 3, source file
{
    memcpy(IP, argv[1], strlen(argv[1]));
    pthread_t threads[THREAD_NUM];

    int args[THREAD_NUM][2];
    int i;
    for(i = 0; i < THREAD_NUM; i++)
    {
        args[i][0] = i;
        args[i][1] = atoi(argv[i + 2]);
        pthread_create(&threads[i], NULL, sender, (void *)args[i]);
    }
 
    FILE *input = fopen(argv[5], "rb");
    clock_t start_time;

    int cwnd = 1;
    int ssthresh = 16;
    int base = 1;
    int Seq_FIN = MAX_SEG_NUM + 1;

    while(ready[0] == 0 || ready[1] == 0 || ready[2] == 0)
        sleep(1);   

    int maxfd = 0;
    for(i = 0; i < THREAD_NUM; i++)
        if(sockfd[i] > maxfd)
            maxfd = sockfd[i];
    pthread_mutex_init(&send_mutex, NULL);
    

    while(base != Seq_FIN)
    {
        puts("****************************************");
        printf("base = %d cwnd = %d\n", base, cwnd);
        for(i = base; i < base + cwnd && i < Seq_FIN; i++)
        {
            if(status[i] == EMPTY)
            {
                struct segment *SEG = (struct segment *)malloc(sizeof(struct segment));
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
                }
            }
            else // retransmit because of timeout or out of order
                printf("resnd\tdata\t#%d\twinSize = %d\n", i, cwnd);
            if(i != Seq_FIN)
            {
                pthread_mutex_lock(&send_mutex);
                send_stack_size++;
                send_stack[send_stack_size] = i;
                pthread_mutex_unlock(&send_mutex);
                status[i] = WAITING;
            }
        }
    
        // monitor if sockfd become ready for recv 
        struct timeval start_time;
        gettimeofday(&start_time, NULL);
        int ACK_num = 0;
        while(time_difference(&start_time) <= TIMEOUT_INTERVAL && ACK_num != cwnd)
        {
            fd_set rfds;
            struct timeval tv;
    
            FD_ZERO(&rfds);
            FD_SET(sockfd[0], &rfds);
            FD_SET(sockfd[1], &rfds);
            FD_SET(sockfd[2], &rfds);
            tv.tv_sec = 0;
            tv.tv_usec = 1000;
    
            if(select(maxfd + 1, &rfds, NULL, NULL, &tv))
            {
                for(i = 0; i < THREAD_NUM; i++)
                {
                    if(FD_ISSET(sockfd[i], &rfds))
                    {
                        struct segment SEG_ACK;
                        recvfrom(sockfd[i], &SEG_ACK, sizeof(struct segment), 0, &agent_sender[i], &agent_sender_len[i]);
                        printf("path %d\trecv\tack\t#%d\n", i, SEG_ACK.HDR.ACK);
                        status[SEG_ACK.HDR.ACK] = ACKED;
                        ACK_num++;
                    }
                }
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
    finish = 1;
   
    for(i = 0; i < THREAD_NUM; i++)
        pthread_join(threads[i], NULL);

    // send FIN
    struct segment SEG_FIN;
    SEG_FIN.HDR.FIN = 1;
    SEG_FIN.HDR.ACK = 0;
    printf("send\tfin\n");
    sendto(sockfd[0], &SEG_FIN, sizeof(struct segment), 0, &agent_sender[0], sizeof(agent_sender[0]));
    struct segment SEG_FINACK;
    recvfrom(sockfd[0], &SEG_FINACK, sizeof(struct segment), 0, &agent_sender[0], &agent_sender_len[0]);
    if(SEG_FINACK.HDR.FIN == 1 && SEG_FINACK.HDR.ACK == 1)
        printf("recv\tfinack\n");
    
    for(i = 0; i < THREAD_NUM; i++)
        close(sockfd[i]);
    
    return 0;
}

