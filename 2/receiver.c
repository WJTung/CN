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

int main(int argc, char **argv) // IP, PORT, destination file 
{
    int AGENT_RECEIVER_PORT = atoi(argv[2]);

    struct sockaddr_in agent_receiver;
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    agent_receiver.sin_family = AF_INET;
    agent_receiver.sin_port = htons(AGENT_RECEIVER_PORT);
    inet_aton(argv[1], &agent_receiver.sin_addr);
    
    int agent_receiver_len = sizeof(agent_receiver);
    struct segment SEG_SYN, SEG_SYNACK;
    SEG_SYN.HDR.SYN = 1; 
    SEG_SYN.HDR.ACK = 0;
    sendto(sockfd, &SEG_SYN, sizeof(struct segment), 0, &agent_receiver, sizeof(agent_receiver));
    puts("send\tSYN");
    recvfrom(sockfd, &SEG_SYNACK, sizeof(struct segment), 0, &agent_receiver, &agent_receiver_len);
    puts("receive\tSYNACK");
    int receiver_port = SEG_SYNACK.HDR.dest_port;
    
    FILE *output = fopen(argv[3], "wb");
    int finish = 0;
    int buffer_begin = 1;
    int buffer_end = BUFFER_SIZE;
    while(!finish)
    {
        struct segment *SEG = (struct segment *)malloc(sizeof(struct segment));
        recvfrom(sockfd, SEG, sizeof(struct segment), 0, &agent_receiver, &agent_receiver_len);
        if(SEG->HDR.FIN == 1 && SEG->HDR.ACK == 0)
        {
            finish = 1;
            puts("recv\tfin");
            SEG->HDR.source_port = receiver_port;
            SEG->HDR.dest_port = AGENT_RECEIVER_PORT;
            SEG->HDR.ACK = 1;
            sendto(sockfd, SEG, sizeof(struct segment), 0, &agent_receiver, sizeof(agent_receiver));
            printf("send\tfinack\n");
            free(SEG); 
            close(sockfd);
        }
        else
        {
            if(SEG->HDR.Seq > buffer_end)
            {
                printf("drop\tdata\t#%d\n", SEG->HDR.Seq);
                if(check_status(buffer_begin, buffer_end))
                {
                    puts("flush");
                    int i;
                    for(i = 0; i < BUFFER_SIZE; i++)
                    {
                        fwrite(SEG_buffer[i]->data, sizeof(char), SEG_buffer[i]->HDR.data_length, output);
                        free(SEG_buffer[i]);
                        SEG_buffer[i] = NULL;
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
                    printf("recv\tdata\t#%d\n", SEG->HDR.Seq);
                    status[SEG->HDR.Seq] = RECEIVED;
                    SEG->HDR.source_port = receiver_port;
                    SEG->HDR.dest_port = AGENT_RECEIVER_PORT;
                    SEG->HDR.ACK = SEG->HDR.Seq;
                    SEG->HDR.Seq = 0;
                    sendto(sockfd, SEG, sizeof(struct segment), 0, &agent_receiver, sizeof(agent_receiver));
                    printf("send\tack\t#%d\n", SEG->HDR.ACK);
                    SEG_buffer[SEG->HDR.ACK - buffer_begin] = SEG;
                }
                else
                {
                    printf("ignr\tdata\t#%d\n", SEG->HDR.Seq);
                    SEG->HDR.source_port = receiver_port;
                    SEG->HDR.dest_port = AGENT_RECEIVER_PORT;
                    SEG->HDR.ACK = SEG->HDR.Seq;
                    SEG->HDR.Seq = 0;
                    sendto(sockfd, SEG, sizeof(struct segment), 0, &agent_receiver, sizeof(agent_receiver));
                    printf("send\tack\t#%d\n", SEG->HDR.ACK);
                    free(SEG);
                }
                // printf("Received packet from %s : %d\n", inet_ntoa(agent_receiver.sin_addr), ntohs(agent_receiver.sin_port));
            }
        }
    }   
   
    puts("flush");
    int i;
    for(i = 0; i < BUFFER_SIZE && status[buffer_begin + i] == RECEIVED; i++)
    {
        fwrite(SEG_buffer[i]->data, sizeof(char), SEG_buffer[i]->HDR.data_length, output);
        free(SEG_buffer[i]);
        SEG_buffer[i] = NULL;
    }
    fclose(output);
    
    return 0;
}
