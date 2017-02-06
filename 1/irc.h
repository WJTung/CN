#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MSG_SIZE 256
#define BUFFER_SIZE 16384
#define COMMAND_NUM 6
#define COMMAND_LENGTH 10
#define NAME_LENGTH 20
#define CHANNEL_LENGTH 20
#define CHANNEL_KEY_LENGTH 20
#define CONTENT_LENGTH 1000
#define SERVER_LENGTH 100
#define GUESS_TIME 5
#define NUM_MAX 100
#define NUM_MIN 1
#define REPEAT 0
#define CAL 1
#define PLAY 2
#define GUESS 3
#define HELP 4
#define WEATHER 5
#define END 0
#define PLAYING 1

#define PORT 80
#define HOST_LENGTH 100
#define PAGE_LENGTH 100
#define IP_LENGTH 15 // xxx.xxx.xxx.xxx
#define REQUEST_LENGTH 200
#define HTML_BUFFER_SIZE 16384
#define KEYWORD_LENGTH 20
#define KEYWORD_NUM 2
#define INFO_LENGTH 50

int compare_command(int now_index, char *command);
int find_command(int *now_index);
int read_msg(int sockfd);
int send_msg(int sockfd);
void get_speaker(int *now_index, char *speaker);
void repeat(int *now_index, int sockfd, char *privmsg);
int play(int *now_index, int sockfd, char *privmsg, char *robot_name, char *player_name, int *chance, int *number);
int guess(int *now_index, int sockfd, char *privmsg, char *player_name, int *chance, int number);
void help(int *now_index, int sockfd, char *privmsg);
int find_keyword(char *html_buffer, int *now_index, char *keyword);
int handle_keyword(char *html_buffer, int *now_index, int sockfd, char *privmsg, char *speaker);
void get_weather(int *now_index, int sockfd, char *privmsg);
