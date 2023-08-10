#include"interface.h"
#include<iostream>
#include<sys/socket.h>
#include<cstring>
#include<unistd.h>
#include<netinet/in.h>
#include<pthread.h>
#include<sys/fcntl.h>
#include<stdlib.h>
#include<sys/epoll.h>

using namespace std;

#define     BUFFER_LENGTH           1024
#define     EVENTS_LENGTH           1024

int main(int argc, char* argv[])
{
    if(argc < 2)
        return -1;
    int port = atoi(argv[1]);

    
    
    return 0;
}