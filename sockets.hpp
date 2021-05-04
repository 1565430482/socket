#ifndef SOCKETS_H
#define SOCKETS_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>

namespace sockets
{
    int createSocket();
    int accept(int sockfd, struct sockaddr* addr);
    void bindOrFail(int sockfd, const struct sockaddr* addr);
    int  connect(int sockfd, const struct sockaddr* addr);
    void listenOrFail(int sockfd, unsigned int backlog);
    ssize_t read(int sockfd, void* buf, size_t count);
    ssize_t write(int sockfd, const void* buf, size_t count);
    void close(int sockfd);
}

int sockets::createSocket()
{
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd == -1)
    {
        perror("socket fd failed");
        EXIT_FAILURE;
    }
    return listen_fd;
}

int sockets::accept(int sockfd, struct sockaddr* addr)
{
    socklen_t addrlen = static_cast<socklen_t>(sizeof *addr);
    int ret = ::accept(sockfd, addr, &addrlen);
    if (ret < 0)
    {
        perror("accept failed!");
        EXIT_FAILURE;
    }
    return ret;
}

void sockets::bindOrFail(int sockfd, const struct sockaddr* addr)
{
    socklen_t addrlen = static_cast<socklen_t>(sizeof *addr);
    int ret = ::bind(sockfd, addr, addrlen);
    if(ret < 0)
    {
        perror("bind failed!");
        EXIT_FAILURE;
    }
}

int sockets::connect(int sockfd, const struct sockaddr* addr)
{
    socklen_t addrlen = static_cast<socklen_t>(sizeof *addr);
    return ::connect(sockfd, addr, addrlen);
}
void sockets::listenOrFail(int sockfd, unsigned int backlog)
{
    int ret = ::listen(sockfd, backlog);
    if (ret < 0)
    {
        perror("listen failed!");
        EXIT_FAILURE;
    }
}
ssize_t sockets::read(int sockfd, void* buf, size_t count)
{
    return ::read(sockfd, buf, count);
}
ssize_t sockets::write(int sockfd, const void* buf, size_t count)
{
    return ::write(sockfd, buf, count);
}

void sockets::close(int sockfd)
{
    if(::close(sockfd)<0)
    {
        perror("close failed!");
        EXIT_FAILURE;
    }
}

#endif