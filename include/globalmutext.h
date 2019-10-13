//
// Created by hww1996 on 2019/10/8.
//

#include <mutex>

#ifndef TOYRAFT_GLOBALMUTEXT_H
#define TOYRAFT_GLOBALMUTEXT_H

namespace ToyRaft{
    struct GlobalMutex {
        static std::mutex recvBufMutex;
        static std::mutex sendBufMutex;
        static std::mutex requestMutex;
        static std::mutex readBufferMutex;
    };
}// namespace ToyRaft
#endif //TOYRAFT_GLOBALMUTEXT_H
