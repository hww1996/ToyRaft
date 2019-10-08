//
// Created by hww1996 on 2019/10/6.
//

#include "networking.h"
#include "globalmutext.h"
#include "config.h"

namespace ToyRaft {
    NetData::NetData(int64_t id, const ::ToyRaft::AllSend &allSend) : id_(id), buf_(allSend){}

    std::deque<std::shared_ptr<::ToyRaft::NetData> > RaftNet::recvBuf;
    std::deque<std::shared_ptr<::ToyRaft::NetData> > RaftNet::sendBuf;
    std::thread RaftNet::recvThread(RaftNet::realRecv);
    std::thread RaftNet::sendThread(RaftNet::realSend);

    std::string RaftNet::nodesConfigPath_;
    std::string RaftNet::serverConfigPath_;

    RaftNet::RaftNet(const std::string &nodesConfigPath, const std::string &serverConfigPath) {
        nodesConfigPath_ = nodesConfigPath;
        serverConfigPath_ = serverConfigPath;
        recvThread.detach();
        sendThread.detach();
    }

    int RaftNet::sendToNet(int64_t id, ::ToyRaft::AllSend &allSend) {
        int ret = 0;
        std::shared_ptr<NetData> netData = std::shared_ptr<NetData>(new NetData(id, allSend));
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
                *allSend = recvBuf.front()->buf_;
                recvBuf.pop_front();
            }
        }
        return ret;
    }

    int RaftNet::realSend() {
        int ret = 0;
        if (0 == nodesConfigPath_.size()) {
            exit(-1);
        }
        auto nodesConfig = NodesConfig(nodesConfigPath_);
        while (true) {
            std::this_thread::sleep_for(1S);
            ret = nodesConfig.loadConfig();
            if (0 != ret) {
                break;
            }
            auto nowNodesConfig = nodesConfig.get();
            std::shared_ptr<NetData> netData = nullptr;
            {
                std::lock_guard<std::mutex> lock(::ToyRaft::GlobalMutex::sendBufMutex);
                netData = sendBuf.front();
                sendBuf.pop_front();
            }
            //TODO: 关闭连接和开启连接
        }
        return ret;
    }
} // namespace ToyRaft