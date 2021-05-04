#include <unistd.h>
#include <signal.h>

#include "Aes.hpp"
#include "Socket.hpp"
#include "Rsa.hpp"
#include "mpeg2_ps.hpp"
#include "h264Handler.hpp"
#include "aacHandler.hpp"

bool quit = false;
static uint8_t s_packet[2 * 1024 * 1024];
FILE* rev_ps = NULL;
Aes obj_Aes(16);

inline const char* ftimestamp(int64_t t, char* buf)
{
    if (PTS_NO_VALUE == t)
    {
        sprintf(buf, "(null)");
    }
    else
    {
        t /= 90;
        sprintf(buf, "%d:%02d:%02d.%03d", (int)(t / 3600000), (int)((t / 60000) % 60), (int)((t / 1000) % 60), (int)(t % 1000));
    }
    return buf;
}

static void onpacket(void* /*param*/, int /*stream*/, int avtype, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    static char s_pts[64], s_dts[64];

    if (PSI_STREAM_AAC == avtype)
    {
        static int64_t a_pts = 0, a_dts = 0;
        if (PTS_NO_VALUE == dts)
            dts = pts;
        //assert(0 == a_dts || dts >= a_dts);
        printf("[A] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d\n", ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - a_pts) / 90, (int)(dts - a_dts) / 90);
        a_pts = pts;
        a_dts = dts;

        fwrite(data, 1, bytes, afp);
    }
    else if (PSI_STREAM_H264 == avtype)
    {
        static int64_t v_pts = 0, v_dts = 0;
        assert(0 == v_dts || dts >= v_dts);
        printf("[V] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d\n", ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - v_pts) / 90, (int)(dts - v_dts) / 90);
        v_pts = pts;
        v_dts = dts;

        fwrite(data, 1, bytes, vfp);
    }
    else
    {
        //assert(0);
    }
}

void mpeg_ps_dec(const char* file)
{
    FILE* fp = fopen(file, "rb");
    vfp = fopen("./client_enc.h264", "wb");
    afp = fopen("./client_enc.aac", "wb");

    if (fp == NULL)
    {
        printf("Error opening");
    }

    size_t n, i= 0;
    ps_demuxer_t* ps = ps_demuxer_create(onpacket, NULL);
    while ((n = fread(s_packet + i, 1, sizeof(s_packet) - i, fp)) > 0)
    {
        size_t r = ps_demuxer_input_gb28181(ps, s_packet, n + i);
        memmove(s_packet, s_packet + r, n + i - r);
        i = n + i - r;
    }
    ps_demuxer_destroy(ps);

    fclose(afp);
    fclose(fp);
    fclose(vfp);
}

void SignalFunc(int sig_num)
{
    switch (sig_num)
    {
        case SIGINT:
        fclose(rev_ps);
        mpeg_ps_dec("./rev.ps");
        h264_decode("./client_enc.h264", "./client_dec.h264", obj_Aes);
        aac_decode("./client_enc.aac", "./client_dec.aac", obj_Aes);
        quit = true;
        exit(1);
        break;
    }
}

int main(int argc, char *argv[])
{
	Aes obj_Aes(16);							//随机生成16位AES密钥
	unsigned short port = 6799;        				// 服务器的端口号
	std::string ip      = "192.168.136.128"; 			// 服务器ip地址
	char pub_key[1024];
	char buf[1024] = {0};
	int i = 0;

	if( argc > 1 )		//函数传参，可以更改服务器的ip地址									
	{		
		ip = argv[1];
	}	
	else if( argc > 2 )	   //函数传参，可以更改服务器的端口号									
	{
		port = atoi(argv[2]);
	}

	signal(SIGINT,SignalFunc);

	Socket s;					//创建套接字
	auto sockfd =s.fd();
	s.connect(ip, port);

	rev_ps = fopen("./rev.ps", "w");
	if (rev_ps == NULL)
	{
		std::cerr << "open failed!" << std::endl;
		EXIT_FAILURE;
	}
	
	std::cout << "send data to " << ip << ":" << port << std::endl;
	while(!quit)
	{
		if (!obj_Aes.key_state())
		{
			sockets::read(sockfd, pub_key, sizeof(pub_key));					//阻塞接收服务器下发的RSA公钥
			std::cout << pub_key << std::endl;
			auto encrypt_text = rsa::RsaPubEncrypt(obj_Aes.key(), pub_key);		//RSA公钥加密AES密钥
			sockets::write(sockfd, encrypt_text.c_str(), encrypt_text.length());//发送AES密钥匙
			std::cout << "write done" << std::endl; 
			obj_Aes.set_key_state();
		}

        auto n_buf = sockets::read(sockfd, &buf, sizeof(buf));
        if(n_buf > 0)
        {
            fwrite(&buf, 1, n_buf, rev_ps);
        }
        memset(buf, 0, sizeof(buf));    //清空数据接收区
	}
	return 0;
}
