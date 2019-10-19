//
// Created by hww1996 on 2019/10/19.
//

#include <cstdarg>

#ifndef TOYRAFT_LOGGER_H
#define TOYRAFT_LOGGER_H
namespace ToyRaft {
    class Logger {
    public:
        static void LogError(const char *fmt, ...);

        static void LogWarning(const char *fmt, ...);

        static void LogNotice(const char *fmt, ...);

        static void LogDebug(const char *fmt, ...);

        static void LogInfo(const char *fmt, ...);

    private:
        static void LogLevel(int level, const char *fmt, va_list args);
    };
} // namespace ToyRaft
#endif //TOYRAFT_LOGGER_H
