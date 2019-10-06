//
// Created by apple on 2019/10/6.
//

#include <deque>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <memory>

#include "raft.pb.h"
#include "raft.grpc.pb.h"

#ifndef TOYRAFT_NET_H
#define TOYRAFT_NET_H
namespace ToyRaft {
    struct NetData {
        int64_t id_;
        ::ToyRaft::AllSend buf_;
        NetData(int64_t id,const ::ToyRaft::AllSend &allSend);
    };

    class RaftNet {
    public:
        RaftNet();
        // 从网络中获取数据
        static int sendToNet(int64_t, ::ToyRaft::AllSend &);

        static int recvFromNet(::ToyRaft::AllSend *);

    private:
        static int realSend();

        static int realRecv();

        static std::deque<::ToyRaft::NetData> recvBuf;
        static std::deque<::ToyRaft::NetData> sendBuf;
        static std::unordered_map<int,
                std::unique_ptr<::ToyRaft::SendAndReply::Stub>> sendIdMapping;
        static std::thread recvThread;
        static std::thread sendThread;
        static std::mutex recvMutex;
        static std::mutex sendMutex;
    };
} // namespace ToyRaft
#endif //TOYRAFT_NET_H
