#include "irc.h"

extern char msg[MSG_SIZE];

int find_keyword(char *html_buffer, int *now_index, char *keyword)
{
    int i, j;
    while((*now_index) < strlen(html_buffer))
    {
        int same = 1;
        for(j = 0; j < strlen(keyword) && same; j++)
            if(html_buffer[(*now_index) + j] != keyword[j])
                same = 0;
        if(same)
            return (*now_index);
        (*now_index)++;
    }
    return -1;
}
        
int handle_keyword(char *html_buffer, int *now_index, int sockfd, char *privmsg, char *speaker)
{
    
    int left_index, right_index, index;
    char info[INFO_LENGTH];
    int len;
    
    /* Date and time */
    left_index = (*now_index);
    right_index = find_keyword(html_buffer, now_index, "</th>");
    memset(info, 0, sizeof(info));
    len = 0;
    for(index = left_index; index < right_index; index++)
    {
        info[len] = html_buffer[index];
        len++;
    }
    info[len] = '\0';
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "%s%s\n", privmsg, info);
    send_msg(sockfd);
    
    /* Temperature */
    left_index = find_keyword(html_buffer, now_index, "<td>") + strlen("<td>");
    right_index = find_keyword(html_buffer, now_index, "</td>");
    memset(info, 0, sizeof(info));
    len = 0;
    for(index = left_index; index < right_index; index++)
    {
        info[len] = html_buffer[index];
        len++;
    }
    info[len] = '\0';
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "%s溫度 : %s\n", privmsg, info);
    send_msg(sockfd);
    
    /* Weather */
    left_index = find_keyword(html_buffer, now_index, "alt=\"") + strlen("alt=\"");
    right_index = find_keyword(html_buffer, now_index, "\" ");
    memset(info, 0, sizeof(info));
    len = 0;
    for(index = left_index; index < right_index; index++)
    {
        info[len] = html_buffer[index];
        len++;
    }
    info[len] = '\0';
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "%s天氣狀況 : %s\n", privmsg, info);
    send_msg(sockfd);
    
    /* Comfort level */
    left_index = find_keyword(html_buffer, now_index, "<td>") + strlen("<td>");
    right_index = find_keyword(html_buffer, now_index, "</td>");
    memset(info, 0, sizeof(info));
    len = 0;
    for(index = left_index; index < right_index; index++)
    {
        info[len] = html_buffer[index];
        len++;
    }
    info[len] = '\0';
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "%s舒適度 : %s\n", privmsg, info);
    send_msg(sockfd);
    
    /* Probability of precipitation */
    left_index = find_keyword(html_buffer, now_index, "<td>") + strlen("<td>");
    right_index = find_keyword(html_buffer, now_index, "</td>");
    memset(info, 0, sizeof(info));
    len = 0;
    for(index = left_index; index < right_index; index++)
    {
        info[len] = html_buffer[index];
        len++;
    }
    info[len] = '\0';
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "%s降雨機率 : %s\n", privmsg, info);
    send_msg(sockfd);
}

void get_weather(int *now_index, int sockfd, char *privmsg)
{
    char speaker[NAME_LENGTH];
    get_speaker(now_index, speaker);
    
    char host[HOST_LENGTH] = "cwb.gov.tw";
    char page[PAGE_LENGTH] = "V7/forecast/taiwan/New_Taipei_City.htm";
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct hostent *hent;
    char *ip = (char *)malloc(IP_LENGTH + 1);
    memset(ip, 0, sizeof(ip));
    hent = gethostbyname(host);
    inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip, IP_LENGTH);
    printf("IP is %s\n", ip);
    struct sockaddr_in *remote = malloc(sizeof(struct sockaddr_in));
    remote->sin_family = AF_INET;
    inet_pton(AF_INET, ip, (void *)(&(remote->sin_addr.s_addr)));
    remote->sin_port = htons(PORT);
    connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr));
    
    // send the query to the server
    char request[REQUEST_LENGTH];
    sprintf(request, "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: HTMLGET 1.0\r\n\r\n", page, host);
    int sent = 0;
    while(sent < strlen(request))
        sent += write(sock, request + sent, strlen(request) - sent);
    
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "%sGetting weather information from http://www.cwb.gov.tw/V7/forecast/taiwan/New_Taipei_City.htm (%s)\n", privmsg, speaker);
    send_msg(sockfd);
    
    // receive the page
    int bytes_read;
    char html_buffer[HTML_BUFFER_SIZE];
    int html_begin = 0;
    int now_keyword = 0;
    char keyword[KEYWORD_LENGTH] = "<th scope=\"row\">";
    while((bytes_read = read(sock, html_buffer, HTML_BUFFER_SIZE - 1)) > 0)
    {
        html_buffer[bytes_read] = '\0';
        char *html_content;
        if(html_begin == 0)
        {
            html_content = strstr(html_buffer, "\r\n\r\n");
            if(html_content != NULL)
            {
                html_begin = 1;
                html_content += 4; // skip \r\n\r\n
            }
        }
        else
            html_content = html_buffer;
        if(html_begin)
        {
            int now_index = 0;
            while(now_keyword < KEYWORD_NUM && find_keyword(html_buffer, &now_index, keyword) >= 0)
            {
                now_index += strlen(keyword);
                handle_keyword(html_buffer, &now_index, sockfd, privmsg, speaker);
                now_keyword++;
            }
            // printf("%s", html_content);
        }
    }
    
    free(ip);
    free(remote);
    close(sock);
    return;
}
       
        
        
        
