//
// Created by hww1996 on 2019/10/6.
//

#include "networking.h"
#include "globalmutext.h"

namespace ToyRaft {
    NetData::NetData(int64_t id, const ::ToyRaft::AllSend &allSend) : id_(id), buf_(allSend){}

    std::deque<::ToyRaft::NetData> RaftNet::recvBuf;
    std::deque<::ToyRaft::NetData> RaftNet::sendBuf;
    std::thread RaftNet::recvThread(RaftNet::realRecv);
    std::thread RaftNet::sendThread(RaftNet::realSend);

    RaftNet::RaftNet() {
        recvThread.detach();
        sendThread.detach();
    }

    int RaftNet::sendToNet(int64_t id, ::ToyRaft::AllSend &allSend) {
        int ret = 0;
        NetData netData(id, allSend);
        {
            std::lock_guard<std::mutex> lock(::ToyRaft::GlobalMutex::sendBufMutex);
            sendBuf.push_back(netData);
        }
        return ret;
    }

    int RaftNet::recvFromNet(::ToyRaft::AllSend *allSend) {
        int ret = 0;
        {
            std::lock_guard<std::mutex> lock(::ToyRaft::GlobalMutex::recvBufMutex);
            ret = recvBuf.size();
            if (0 != ret) {
                *allSend = recvBuf.front().buf_;
                recvBuf.pop_front();
            }
        }
        return ret;
    }
} // namespace ToyRaft