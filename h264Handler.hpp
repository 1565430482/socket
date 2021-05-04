#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <queue>
#include "Aes.hpp"
#include "mpeg2_ps.hpp"

typedef enum
{
    NALU_TYPE_SLICE     = 1,
    NALU_TYPE_DPA       = 2,
    NALU_TYPE_DPB       = 3,
    NALU_TYPE_DPC       = 4,
    NALU_TYPE_IDR       = 5,
    NALU_TYPE_SEI       = 6,
    NALU_TYPE_SPS       = 7,
    NALU_TYPE_PPS       = 8,
    NALU_TYPE_AUD       = 9,
    NALU_TYPE_EOSEQ     = 10,
    NALU_TYPE_EOSTREAM  = 11,
    NALU_TYPE_FILL      = 12, //第一处修改
}Nalu_type;

typedef enum
{
    NALU_PRIORITY_DISPOSABLE = 0,
	NALU_PRIRITY_LOW         = 1,
	NALU_PRIORITY_HIGH       = 2,
	NALU_PRIORITY_HIGHEST    = 3
}Nalu_priority;

typedef struct
{
    char* buf ;
    int start_code_prefix_len;
    int forbidden_bit;
    int nalu_ref_idc;
    int nalu_unit_type;
    unsigned len;
    unsigned max_size;
    unsigned pts;
}Nalu_t;

static int find_start_code_3byte(unsigned char* buf)//判断起始吗位数
{
    if(buf[0] != 0 || buf[1] != 0 || buf[2] != 1)
    {
        return 0;
    }
    else
    {
        return 1;
    }    
}

static int find_start_code_4byte(unsigned char* buf)//判断起始吗位数
{
    if(buf[0] != 0 || buf[1] != 0 || buf[2] != 0 || buf[3] != 1)
    {
        return 0;
    }
    else
    {
        return 1;
    } 
}

int get_annexb_Nalu(Nalu_t* nalu, FILE* fp)//获取每个NALU单位
{
    unsigned char* buf;
    int info_first  = 0;
    int info_second = 0;

    if ((buf = (unsigned char*)calloc(nalu->max_size, sizeof(char))) == NULL)
    {
        printf ("GetAnnexbNALU: Could not allocate Buf memory\n");
    }
    
    nalu->start_code_prefix_len = 3;
    
    if (3 != fread(buf, 1, 3, fp))
    {
        free(buf);
        return 0;
    }
    int pos    = 0;
    info_first = find_start_code_3byte(buf);
    if (info_first != 1)
    {
        if (1 != fread(buf+3, 1, 1, fp))
        {
            free(buf);
            return 0;
        }
        info_second = find_start_code_4byte(buf);
        if (info_second != 1)
        {
            free(buf);
            return -1;
        }
        else
        {
            pos = 4;
            nalu->start_code_prefix_len = 4;
        }
    }
    else
    {
        nalu->start_code_prefix_len = 3;
        pos = 3;
    }

    int start_code_found = 0;
    info_first           = 0;
    info_second          = 0;
    while (!start_code_found)
    {
        //nalu结尾
        if(feof(fp))
        {
            //start_code + nalu header + raw data(负荷)
            nalu->len = (pos-1)-nalu->start_code_prefix_len;//pos-1猜测是从end-1回来
			memcpy(nalu->buf, &buf[nalu->start_code_prefix_len], nalu->len);//拷贝结尾到插入的头部  
			nalu->forbidden_bit  = (nalu->buf[0]) & 0x80;  // 1 bit   1000 0000
			nalu->nalu_ref_idc   = (nalu->buf[0]) & 0x60;  // 2 bit   0110 0000
			nalu->nalu_unit_type = (nalu->buf[0]) & 0x1f;  // 5 bit   0001 1111
			free(buf);
			return pos-1;
        }
        
        buf[pos++]  = fgetc(fp);
        
        //判断是否到达下一块nalu；
        info_second = find_start_code_4byte(&buf[pos-4]);
        if (info_second != 1)
        {
            info_first = find_start_code_3byte(&buf[pos-3]);
        }
        start_code_found = (info_first == 1 || info_second == 1);
    }
    //重置
    int rewind = (info_second == 1) ? -4 : -3;
    if (0 != fseek (fp, rewind, SEEK_CUR))
    {
		free(buf);
		printf("GetAnnexbNALU: Cannot fseek in the bit stream file");
	}
    //start_code + nalu header + raw data(负荷)
    nalu->len = (pos+rewind)-nalu->start_code_prefix_len;
    memcpy(nalu->buf, &buf[nalu->start_code_prefix_len], nalu->len);     
    nalu->forbidden_bit  = (nalu->buf[0]) & 0x80;    // 1 bit   1000 0000
    nalu->nalu_ref_idc   = (nalu->buf[0]) & 0x60;    // 2 bit   0110 0000
    nalu->nalu_unit_type = (nalu->buf[0]) & 0x1f;    // 5 bit   0001 1111
    free(buf);  

    return (pos+rewind);
}

int h264_parser(const char* url)//解析每一个NALU的具体信息
{
    Nalu_t* n;
    int buf_size = 100000;

    FILE* myout = stdout;
    FILE* h264_bit_stream = fopen(url, "r");
    if(h264_bit_stream == NULL) 
    {
        perror("Open file error\n");
		return 0;
    }
    n = (Nalu_t*)calloc(1, sizeof(Nalu_t));
    if (n == NULL)
    {
		printf("Alloc NALU Error\n");
		return 0;
	}

    n->max_size = buf_size;
    n->buf = (char*)calloc (buf_size, sizeof (char));
	if (n->buf == NULL)
    {
		free (n);
		printf ("AllocNALU: n->buf");
		return 0;
	}

	int data_offset = 0;
	int nal_num     = 0;
	printf("-----+-------- NALU Table ------+---------+\n");
	printf(" NUM |    POS  |    IDC |  TYPE |   LEN   |\n");
	printf("-----+---------+--------+-------+---------+\n");

    while (!feof (h264_bit_stream))
    {
        int data_length   = 0;
        data_length       = get_annexb_Nalu(n, h264_bit_stream);
        
        char type_str[20] = {0};
		switch(n->nalu_unit_type)
        {
			case NALU_TYPE_SLICE:sprintf(type_str,"SLICE");break;
			case NALU_TYPE_DPA:sprintf(type_str,"DPA");break;
			case NALU_TYPE_DPB:sprintf(type_str,"DPB");break;
			case NALU_TYPE_DPC:sprintf(type_str,"DPC");break;
			case NALU_TYPE_IDR:sprintf(type_str,"IDR");break;
			case NALU_TYPE_SEI:sprintf(type_str,"SEI");break;
			case NALU_TYPE_SPS:sprintf(type_str,"SPS");break;
			case NALU_TYPE_PPS:sprintf(type_str,"PPS");break;
			case NALU_TYPE_AUD:sprintf(type_str,"AUD");break;
			case NALU_TYPE_EOSEQ:sprintf(type_str,"EOSEQ");break;
			case NALU_TYPE_EOSTREAM:sprintf(type_str,"EOSTREAM");break;
			case NALU_TYPE_FILL:sprintf(type_str,"FILL");break;
		}
		char idc_str[20]={0};
		switch(n->nalu_ref_idc >> 5)
        {
			case NALU_PRIORITY_DISPOSABLE:sprintf(idc_str,"DISPOS");break;
			case NALU_PRIRITY_LOW:sprintf(idc_str,"LOW");break;
			case NALU_PRIORITY_HIGH:sprintf(idc_str,"HIGH");break;
			case NALU_PRIORITY_HIGHEST:sprintf(idc_str,"HIGHEST");break;
		}
 
		fprintf(myout,"%5d| %8d| %7s| %6s| %8d|\n",nal_num,data_offset,idc_str,type_str,n->len);
		data_offset += data_length;
 
		nal_num++;
	}

    if (n)
    {
		if (n->buf)
        {
			free(n->buf);
			n->buf=NULL;
		}
		free (n);
	}
	return 0;
}

void h264_encode(const char* in_FileName, const char* out_FileName, Aes& obj_Aes)
{
    char c_Start_1code = 0x00;
    char c_Start_2code = 0x00;
    char c_Start_3code = 0x01;
    int buf_size       = 10000;
    Nalu_t* n;

    FILE* h264_bit_stream = fopen(in_FileName, "r");
    if(h264_bit_stream == NULL) 
    {
        perror("Open file error\n");
		EXIT_FAILURE;
    }
    FILE* h264_enc_stream = fopen(out_FileName, "w");
    if(h264_enc_stream == NULL) 
    {
        perror("Open file error\n");
		EXIT_FAILURE;
    }

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

    while (!feof (h264_bit_stream))
    {
        int data_length          = get_annexb_Nalu(n, h264_bit_stream);
        int num_Aes_blocks       = (n->len - 1) / AES_BLOCK_SIZE;
        std::string str_Raw_data = std::string(&n->buf[1], num_Aes_blocks * AES_BLOCK_SIZE);
        auto str_Encode_data     = obj_Aes.EncryptAES(str_Raw_data);
        memcpy(n->buf+1, str_Encode_data.c_str(), num_Aes_blocks * AES_BLOCK_SIZE);
        

        //填充
        if (n->start_code_prefix_len == 4)
        {
            fwrite(&c_Start_1code, 1, 1, h264_enc_stream);
            fwrite(&c_Start_1code, 1, 1, h264_enc_stream);
            fwrite(&c_Start_2code, 1, 1, h264_enc_stream);
            fwrite(&c_Start_3code, 1, 1, h264_enc_stream);
            fwrite(n->buf, 1, n->len, h264_enc_stream);
        }
        else if (n->start_code_prefix_len == 3)
        {
            fwrite(&c_Start_1code, 1, 1, h264_enc_stream);
            fwrite(&c_Start_2code, 1, 1, h264_enc_stream);
            fwrite(&c_Start_3code, 1, 1, h264_enc_stream);
            fwrite(n->buf, 1, n->len, h264_enc_stream);
        }
	}

    if (n)
    {
		if (n->buf)
        {
			free(n->buf);
			n->buf=NULL;
		}
		free (n);
	}
    fclose(h264_enc_stream);
    fclose(h264_bit_stream);
}

void h264_decode(const char* in_FileName, const char* out_FileName, Aes& obj_Aes)
{
    char c_Start_1code = 0x00;
    char c_Start_2code = 0x00;
    char c_Start_3code = 0x01;
    int buf_size       = 10000;
    Nalu_t* n;

    FILE* h264_bit_stream = fopen(in_FileName, "r");
    if(h264_bit_stream == NULL) 
    {
        perror("Open file error\n");
		EXIT_FAILURE;
    }
    FILE* h264_dec_stream = fopen(out_FileName, "w");
    if(h264_dec_stream == NULL) 
    {
        perror("Open file error\n");
		EXIT_FAILURE;
    }

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

    while (!feof (h264_bit_stream))
    {
        int data_length    = get_annexb_Nalu(n, h264_bit_stream);
        int num_Aes_blocks = (n->len - 1) / AES_BLOCK_SIZE;
        std::string str_Encode_data = std::string(&n->buf[1], num_Aes_blocks * AES_BLOCK_SIZE);
        auto str_Decode_data        = obj_Aes.DecryptAES(str_Encode_data);
        memcpy(n->buf+1, str_Decode_data.c_str(), num_Aes_blocks * AES_BLOCK_SIZE);
        //填充
        if (n->start_code_prefix_len == 4)
        {
            fwrite(&c_Start_1code, 1, 1, h264_dec_stream);//写入起始码
            fwrite(&c_Start_1code, 1, 1, h264_dec_stream);
            fwrite(&c_Start_2code, 1, 1, h264_dec_stream);
            fwrite(&c_Start_3code, 1, 1, h264_dec_stream);
            fwrite(n->buf, 1, n->len, h264_dec_stream);
        }
        else if (n->start_code_prefix_len == 3)
        {
            fwrite(&c_Start_1code, 1, 1, h264_dec_stream);//写入起始码
            fwrite(&c_Start_2code, 1, 1, h264_dec_stream);
            fwrite(&c_Start_3code, 1, 1, h264_dec_stream);
            fwrite(n->buf, 1, n->len, h264_dec_stream);
        }
	}

    if (n)
    {
		if (n->buf)
        {
			free(n->buf);
			n->buf=NULL;
		}
		free (n);
	}
    fclose(h264_dec_stream);
    fclose(h264_bit_stream);
}

void h264_to_PS(const char* in_FileName, const char* out_FileName)
{
    int buf_size = 10000;
    Nalu_t* n;

    FILE* h264_bit_stream = fopen(in_FileName, "r");
    if(h264_bit_stream == NULL) 
    {
        perror("Open file error\n");
		EXIT_FAILURE;
    }
    FILE* program_stream = fopen(out_FileName, "w");
    if(program_stream == NULL) 
    {
        perror("Open file error\n");
		EXIT_FAILURE;
    }

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

    while (!feof (h264_bit_stream))
    {
        char header_info[75] = { 0 };
        int n_header_pos  = 0;
        int n_header_size = 0;
        int n_buffer_pos  = 0;
        unsigned long long pts = 0;
        int data_length = get_annexb_Nalu(n, h264_bit_stream);

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
        memset(header_info, 0, n_header_size);
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

    if (n)
    {
		if (n->buf)
        {
			free(n->buf);
			n->buf=NULL;
		}
		free (n);
	}
    fclose(program_stream);
    fclose(h264_bit_stream);
}

