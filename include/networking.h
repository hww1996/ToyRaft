//
// Created by apple on 2019/10/6.
//

#include <deque>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>

#include "raft.pb.h"

#ifndef TOYRAFT_NET_H
#define TOYRAFT_NET_H
namespace ToyRaft {
    struct NetData {
        int64_t id;
        std::string buf;
    };

    class RaftNet {
    public:
        RaftNet();

        // 从网络中获取数据
        static int sendToNet(int64_t, ::ToyRaft::AllSend &);

        static int recvFromNet(::ToyRaft::AllSend &);

    private:
        static int realSend();

        static int realRecv();

        static std::deque<::ToyRaft::NetData> recvBuf;
        static std::deque<::ToyRaft::NetData> sendBuf;
        static std::unordered_map<int, int> sendIdMapping;
        static std::thread recvThread;
        static std::thread sendThread;
        static std::mutex recvMutex;
        static std::mutex sendMutex;
    };
} // namespace ToyRaft
#endif //TOYRAFT_NET_H
