//
// Created by hww1996 on 2019/10/6.
//

#include "networking.h"

namespace ToyRaft {
    std::deque<::ToyRaft::NetData> RaftNet::recvBuf;
    std::deque<::ToyRaft::NetData> RaftNet::sendBuf;
    std::mutex RaftNet::recvMutex;
    std::mutex RaftNet::sendMutex;
    std::thread RaftNet::recvThread(RaftNet::realRecv);
    std::thread RaftNet::sendThread(RaftNet::realSend);


} // namespace ToyRaft