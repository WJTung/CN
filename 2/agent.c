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
#include "protocol.h"

int main(int argc, char **argv) // SENDER_PORT RECEIVER_PORT drop_rate
{
    int AGENT_SENDER_PORT = atoi(argv[1]);
    int AGENT_RECEIVER_PORT = atoi(argv[2]);
    double drop_rate = atof(argv[3]);

    struct segment SEG_SYN, SEG_SYNACK;
    
    struct sockaddr_in agent_receiver, receiver;
    int sockfd_receiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    agent_receiver.sin_family = AF_INET;
    agent_receiver.sin_port = htons(AGENT_RECEIVER_PORT); 
    agent_receiver.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sockfd_receiver, &agent_receiver, sizeof(agent_receiver));
    
    int receiver_len = sizeof(receiver);
    recvfrom(sockfd_receiver, &SEG_SYN, sizeof(struct segment), 0, &receiver, &receiver_len);
    puts("receive SYN from receiver");
    SEG_SYNACK.HDR.SYN = 1;
    SEG_SYNACK.HDR.ACK = 1;
    SEG_SYNACK.HDR.source_port = AGENT_RECEIVER_PORT;
    int receiver_port = ntohs(receiver.sin_port);
    SEG_SYNACK.HDR.dest_port = receiver_port;
    sendto(sockfd_receiver, &SEG_SYNACK, sizeof(struct segment), 0, &receiver, sizeof(receiver));
    puts("send SYNACK to receiver");
    
    struct sockaddr_in agent_sender, sender;
    int sockfd_sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    agent_sender.sin_family = AF_INET;
    agent_sender.sin_port = htons(AGENT_SENDER_PORT); // converts the unsigned short integer from host byte order to network byte order
    agent_sender.sin_addr.s_addr = htonl(INADDR_ANY); // bind the socket to all available interfaces
    bind(sockfd_sender, &agent_sender, sizeof(agent_sender));
    
    int sender_len = sizeof(sender);
    recvfrom(sockfd_sender, &SEG_SYN, sizeof(struct segment), 0, &sender, &sender_len);
    puts("receive SYN from sender");
    SEG_SYNACK.HDR.SYN = 1;
    SEG_SYNACK.HDR.ACK = 1;
    SEG_SYNACK.HDR.source_port = AGENT_SENDER_PORT;
    int sender_port = ntohs(sender.sin_port);
    SEG_SYNACK.HDR.dest_port = sender_port;
    sendto(sockfd_sender, &SEG_SYNACK, sizeof(struct segment), 0, &sender, sizeof(sender));
    puts("send SYNACK to sender");
    
    int maxfd;
    if(sockfd_sender > sockfd_receiver)
        maxfd = sockfd_sender;
    else
        maxfd = sockfd_receiver;
    
    struct segment SEG, SEG_ACK;
    int total_data = 0, dropped_data = 0;
    int finish = 0;
    srand(time(NULL));
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
                    printf("get\tfin\n");  
                    printf("fwd\tfin\n"); 
                    SEG.HDR.source_port = AGENT_RECEIVER_PORT;
                    SEG.HDR.dest_port = receiver_port;
                    sendto(sockfd_receiver, &SEG, sizeof(struct segment), 0, &receiver, sizeof(receiver));
                }
                else
                {
                    printf("get\tdata\t#%d\n", SEG.HDR.Seq);  
                    total_data++;
                    if(rand() % (int)1E6 < (int)(drop_rate * 1E6))
                    {
                        dropped_data++;
                        printf("drop\tdata\t#%d\tloss rate = %4f\n", SEG.HDR.Seq, (double)dropped_data / (double)total_data);  
                    }
                    else
                    {
                        printf("fwd\tdata\t#%d\tloss rate = %4f\n", SEG.HDR.Seq, (double)dropped_data / (double)total_data); 
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
                    printf("get\tfinack\n");  
                    printf("fwd\tfinack\n");
                    SEG_ACK.HDR.source_port = AGENT_SENDER_PORT;
                    SEG_ACK.HDR.dest_port = sender_port;
                    sendto(sockfd_sender, &SEG_ACK, sizeof(struct segment), 0, &sender, sizeof(sender));
                    finish = 1;
                }
                else
                {
                    printf("get\tack\t#%d\n", SEG_ACK.HDR.ACK);  
                    SEG_ACK.HDR.source_port = AGENT_SENDER_PORT;
                    SEG_ACK.HDR.dest_port = sender_port;
                    printf("fwd\tack\t#%d\n", SEG_ACK.HDR.ACK);  
                    sendto(sockfd_sender, &SEG_ACK, sizeof(struct segment), 0, &sender, sizeof(sender));
                }
            }
        }
    }
    
    close(sockfd_sender);
    close(sockfd_receiver);
    return 0;
}
