//
// Created by hww1996 on 2019/10/8.
//

#include "globalmutex.h"

namespace ToyRaft {

    std::mutex GlobalMutex::recvBufMutex;
    std::mutex GlobalMutex::sendBufMutex;
    std::mutex GlobalMutex::requestMutex;
    std::mutex GlobalMutex::raftSaveMutex;

} // namespace ToyRaft

