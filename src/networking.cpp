//
// Created by hww1996 on 2019/10/6.
//

#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "networking.h"
#include "globalmutext.h"
#include "config.h"

namespace ToyRaft {
    NetData::NetData(int64_t id, const ::ToyRaft::AllSend &allSend) : id_(id), buf_(allSend) {}

    std::deque<std::shared_ptr<::ToyRaft::NetData> > RaftNet::recvBuf;
    std::deque<std::shared_ptr<::ToyRaft::NetData> > RaftNet::sendBuf;
    std::thread RaftNet::recvThread(RaftNet::realRecv);
    std::thread RaftNet::sendThread(RaftNet::realSend);

    std::unordered_map<int, std::unique_ptr<::ToyRaft::SendAndReply::Stub>> RaftNet::sendIdMapping;

    std::string RaftNet::nodesConfigPath_;
    std::string RaftNet::serverConfigPath_;

    RaftNet::RaftNet(const std::string &nodesConfigPath, const std::string &serverConfigPath) {
        nodesConfigPath_ = nodesConfigPath;
        serverConfigPath_ = serverConfigPath;
        recvThread.detach();
        sendThread.detach();
    }

    static int inertConnectPool(std::unordered_map<int, std::unique_ptr<::ToyRaft::SendAndReply::Stub> > &sendMap,
                                std::unordered_map<int, std::shared_ptr<NodeConfig> > &nodesConfig, int id) {
        int ret = 0;
        for (auto nodeIt = nodesConfig.begin(); nodesConfig.end() != nodeIt; ++nodeIt) {
            if (nodeIt->first == id) {
                continue;
            }
            if (sendMap.end() == sendMap.find(nodeIt->first)) {
                std::string serverIPPort = nodeIt->second->ip_ + std::to_string(nodeIt->second->port_);
                sendMap[nodeIt->first] = std::move(::ToyRaft::SendAndReply::NewStub(
                        grpc::CreateChannel(serverIPPort, grpc::InsecureChannelCredentials())));
            }
        }
        for (auto sendMapIt = sendMap.begin(); sendMap.end() != sendMapIt;) {
            if (nodesConfig.find(sendMapIt->first) == nodesConfig.end()) {
                sendMap.erase(sendMapIt++);
                continue;
            }
            ++sendMapIt;
        }
        return ret;
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
        auto serverConfig = ServerConfig(serverConfigPath_);
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ret = nodesConfig.loadConfig();
            if (0 != ret) {
                break;
            }
            auto nowNodesConfig = nodesConfig.get();
            inertConnectPool(sendIdMapping, nowNodesConfig, serverConfig.getId());
            std::shared_ptr<NetData> netData = nullptr;
            {
                std::lock_guard<std::mutex> lock(::ToyRaft::GlobalMutex::sendBufMutex);
                netData = sendBuf.front();
                sendBuf.pop_front();
            }

            if (sendIdMapping.end() == sendIdMapping.find(netData->id_)) {
                continue;
            }
            ::ToyRaft::ServerSendBack sendBack;
            grpc::ClientContext context;
            sendIdMapping[netData->id_]->serverRaft(&context, netData->buf_, &sendBack);
        }
        return ret;
    }
} // namespace ToyRaft