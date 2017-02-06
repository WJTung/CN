#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h> // sockaddr_in
#include <sys/types.h>
#include <sys/socket.h> // socket
#include <arpa/inet.h>
#include <stdlib.h>
#include "protocol.h"

#define BUFFER_SIZE 32
#define EMPTY 0
#define RECEIVED 1
#define THREAD_NUM 3

struct segment *SEG_buffer[BUFFER_SIZE];
int status[MAX_SEG_NUM];

int check_status(int buffer_begin, int buffer_end)
{
    int i;
    for(i = buffer_begin; i <= buffer_end; i++)
        if(status[i] != RECEIVED)
            return 0;
    return 1;
}

int main(int argc, char **argv) // IP, PORT * 3, destination file 
{
    int AGENT_RECEIVER_PORT[THREAD_NUM];
    struct sockaddr_in agent_receiver[THREAD_NUM];
    int sockfd[THREAD_NUM];
    int agent_receiver_len[THREAD_NUM];
    
    int i;
    for(i = 0; i < THREAD_NUM; i++)
    {  
        AGENT_RECEIVER_PORT[i] = atoi(argv[i + 2]);
        sockfd[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        agent_receiver[i].sin_family = AF_INET;
        agent_receiver[i].sin_port = htons(AGENT_RECEIVER_PORT[i]);
        inet_aton(argv[1], &agent_receiver[i].sin_addr);
        
        agent_receiver_len[i] = sizeof(agent_receiver[i]);
        struct segment SEG_SYN, SEG_SYNACK;
        SEG_SYN.HDR.SYN = 1; 
        SEG_SYN.HDR.ACK = 0;
        sendto(sockfd[i], &SEG_SYN, sizeof(struct segment), 0, &agent_receiver[i], sizeof(agent_receiver[i]));
        printf("path %d\tsend\tSYN\n", i);
        recvfrom(sockfd[i], &SEG_SYNACK, sizeof(struct segment), 0, &agent_receiver[i], &agent_receiver_len[i]);
        printf("path %d\treceive\tSYNACK\n", i);
    }
        
    FILE *output = fopen(argv[5], "wb");
    int finish = 0;
    int buffer_begin = 1;
    int buffer_end = BUFFER_SIZE;
    
    int maxfd = 0;
    for(i = 0; i < THREAD_NUM; i++)
        if(sockfd[i] > maxfd)
            maxfd = sockfd[i];
        
    while(!finish)
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
                    struct segment *SEG = (struct segment *)malloc(sizeof(struct segment));
                    recvfrom(sockfd[i], SEG, sizeof(struct segment), 0, &agent_receiver[i], &agent_receiver_len[i]);
                    if(SEG->HDR.FIN == 1 && SEG->HDR.ACK == 0)
                    {
                        finish = 1;
                        puts("recv\tfin");
                        SEG->HDR.dest_port = AGENT_RECEIVER_PORT;
                        SEG->HDR.ACK = 1;
                        sendto(sockfd[i], SEG, sizeof(struct segment), 0, &agent_receiver[i], sizeof(agent_receiver[i]));
                        puts("send\tfinack\n");
                        free(SEG); 
                    }
                    else
                    {
                        if(SEG->HDR.Seq > buffer_end)
                        {
                            printf("path %d\tdrop\tdata\t#%d\n", i, SEG->HDR.Seq);
                            if(check_status(buffer_begin, buffer_end))
                            {
                                puts("flush");
                                int j;
                                for(j = 0; j < BUFFER_SIZE; j++)
                                {
                                    fwrite(SEG_buffer[j]->data, sizeof(char), SEG_buffer[j]->HDR.data_length, output);
                                    free(SEG_buffer[j]);
                                    SEG_buffer[j] = NULL;
                                }
                                buffer_begin += BUFFER_SIZE;
                                buffer_end += BUFFER_SIZE;
                            }
                            free(SEG);
                        }
                        else
                        {
                            if(status[SEG->HDR.Seq] == EMPTY)
                            {
                                printf("path %d\trecv\tdata\t#%d\n", i, SEG->HDR.Seq);
                                status[SEG->HDR.Seq] = RECEIVED;
                                SEG->HDR.dest_port = AGENT_RECEIVER_PORT;
                                SEG->HDR.ACK = SEG->HDR.Seq;
                                SEG->HDR.Seq = 0;
                                sendto(sockfd[i], SEG, sizeof(struct segment), 0, &agent_receiver[i], sizeof(agent_receiver[i]));
                                printf("path %d\tsend\tack\t#%d\n", i, SEG->HDR.ACK);
                                SEG_buffer[SEG->HDR.ACK - buffer_begin] = SEG;
                            }
                            else
                            {
                                printf("path %d\tignr\tdata\t#%d\n", i, SEG->HDR.Seq);
                                SEG->HDR.dest_port = AGENT_RECEIVER_PORT;
                                SEG->HDR.ACK = SEG->HDR.Seq;
                                SEG->HDR.Seq = 0;
                                sendto(sockfd[i], SEG, sizeof(struct segment), 0, &agent_receiver[i], sizeof(agent_receiver[i]));
                                printf("path %d\tsend\tack\t#%d\n", i, SEG->HDR.ACK);
                                free(SEG);
                            }
                            // printf("Received packet from %s : %d\n", inet_ntoa(agent_receiver.sin_addr), ntohs(agent_receiver.sin_port));
                        }
                    }
                }
            }
        }
    }   
   
    for(i = 0; i < THREAD_NUM; i++)
        close(sockfd[i]);
    
    puts("flush");
    for(i = 0; i < BUFFER_SIZE && status[buffer_begin + i] == RECEIVED; i++)
    {
        fwrite(SEG_buffer[i]->data, sizeof(char), SEG_buffer[i]->HDR.data_length, output);
        free(SEG_buffer[i]);
        SEG_buffer[i] = NULL;
    }
    fclose(output);
    
    return 0;
}
