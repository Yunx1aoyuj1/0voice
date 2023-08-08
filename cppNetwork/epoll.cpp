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

#define         BUFFER_LENGTH           1024
#define         EVENTS_LENGTH           1024


int main(int argc, char* argv[])
{
    if(argc < 2)
        return -1;
    
    int port = atoi(argv[1]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd <= 0)
    {
        errlog<<"create sockfd err."<<std::endl;
        return -1;
    }
    //非阻塞IO
    bool flags = fcntl(sockfd, F_GETFL);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    if(bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
    {
        errlog<<"bind err.sockfd:"<<sockfd<<"   port:"<<port<<std::endl;
        return -1;
    }

    if(listen(sockfd, 20) < 0)
    {
        errlog<<"listen err."<<std::endl;
        return -1;
    }

    errlog<<"listen port:"<<port<<std::endl;

    //epoll
    int epfd = epoll_create(1);     //只需要 > 0 即可

    struct epoll_event ev;
    ev.data.fd = sockfd;
    ev.events = EPOLLIN;

    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    struct epoll_event events[EVENTS_LENGTH] = {0};

    struct sockaddr_in clientAddr;
    socklen_t clientlen = sizeof(clientAddr);

    while (1)
    {
        int nready = epoll_wait(epfd, events, EVENTS_LENGTH, -1);   //最后一个参数的含义

        if(nready <= 0)
        {
            continue;
        }

        for(int i = 0; i < nready; ++i)
        {
            if (events[i].data.fd == sockfd)
            {
                int clientfd = accept(sockfd, (struct sockaddr*)&clientAddr, &clientlen);
                if(clientfd <= 0)
                {
                    errlog<<"accept err."<<std::endl;
                    return -1;
                }

                errlog<<"accept clientfd:"<<clientfd<<std::endl;

                ev.data.fd = clientfd;
                ev.events = EPOLLIN;

                epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);
            }
            else if(events[i].events & EPOLLIN)
            {
                char buff[BUFFER_LENGTH] = {0};
                int len = recv(events[i].data.fd, buff, BUFFER_LENGTH, 0);
                if(len < 0)
                {
                    errlog<<"recv err."<<std::endl;
                    return -1;
                }
                else if(len == 0)
                {
                    errlog<<"close clientfd:"<<events[i].data.fd<<std::endl;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);

                    close(events[i].data.fd);

                    continue;
                }

                errlog<<"recv:"<<buff<<std::endl;

                send(events[i].data.fd, buff, len, 0);
            }
        }
    }
    
    close(sockfd);

    return 0;
}