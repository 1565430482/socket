#include <log4cpp/PatternLayout.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/Priority.hh>
#include "log.hpp"

std::unique_ptr<Log> Log::plog_ = NULL;

std::unique_ptr<Log> Log::getInstance()
{
    return std::unique_ptr<Log>(new Log);
}

void Log::destory()
{
    if(plog_)
    {
        plog_->category_.info("destory");
        plog_->category_.shutdown();
    }
}

Log::Log() : category_(log4cpp::Category::getRoot())
{
    //自定义输出格式
    log4cpp::PatternLayout *pattern_one =
        new log4cpp::PatternLayout;
    pattern_one->setConversionPattern("%d: %p %c %x:%m%n");

    log4cpp::PatternLayout *pattern_two =
        new log4cpp::PatternLayout;
    pattern_two->setConversionPattern("%d: %p %c %x:%m%n");

    //获取屏幕输出
    log4cpp::OstreamAppender *os_appender = 
        new log4cpp::OstreamAppender("osAppender",&std::cout);
    os_appender->setLayout(pattern_one);

    //获取文件日志输出 
    log4cpp::FileAppender *file_appender = 
        new log4cpp::FileAppender("fileAppender","./server.log");
    file_appender->setLayout(pattern_two);

    //设置优先级
    category_.setPriority(log4cpp::Priority::DEBUG);
    category_.addAppender(os_appender);
    category_.addAppender(file_appender);

    category_.info("Log created!");
}

//设置优先级
void Log::setPriority(Priority priority) 
{
    switch (priority) 
    {
        case (Priority::ERROR):
            category_.setPriority(log4cpp::Priority::ERROR);
            break;

        case (Priority::WARN):
            category_.setPriority(log4cpp::Priority::WARN);
            break;

        case (Priority::INFO):
            category_.setPriority(log4cpp::Priority::INFO);
            break;

        case (Priority::DEBUG):
            category_.setPriority(log4cpp::Priority::DEBUG);
            break;

        default:
            category_.setPriority(log4cpp::Priority::DEBUG);
            break;    
    }
}

void Log::error(const char* msg) 
{
    category_.error(msg);
}

void Log::warn(const char* msg) 
{
    category_.warn(msg);
}

void Log::info(const char* msg) 
{
    category_.info(msg);
}

void Log::debug(const char* msg) 
{
    category_.debug(msg);
}

