//
// Created by hww1996 on 2019/10/19.
//

#include <cstdarg>

#ifndef TOYRAFT_LOGGER_H
#define TOYRAFT_LOGGER_H
namespace ToyRaft {
    class Logger {
    public:
        static void LogLevel(int level, const char *fileName, const char *functionName, int line, const char *fmt, ...);
    };

#define LOGERROR(fmt, args...) Logger::LogLevel(1, __FILE__, __FUNCTION__, __LINE__, fmt, ##args)
#define LOGWARING(fmt, args...) Logger::LogLevel(2, __FILE__, __FUNCTION__, __LINE__, fmt, ##args)
#define LOGNOTICE(fmt, args...) Logger::LogLevel(3, __FILE__, __FUNCTION__, __LINE__, fmt, ##args)
#define LOGDEBUG(fmt, args...) Logger::LogLevel(4, __FILE__, __FUNCTION__, __LINE__, fmt, ##args)
#define LOGINFO(fmt, args...) Logger::LogLevel(5, __FILE__, __FUNCTION__, __LINE__, fmt, ##args)

} // namespace ToyRaft
#endif //TOYRAFT_LOGGER_H
