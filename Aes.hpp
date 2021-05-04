#ifndef AES_H
#define AES_H

#include <string>
#include <openssl/aes.h>
#include <cassert>
#include <cstring>
#include <random>
#include <iostream>

using namespace std;
using std::random_device;
using std::default_random_engine;

class Aes
{
public:
    Aes(unsigned int length);
    Aes();
    ~Aes() = default;

    string key() {  return key_; }
    string changeKey(string& str) { return key_ = str; }
    string randomPool(unsigned length);
    string EncryptAES(const std::string& original);
    string DecryptAES(const std::string& original);

    bool key_state()        { return is_key_;   }
    void set_key_state()    { is_key_ = true;   }
private:
    string key_;
    bool   is_key_ = false;
};


Aes::Aes(unsigned int length) : 
    key_(randomPool(length))
{   assert(key_.length()==16);}

Aes::Aes()
{};

string Aes::randomPool(unsigned int length)//随机生成
{
    char tmp;
	std::string randomStr;

	std::random_device rd;  // 产生一个 std::random_device 对象 rd
    std::default_random_engine random(rd());	// 用 rd 初始化一个随机数发生器 random

	for(int i = 0; i < length; ++i)
	{
		tmp = random() % 36;	// 随机一个小于 36 的整数，0-9、A-Z 共 36 种字符
        if (tmp < 10) {			// 如果随机数小于 10，变换成一个阿拉伯数字的 ASCII
            tmp += '0';
        } else {				// 否则，变换成一个大写字母的 ASCII
            tmp -= 10;
            tmp += 'A';
        }
		randomStr.push_back(tmp);
	}
	return randomStr;
}

string Aes::EncryptAES(const std::string& original)//加密明文
{
    AES_KEY aes_key;
    if (AES_set_encrypt_key((const unsigned char*)key_.c_str(), key_.length() * 8, &aes_key) < 0)
    {
        assert(false);
        return "";
    }
    string strRet;
    string strDataBak = original;
    unsigned int data_length = strDataBak.length();
    int padding = 0;
    if (strDataBak.length() % AES_BLOCK_SIZE > 0)
    {
        padding = AES_BLOCK_SIZE - strDataBak.length() % AES_BLOCK_SIZE;
    }
    data_length += padding;//补充位数
    while (padding > 0)
    {
        strDataBak += '\0';//填充
        padding--;
    }
    for (unsigned int i = 0; i < data_length / AES_BLOCK_SIZE; i++)//分段加密
    {
        string strBlock = strDataBak.substr(i*AES_BLOCK_SIZE, AES_BLOCK_SIZE);
        unsigned char out[AES_BLOCK_SIZE];
        ::memset(out, 0, AES_BLOCK_SIZE);
        AES_encrypt((const unsigned char*)strBlock.c_str(), out, &aes_key);
        strRet += string((const char*)out, AES_BLOCK_SIZE);//字符串拼接
    }
    return strRet;
}

string Aes::DecryptAES(const std::string& original)//解密明文
{
    AES_KEY aes_key;
    if (AES_set_decrypt_key((const unsigned char*)key_.c_str(), key_.length() * 8, &aes_key) < 0)
    {
        assert(false);
        return "";
    }
    std::string strRet;
    for (unsigned int i = 0; i < original.length() / AES_BLOCK_SIZE; i++)//分段解密
    {
        std::string strBlock = original.substr(i*AES_BLOCK_SIZE, AES_BLOCK_SIZE);
        unsigned char out[AES_BLOCK_SIZE];
        ::memset(out, 0, AES_BLOCK_SIZE);
        AES_decrypt((const unsigned char*)strBlock.c_str(), out, &aes_key);//解密
        strRet += std::string((const char*)out, AES_BLOCK_SIZE);
    }
    string::size_type pos = strRet.find_last_not_of('\0');
    if (pos != string::npos)
    {
        strRet = strRet.substr(0, pos + 1);
    }
    return strRet;
}
#endif // !AES_H