#include "irc.h"

extern char command_list[COMMAND_NUM][COMMAND_LENGTH];
extern char buffer[BUFFER_SIZE];
extern char msg[MSG_SIZE];

int compare_command(int now_index, char *command)
{
    int len = strlen(command);
    int i;
    for(i = 0; i < len; i++)
        if(buffer[now_index + i] != command[i])
            return 0;
    return 1;
}

int find_command(int *now_index)
{
    int command_index;
    for(command_index = 0; command_index < COMMAND_NUM; command_index++)
    {
        if(compare_command((*now_index), command_list[command_index]))
        {   
            (*now_index) += strlen(command_list[command_index]);
            return command_index;
        }
    }
    (*now_index)++;
    return -1;
}
    
int read_msg(int sockfd)
{
    memset(buffer, 0, sizeof(buffer));
    int len;
    if((len = read(sockfd, buffer, BUFFER_SIZE - 1)) == -1)
    {
        puts("read msg error");
        return -1;
    }
    buffer[len] = '\0';
    printf("%s", buffer);
    return len;
}

int send_msg(int sockfd)
{
    int len = strlen(msg);
    if(write(sockfd, msg, len) == -1)
    {    
        puts("send msg error");
        return -1;
    }
    return len;
}

void get_speaker(int *now_index, char *speaker)
{
    int line_start = (*now_index);
    while(line_start != 0 && buffer[line_start - 1] != '\n')
        line_start--;
    int i = line_start + 1, len = 0;
    while(buffer[i] != '!')
    {
        speaker[len] = buffer[i];
        i++;
        len++;
    }
    speaker[len] = '\0';
}

void repeat(int *now_index, int sockfd, char *privmsg)
{
    char speaker[NAME_LENGTH];
    get_speaker(now_index, speaker);
    
    char content[CONTENT_LENGTH];
    int len = 0;
    while(buffer[(*now_index)] != '\r')
    {
        content[len] = buffer[(*now_index)];
        len++;
        (*now_index)++;
    }
    content[len] = '\0';
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "%s%s (%s)\n", privmsg, content, speaker);
    (*now_index) += 2; // skip \r \n
    send_msg(sockfd);
    return;
}

int play(int *now_index, int sockfd, char *privmsg, char *robot_name, char *player_name, int *chance, int *number)
{
    char speaker[NAME_LENGTH];
    get_speaker(now_index, speaker);
    
    char specified_robot[NAME_LENGTH];
    int len = 0;
    while(buffer[(*now_index)] != '\r')
    {
        specified_robot[len] = buffer[(*now_index)];
        len++;
        (*now_index)++;
    }
    specified_robot[len] = '\0';
    (*now_index) += 2; // skip \r \n
    
    if(strcmp(specified_robot, robot_name) == 0)
    {
        (*chance) = GUESS_TIME;
        (*number) = rand() % (NUM_MAX - NUM_MIN + 1) + NUM_MIN;
        memset(player_name, 0, sizeof(player_name));
        strcpy(player_name, speaker);
        memset(msg, 0, sizeof(msg));
        sprintf(msg, "%sHello, I am %s! Let's play a game, player %s! You have %d chances to guess a number from %d to %d. (%s)\n", privmsg, robot_name, player_name, GUESS_TIME, NUM_MIN, NUM_MAX, speaker);
        send_msg(sockfd);
        return PLAYING;
    }
    return END;
}

int guess(int *now_index, int sockfd, char *privmsg, char *player_name, int *chance, int number)
{
    char speaker[NAME_LENGTH];
    get_speaker(now_index, speaker);
    
    int guess_num = 0; 
    while(buffer[(*now_index)] != '\r')
    {
        guess_num *= 10;
        guess_num += buffer[(*now_index)] - '0';
        (*now_index)++;
    }
    (*now_index) += 2; // skip \r \n
    
    if(strcmp(speaker, player_name) == 0)
    {
        int state = PLAYING;
        memset(msg, 0, sizeof(msg));
        if(guess_num < NUM_MIN || guess_num > NUM_MAX)
            sprintf(msg, "%sThis number is not in the possible range. (%s)\n", privmsg, speaker);
        else if(guess_num > number)
        {
            (*chance)--;
            if((*chance) == 0)
            {
                sprintf(msg, "%sSmaller than %d. Sorry, you lose! The answer is %d. (%s)\n", privmsg, guess_num, number, speaker);
                state = END;
            }
            else if((*chance) == 1)
                sprintf(msg, "%sSmaller than %d. You still have %d chance to guess! (%s)\n", privmsg, guess_num, (*chance), speaker);
            else
                sprintf(msg, "%sSmaller than %d. You still have %d chances to guess! (%s)\n", privmsg, guess_num, (*chance), speaker);
        }
        if(guess_num < number)
        {
            (*chance)--;
            if((*chance) == 0)
            {
                sprintf(msg, "%sLarger than %d. Sorry, you lose! The answer is %d. (%s)\n", privmsg, guess_num, number, speaker);
                state = END;
            }
            else if((*chance) == 1)
                sprintf(msg, "%sLarger than %d. You still have %d chance to guess! (%s)\n", privmsg, guess_num, (*chance), speaker);
            else 
                sprintf(msg, "%sLarger than %d. You still have %d chances to guess! (%s)\n", privmsg, guess_num, (*chance), speaker);
        }
        if(guess_num == number)
        {
            sprintf(msg, "%sCongratulations! You win! (%s)\r\n", privmsg, speaker);
            state = END;
        }
        send_msg(sockfd);
        return state;
    }
    return PLAYING;
}

void help(int *now_index, int sockfd, char *privmsg)
{
    char speaker[NAME_LENGTH];
    get_speaker(now_index, speaker);
    
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "%s@repeat <string> (%s)\r\n", privmsg, speaker);
    send_msg(sockfd);
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "%s@play <robot name> (%s)\r\n", privmsg, speaker);
    send_msg(sockfd);
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "%s@guess <integer> (%s)\r\n", privmsg, speaker);
    send_msg(sockfd);
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "%s@cal <expression> (%s)\r\n", privmsg, speaker);
    send_msg(sockfd);
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "%s@weather (%s)\r\n", privmsg, speaker);
    send_msg(sockfd);
    
    while(buffer[(*now_index)] != '\r')
        (*now_index)++;
    (*now_index) += 2; // skip \r \n
    
    return;
}
