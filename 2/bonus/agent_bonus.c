#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h> // sockaddr_in
#include <sys/types.h>
#include <sys/socket.h> // socket
#include <arpa/inet.h>
#include <time.h>
#include <stdlib.h>
#include <sys/select.h>
#include <pthread.h>
#include "protocol.h"

#define THREAD_NUM 3

double drop_rate;
int finish = 0;
int total_data = 0, dropped_data = 0;
pthread_mutex_t loss_rate_mutex;

void agent(void *arg)
{
    int thread_id = *((int *)arg);
    int AGENT_SENDER_PORT = *((int *)arg + 1);
    int AGENT_RECEIVER_PORT = *((int *)arg + 2);
    printf("path %d\tsender port = %d\treceiver port = %d\n", thread_id, AGENT_SENDER_PORT, AGENT_RECEIVER_PORT);

    struct segment SEG_SYN, SEG_SYNACK;
    
    struct sockaddr_in agent_receiver, receiver;
    int sockfd_receiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    agent_receiver.sin_family = AF_INET;
    agent_receiver.sin_port = htons(AGENT_RECEIVER_PORT); 
    agent_receiver.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sockfd_receiver, &agent_receiver, sizeof(agent_receiver));
    
    int receiver_len = sizeof(receiver);
    recvfrom(sockfd_receiver, &SEG_SYN, sizeof(struct segment), 0, &receiver, &receiver_len);
    printf("path %d\treceive SYN from receiver\n", thread_id);
    SEG_SYNACK.HDR.SYN = 1;
    SEG_SYNACK.HDR.ACK = 1;
    SEG_SYNACK.HDR.source_port = AGENT_RECEIVER_PORT;
    int receiver_port = ntohs(receiver.sin_port);
    SEG_SYNACK.HDR.dest_port = receiver_port;
    sendto(sockfd_receiver, &SEG_SYNACK, sizeof(struct segment), 0, &receiver, sizeof(receiver));
    printf("path %d\tsend SYNACK to receiver\n", thread_id);
    
    struct sockaddr_in agent_sender, sender;
    int sockfd_sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    agent_sender.sin_family = AF_INET;
    agent_sender.sin_port = htons(AGENT_SENDER_PORT); // converts the unsigned short integer from host byte order to network byte order
    agent_sender.sin_addr.s_addr = htonl(INADDR_ANY); // bind the socket to all available interfaces
    bind(sockfd_sender, &agent_sender, sizeof(agent_sender));
    
    int sender_len = sizeof(sender);
    recvfrom(sockfd_sender, &SEG_SYN, sizeof(struct segment), 0, &sender, &sender_len);
    printf("path %d\treceive SYN from sender\n", thread_id);
    SEG_SYNACK.HDR.SYN = 1;
    SEG_SYNACK.HDR.ACK = 1;
    SEG_SYNACK.HDR.source_port = AGENT_SENDER_PORT;
    int sender_port = ntohs(sender.sin_port);
    SEG_SYNACK.HDR.dest_port = sender_port;
    sendto(sockfd_sender, &SEG_SYNACK, sizeof(struct segment), 0, &sender, sizeof(sender));
    printf("path %d\tsend SYNACK to sender\n", thread_id);
    
    int maxfd;
    if(sockfd_sender > sockfd_receiver)
        maxfd = sockfd_sender;
    else
        maxfd = sockfd_receiver;
    
    struct segment SEG, SEG_ACK;
    while(!finish)
    {
        fd_set rfds;
        struct timeval tv;
    
        FD_ZERO(&rfds);
        FD_SET(sockfd_sender, &rfds);
        FD_SET(sockfd_receiver, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 1000;
    
        if(select(maxfd + 1, &rfds, NULL, NULL, &tv))
        {
            if(FD_ISSET(sockfd_sender, &rfds))
            {
                recvfrom(sockfd_sender, &SEG, sizeof(struct segment), 0, &sender, &sender_len);
                if(SEG.HDR.FIN == 1)
                {
                    printf("path %d\tget\tfin\n", thread_id);  
                    printf("path %d\tfwd\tfin\n", thread_id); 
                    SEG.HDR.source_port = AGENT_RECEIVER_PORT;
                    SEG.HDR.dest_port = receiver_port;
                    sendto(sockfd_receiver, &SEG, sizeof(struct segment), 0, &receiver, sizeof(receiver));
                }
                else
                {
                    printf("path %d\tget\tdata\t#%d\n", thread_id, SEG.HDR.Seq);  
                    if(rand() % (int)1E6 < (int)(drop_rate * 1E6))
                    {
                        pthread_mutex_lock(&loss_rate_mutex);
                        total_data++;
                        dropped_data++;
                        printf("path %d\tdrop\tdata\t#%d\tloss rate = %4f\n", thread_id, SEG.HDR.Seq, (double)dropped_data / (double)total_data);  
                        pthread_mutex_unlock(&loss_rate_mutex);
                    }
                    else
                    {
                        pthread_mutex_lock(&loss_rate_mutex);
                        total_data++;
                        printf("path %d\tfwd\tdata\t#%d\tloss rate = %4f\n", thread_id, SEG.HDR.Seq, (double)dropped_data / (double)total_data); 
                        pthread_mutex_unlock(&loss_rate_mutex);
                        SEG.HDR.source_port = AGENT_RECEIVER_PORT;
                        SEG.HDR.dest_port = receiver_port;
                        sendto(sockfd_receiver, &SEG, sizeof(struct segment), 0, &receiver, sizeof(receiver));
                    }
                }
            }
            else
            {
                recvfrom(sockfd_receiver, &SEG_ACK, sizeof(struct segment), 0, &receiver, &receiver_len);
                if(SEG_ACK.HDR.FIN == 1)
                {
                    printf("path %d\tget\tfinack\n", thread_id);  
                    printf("path %d\tfwd\tfinack\n", thread_id);
                    SEG_ACK.HDR.source_port = AGENT_SENDER_PORT;
                    SEG_ACK.HDR.dest_port = sender_port;
                    sendto(sockfd_sender, &SEG_ACK, sizeof(struct segment), 0, &sender, sizeof(sender));
                    finish = 1;
                }
                else
                {
                    printf("path %d\tget\tack\t#%d\n", thread_id, SEG_ACK.HDR.ACK);  
                    SEG_ACK.HDR.source_port = AGENT_SENDER_PORT;
                    SEG_ACK.HDR.dest_port = sender_port;
                    printf("path %d\tfwd\tack\t#%d\n", thread_id, SEG_ACK.HDR.ACK);  
                    sendto(sockfd_sender, &SEG_ACK, sizeof(struct segment), 0, &sender, sizeof(sender));
                }
            }
        }
    }
    
    close(sockfd_sender);
    close(sockfd_receiver);
    return;
}

int main(int argc, char **argv) // SENDER_PORT * 3 RECEIVER_PORT * 3 drop_rate
{
    drop_rate = atof(argv[7]);
    srand(time(NULL));
    int arg[THREAD_NUM][3];
    pthread_t threads[THREAD_NUM];

    pthread_mutex_init(&loss_rate_mutex, NULL);
    int i;
    for(i = 0; i < THREAD_NUM; i++)
    {
        arg[i][0] = i;
        arg[i][1] = atoi(argv[i + 1]); // sender
        arg[i][2] = atoi(argv[i + 1 + THREAD_NUM]); // receiver
        pthread_create(&threads[i], NULL, agent, (void *)arg[i]);
    }

    // wait for FINACK
    while(!finish)
        sleep(1);
        
    for(i = 0; i < THREAD_NUM; i++)
        pthread_join(threads[i], NULL);

    return 0;
}
