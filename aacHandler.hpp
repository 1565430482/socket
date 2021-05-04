#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "Aes.hpp"
#include "mpeg2_ps.hpp"



int get_ADTS_frame(unsigned char* buffer, int buf_size, unsigned char* data, int* data_size)
{
    int size = 0;
    
    if(!buffer || !data || !data_size)
    {
        return -1;
    }

    while (1)
    {
        if (buf_size < 7)
        {
            return -1;
        }
        //Sync words
        if (buffer[0] == 0xff && ((buffer[1] & 0xf0) == 0xf0))
        {
            size |= ((buffer[3] & 0x03) << 11);
            size |= buffer[4] << 3;
            size |= ((buffer[5] & 0xe0)>>5);
            break;
        }
        --buf_size;
        ++buffer;
    }
    memcpy(data, buffer,size);
    *data_size = size;

    return 0;
}

int aac_parser(const char* url)
{
    int data_size = 0;
    int size      = 0;
    int cnt       = 0;
    unsigned char* aacframe  = (unsigned char*)malloc(1024*5);
    unsigned char* aacbuffer = (unsigned char*)malloc(1024*1024);

    FILE* fp = stdout;
    FILE* in_file = fopen(url, "r");
    if (!in_file)
    {
        perror("open file error");
        return -1;
    }
    
    printf("-----+- ADTS Frame Table -+------+\n");
	printf(" NUM | Profile | Frequency| Size |\n");
	printf("-----+---------+----------+------+\n");
    while (!feof(in_file))
    {
        data_size = fread(aacbuffer, 1, 1024*1024, in_file);
        unsigned char* input_data = aacbuffer;
        
        while (1)
        {
            int ret = get_ADTS_frame(input_data, data_size, aacframe, &size);
            if (ret == -1) 
            {
                break;
            }

            char profile_str[10]   = {0};
			char frequence_str[10] = {0};
 
			unsigned char profile  = aacframe[2] & 0xC0; //  1100 0000 profile两位 位于17、18位
			profile = profile >> 6;             //右边 即相除 转为十进制
			switch(profile)
            {
                case 0: sprintf(profile_str,"Main");        break;
                case 1: sprintf(profile_str,"LC");          break;
                case 2: sprintf(profile_str,"SSR");         break;
                default:sprintf(profile_str,"unknown");     break;
			}
            unsigned char sampling_frequency_index = aacframe[2] & 0x3C; // 0011 1100 sampling_frequency_index4位
			sampling_frequency_index               = sampling_frequency_index >> 2;//右边 即相除 转为十进制
			switch(sampling_frequency_index)
            {
                case 0: sprintf(frequence_str,"96000Hz");   break;
                case 1: sprintf(frequence_str,"88200Hz");   break;
                case 2: sprintf(frequence_str,"64000Hz");   break;
                case 3: sprintf(frequence_str,"48000Hz");   break;
                case 4: sprintf(frequence_str,"44100Hz");   break;
                case 5: sprintf(frequence_str,"32000Hz");   break;
                case 6: sprintf(frequence_str,"24000Hz");   break;
                case 7: sprintf(frequence_str,"22050Hz");   break;
                case 8: sprintf(frequence_str,"16000Hz");   break;
                case 9: sprintf(frequence_str,"12000Hz");   break;
                case 10: sprintf(frequence_str,"11025Hz");  break;
                case 11: sprintf(frequence_str,"8000Hz");   break;
                default:sprintf(frequence_str,"unknown");   break;
			}
 
			fprintf(fp,"%5d| %8s|  %8s| %5d|\n",cnt,profile_str ,frequence_str,size);
			data_size -= size;
			input_data += size;
			cnt++;
        }
    }
    fclose(in_file);
    return 0;
}

void aac_encode(const char* in_FileName, const char* out_FileName, Aes& obj_Aes)
{
    int data_size = 0;
    int size      = 0;
    unsigned char* aacframe  = (unsigned char*)malloc(1024*5);
    unsigned char* aacbuffer = (unsigned char*)malloc(1024*1024);

    FILE* aac_bit_stream = fopen(in_FileName, "r");
    if(aac_bit_stream == NULL) 
    {
        perror("Open file error\n");
		EXIT_FAILURE;
    }
    FILE* aac_enc_stream = fopen(out_FileName, "w");
    if(aac_enc_stream == NULL) 
    {
        perror("Open file error\n");
		EXIT_FAILURE;
    }
    
    while (!feof(aac_bit_stream))
    {
        data_size = fread(aacbuffer, 1, 1024*1024, aac_bit_stream);
        unsigned char* input_data = aacbuffer;

        while (1)
        {
            int ret = get_ADTS_frame(input_data, data_size, aacframe, &size);
            if (ret == -1) 
            {
                std::cout << "break" << std::endl;
                break;
            }
            int num_Aes_blocks   = (size-7) / AES_BLOCK_SIZE;
            auto str_Raw_data    = std::string((const char*)&aacframe[7], num_Aes_blocks * AES_BLOCK_SIZE);
            auto str_Encode_data = obj_Aes.EncryptAES(str_Raw_data);
            memcpy(aacframe + 7, str_Encode_data.c_str(), num_Aes_blocks * AES_BLOCK_SIZE);
            fwrite(aacframe, 1, size, aac_enc_stream);

            data_size  -= size;
			input_data += size;
        }
    }
    free(aacframe);
    free(aacbuffer);
    fclose(aac_enc_stream);
    fclose(aac_bit_stream);
}

void aac_decode(const char* in_FileName, const char* out_FileName, Aes& obj_Aes)
{
    int data_size = 0;
    int size      = 0;
    unsigned char* aacframe  = (unsigned char*)malloc(1024*5);
    unsigned char* aacbuffer = (unsigned char*)malloc(1024*1024);

    FILE* aac_enc_stream = fopen(in_FileName, "r");
    if(aac_enc_stream == NULL) 
    {
        perror("Open file error\n");
		EXIT_FAILURE;
    }
    FILE* aac_dec_stream = fopen(out_FileName, "w");
    if(aac_dec_stream == NULL) 
    {
        perror("Open file error\n");
		EXIT_FAILURE;
    }

    while (!feof(aac_enc_stream))
    {
        data_size = fread(aacbuffer, 1, 1024*1024, aac_enc_stream);
        unsigned char* input_data = aacbuffer;
        
        while (1)
        {
            int ret = get_ADTS_frame(input_data, data_size, aacframe, &size);
            if (ret == -1) 
            {
                break;
            }
            int num_Aes_blocks = (size-7) / AES_BLOCK_SIZE;
            std::string str_Enc_data = std::string((const char*)&aacframe[7], num_Aes_blocks * AES_BLOCK_SIZE);
            auto str_Dec_data        = obj_Aes.DecryptAES(str_Enc_data);
            memcpy(&aacframe[7], str_Dec_data.c_str(), num_Aes_blocks * AES_BLOCK_SIZE);
            fwrite(aacframe, 1, size, aac_dec_stream);

            data_size  -= size;
			input_data += size;
        }
    }
    free(aacframe);
    free(aacbuffer);
    fclose(aac_dec_stream);
    fclose(aac_enc_stream);
}

void aac_to_ps(const char* in_FileName, const char* out_FileName)
{
    int data_size = 0;
    int size      = 0;
    unsigned char* aacframe  = (unsigned char*)malloc(1024*5);
    unsigned char* aacbuffer = (unsigned char*)malloc(1024*1024);

    FILE* aac_bit_stream = fopen(in_FileName, "r");
    if(aac_bit_stream == NULL) 
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

    while (!feof(aac_bit_stream))
    {
        data_size = fread(aacbuffer, 1, 1024*1024, aac_bit_stream);
        unsigned char* input_data = aacbuffer;
        char header_info[50] = {0};
        while (1)
        {
            int ret = get_ADTS_frame(input_data, data_size, aacframe, &size);
            if (ret == -1) 
            {
                break;
            }
            
            gb28181_make_ps_header(header_info, 0);
            gb28181_make_pes_header(header_info + PS_HDR_LEN, 0xC0, 
                size > PS_PES_PAYLOAD_SIZE ? PS_PES_PAYLOAD_SIZE : size, 0, 0);
            fwrite(header_info, 1, PS_HDR_LEN + PES_HDR_LEN, program_stream);
            fwrite(aacframe, 1, size , program_stream);
            data_size  -= size;
			input_data += size;
        }
    }
    free(aacframe);
    free(aacbuffer);
    fclose(aac_bit_stream);
    fclose(program_stream);
}