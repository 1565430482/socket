#define _LOG4CPP_ 

#include <sys/epoll.h>
#include <signal.h>
#include "Aes.hpp"
#include "Rsa.hpp"
#include "log.hpp"
#include "Socket.hpp"
#include "h264Handler.hpp"
#include "aacHandler.hpp"

bool quit = false;

void SignalFunc(int sig_num)
{
    switch (sig_num)
    {
        case SIGINT:
        quit = true;
        exit(1);
        break;
    }
}

void h264_aac_enc_to_ps(const char* h264_fileName, const char* aac_fileName, 
                    const char* ps_fileName, Aes& obj_Aes)
{
    FILE* h264_bit_stream = fopen(h264_fileName, "r");
    FILE* aac_bit_stream = fopen(aac_fileName, "r");
    FILE* program_stream = fopen(ps_fileName, "w");
    if(h264_bit_stream == NULL) 
    {
        perror("Open file error\n");
		EXIT_FAILURE;
    }
    else if(aac_bit_stream == NULL) 
    {
        perror("Open file error\n");
		EXIT_FAILURE;
    }
    else if(aac_bit_stream == NULL) 
    {
        perror("Open file error\n");
		EXIT_FAILURE;
    }

    unsigned char* aacframe  = (unsigned char*)malloc(1024*5);
    unsigned char* aacbuffer = (unsigned char*)malloc(1024*1024);
    int data_size = fread(aacbuffer, 1, 1024*1024, aac_bit_stream);
    int size      = 0;
    
    
    int buf_size       = 10000;
    Nalu_t* n;
    n = (Nalu_t*)calloc(1, sizeof(Nalu_t));
    if (n == NULL)
    {
		printf("Alloc NALU Error\n");
		EXIT_FAILURE;
	}

    n->max_size = buf_size;
    n->buf = (char*)calloc (buf_size, sizeof (char));
	if (n->buf == NULL)
    {
		free (n);
		printf ("AllocNALU: n->buf");
		EXIT_FAILURE;
	}
    while (!feof(h264_bit_stream)|| !(data_size <= 0))
    {
        if(!feof(h264_bit_stream))
        {
            int data_length          = get_annexb_Nalu(n, h264_bit_stream);
            int num_Aes_blocks       = (n->len - 1) / AES_BLOCK_SIZE;
            std::string str_Raw_data = std::string(&n->buf[1], num_Aes_blocks * AES_BLOCK_SIZE);
            auto str_Encode_data     = obj_Aes.EncryptAES(str_Raw_data);
            memcpy(n->buf+1, str_Encode_data.c_str(), num_Aes_blocks * AES_BLOCK_SIZE);
            
            char header_info[75] = { 0 };
            int n_header_pos  = 0;
            int n_header_size = 0;
            int n_buffer_pos  = 0;
            unsigned long long pts = 0;
            if (n->nalu_unit_type == NALU_TYPE_SLICE)
            {
                gb28181_make_ps_header(header_info, pts);
                n_header_pos  += PS_HDR_LEN;
                n_header_size += PS_HDR_LEN;
            }
            else if (n->nalu_unit_type == NALU_TYPE_SPS)
            {
                gb28181_make_ps_header(header_info, pts);
                n_header_pos += PS_HDR_LEN;

                gb28181_make_sys_header(header_info + n_header_pos);
                n_header_pos += SYS_HDR_LEN;

                gb28181_make_psm_header(header_info + n_header_pos);
                n_header_pos += PSM_HDR_LEN;

                n_header_size = PS_HDR_LEN + SYS_HDR_LEN + PSM_HDR_LEN;
            
            }
            fwrite(header_info, 1, n_header_size, program_stream);
            ::memset(header_info, 0, n_header_size);
            while (n->len > PS_PES_PAYLOAD_SIZE)
            {
                gb28181_make_pes_header(header_info , 0xE0, PS_PES_PAYLOAD_SIZE, pts, 0);
                fwrite(header_info, 1, PES_HDR_LEN, program_stream);
                fwrite(n->buf + n_buffer_pos, 1, PS_PES_PAYLOAD_SIZE, program_stream);
                n->len       -= PS_PES_PAYLOAD_SIZE;
                n_buffer_pos += PS_PES_PAYLOAD_SIZE; 
            }
            
            gb28181_make_pes_header(header_info, 0xE0, n->len, pts, 0);
            fwrite(header_info, 1, PES_HDR_LEN, program_stream);
            fwrite(n->buf+n_buffer_pos, 1, n->len, program_stream);    //尾数
        }
        if (data_size > 0)
        {
            char header_info[50] = {0};
            int ret = get_ADTS_frame(aacbuffer, data_size, aacframe, &size);
            if (ret == -1) 
            {
                std::cout << "break" << std::endl;
                break;
            }
            int num_Aes_blocks   = (size-7) / AES_BLOCK_SIZE;
            auto str_Raw_data    = std::string((const char*)&aacframe[7], num_Aes_blocks * AES_BLOCK_SIZE);
            auto str_Encode_data = obj_Aes.EncryptAES(str_Raw_data);
            memcpy(aacframe + 7, str_Encode_data.c_str(), num_Aes_blocks * AES_BLOCK_SIZE);

            // while (size > PS_PES_PAYLOAD_SIZE)
            // {
            //     gb28181_make_pes_header(header_info, 0xC0, PS_PES_PAYLOAD_SIZE, 0, 0);
            //     fwrite(header_info, 1, PES_HDR_LEN, program_stream);
            //     fwrite(aacframe, 1, PS_PES_PAYLOAD_SIZE, program_stream);
            // }
            if (feof(h264_bit_stream))
            {
                gb28181_make_ps_header(header_info, 0);
                gb28181_make_pes_header(header_info + PS_HDR_LEN, 0xC0, 
                    size > PS_PES_PAYLOAD_SIZE ? PS_PES_PAYLOAD_SIZE : size, 0, 0);
                fwrite(header_info, 1, PS_HDR_LEN + PES_HDR_LEN, program_stream);
                fwrite(aacframe, 1, size , program_stream);
            }
            else
            {
                gb28181_make_pes_header(header_info, 0xC0, 
                    size > PS_PES_PAYLOAD_SIZE ? PS_PES_PAYLOAD_SIZE : size, 0, 0);
                fwrite(header_info, 1, PES_HDR_LEN, program_stream);
                fwrite(aacframe, 1, size , program_stream);
            }
            data_size -= size;
            aacbuffer += size;
        }
    } 
    fclose(program_stream);
    fclose(h264_bit_stream);
    fclose(aac_bit_stream);   
}

int main(int argc, char** argv)
{
    //非对称加密配合对称加密
    Aes obj_Aes;       //默认构造 
    std::pair<std::string, std::string> rsakey;//先公钥，后私钥
    rsa::GenerateRSAKey(rsakey.first, rsakey.second);
    FILE* send_ps = NULL;
    LOG->info(rsakey.first.c_str());//日志记录公钥
    LOG->info(rsakey.second.c_str());//日志记录私钥

    Socket s;
    s.bindAddress(6799);
    s.listen(10);
    signal(SIGINT, SignalFunc);
    
    char buf[1024] = {0};
    struct sockaddr_in client_addr; //客户端
    struct epoll_event events[20], ev;
    int epfd   = epoll_create(256);
    ev.data.fd = s.fd();
    ev.events  = EPOLLIN|EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, s.fd(), &ev);

    while(!quit)
    {
        auto nfd  = epoll_wait(epfd, events, 20, 1000);//等待事件发生
        for (int i = 0; i < nfd; i++)
        {
            if (events[i].data.fd == s.fd())//连接事件
            {
                int con_fd = s.acceptAndSendKey((struct sockaddr *)&client_addr, rsakey.first);
                char *str  = inet_ntoa(client_addr.sin_addr);
                LOG->info("send pub_key once by accepted successfully");

                ev.data.fd = con_fd;
                ev.events  = EPOLLIN;//设置用于注册的读操作事件
                epoll_ctl(epfd, EPOLL_CTL_ADD, con_fd, &ev);//新文件符添加监听队列
            }
            else if (events[i].events & EPOLLIN)//可读事件
            {
                auto sock_fd = events[i].data.fd;
                auto n_buf   = sockets::read(sock_fd, &buf, sizeof(buf));   //read也阻塞，直到读取客服端传来信息 
                if (!obj_Aes.key_state())
                {
                    auto aesKey = rsa::RsaPriDecrypt(std::string(buf,n_buf), rsakey.second);//私钥解密AES密钥，并保存
                    LOG->info(aesKey.c_str());      //记入server.log日志
                    obj_Aes.changeKey(aesKey);      //存入客户端的AES密钥
                    obj_Aes.set_key_state();
                    ::memset(buf, 0, sizeof(buf));    //清空数据接收区
                    h264_aac_enc_to_ps("server.h264", "server.aac", "server.ps", obj_Aes); //加密封成PS
                    send_ps = fopen("./server.ps", "r");
                    if (send_ps == NULL)
                    {
                        std::cerr << "open failed!" << std::endl;
                        exit(1);
                    }
                    ev.data.fd = sock_fd;
                    ev.events  = EPOLLOUT;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, sock_fd, &ev);//修改监听事件
                    continue;
                }
                ev.data.fd = sock_fd;
                ev.events  = EPOLLOUT;
                epoll_ctl(epfd, EPOLL_CTL_MOD, sock_fd, &ev);//修改监听事件
            }
            else if (events[i].events & EPOLLOUT)//可写事件
            {
                auto sock_fd = events[i].data.fd;
                while(!feof(send_ps))
                {
                    auto n_ret = fread(&buf, 1, sizeof(buf), send_ps);
                    if (n_ret % 1024 > 0)
                    {
                        auto last_block = n_ret % 1024;
                        ::send(sock_fd, buf, last_block, 0);
                        std::cout << "发送最后一块:" << last_block << std::endl;
                        fclose(send_ps);
                        break;
                    }
                    ::send(sock_fd, buf, n_ret, 0);
                    ::memset(&buf, 0, sizeof(buf));
                    
                }
                ev.data.fd = sock_fd;
                ev.events  = EPOLLIN;
                epoll_ctl(epfd, EPOLL_CTL_MOD, sock_fd, &ev);
            }
        }
    }
    LOG->destory();
    return 0;
}

