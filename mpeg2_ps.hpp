#ifndef MPEG2_PS_H
#define MPEG2_PS_H


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include "mpeg-ps.h"
#include "mpeg-ps-proto.h"
#include "mpeg-ts-proto.h"
#include "mpeg-pes-proto.h"
#include "mpeg-element-descriptor.h"
#include "mpeg-types.h"

#define PS_HDR_LEN  14
#define SYS_HDR_LEN 18
#define PSM_HDR_LEN 24
#define PES_HDR_LEN 19
#define RTP_HDR_LEN 12
#define RTP_HDR_SIZE 12
#define RTP_VERSION 2
#define PS_PES_PAYLOAD_SIZE 1300

FILE* vfp = NULL;
FILE* afp = NULL;

typedef struct                              //0x000001BA
{  
    int i_size;
    int i_data;
    unsigned char i_mask;
    unsigned char *p_data;
} bits_buffer_s;  

#define bits_write(buffer, count, bits)\
{\
    bits_buffer_s *p_buffer = (buffer);\
    int i_count = (count);\
    unsigned long long i_bits = (bits);\
    while( i_count > 0 )\
    {\
        i_count--;\
        if( ( i_bits >> i_count )&0x01 )\
        {\
            p_buffer->p_data[p_buffer->i_data] |= p_buffer->i_mask;\
        }\
        else\
        {\
            p_buffer->p_data[p_buffer->i_data] &= ~p_buffer->i_mask;\
        }\
        p_buffer->i_mask >>= 1;\
        if( p_buffer->i_mask == 0 )\
        {\
            p_buffer->i_data++;\
            p_buffer->i_mask = 0x80;\
        }\
    }\
}

struct ps_demuxer_t
{
    struct psm_t psm;
    struct psd_t psd;

    struct ps_pack_header_t pkhd;
    struct ps_system_header_t system;

    ps_demuxer_onpacket onpacket;
	void* param;	
};

static struct pes_t* psm_fetch(struct psm_t* psm, uint8_t sid)
{
    size_t i;
    for (i = 0; i < psm->stream_count; ++i)
    {
        if (psm->streams[i].sid == sid)
        {
            return &psm->streams[i];
        }
    }

    if (psm->stream_count < sizeof(psm->streams) / sizeof(psm->streams[0]))
    {
		// '110x xxxx'
		// ISO/IEC 13818-3 or ISO/IEC 11172-3 or ISO/IEC 13818-7 or 
		// ISO/IEC 14496-3 or ISO/IEC 23008-3 audio stream number 'x xxxx'

		// '1110 xxxx'
		// Rec. ITU-T H.262 | ISO/IEC 13818-2, ISO/IEC 11172-2, ISO/IEC 14496-2, 
		// Rec. ITU-T H.264 | ISO/IEC 14496-10 or 
		// Rec. ITU-T H.265 | ISO/IEC 23008-2 video stream number 'xxxx'

        // guess stream codec id
        if (0xE0 <= sid && sid <= 0xEF)
        {
            psm->streams[psm->stream_count].codecid = PSI_STREAM_H264;
        }
           
        else if(0xC0 <= sid && sid <= 0xDF)
        {
            psm->streams[psm->stream_count].codecid = PSI_STREAM_AAC;
        }
            
        return &psm->streams[psm->stream_count++];
    }

    return NULL;
}

static int gb28181_make_ps_header(char *pData, unsigned long long s64Scr)
{
    unsigned long long lScrExt = (s64Scr) % 100;    
    s64Scr = s64Scr / 100;
    bits_buffer_s      bitsBuffer;
    bitsBuffer.i_size = PS_HDR_LEN;    
    bitsBuffer.i_data = 0;
    bitsBuffer.i_mask = 0x80;
    bitsBuffer.p_data = (unsigned char *)(pData);
    memset(bitsBuffer.p_data, 0, PS_HDR_LEN);
    bits_write(&bitsBuffer, 32, 0x000001BA);                  /*start codes*/
    bits_write(&bitsBuffer, 2,     1);                        /*marker bits '01b'*/
    bits_write(&bitsBuffer, 3,     (s64Scr>>30)&0x07);        /*System clock [32..30]*/
    bits_write(&bitsBuffer, 1,     1);                        /*marker bit*/
    bits_write(&bitsBuffer, 15, (s64Scr>>15)&0x7FFF);         /*System clock [29..15]*/
    bits_write(&bitsBuffer, 1,     1);                        /*marker bit*/
    bits_write(&bitsBuffer, 15, s64Scr & 0x7fff);             /*System clock [29..15]*/
    bits_write(&bitsBuffer, 1,     1);                        /*marker bit*/
    bits_write(&bitsBuffer, 9,     lScrExt&0x01ff);           /*System clock [14..0]*/
    bits_write(&bitsBuffer, 1,     1);                        /*marker bit*/
    bits_write(&bitsBuffer, 22, (255)&0x3fffff);              /*bit rate(n units of 50 bytes per second.)*/
    bits_write(&bitsBuffer, 2,     3);                        /*marker bits '11'*/
    bits_write(&bitsBuffer, 5,     0x1f);                     /*reserved(reserved for future use)*/
    bits_write(&bitsBuffer, 3,     0);                        /*stuffing length*/
    return 0;
}

static int gb28181_make_sys_header(char *pData)
{    
    bits_buffer_s      bitsBuffer;
    bitsBuffer.i_size = SYS_HDR_LEN;
    bitsBuffer.i_data = 0;
    bitsBuffer.i_mask = 0x80;
    bitsBuffer.p_data =    (unsigned char *)(pData);
    memset(bitsBuffer.p_data, 0, SYS_HDR_LEN);
    /*system header*/
    bits_write( &bitsBuffer, 32, 0x000001BB);    /*start code*/
    bits_write( &bitsBuffer, 16, SYS_HDR_LEN-6); /*header_length 表示次字节后面的长度*/
    bits_write( &bitsBuffer, 1,  1);             /*marker_bit*/
    bits_write( &bitsBuffer, 22, 50000);         /*rate_bound*/
    bits_write( &bitsBuffer, 1,  1);             /*marker_bit*/
    bits_write( &bitsBuffer, 6,  1);             /*audio_bound*/
    bits_write( &bitsBuffer, 1,  0);             /*fixed_flag */
    bits_write( &bitsBuffer, 1,  1);             /*CSPS_flag */
    bits_write( &bitsBuffer, 1,  1);             /*system_audio_lock_flag*/
    bits_write( &bitsBuffer, 1,  1);             /*system_video_lock_flag*/
    bits_write( &bitsBuffer, 1,  1);             /*marker_bit*/
    bits_write( &bitsBuffer, 5,  1);             /*video_bound*/
    bits_write( &bitsBuffer, 1,  0);             /*dif from mpeg1*/
    bits_write( &bitsBuffer, 7,  0x7F);          /*reserver*/
    /*audio stream bound*/
    bits_write( &bitsBuffer, 8,  0xC0);          /*stream_id*/
    bits_write( &bitsBuffer, 2,  3);             /*marker_bit */
    bits_write( &bitsBuffer, 1,  0);             /*PSTD_buffer_bound_scale*/
    bits_write( &bitsBuffer, 13, 512);           /*PSTD_buffer_size_bound*/
    /*video stream bound*/
    bits_write( &bitsBuffer, 8,  0xE0);          /*stream_id*/
    bits_write( &bitsBuffer, 2,  3);             /*marker_bit */
    bits_write( &bitsBuffer, 1,  1);             /*PSTD_buffer_bound_scale*/
    bits_write( &bitsBuffer, 13, 2048);          /*PSTD_buffer_size_bound*/
    return 0;
}

static int gb28181_make_psm_header(char *pData)
{
    
    bits_buffer_s      bitsBuffer;
    bitsBuffer.i_size = PSM_HDR_LEN; 
    bitsBuffer.i_data = 0;
    bitsBuffer.i_mask = 0x80;
    bitsBuffer.p_data =    (unsigned char *)(pData);
    memset(bitsBuffer.p_data, 0, PSM_HDR_LEN);//24Bytes
    bits_write(&bitsBuffer, 24,0x000001);     /*start code*/
    bits_write(&bitsBuffer, 8, 0xBC);         /*map stream id*/
    bits_write(&bitsBuffer, 16, 18);           /*program stream map length*/ 
    bits_write(&bitsBuffer, 1, 1);            /*current next indicator */
    bits_write(&bitsBuffer, 2, 3);            /*reserved*/
    bits_write(&bitsBuffer, 5, 0);            /*program stream map version*/
    bits_write(&bitsBuffer, 7, 0x7F);         /*reserved */
    bits_write(&bitsBuffer, 1, 1);            /*marker bit */
    bits_write(&bitsBuffer, 16, 0);           /*programe stream info length*/
    bits_write(&bitsBuffer, 16, 8);           /*elementary stream map length    is*/
    /*audio*/
    bits_write(&bitsBuffer, 8, 0x0F);         /*stream_type*/
    bits_write(&bitsBuffer, 8, 0xC0);         /*elementary_stream_id*/
    bits_write(&bitsBuffer, 16, 0);           /*elementary_stream_info_length is*/
    /*video*/
    bits_write(&bitsBuffer, 8, 0x1B);         /*stream_type*/
    bits_write(&bitsBuffer, 8, 0xE0);         /*elementary_stream_id*/
    bits_write(&bitsBuffer, 16, 0);           /*elementary_stream_info_length */
    /*crc (2e b9 0f 3d)*/
    bits_write(&bitsBuffer, 8, 0x45);         /*crc (24~31) bits*/
    bits_write(&bitsBuffer, 8, 0xBD);         /*crc (16~23) bits*/
    bits_write(&bitsBuffer, 8, 0xDC);         /*crc (8~15) bits*/
    bits_write(&bitsBuffer, 8, 0xF4);         /*crc (0~7) bits*/
    return 0;
}

static int gb28181_make_pes_header(char *pData, int stream_id, int payload_len, 
                    unsigned long long pts, unsigned long long dts)
{
    
    bits_buffer_s      bitsBuffer;
    bitsBuffer.i_size = PES_HDR_LEN;
    bitsBuffer.i_data = 0;
    bitsBuffer.i_mask = 0x80;
    bitsBuffer.p_data =    (unsigned char *)(pData);
    memset(bitsBuffer.p_data, 0, PES_HDR_LEN);
    /*system header*/
    bits_write( &bitsBuffer, 24,0x000001);            /*start code*/
    bits_write( &bitsBuffer, 8, (stream_id));         /*streamID*/
    bits_write( &bitsBuffer, 16,(payload_len)+19);    /*packet_len*/ //指出pes分组中数据长度和该字节后的长度和
    bits_write( &bitsBuffer, 2, 2 );                  /*'10'*/
    bits_write( &bitsBuffer, 2, 0 );                  /*scrambling_control*/
    bits_write( &bitsBuffer, 1, 0 );                  /*priority*/
    bits_write( &bitsBuffer, 1, 0 );                  /*data_alignment_indicator*/
    bits_write( &bitsBuffer, 1, 0 );                  /*copyright*/
    bits_write( &bitsBuffer, 1, 0 );                  /*original_or_copy*/
    bits_write( &bitsBuffer, 1, 1 );                  /*PTS_flag*/
    bits_write( &bitsBuffer, 1, 1 );                  /*DTS_flag*/
    bits_write( &bitsBuffer, 1, 0 );                  /*ESCR_flag*/
    bits_write( &bitsBuffer, 1, 0 );                  /*ES_rate_flag*/
    bits_write( &bitsBuffer, 1, 0 );                  /*DSM_trick_mode_flag*/
    bits_write( &bitsBuffer, 1, 0 );                  /*additional_copy_info_flag*/
    bits_write( &bitsBuffer, 1, 0 );                  /*PES_CRC_flag*/
    bits_write( &bitsBuffer, 1, 0 );                  /*PES_extension_flag*/
    bits_write( &bitsBuffer, 8, 10);                  /*header_data_length*/ 
    
    /*PTS,DTS*/    
    bits_write( &bitsBuffer, 4, 3 );                    /*'0011'*/
    bits_write( &bitsBuffer, 3, ((pts)>>30)&0x07 );     /*PTS[32..30]*/
    bits_write( &bitsBuffer, 1, 1 );
    bits_write( &bitsBuffer, 15,((pts)>>15)&0x7FFF);    /*PTS[29..15]*/
    bits_write( &bitsBuffer, 1, 1 );
    bits_write( &bitsBuffer, 15,(pts)&0x7FFF);          /*PTS[14..0]*/
    bits_write( &bitsBuffer, 1, 1 );
    bits_write( &bitsBuffer, 4, 1 );                    /*'0001'*/
    bits_write( &bitsBuffer, 3, ((dts)>>30)&0x07 );     /*DTS[32..30]*/
    bits_write( &bitsBuffer, 1, 1 );
    bits_write( &bitsBuffer, 15,((dts)>>15)&0x7FFF);    /*DTS[29..15]*/
    bits_write( &bitsBuffer, 1, 1 );
    bits_write( &bitsBuffer, 15,(dts)&0x7FFF);          /*DTS[14..0]*/
    bits_write( &bitsBuffer, 1, 1 );
    return 0;
}

static int gb28181_make_rtp_header(char *pData, int marker_flag, unsigned short cseq, 
                    long long curpts, unsigned int ssrc)
{
    bits_buffer_s      bitsBuffer;
    if (pData == NULL)
        return -1;
    bitsBuffer.i_size = RTP_HDR_LEN;
    bitsBuffer.i_data = 0;
    bitsBuffer.i_mask = 0x80;
    bitsBuffer.p_data =    (unsigned char *)(pData);
    
    memset(bitsBuffer.p_data, 0, RTP_HDR_SIZE);
    bits_write(&bitsBuffer, 2, RTP_VERSION);    	/* rtp version */
    bits_write(&bitsBuffer, 1, 0);                	/* rtp padding */
    bits_write(&bitsBuffer, 1, 0);                	/* rtp extension */
    bits_write(&bitsBuffer, 4, 0);                	/* rtp CSRC count */
    bits_write(&bitsBuffer, 1, (marker_flag));		/* rtp marker */
    bits_write(&bitsBuffer, 7, 96);            		/* rtp payload type*/
    bits_write(&bitsBuffer, 16, (cseq));            /* rtp sequence */
    bits_write(&bitsBuffer, 32, (curpts));         	/* rtp timestamp */
    bits_write(&bitsBuffer, 32, (ssrc));         	/* rtp SSRC */
    return 0;
}


size_t pack_header_read(struct ps_pack_header_t *h, const uint8_t* data, size_t bytes)
{
    uint8_t stuffing_length;

    if (bytes < 14) return 0;
    assert(0x00 == data[0] && 0x00 == data[1] && 0x01 == data[2] && PES_SID_START == data[3]);
	if (0 == (0xC0 & data[4]))
	{
		// MPEG-1
		h->mpeg2 = 0;
		assert(0x20 == (0xF0 & data[4]));
		h->system_clock_reference_base = ((uint64_t)(data[4] >> 1) << 30) | ((uint64_t)data[5] << 22) | (((uint64_t)data[6] >> 1) << 15) | ((uint64_t)data[7] << 7) | (data[8] >> 1);
		h->system_clock_reference_extension = 1;
		h->program_mux_rate = ((data[9] >> 1) << 15) | (data[10] << 7) | (data[11] >> 1);
		return 12;
	}
	else
	{
		h->mpeg2 = 1;
		assert((0x44 & data[4]) == 0x44); // '01xxx1xx'
		assert((0x04 & data[6]) == 0x04); // 'xxxxx1xx'
		assert((0x04 & data[8]) == 0x04); // 'xxxxx1xx'
		assert((0x01 & data[9]) == 0x01); // 'xxxxxxx1'
		h->system_clock_reference_base = (((uint64_t)(data[4] >> 3) & 0x07) << 30) | (((uint64_t)data[4] & 0x3) << 28) | ((uint64_t)data[5] << 20) | ((((uint64_t)data[6] >> 3) & 0x1F) << 15) | (((uint64_t)data[6] & 0x3) << 13) | ((uint64_t)data[7] << 5) | ((data[8] >> 3) & 0x1F);
		h->system_clock_reference_extension = ((data[8] & 0x3) << 7) | ((data[9] >> 1) & 0x7F);

		assert((0x03 & data[12]) == 0x03); // 'xxxxxx11'
		h->program_mux_rate = (data[10] << 14) | (data[11] << 6) | ((data[12] >> 2) & 0x3F);

		//assert((0xF8 & data[13]) == 0x00); // '00000xxx'
		stuffing_length = data[13] & 0x07; // stuffing

		return 14 + stuffing_length;
	}
}

size_t psm_read(struct psm_t *psm, const uint8_t* data, size_t bytes)
{
	size_t i, j, k;
	uint8_t current_next_indicator;
	uint8_t single_extension_stream_flag;
	uint16_t program_stream_map_length;
	uint16_t program_stream_info_length;
	uint16_t element_stream_map_length;
	uint16_t element_stream_info_length;

	// Table 2-41 �C Program stream map(p79)
	assert(0x00==data[0] && 0x00==data[1] && 0x01==data[2] && 0xBC==data[3]);
	program_stream_map_length = (data[4] << 8) | data[5];

	//assert((0x20 & data[6]) == 0x00); // 'xx0xxxxx'
	current_next_indicator = (data[6] >> 7) & 0x01;
	single_extension_stream_flag = (data[6] >> 6) & 0x01;
	psm->ver = data[6] & 0x1F;
	//assert(data[7] == 0x01); // '00000001'

	// program stream descriptor
	program_stream_info_length = (data[8] << 8) | data[9];
	if ((size_t)program_stream_info_length + 6 > bytes)
	{
		std::cout << "program_stream_info_length + 6 > bytes" << std::endl;
		return bytes; // TODO: error
	}
		

	// TODO: parse descriptor

	// program element stream
	i = 10 + program_stream_info_length;
	element_stream_map_length = (data[i] << 8) | data[i+1];
	 /* Ignore es_map_length, trust psm_length */
	element_stream_map_length = program_stream_map_length - program_stream_info_length - 10;

	j = i + 2;
	psm->stream_count = 0;
	while(j < i+2+element_stream_map_length && psm->stream_count < sizeof(psm->streams)/sizeof(psm->streams[0]))
	{
		psm->streams[psm->stream_count].codecid = data[j];
		psm->streams[psm->stream_count].sid = data[j+1];
		element_stream_info_length = (data[j+2] << 8) | data[j+3];
		if (j + 4 + element_stream_info_length > bytes)
		{
			std::cout << "program_stream_info_length + 6 > bytes" << std::endl;
			return bytes; // TODO: error
		}

		k = j + 4;
		if(0xFD == psm->streams[psm->stream_count].sid && 0 == single_extension_stream_flag)
		{
//			uint8_t pseudo_descriptor_tag = data[k];
//			uint8_t pseudo_descriptor_length = data[k+1];
//			uint8_t element_stream_id_extension = data[k+2] & 0x7F;
			assert((0x80 & data[k+2]) == 0x80); // '1xxxxxxx'
			k += 3;
		}

		while(k + 2 < j + 4 + element_stream_info_length)
		{
			// descriptor()
			k += mpeg_elment_descriptor(data+k, j + 4 + element_stream_info_length - k);
		}

		++psm->stream_count;
		assert(k - j - 4 == element_stream_info_length);
		j += 4 + element_stream_info_length;
	}

//	assert(j+4 == program_stream_map_length+6);
//	assert(0 == mpeg_crc32(0xffffffff, data, program_stream_map_length+6));第一次修改
	return program_stream_map_length+6;
}

size_t mpeg_elment_descriptor(const uint8_t* data, size_t bytes)
{
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];
	if (descriptor_len + 2 > bytes)
		return bytes;

	switch(descriptor_tag)
	{
	case 2:
		video_stream_descriptor(data, bytes);
		break;

	case 3:
		audio_stream_descriptor(data, bytes);
		break;

	case 4:
		hierarchy_descriptor(data, bytes);
		break;

	case 10:
		language_descriptor(data, bytes);
		break;

	case 11:
		system_clock_descriptor(data, bytes);
		break;

	case 27:
		mpeg4_video_descriptor(data, bytes);
		break;

	case 28:
		mpeg4_audio_descriptor(data, bytes);
		break;

	case 40:
		avc_video_descriptor(data, bytes);
		break;

	case 42:
		avc_timing_hrd_descriptor(data, bytes);
		break;

	case 43:
		mpeg2_aac_descriptor(data, bytes);
		break;

	case 48:
		svc_extension_descriptor(data, bytes);
		break;

	case 49:
		mvc_extension_descriptor(data, bytes);
		break;

	//default:
	//	assert(0);
	}

	return descriptor_len+2;
}

size_t pes_read_header_gb28181(struct pes_t *pes, const uint8_t* data, size_t bytes)
{
    size_t i;

    assert(0x00 == data[0] && 0x00 == data[1] && 0x01 == data[2]);
    pes->sid = data[3];
    pes->len = (data[4] << 8) | data[5];

    i = 6;
    assert(0x02 == ((data[i] >> 6) & 0x3));
    pes->PES_scrambling_control = (data[i] >> 4) & 0x3;
    pes->PES_priority = (data[i] >> 3) & 0x1;
    pes->data_alignment_indicator = (data[i] >> 2) & 0x1;
    pes->copyright = (data[i] >> 1) & 0x1;
    pes->original_or_copy = data[i] & 0x1;

    i++;
    pes->PTS_DTS_flags = (data[i] >> 6) & 0x3;
    pes->ESCR_flag = (data[i] >> 5) & 0x1;
    pes->ES_rate_flag = (data[i] >> 4) & 0x1;
    pes->DSM_trick_mode_flag = (data[i] >> 3) & 0x1;
    pes->additional_copy_info_flag = (data[i] >> 2) & 0x1;
    pes->PES_CRC_flag = (data[i] >> 1) & 0x1;
    pes->PES_extension_flag = data[i] & 0x1;

    i++;
    pes->PES_header_data_length = data[i];
    if (bytes < pes->PES_header_data_length + 9)
        return 0; // invalid data length

    i++;
    if (0x02 & pes->PTS_DTS_flags)
    {
        assert(0x20 == (data[i] & 0x20));
        pes->pts = ((((uint64_t)data[i] >> 1) & 0x07) << 30) | ((uint64_t)data[i + 1] << 22) | ((((uint64_t)data[i + 2] >> 1) & 0x7F) << 15) | ((uint64_t)data[i + 3] << 7) | ((data[i + 4] >> 1) & 0x7F);

        i += 5;
    }
    //else
    //{
    //    pes->pts = PTS_NO_VALUE;
    //}

    if (0x01 & pes->PTS_DTS_flags)
    {
        assert(0x10 == (data[i] & 0x10));
        pes->dts = ((((uint64_t)data[i] >> 1) & 0x07) << 30) | ((uint64_t)data[i + 1] << 22) | ((((uint64_t)data[i + 2] >> 1) & 0x7F) << 15) | ((uint64_t)data[i + 3] << 7) | ((data[i + 4] >> 1) & 0x7F);
        i += 5;
    }
    else if(0x02 & pes->PTS_DTS_flags)
    {
        // has pts
        pes->dts = pes->pts;
    }
    //else
    //{
    //    pes->dts = PTS_NO_VALUE;
    //}

    if (pes->ESCR_flag)
    {
        pes->ESCR_base = ((((uint64_t)data[i] >> 3) & 0x07) << 30) | (((uint64_t)data[i] & 0x03) << 28) | ((uint64_t)data[i + 1] << 20) | ((((uint64_t)data[i + 2] >> 3) & 0x1F) << 15) | (((uint64_t)data[i + 2] & 0x3) << 13) | ((uint64_t)data[i + 3] << 5) | ((data[i + 4] >> 3) & 0x1F);
        pes->ESCR_extension = ((data[i + 4] & 0x03) << 7) | ((data[i + 5] >> 1) & 0x7F);
        i += 6;
    }

    if (pes->ES_rate_flag)
    {
        pes->ES_rate = ((data[i] & 0x7F) << 15) | (data[i + 1] << 7) | ((data[i + 2] >> 1) & 0x7F);
        i += 3;
    }

    if (pes->DSM_trick_mode_flag)
    {
        // TODO:
        i += 1;
    }

    if (pes->additional_copy_info_flag)
    {
        i += 1;
    }

    if (pes->PES_CRC_flag)
    {
        i += 2;
    }

    if (pes->PES_extension_flag)
    {
    }

    if (pes->len > 0)
        pes->len -= pes->PES_header_data_length + 3;

    return pes->PES_header_data_length + 9;
}

size_t system_header_read_gb28181(struct ps_system_header_t *h, const uint8_t* data, size_t bytes)
{
    size_t i, j;
    size_t len;

    if (bytes < 12) return 0;

    assert(0x00 == data[0] && 0x00 == data[1] && 0x01 == data[2] && PES_SID_SYS == data[3]);
    len = (data[4] << 8) | data[5];
    assert(len + 6 <= bytes);

    assert((0x80 & data[6]) == 0x80); // '1xxxxxxx'
    assert((0x01 & data[8]) == 0x01); // 'xxxxxxx1'
    h->rate_bound = ((data[6] & 0x7F) << 15) | (data[7] << 7) | ((data[8] >> 1) & 0x7F);

    h->audio_bound = (data[9] >> 2) & 0x3F;
    h->fixed_flag = (data[9] >> 1) & 0x01;
    h->CSPS_flag = (data[9] >> 0) & 0x01;

    assert((0x20 & data[10]) == 0x20); // 'xx1xxxxx'
    h->system_audio_lock_flag = (data[10] >> 7) & 0x01;
    h->system_video_lock_flag = (data[10] >> 6) & 0x01;
    h->video_bound = data[10] & 0x1F;

    //	assert((0x7F & data[11]) == 0x00); // 'x0000000'
    h->packet_rate_restriction_flag = (data[11] >> 7) & 0x01;

    i = 12;
    for (j = 0; (data[i] & 0x80) == 0x80 && j < sizeof(h->streams) / sizeof(h->streams[0]) && i < bytes; j++)
    {
        h->streams[j].stream_id = data[i++];
        if (h->streams[j].stream_id == PES_SID_EXTENSION) // '10110111'
        {
            assert(data[i] == 0xC0); // '11000000'
            assert((data[i + 1] & 80) == 0); // '1xxxxxxx'
            h->streams[j].stream_id = (h->streams[j].stream_id << 7) | (data[i + 1] & 0x7F);
            assert(data[i + 2] == 0xB6); // '10110110'
            i += 3;
        }

        assert((data[i] & 0xC0) == 0xC0); // '11xxxxxx'
        h->streams[j].buffer_bound_scale = (data[i] >> 5) & 0x01;
        h->streams[j].buffer_size_bound = (data[i] & 0x1F) | data[i + 1];
        i += 2;
    }

    return len + 4 + 2;
}

size_t psd_read_gb28181(struct psd_t *psd, const uint8_t* data, size_t bytes)
{
	int i, j;
	uint16_t packet_length;
	uint16_t number_of_access_units;

	// Table 2-42 �C Program stream directory packet(p81)
	assert(0x00==data[0] && 0x00==data[1] && 0x01==data[2] && 0xFF==data[3]);
	packet_length = (((uint16_t)data[4]) << 8) | data[5];
	assert(bytes >= (size_t)packet_length + 6);

	assert((0x01 & data[7]) == 0x01); // 'xxxxxxx1'
	number_of_access_units = (data[6] << 8) | ((data[7] >> 7) & 0x7F);
	assert(number_of_access_units <= N_ACCESS_UNIT);

	assert((0x01 & data[9]) == 0x01); // 'xxxxxxx1'
	assert((0x01 & data[11]) == 0x01); // 'xxxxxxx1'
	assert((0x01 & data[13]) == 0x01); // 'xxxxxxx1'
	psd->prev_directory_offset = (uint64_t)(((uint64_t)data[8] << 38) | ((((uint64_t)data[9] >> 7) & 0x7F) << 30) | ((uint64_t)data[10] << 22) | ((((uint64_t)data[11] >> 7) & 0x7F) << 15) | ((uint64_t)data[12] << 7) | (((uint64_t)data[13] >> 7) & 0x7F));
	assert((0x01 & data[15]) == 0x01); // 'xxxxxxx1'
	assert((0x01 & data[17]) == 0x01); // 'xxxxxxx1'
	assert((0x01 & data[19]) == 0x01); // 'xxxxxxx1'
	psd->next_directory_offset = (uint64_t)(((uint64_t)data[14] << 38) | ((((uint64_t)data[15] >> 7) & 0x7F) << 30) | ((uint64_t)data[16] << 22) | ((((uint64_t)data[17] >> 7) & 0x7F) << 15) | ((uint64_t)data[18] << 7) | (((uint64_t)data[19] >> 7) & 0x7F));

	// access unit
	j = 20;
	for(i = 0; i < number_of_access_units; i++)
	{
		psd->units[i].packet_stream_id = data[j];
		psd->units[i].pes_header_position_offset_sign = (data[j+1] >> 7) & 0x01;
		assert((0x01 & data[j+2]) == 0x01); // 'xxxxxxx1'
		assert((0x01 & data[j+4]) == 0x01); // 'xxxxxxx1'
		assert((0x01 & data[j+6]) == 0x01); // 'xxxxxxx1'
		psd->units[i].pes_header_position_offset = (uint64_t)(((uint64_t)(data[j+1] & 0x7F) << 38) | ((((uint64_t)data[j+2] >> 7) & 0x7F) << 30) | ((uint64_t)data[j+3] << 22) | ((((uint64_t)data[j+4] >> 7) & 0x7F) << 15) | ((uint64_t)data[j+5] << 7) | (((uint64_t)data[j+6] >> 7) & 0x7F));
		psd->units[i].reference_offset = (data[j+7] << 8) | data[j+8];

		assert((0x81 & data[j+9]) == 0x81); // '1xxxxxx1'
		if(psd->units[i].packet_stream_id == 0xFD)
		{
			psd->units[i].packet_stream_id_extension_msbs = (data[j+9] >> 4) & 0x07;
		}
		else
		{
			assert((0x70 & data[j+9]) == 0x00); // '1000xxx1'
		}

		assert((0x01 & data[j+11]) == 0x01); // 'xxxxxxx1'
		assert((0x01 & data[j+13]) == 0x01); // 'xxxxxxx1'
		psd->units[i].PTS = (uint64_t)(((uint64_t)((data[j+9] >> 1) & 0x07) << 30) | ((uint64_t)data[j+10] << 22) | ((((uint64_t)data[j+11] >> 7) & 0x7F) << 15) | ((uint64_t)data[j+12] << 7) | (((uint64_t)data[j+13] >> 7) & 0x7F));

		assert((0x01 & data[j+15]) == 0x01); // 'xxxxxxx1'
		psd->units[i].bytes_to_read = (uint32_t)( ((uint32_t)data[j+14] << 15) | (((data[j+15] >> 1) & 0x7F) << 8) | data[j+16]);

		assert((0x80 & data[j+17]) == 0x80); // '1xxxxxxx'
		psd->units[i].intra_coded_indicator = (data[j+17] >> 6) & 0x01;
		psd->units[i].coding_parameters_indicator = (data[j+17] >> 4) & 0x03;
		if(0xFD == psd->units[i].packet_stream_id)
		{
			psd->units[i].packet_stream_id_extension_lsbs = data[j+17] & 0x0F;
		}
		else
		{
			assert((0x0F & data[j+17]) == 0x00); // '1xxx0000'
		}

		j += 18;
	}

	return j+1;
}

static size_t gb28181_make_ps_parser_gb28181(struct ps_demuxer_t *ps, const uint8_t* data, size_t bytes)
{
    size_t i = 0;
    size_t j = 0;
    size_t pes_packet_length;
    struct pes_t* pes;
    bool is_start_3byte = false;
    char c_Start_1code = 0x00;
    char c_Start_2code = 0x00;
    char c_Start_3code = 0x01;
    int add_start4byte = 0, add_start3byte = 0;
    // MPEG_program_end_code = 0x000001B9
    for (i = 0; i + 5 < bytes && 0x00 == data[i] && 0x00 == data[i + 1] && 0x01 == data[i + 2]
        && PES_SID_END != data[i + 3]
        && PES_SID_START != data[i + 3];
        i += pes_packet_length) 
    {
        pes_packet_length = (data[i + 4] << 8) | data[i + 5];
        if (i + pes_packet_length > bytes)
        {
            return i;
        }
            

        // stream id
        switch (data[i+3])
        {
        case PES_SID_PSM:
            j = psm_read(&ps->psm, data + i, bytes - i);
            pes_packet_length = j;
            is_start_3byte = true;
            break;

        case PES_SID_PSD:
            j = psd_read_gb28181(&ps->psd, data + i, bytes - i);
            assert(j == pes_packet_length + 6);
            break;

        case PES_SID_PRIVATE_2:
        case PES_SID_ECM:
        case PES_SID_EMM:
        case PES_SID_DSMCC:
        case PES_SID_H222_E:
            // stream data
            break;

        case PES_SID_PADDING:
            // padding
            break;

        default:
            pes = psm_fetch(&ps->psm, data[i+3]);
            if (NULL == pes)
                continue;

            assert(PES_SID_END != data[i + 3]);
			if (ps->pkhd.mpeg2)
            {
                j = pes_read_header_gb28181(pes, data + i, bytes - i);
            }

			if (0 == j) continue;

            if (*(data + i + 3) == 0xE0)
            {
                if (is_start_3byte && add_start4byte < 2) // 插入起始码的操作：规律4433444444....
                {                                         // 其中IDR和SEI插入3字节，I帧后面分包不插入
                    fwrite(&c_Start_1code, 1, 1, vfp);
                    fwrite(&c_Start_1code, 1, 1, vfp);
                    fwrite(&c_Start_2code, 1, 1, vfp);
                    fwrite(&c_Start_3code, 1, 1, vfp);
                    fwrite(data + i + j , 1, pes_packet_length - j, vfp);
                    add_start4byte++;
                } 
                else if (is_start_3byte && add_start4byte == 2)
                {
                    if (add_start3byte == 2)
                    {
                        fwrite(data + i + j , 1, pes_packet_length - j, vfp);
                        if (pes_packet_length < PS_PES_PAYLOAD_SIZE)
                        {
                            is_start_3byte = false;
                            add_start4byte = 0;
                            add_start3byte = 0;
                        }
                    }
                    else
                    {
                        fwrite(&c_Start_1code, 1, 1, vfp);
                        fwrite(&c_Start_2code, 1, 1, vfp);
                        fwrite(&c_Start_3code, 1, 1, vfp);
                        fwrite(data + i + j, 1, pes_packet_length - j, vfp);
                        add_start3byte++;
                    }
                }
                else if (!is_start_3byte)
                {
                    fwrite(&c_Start_1code, 1, 1, vfp);
                    fwrite(&c_Start_1code, 1, 1, vfp);
                    fwrite(&c_Start_2code, 1, 1, vfp);
                    fwrite(&c_Start_3code, 1, 1, vfp);
                    fwrite(data + i + j , 1, pes_packet_length - j, vfp);
                }
            }
            else if (*(data + i + 3) == 0xC0)
            {
                fwrite(data + i + j , 1, pes_packet_length - j, afp);
            }
        }
    }

    return i;
}

size_t ps_demuxer_input_gb28181(struct ps_demuxer_t* ps, const uint8_t* data, size_t bytes)//bytes = 12708
{
	size_t i, n;
    const uint8_t* p, *pend;
	
    // location ps start
    p = data;
    pend = data + bytes;
    while(p && pend - p > 3) // pend - p = byte
    {
        p = (const uint8_t*)memchr(p + 3, PES_SID_START, pend - p - 3);
        if(p && 0x01 == *(p-1) && 0x00 == *(p - 2) && 0x00 == *(p-3))
            break;
    }
    
    for (i = (p && p >= data+3) ? p - data - 3 : 0; 
            i + 3 < bytes && 0x00 == data[i] && 0x00 == data[i + 1] && 0x01 == data[i + 2]; )
    {
        if (PES_SID_START == data[i + 3])
        {
            i += pack_header_read(&ps->pkhd, data + i, bytes - i);
        }
        else if (PES_SID_SYS == data[i + 3])
        {
            i += system_header_read_gb28181(&ps->system, data + i, bytes - i);
        }
        else if (PES_SID_END == data[i + 3])
        {
            i += 4;
        }
        else
        {
            n = gb28181_make_ps_parser_gb28181(ps, data + i, bytes - i);
            i += n;

            if (0 == n)
                break;
        }
    }

	return i;
}

size_t video_stream_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.2 Video stream descriptor(p85)
	size_t i;
	video_stream_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.multiple_frame_rate_flag = (data[i] >> 7) & 0x01;
	desc.frame_rate_code = (data[i] >> 3) & 0x0F;
	desc.MPEG_1_only_flag = (data[i] >> 2) & 0x01;
	desc.constrained_parameter_flag = (data[i] >> 1) & 0x01;
	desc.still_picture_flag = data[i] & 0x01;

	if(0 == desc.MPEG_1_only_flag)
	{
		desc.profile_and_level_indication = data[i+1];
		desc.chroma_format = (data[i+2] >> 6) & 0x03;
		desc.frame_rate_code = (data[i+2] >> 5) & 0x01;
		assert((0x1F & data[i+2]) == 0x00); // 'xxx00000'
	}

	return descriptor_len+2;
}

size_t audio_stream_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.4 Audio stream descriptor(p86)
	size_t i;
	audio_stream_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.free_format_flag = (data[i] >> 7) & 0x01;
	desc.ID = (data[i] >> 6) & 0x01;
	desc.layer = (data[i] >> 4) & 0x03;
	desc.variable_rate_audio_indicator = (data[i] >> 3) & 0x01;

	assert(4 == descriptor_len);
	return descriptor_len+2;
}

size_t hierarchy_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.6 Hierarchy descriptor(p86)
	size_t i;
	hierarchy_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.temporal_scalability_flag = (data[i] >> 6) & 0x01;
	desc.spatial_scalability_flag = (data[i] >> 5) & 0x01;
	desc.quality_scalability_flag = (data[i] >> 4) & 0x01;
	desc.hierarchy_type = data[i] & 0x0F;
	desc.hierarchy_layer_index = data[i+1] & 0x3F;
	desc.tref_present_flag = (data[i+2] >> 7) & 0x01;
	desc.hierarchy_embedded_layer_index = data[i+2] & 0x3F;
	desc.hierarchy_channel = data[i+3] & 0x3F;

	assert(4 == descriptor_len);
	return descriptor_len+2;
}

size_t language_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.18 ISO 639 language descriptor(p92)
	size_t i;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	for(i = 2; i < descriptor_len; i += 4)
	{
		language_descriptor_t desc;
		memset(&desc, 0, sizeof(desc));

		desc.code = (data[i] << 16) | (data[i+1] << 8) | data[i+2];
		desc.audio = data[i+3];
	}

	return descriptor_len+2;
}

size_t system_clock_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.20 System clock descriptor(p92)
	size_t i;
	system_clock_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.external_clock_reference_indicator = (data[i] >> 7) & 0x01;
	desc.clock_accuracy_integer = data[i] & 0x3F;
	desc.clock_accuracy_exponent = (data[i+1] >> 5) & 0x07;

	assert(2 == descriptor_len);
	return descriptor_len+2;
}

size_t mpeg4_video_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.36 MPEG-4 video descriptor(p96)
	size_t i;
	mpeg4_video_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.visual_profile_and_level = data[i];

	assert(1 == descriptor_len);
	return descriptor_len+2;
}

size_t mpeg4_audio_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.38 MPEG-4 audio descriptor(p97)
	size_t i;
	mpeg4_audio_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.profile_and_level = data[i];

	assert(1 == descriptor_len);
	return descriptor_len+2;
}

size_t avc_video_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.64 AVC video descriptor(p110)
	size_t i;
	avc_video_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.profile_idc = data[i];
	desc.constraint_set0_flag = (data[i+1] >> 7) & 0x01;
	desc.constraint_set1_flag = (data[i+1] >> 6) & 0x01;
	desc.constraint_set2_flag = (data[i+1] >> 5) & 0x01;
	desc.constraint_set3_flag = (data[i+1] >> 4) & 0x01;
	desc.constraint_set4_flag = (data[i+1] >> 3) & 0x01;
	desc.constraint_set5_flag = (data[i+1] >> 2) & 0x01;
	desc.AVC_compatible_flags = data[i+1] & 0x03;
	desc.level_idc = data[i+2];
	desc.AVC_still_present = (data[i+3] >> 7) & 0x01;
	desc.AVC_24_hour_picture_flag = (data[i+3] >> 6) & 0x01;
	desc.frame_packing_SEI_not_present_flag = (data[i+3] >> 5) & 0x01;

	assert(4 == descriptor_len);
	return descriptor_len+2;
}

size_t avc_timing_hrd_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.66 AVC timing and HRD descriptor(p112)
	size_t i;
	avc_timing_hrd_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.hrd_management_valid_flag = (data[i] >> 7) & 0x01;
	desc.picture_and_timing_info_present = (data[i] >> 0) & 0x01;
	++i;
	if(desc.picture_and_timing_info_present)
	{
		desc._90kHZ_flag = (data[i] >> 7) & 0x01;
		if(0 == desc._90kHZ_flag)
		{
			desc.N = (data[i+1] << 24) | (data[i+2] << 16) | (data[i+3] << 8) | data[i+4];
			desc.K = (data[i+5] << 24) | (data[i+6] << 16) | (data[i+7] << 8) | data[i+8];
			i += 8;
		}
		desc.num_unit_in_tick = (data[i+1] << 24) | (data[i+2] << 16) | (data[i+3] << 8) | data[i+4];
		i += 5;
	}

	desc.fixed_frame_rate_flag = (data[i] >> 7) & 0x01;
	desc.temporal_poc_flag = (data[i] >> 6) & 0x01;
	desc.picture_to_display_conversion_flag = (data[i] >> 5) & 0x01;

	return descriptor_len+2;
}

size_t mpeg2_aac_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.68 MPEG-2 AAC audio descriptor(p113)
	size_t i;
	mpeg2_aac_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.profile = data[i];
	desc.channel_configuration = data[i+1];
	desc.additional_information = data[i+2];

	assert(3 == descriptor_len);
	return descriptor_len+2;
}

size_t svc_extension_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.76 SVC extension descriptor(p116)
	size_t i;
	svc_extension_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.width = (data[i] << 8) | data[i+1];
	desc.height = (data[i+2] << 8) | data[i+3];
	desc.frame_rate = (data[i+4] << 8) | data[i+5];
	desc.average_bitrate = (data[i+6] << 8) | data[i+7];
	desc.maximum_bitrate = (data[i+8] << 8) | data[i+9];
	desc.dependency_id = (data[i+10] >> 5) & 0x07;
	desc.quality_id_start = (data[i+11] >> 4) & 0x0F;
	desc.quality_id_end = (data[i+11] >> 0) & 0x0F;
	desc.temporal_id_start = (data[i+12] >> 5) & 0x07;
	desc.temporal_id_end = (data[i+12] >> 2) & 0x07;
	desc.no_sei_nal_unit_present = (data[i+12] >> 1) & 0x01;

	assert(13 == descriptor_len);
	return descriptor_len+2;
}

size_t mvc_extension_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.78 MVC extension descriptor(p117)
	size_t i;
	mvc_extension_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.average_bit_rate = (data[i] << 8) | data[i+1];
	desc.maximum_bitrate = (data[i+2] << 8) | data[i+3];
	desc.view_order_index_min = ((data[i+4] & 0xF) << 6) | ((data[i+5] >> 2) & 0x3F);
	desc.view_order_index_max = ((data[i+5] & 0x3) << 8) | data[i+6];
	desc.temporal_id_start = (data[i+7] >> 5) & 0x07;
	desc.temporal_id_end = (data[i+7] >> 2) & 0x07;
	desc.no_sei_nal_unit_present = (data[i+7] >> 1) & 0x01;
	desc.no_prefix_nal_unit_present = (data[i+7] >> 0) & 0x01;

	assert(8 == descriptor_len);
	return descriptor_len+2;
}

#endif
