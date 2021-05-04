#include <thread>
#include <cstdlib>
#include <cstdio>
#include <signal.h>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include "circleBuffer.hpp"

#ifdef __cplusplus
extern "C"{
#endif // DEBUG

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <SDL/SDL.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/samplefmt.h>


#ifdef __cplusplus
} // endof extern "C"
#endif

std::atomic<bool> run_flag(false);
std::mutex videoMutex;
std::condition_variable videoCondVar;
std::queue<AVFrame*> videoQueue, audioQueue;

static void encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt, FILE* outfile)
{
	int ret;
	// if (frame)
	// 	printf("Send frame %3lld\n", frame->linesize);
	ret = avcodec_send_frame(enc_ctx, frame);
	if (ret < 0)
	{
		fprintf(stderr, "Error sending a frame for encoding\n");
		exit(1);
	}
	while (ret >= 0)
	{
		ret = avcodec_receive_packet(enc_ctx, pkt);
		if (ret == AVERROR(EAGAIN))
        {
            return;
        }
        else if (ret == AVERROR_EOF)
        {
            return;
        }
		else if (ret < 0)
		{
			fprintf(stderr, "Error during encoding\n");
			exit(1);
		}
		//printf("Write packet %3lld(size=%5d)\n", pkt->pts, pkt->size);
		//fflush(stdout);
		fwrite(pkt->data, 1, pkt->size, outfile);
		
        // memcpy(av_iobuf, pkt->data, pkt->size);
		av_packet_unref(pkt);
	}
}
/*@br: 注册退出函数
*/
void exit_sighandler(int sig)
{
    run_flag = 1;
}

void capturePCM(const char* device, int frameNum)
{   
    AVDictionary* option = NULL;
    av_log_set_level(AV_LOG_INFO);
    av_register_all();
    avformat_network_init();
    avcodec_register_all();
    avdevice_register_all();
    
    AVFormatContext* fmt = avformat_alloc_context();
    AVCodecContext* codec_fmt;
    AVCodec* codec;
    AVInputFormat* input = av_find_input_format("alsa");
    if (avformat_open_input(&fmt, device, input, &option) != 0)
    {
        perror("Could not open input stream.\n");
        EXIT_FAILURE;
    }

    if (avformat_find_stream_info(fmt, NULL) < 0)
    {
        perror("Could not find stream info.\n");
        EXIT_FAILURE;
    }

    int audio_index = -1;
    for (int i = 0; i< fmt->nb_streams; i++)
    {
        if (fmt->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_index = i;
            break;
        }
    }
    if (audio_index == -1)
    {
        printf("could not find a video stream.\n");
        EXIT_FAILURE;
    }
    codec_fmt = fmt->streams[audio_index]->codec;
    codec     = avcodec_find_decoder(codec_fmt->codec_id);
    if (codec == NULL)
    {
        printf("Could not find codec.\n");
        EXIT_FAILURE;
    }
    if (avcodec_open2(codec_fmt, codec, NULL)< 0)
    {
        printf("Could not open codec.\n");
        EXIT_FAILURE;
    }

    int ret, got_audio;  
    AVPacket *packet     = (AVPacket *)av_malloc(sizeof(AVPacket));    
    AVFrame* pAudioFrame = av_frame_alloc();  
    if(NULL == pAudioFrame)  
    {  
        printf("could not alloc pAudioFrame\n");  
        EXIT_FAILURE;
    }  
    
    //audio output paramter //resample   
    uint64_t out_channel_layout = AV_CH_LAYOUT_MONO;  
    int out_sample_fmt  = AV_SAMPLE_FMT_S16;  
    int out_nb_samples  = 1024; //pCodecCtx->frame_size;  
    int out_sample_rate = 44100;  
    int out_nb_channels = av_get_channel_layout_nb_channels(out_channel_layout);  
    int out_buffer_size = av_samples_get_buffer_size(NULL, out_nb_channels, out_nb_samples, 
                            (AVSampleFormat)out_sample_fmt, 1);    
    
    AVFrame* framePCM = av_frame_alloc();
    uint8_t* buffer = (uint8_t *)av_malloc(out_nb_channels * out_nb_samples * 
                    av_get_bytes_per_sample(AV_SAMPLE_FMT_S16));   
    int64_t in_channel_layout = av_get_default_channel_layout(codec_fmt->channels);    
    av_samples_fill_arrays(framePCM->data, framePCM->linesize, buffer, codec_fmt->channels, 
                        1024, AV_SAMPLE_FMT_S16, 0);

    codec_fmt->channels    = out_nb_channels;
    printf("audio sample_fmt=%d size=%d channel=%d in_channel_layout=%d sample_rate=%d\n",
        codec_fmt->sample_fmt, codec_fmt->frame_size,  
        codec_fmt->channels,   in_channel_layout, codec_fmt->sample_rate);  
    
    struct SwrContext *audio_convert_ctx = swr_alloc();
    if (audio_convert_ctx == NULL)    
    {    
        printf("Could not allocate SwrContext\n");    
        EXIT_FAILURE;
    }    
    
    /* set options */  
    av_opt_set_int       (audio_convert_ctx, "in_channel_count",   codec_fmt->channels,       0);  
    av_opt_set_int       (audio_convert_ctx, "in_sample_rate",     codec_fmt->sample_rate,    0);  
    av_opt_set_sample_fmt(audio_convert_ctx, "in_sample_fmt",      codec_fmt->sample_fmt,     0);  
    av_opt_set_int       (audio_convert_ctx, "out_channel_count",  out_nb_channels,           0);  
    av_opt_set_int       (audio_convert_ctx, "out_sample_rate",    out_sample_rate,           0);  
    av_opt_set_sample_fmt(audio_convert_ctx, "out_sample_fmt",(AVSampleFormat)out_sample_fmt, 0);  
    /* initialize the resampling context */  
    if ((ret = swr_init(audio_convert_ctx)) < 0) 
    {  
        fprintf(stderr, "Failed to initialize the resampling context\n");  
        EXIT_FAILURE;
    }   
    while(1)
    {  
        if(av_read_frame(fmt, packet)>=0)
        {  
            if(packet->stream_index==audio_index)
            {  
                // ret = avcodec_send_packet(codec_fmt, packet);
                ret = avcodec_decode_audio4(codec_fmt, pAudioFrame, &got_audio, packet);  
                if(ret < 0)
                {  
                    printf("Decode Error.\n");  
                    EXIT_FAILURE;
                }  
                if (got_audio)
                {  
                    swr_convert(audio_convert_ctx, (uint8_t **)framePCM->data, 
                        out_nb_channels * out_nb_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16), 
                                (const uint8_t **)pAudioFrame->data, pAudioFrame->nb_samples);    
                    {
                        run_flag.store(true);
                        audioQueue.push(framePCM);
                        frameNum--;
                    }
                    if(frameNum == 0)
                    {
                        break;
                    }   
                }  
                av_free_packet(packet);          
            } 
        }
    }  
    if (codec_fmt != NULL)
    {
        avcodec_close(codec_fmt);
    }
    avformat_close_input(&fmt);
}

void captureYUV(const char* device, int frameNum)
{   
    AVDictionary* option = NULL;
    av_log_set_level(AV_LOG_INFO);
    av_register_all();
    avformat_network_init();
    avcodec_register_all();
    avdevice_register_all();
    
    AVFormatContext* fmt = avformat_alloc_context();
    AVCodecContext* codec_fmt;
    AVCodec* codec;
    AVInputFormat* input = av_find_input_format("video4linux2");
    if (avformat_open_input(&fmt, device, input, &option) != 0)
    {
        perror("Could not open input stream.\n");
        exit(1);
    }

    if (avformat_find_stream_info(fmt, NULL) < 0)
    {
        perror("Could not find stream info.\n");
        exit(1);
    }

    int video_index = -1;
    for (int i = 0; i< fmt->nb_streams; i++)
    {
        if (fmt->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_index = i;
            break;
        }
    }
    if (video_index == -1)
    {
        printf("could not find a video stream.\n");
        exit(1);
    }
    codec_fmt = fmt->streams[video_index]->codec;
    codec     = avcodec_find_decoder(codec_fmt->codec_id);
    if (codec == NULL)
    {
        printf("Could not find codec.\n");
        exit(1);
    }
    if (avcodec_open2(codec_fmt, codec, NULL)< 0)
    {
        printf("Could not open codec.\n");
        exit(1);
    }

    AVFrame* frame    = av_frame_alloc();
    AVFrame* frameYUV = av_frame_alloc();
    AVPacket* avp     = (AVPacket*)av_malloc(sizeof(AVPacket));
    int8_t*   buf     = (int8_t*)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, 
                                                              codec_fmt->width, 
                                                              codec_fmt->height));
    avpicture_fill((AVPicture*)frameYUV, 
                   (const uint8_t*)buf, AV_PIX_FMT_YUV420P, codec_fmt->width, codec_fmt->height);
    
    struct SwsContext* sws = sws_getContext(codec_fmt->width, codec_fmt->height, codec_fmt->pix_fmt,
                                            codec_fmt->width, codec_fmt->height, AV_PIX_FMT_YUV420P, 
                                            SWS_BICUBIC, NULL, NULL, NULL);
    int got_pic = 0, frame_count = 0;
    int y_size = codec_fmt->width * codec_fmt->height;
    while(frameNum--)
    {
        if(av_read_frame(fmt, avp) >= 0)
        {
            if(avp->stream_index == video_index)
            {
                int ret = avcodec_decode_video2(codec_fmt, frame, &got_pic, avp);
                if (ret < 0)
                {
                    av_log(NULL,AV_LOG_ERROR,"decode is error");
                }
                if (got_pic)
                {
                    sws_scale(sws, frame->data, frame->linesize, 0,
                            codec_fmt->height, frameYUV->data, frameYUV->linesize);
                    {
                        std::lock_guard<std::mutex> lg(videoMutex);
                        videoQueue.push(frameYUV);
                    }
                    videoCondVar.notify_one();
                    frame_count++;
                    av_log(NULL,AV_LOG_INFO,"解析完第%d帧\n",frame_count);
                }
            }
            av_free_packet(avp);
        }
    }

    if(codec_fmt != NULL)
    {
        avcodec_close(codec_fmt);
    }
    avformat_close_input(&fmt);
}

void YUVtoH264(const char* outFile, int in_w, int in_h, int framenum)
{   
	// 打开输出视频文件
	FILE *out_file = NULL;
	out_file = fopen(outFile, "wb");

    //初始化aviocontext，使得数据存放在内存中
    unsigned char* cb_ioBuffer = (unsigned char*)av_malloc(1024*64);
    AVIOContext*   av_ioctx    = avio_alloc_context(cb_ioBuffer, 1024*64, 0, NULL, NULL, NULL, NULL);

	// 初始化AVFormatContext结构体,根据文件名获取到合适的封装格式
	AVFormatContext* pFormatCtx = avformat_alloc_context();
	avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, outFile);
    pFormatCtx->pb      = av_ioctx;
	AVOutputFormat *fmt = pFormatCtx->oformat;

	// 初始化视频码流
	AVStream *video_st = avformat_new_stream(pFormatCtx, 0);
	if (video_st == NULL)
	{
		printf("failed allocating output stream\n");
		exit(1);
	}
	video_st->time_base.num = 1;
	video_st->time_base.den = 25;

	// 编码器Context设置参数
	AVCodecContext *pCodecCtx = avcodec_alloc_context3(NULL);
	avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[0]->codecpar);
	pCodecCtx->codec_id         = fmt->video_codec;
	pCodecCtx->codec_type       = AVMEDIA_TYPE_VIDEO;
	pCodecCtx->pix_fmt          = AV_PIX_FMT_YUV420P;
	pCodecCtx->width            = in_w;
	pCodecCtx->height           = in_h;
	pCodecCtx->time_base.num    = 1;
	pCodecCtx->time_base.den    = 25;
	pCodecCtx->bit_rate         = 400000;
	pCodecCtx->gop_size         = 15;
    pCodecCtx->profile          = FF_PROFILE_H264_BASELINE;//只包含I、P帧
	if (pCodecCtx->codec_id == AV_CODEC_ID_H264)
	{
		pCodecCtx->qmin = 10;
		pCodecCtx->qmax = 51;
		pCodecCtx->qcompress = 0.6;
	}
	if (pCodecCtx->codec_id == AV_CODEC_ID_MPEG2VIDEO)
		pCodecCtx->max_b_frames = 2;
	if (pCodecCtx->codec_id == AV_CODEC_ID_MPEG1VIDEO)
		pCodecCtx->mb_decision = 2;

	// 寻找编码器 
	AVCodec *pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!pCodec)
	{
		printf("no right encoder!");
		exit(1);
	}

	// 打开编码器
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	{
		printf("open encoder fail!");
		exit(1);
	}

	// 输出格式信息
	av_dump_format(pFormatCtx, 0, outFile, 1);

	// 初始化帧
	AVFrame *pictureFrame = av_frame_alloc();
	pictureFrame->width   = pCodecCtx->width;
	pictureFrame->height  = pCodecCtx->height;
	pictureFrame->format  = pCodecCtx->pix_fmt;

	// ffmpeg4.0
	int size = av_image_get_buffer_size(pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, 1);
	uint8_t* cb_Buffer = (uint8_t *)av_malloc(size);
	av_image_fill_arrays(pictureFrame->data, pictureFrame->linesize, cb_Buffer, 
                AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);

	// 写头文件
	avformat_write_header(pFormatCtx, NULL);

	// 创建已编码帧
	AVPacket* av_pkt = av_packet_alloc();
	if (!av_pkt)
	{
		exit(1);
	}

	// 编码器Context大小
	int y_size = pCodecCtx->width*pCodecCtx->height;

	//循环每一帧
	for (int i = 0; i < framenum; i++)
	{
		// 读入YUV
		{
            std::unique_lock<std::mutex> ul(videoMutex);
            videoCondVar.wait(ul, [] { return !videoQueue.empty(); });
            pictureFrame = videoQueue.front();
            videoQueue.pop();
        }
		// encode the image
		encode(pCodecCtx, pictureFrame, av_pkt, out_file);
	}
	// flush the encoder 
	encode(pCodecCtx, NULL, av_pkt, out_file);
	av_write_trailer(pFormatCtx);
	// 释放内存
	av_frame_free(&pictureFrame);
	av_packet_free(&av_pkt);
	avcodec_free_context(&pCodecCtx);
	av_free(cb_Buffer);
	avformat_free_context(pFormatCtx);
	fclose(out_file);
}

void PCMtoAAC(const char* outFile, int framenum)
{
    const AVCodec* codec;
    AVCodecContext*  codec_ctx  = NULL;
    AVFrame* frame;
    AVPacket* packet;

    FILE* of = fopen(outFile, "wb");
    if (!of) 
    {
        fprintf(stderr, "Could not open %s\n", outFile);
        exit(1);
    }
    av_register_all();
    avcodec_register_all();

	// 初始化AVFormatContext结构体,根据文件名获取到合适的封装格式
	AVFormatContext* pFormatCtx = avformat_alloc_context();
	avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, outFile);
	AVOutputFormat *fmt = pFormatCtx->oformat;

    codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec)
    {
        fprintf(stderr, "avcodec_find_encoder open failed!\n"); //找不到编码器
        exit(1);
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
    {
        fprintf(stderr, "audio encoder context allocated failed!\n"); //找不到编码器上下文
        exit(1);
    }
    codec_ctx->codec_id       = AV_CODEC_ID_AAC;
    codec_ctx->codec_type     = AVMEDIA_TYPE_AUDIO;
    codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
    codec_ctx->sample_rate    = 44100; 
    codec_ctx->channels       = av_get_channel_layout_nb_channels(codec_ctx->channel_layout);
    codec_ctx->profile        = FF_PROFILE_AAC_LOW;  
    codec_ctx->sample_fmt     = AV_SAMPLE_FMT_S16;//AV_SAMPLE_FMT_S16;
    codec_ctx->bit_rate       = 64000;
    if (avcodec_open2(codec_ctx, codec, NULL) < 0)
    {
        fprintf(stderr, "can not open codec!");//打不开编码器
        exit(1);
    }
    printf("2 frame_size:%d\n\n", codec_ctx->frame_size); 

    packet = av_packet_alloc();
    if (!packet)
    {
        fprintf(stderr, "can not allocate packet!");//分配包失败
        exit(1);
    }
    frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "can not allocate frame!");//分配帧失败
        exit(1);
    }
    /* 每次送多少数据给编码器由：
     *  (1)frame_size(每帧单个通道的采样点数);
     *  (2)sample_fmt(采样点格式);
     *  (3)channel_layout(通道布局情况);
     * 3要素决定
     */
    frame->nb_samples     = codec_ctx->frame_size;
    frame->format         = codec_ctx->sample_fmt;
    frame->channel_layout = codec_ctx->channel_layout;
    frame->channels       = av_get_channel_layout_nb_channels(frame->channel_layout);
    printf("frame nb_samples:%d\n", frame->nb_samples);
    printf("frame sample_fmt:%d\n", frame->format);
    printf("frame channel_layout:%lu\n\n", frame->channel_layout);

    int ret = av_frame_get_buffer(frame, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "can not allocate buffer!");
        exit(1);
    }

     // 计算出每一帧的数据 单个采样点的字节 * 通道数目 * 每帧采样点数量
    int frame_bytes = av_get_bytes_per_sample((AVSampleFormat)frame->format)
                     * frame->channels * frame->nb_samples;
    uint8_t* pcm_buf = (uint8_t *)av_malloc(frame_bytes);
    if(!pcm_buf) 
    {
        printf("pcm_buf malloc failed\n");
        exit(1);
    }
    printf("frame_bytes %d\n", frame_bytes);
    av_samples_fill_arrays(frame->data, frame->linesize, pcm_buf, frame->channels,
                                      frame->nb_samples, (AVSampleFormat)frame->format, 0);

    int64_t pts = 0;
    printf("开始AAC编码\n");
    while (framenum > 0)
    {
        {
            while (!run_flag.load())
            {}
            frame->data[0] = audioQueue.front()->data[0];
            audioQueue.pop();
        }
        encode(codec_ctx, frame, packet, of);
        framenum--;
    }
    encode(codec_ctx, NULL, packet, of);

    fclose(of);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
}

