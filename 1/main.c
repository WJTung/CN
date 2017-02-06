#include "cal.h"
#include "irc.h"

char command_list[COMMAND_NUM][COMMAND_LENGTH];
char buffer[BUFFER_SIZE];
char msg[MSG_SIZE];

char operator_stack[MAX_STACK_SIZE];
double number_stack[MAX_STACK_SIZE];
int operator_stack_size;
int number_stack_size;
int calculator_error;

int main()
{
    struct addrinfo hints; 
    // points to an addrinfo structure that specifies criteria for selecting the socket address structures returned in the list pointed to by res
    struct addrinfo *result;
    
    // set up hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    char host[20] = "irc.ubuntu.com";
    char port[5] = "6667";

    if(getaddrinfo(host, port, &hints, &result) < 0)
    {
        printf("getaddrinfo error\n");
        return -1;
    }

    // socket creates an endpoint for communication and returns a descriptor
    int sockfd;
    if((sockfd = socket(result -> ai_family, result->ai_socktype, result->ai_protocol)) < 0)
    {
        printf("socket error\n");
        return -1;
    }

    // connects the socket referred to by the file descriptor sockfd to the address specified by addr
    if(connect(sockfd, result->ai_addr, result->ai_addrlen) < 0)
    {
        printf("connect error\n");
        return -1;
    }
    puts("Connection success");
    freeaddrinfo(result);
    
    read_msg(sockfd);
    
    char robot_name[NAME_LENGTH] = "ROBOT62";
    FILE *config = fopen("config", "r");
    int len;
    char c;
    
    char channel[CHANNEL_LENGTH];
    while((c = fgetc(config)) != '\''){} // left '
    len = 0;
    while((c = fgetc(config)) != '\'') // right '
    {
        channel[len] = c;
        len++;
    }
    channel[len] = '\0';
    
    char channel_key[CHANNEL_KEY_LENGTH];
    while((c = fgetc(config)) != '\''){} // left '
    len = 0;
    while((c = fgetc(config)) != '\'') // right '
    {
        channel_key[len] = c;
        len++;
    }
    channel_key[len] = '\0';
    
    fclose(config);
    
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "NICK %s\r\n", robot_name);
    send_msg(sockfd);
    read_msg(sockfd);

    memset(msg, 0, sizeof(msg));
    strcpy(msg, "USER CN2016_62 NTUCSIE NTUCSIE :WJT\r\n");
    send_msg(sockfd);
    read_msg(sockfd);
    
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "JOIN %s %s\r\n", channel, channel_key);
    send_msg(sockfd);
    read_msg(sockfd);
    
    char privmsg[20]; 
    sprintf(privmsg, "PRIVMSG %s :", channel);
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "%sHello, I am %s!\n", privmsg, robot_name); // Introduction message when robot enters the channel
    send_msg(sockfd);

    strcpy(command_list[0], "@repeat ");
    strcpy(command_list[1], "@cal ");
    strcpy(command_list[2], "@play ");
    strcpy(command_list[3], "@guess ");
    strcpy(command_list[4], "@help");
    strcpy(command_list[5], "@weather");
    strcpy(command_list[5], "@weather");
    char PING[COMMAND_LENGTH] = "PING";
    char player_name[NAME_LENGTH];
    srand(time(NULL));
    int chance, number, playing = 0;
    while(1)
    {
        int len = read_msg(sockfd);
        int now_index = 0;
        int command;
        int count = 0;
        while(now_index < len)
        {
            if(buffer[now_index] == ':')
            {
                count++;
                now_index++;
                if(count == 2)
                {
                    command = find_command(&now_index);
                    if(command == REPEAT)
                    {
                        repeat(&now_index, sockfd, privmsg);
                        count = 0;
                    }
                    if(command == PLAY)
                    {
                        if(!playing)
                            playing = play(&now_index, sockfd, privmsg, robot_name, player_name, &chance, &number);
                        count = 0;
                    }
                    if(command == GUESS)
                    {
                        if(playing)
                            playing = guess(&now_index,sockfd, privmsg, player_name, &chance, number);
                        count = 0;
                    }
                    if(command == CAL)
                    {
                        char speaker[NAME_LENGTH];
                        get_speaker(&now_index, speaker);
                        
                        memset(operator_stack, 0, sizeof(operator_stack));
                        memset(number_stack, 0, sizeof(number_stack));
                        operator_stack_size = 0;
                        number_stack_size = 0;
                        calculator_error = 0;
                        double result = calculator(&now_index);
                        if(calculator_error == 0)
                        {
                            memset(msg, 0, sizeof(msg));
                            sprintf(msg, "%sANSWER = %.6f (%s)\n", privmsg, result, speaker);
                            send_msg(sockfd);
                        }
                        if(calculator_error == UNMATCHED_PARENTHESES)
                        {
                            memset(msg, 0, sizeof(msg));
                            sprintf(msg, "%sUnmatched Parenthesis, please check format of the expression. (%s)\n", privmsg, speaker);
                            send_msg(sockfd);
                        }
                        if(calculator_error == UNEXPECTED_CHARACTER)
                        {
                            memset(msg, 0, sizeof(msg));
                            sprintf(msg, "%sUnexpected character, please check format of the expression. (%s)\n", privmsg, speaker);
                            send_msg(sockfd);
                        }
                        if(calculator_error == DIVIDE_ZERO)
                        {
                            memset(msg, 0, sizeof(msg));
                            sprintf(msg, "%sDivision by zero. (%s)\n", privmsg, speaker);
                            send_msg(sockfd);
                        }
                        if(calculator_error == ILLEGAL_EXPRESSION)
                        {
                            memset(msg, 0, sizeof(msg));
                            sprintf(msg, "%sIllegal expression, please check format of the expression. (%s)\n", privmsg, speaker);
                            send_msg(sockfd);
                        }
                        count = 0;
                    }
                    if(command == HELP)
                    {
                        help(&now_index, sockfd, privmsg);
                        count = 0;
                    }
                    if(command == WEATHER)
                    {
                        get_weather(&now_index, sockfd, privmsg);
                        count = 0;
                    }
                }
            }
            else
            {
                if(compare_command(now_index, PING))
                {
                    now_index += (strlen(PING) + 1);
                    char server[SERVER_LENGTH];
                    int len = 0;
                    while(buffer[now_index] != '\r')
                    {
                        server[len] = buffer[now_index];
                        len++;
                        now_index++;
                    }
                    server[len] = '\0';
                    now_index += 2; // skip \r \n\
                    count = 0;
                    memset(msg, 0, sizeof(msg));
                    sprintf(msg, "PONG %s\r\n", server);
                    send_msg(sockfd);
                }
                else
                {
                    if(buffer[now_index] == '\n')
                        count = 0; 
                    now_index++;
                }
            }
        }
    }
                
    return 0;
}
    
