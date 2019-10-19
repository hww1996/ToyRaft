//
// Created by hww1996 on 2019/10/19.
//

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <string>
#include <cstring>

#include <unistd.h>

#include "logger.h"

namespace ToyRaft {
    void Logger::LogLevel(int level, const char *fmt, va_list args) {
        std::string hit;
        switch (level) {
            case 0:
                return;
            case 1:
                hit = "ERR";
                break;
            case 2:
                hit = "WARNING";
                break;
            case 3:
                hit = "NOTICE";
                break;
            case 4:
                hit = "DEBUG";
                break;
            case 5:
                hit = "INFO";
                break;
            default:
                hit = "INFO";
        }
        char buf[8192] = {0};
        vsnprintf(buf, 8191, fmt, args);
        struct tm info;
        time_t nowTime = time(NULL);
        localtime_r(&nowTime, &info);
        buf[8191] = '\0';
        char pbuf[10240] = {0};
        snprintf(pbuf, 10239, "[%d %d %d %d:%d:%d] [%s] [%s][%s][%d]: %s", info.tm_year + 1900, info.tm_mon + 1,
                 info.tm_mday, info.tm_hour, info.tm_min, info.tm_sec, hit.c_str(), __FILE__, __FUNCTION__, __LINE__,
                 buf);
        pbuf[10239] = '\0';
        write(STDOUT_FILENO, pbuf, strlen(pbuf));
    }
    void Logger::LogError(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        LogLevel(1, fmt, args);
        va_end(args);
    }
    void Logger::LogWarning(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        LogLevel(2, fmt, args);
        va_end(args);
    }
    void Logger::LogNotice(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        LogLevel(3, fmt, args);
        va_end(args);
    }
    void Logger::LogDebug(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        LogLevel(4, fmt, args);
        va_end(args);
    }
    void Logger::LogInfo(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        LogLevel(5, fmt, args);
        va_end(args);
    }
}
