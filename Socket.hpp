#ifndef SOCKET_H
#define SOCKET_H

#include <string>
#include "sockets.hpp"

class Socket
{
public:
    explicit Socket();   
    ~Socket();

    int fd() const { return sockfd_; }
    void bindAddress(unsigned short port);
    void connect(std::string& ip, unsigned short port);
    void listen(unsigned int backlog);
    int acceptAndSendKey(struct sockaddr* addr, std::string& pub_key);
private:
    int sockfd_;
};

Socket::~Socket()
{
    sockets::close(sockfd_);
}

Socket::Socket()
{
    sockfd_ = sockets::createSocket();
}

void Socket::bindAddress(unsigned short port)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    memset(&addr, 0, sizeof(addr));
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sockets::bindOrFail(sockfd_, (const struct sockaddr*)&addr);
}

void Socket::connect(std::string& ip, unsigned short port)
{
	struct sockaddr_in server_addr;
	bzero(&server_addr,sizeof(server_addr)); // 初始化服务器地址
	server_addr.sin_family = AF_INET;	// IPv4
	server_addr.sin_port = htons(port);	// 端口
	inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr.s_addr);	// ip
    sockets::connect(sockfd_, (const struct sockaddr*)&server_addr);
}

void Socket::listen(unsigned int backlog)
{
    sockets::listenOrFail(sockfd_, backlog);
}

int Socket::acceptAndSendKey(struct sockaddr* addr, std::string& pub_key)
{
    auto fd = sockets::accept(sockfd_, addr);
    if(fd > 0)
    {
        auto ret = sockets::write(fd, pub_key.c_str(), pub_key.length());
        if(ret < 0) {return -1;}
    }
    return fd;
}

#endif // ! SOCKET_H