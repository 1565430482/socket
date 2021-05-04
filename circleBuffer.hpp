#ifndef CIRCLEBUFFER_H
#define CIRCLEBUFFER_H 

#include <cassert>
#include <cstring>
#include <memory>

class circleBuffer
{
public:
    explicit circleBuffer(size_t size);
    circleBuffer() = default;
    ~circleBuffer();

    bool isEmpty();
    bool isFull();

    void clear();
    int getLength();

    void setSize(size_t size) 
    {
        nBufSize_   = size;
        pBuf_       = new char[nBufSize_];
        nReadPos_   = 0;
        nWritePos_  = 0;
        bFull_      = false;
        bEmpty_     = true;
    }
    
    int write(char* pInBuf, int count);
    int read(char* pOutBuf, int count);

    int getReadPos();
    int getWritePos();

private:
    bool        bEmpty_, bFull_;
    char*       pBuf_;
    size_t      nBufSize_;   //环形缓冲区大小
    int         nReadPos_;   //读取索引
    int         nWritePos_;  //写入索引
};


circleBuffer::circleBuffer(size_t size)
{
    nBufSize_   = size;
    pBuf_       = new char[nBufSize_];
    nReadPos_   = 0;
    nWritePos_  = 0;
    bFull_      = false;
    bEmpty_     = true;
}

circleBuffer::~circleBuffer()
{
    // if (pBuf_)
    // {
    //     delete[] pBuf_;
    //     pBuf_ = nullptr;
    // }
}

bool circleBuffer::isEmpty()
{
    return bEmpty_;
}

bool circleBuffer::isFull()
{
    return bFull_;
}

int circleBuffer::getWritePos()
{
    return nWritePos_;
}

int circleBuffer::getReadPos()
{
    return nReadPos_;
}

void circleBuffer::clear()
{
    nWritePos_ = 0;
    nReadPos_  = 0;
    bEmpty_    = true;
    bFull_     = false;
}

int circleBuffer::getLength()
{
    if (bFull_)
    {
        return nBufSize_;
    }
    else if (bEmpty_)
    {
        return 0;
    }
    else if (nReadPos_ < nWritePos_)
    {
        return nWritePos_ - nReadPos_;
    }
    else
    {
        return nBufSize_ - (nReadPos_ - nWritePos_);
    }
}

int circleBuffer::read(char* pOutBuf, int count)
{
    if (count <= 0)
    {
        return 0;
    }
    bFull_ = false;
    if (bEmpty_)
    {
        return 0;
    }
    else if (nReadPos_ == nWritePos_)   //缓冲区满，两个索引相等
    {
    /*                         == 内存模型 ==
                (data)          m_nReadPos                (data)
    |--------------------------------|---------------------------------------|
                                m_nWritePos                               m_nBufSize
    */
       int leftCount = nBufSize_ - nReadPos_;
       if (leftCount > count)            //活动在右半区
       {
            memcpy(pOutBuf, pBuf_+nReadPos_, count);
            nReadPos_ += count;
            bEmpty_    = (nReadPos_ == nWritePos_);
            return count;
       }
       else                             //越过右半区，到左半区
       {
            memcpy(pOutBuf, pBuf_+ nReadPos_, leftCount);
            //count-leftCount代表左半区未到写索引;等于读索引等于写索引则代表缓冲区空
            nReadPos_ = (nWritePos_ > count - leftCount) ? count - leftCount : nWritePos_;
            memcpy(pOutBuf+leftCount, pBuf_, nReadPos_);
            bEmpty_ = (nReadPos_ == nWritePos_);
            return leftCount + nReadPos_;
       }
    }
    else if (nReadPos_ < nWritePos_)//写指针在前， 未读数据连续
    {
    /*                          == 内存模型 ==
            (read)                 (unread)                      (read)
    |-------------------|----------------------------|---------------------------|
                    m_nReadPos                m_nWritePos                     m_nBufSize
    */
        int leftCount = nWritePos_ - nReadPos_;
        int length    = (leftCount > count) ? count : leftCount;
        memcpy(pOutBuf, pBuf_+ nReadPos_, length);
        nReadPos_    += length;
        bEmpty_       = (nReadPos_ == nWritePos_);
        assert(nReadPos_ < nBufSize_);
        assert(nWritePos_ < nBufSize_);
        return length;
    }
    else    //写指针在后，未读数据不连续
    {
    /*                          == 内存模型 ==
    (unread)                (read)                      (unread)
    |-------------------|----------------------------|---------------------------|
                m_nWritePos                  m_nReadPos                  m_nBufSize

    */
        int leftCount = nBufSize_ - nReadPos_;
        if (leftCount > count)  // 读出数据连续
        {
            memcpy(pOutBuf, pBuf_+ nReadPos_, count);
            nReadPos_ += count;
            bEmpty_    = (nReadPos_ == nWritePos_);
            assert(nReadPos_ < nBufSize_);
            assert(nWritePos_ < nBufSize_);
            return count;
        }
        else //读出数据不连续
        {
            memcpy(pOutBuf, pBuf_+ nReadPos_, leftCount);
            //count-leftCount代表左半区未到写索引;等于读索引等于写索引则代表缓冲区空
            nReadPos_ = (nWritePos_ > count - leftCount) ? count - leftCount : nWritePos_;
            memcpy(pOutBuf+leftCount, pBuf_, nReadPos_);
            bEmpty_ = (nReadPos_ == nWritePos_);
            assert(nReadPos_ < nBufSize_);
            assert(nWritePos_ < nBufSize_);
            return leftCount + nReadPos_;
        }
    }
}

int circleBuffer::write(char* pInBuf, int count)
{
    if (count <= 0)
    {
        return 0;
    }
    bEmpty_ = false;
    if (bFull_)
    {
        return 0;
    }
    else if (nReadPos_ == nWritePos_) //缓冲区为空时
    {
    /*                            == 内存模型 ==
            (empty)                 m_nReadPos                (empty)
    |----------------------------------|-----------------------------------------|
                                    m_nWritePos                             m_nBufSize
    */
        int leftCount = nBufSize_ - nWritePos_;//计算可写入的空间;
        if (leftCount > count)
        {
            memcpy(pBuf_+nWritePos_, pInBuf, count);
            nWritePos_ += count;
            bFull_      = (nWritePos_==nReadPos_);
            return count;
        }
        else
        {
            memcpy(pBuf_+nWritePos_, pInBuf, leftCount);
            nWritePos_ += (nReadPos_ > count - leftCount) ? count - leftCount : leftCount;
            memcpy(pBuf_, pInBuf+leftCount, nWritePos_);
            bFull_      = (nWritePos_ == nReadPos_);
            return leftCount + nWritePos_;
        }
    }
    else if (nReadPos_ < nWritePos_) //有剩余空间，写索引在读索引前面
    {
    /*                           == 内存模型 ==
        (empty)                        (data)                            (empty)
        |-------------------|----------------------------|---------------------------|
                    m_nReadPos                m_nWritePos       (leftcount)
    */
        int leftCount = nBufSize_ - nWritePos_;
        if (leftCount > count)
        {
            memcpy(pBuf_+nWritePos_, pInBuf, count);
            nWritePos_ += count;
            bFull_      = (nWritePos_ == nReadPos_);
            assert(nReadPos_ < nBufSize_);
            assert(nWritePos_ < nBufSize_);
            return count;
        }
        else
        {
            memcpy(pBuf_+nWritePos_, pInBuf, leftCount);
            nWritePos_ = (nReadPos_ > count - leftCount) ? count- leftCount : nReadPos_;
            memcpy(pBuf_, pInBuf+leftCount, nWritePos_);
            bFull_     = (nWritePos_ == nReadPos_);
            assert(nReadPos_ < nBufSize_);
            assert(nWritePos_ < nBufSize_);
            return leftCount + nWritePos_;
        }
    }
    else
    {
    /*                          == 内存模型 ==
    (unread)                 (read)                     (unread)
    |-------------------|----------------------------|---------------------------|
                m_nWritePos        (leftcount)         m_nReadPos
    */
        int leftCount = nReadPos_ - nWritePos_;
        if (leftCount > count)
        {
            memcpy(pBuf_+nWritePos_, pInBuf, count);
            nWritePos_ += count;
            bFull_      = (nWritePos_ == nReadPos_);
            assert(nReadPos_ < nBufSize_);
            assert(nWritePos_ < nBufSize_);
            return count;
        }
        else//剩余空间不足，丢弃数据
        {
            memcpy(pBuf_+nWritePos_, pInBuf, leftCount);
            nWritePos_ += leftCount; 
            bFull_      = (nWritePos_ == nReadPos_);
            assert(bFull_);
            assert(nReadPos_ < nBufSize_);
            assert(nWritePos_ < nBufSize_);
            return leftCount + nWritePos_;
        }

    }
}
#endif 



