#define MAX_DATA_SIZE 1024
#define MAX_SEG_NUM 10000

struct header
{
    int source_port;
    int dest_port;
    int Seq;
    int ACK;
    int SYN;
    int FIN;
    int data_length;
};

struct segment
{
    struct header HDR;
    char data[MAX_DATA_SIZE];
};
