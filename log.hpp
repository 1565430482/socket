#ifndef LOG_H
#define LOG_H

#include <log4cpp/Category.hh>
#include <iostream>
#include <memory>

enum class Priority 
{
    ERROR,
    WARN,
    INFO,
    DEBUG
};

class Log  //单例模式
{
public:
    static std::unique_ptr<Log> getInstance();
    static void destory();

    void setPriority(Priority priority);//设置优先级
    void error(const char* msg);//打印不同等级日志
    void warn(const char* msg);
    void info(const char* msg);
    void debug(const char* msg);
protected:
    Log();
private:
    static std::unique_ptr<Log> plog_;
    log4cpp::Category& category_;
}; 

#ifdef _LOG4CPP_
 //   Log& log = Log::getInstance(); 
    std::unique_ptr<Log> LOG = Log::getInstance();
#endif // DEBUG

#endif