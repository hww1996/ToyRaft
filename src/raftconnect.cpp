//
// Created by hww1996 on 2019/10/6.
//

#include <memory>
#include <unordered_map>

#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "raftconnect.h"
#include "globalmutex.h"
#include "raftconfig.h"
#include "logger.h"

namespace ToyRaft {
    NetData::NetData(int64_t id, const ::ToyRaft::AllSend &allSend) : id_(id), buf_(allSend) {}

    std::deque<std::shared_ptr<::ToyRaft::NetData> > RaftNet::recvBuf;
    std::deque<std::shared_ptr<::ToyRaft::NetData> > RaftNet::sendBuf;

    RaftNet::RaftNet() {
        std::thread recvThread(RaftNet::realRecv);
        std::thread sendThread(RaftNet::realSend);
        recvThread.detach();
        sendThread.detach();
    }

    ::grpc::Status ServerRaftImpl::serverRaft(::grpc::ServerContext *context, const ::ToyRaft::AllSend *request,
                                              ::ToyRaft::ServerSendBack *response) {
        ::grpc::Status sta = ::grpc::Status::OK;
        {
            std::lock_guard<std::mutex> lock(::ToyRaft::GlobalMutex::recvBufMutex);
            ::ToyRaft::RaftNet::recvBuf.push_back(std::shared_ptr<::ToyRaft::NetData>(new NetData(-1, *request)));
        }
        response->set_num(0);
        return sta;
    }

    static void initInnerServer(int port) {
        std::string server_address = "0.0.0.0:";
        server_address += std::to_string(port);
        ::ToyRaft::ServerRaftImpl service;

        ::grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
        server->Wait();
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
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            {
                std::lock_guard<std::mutex> lock(::ToyRaft::GlobalMutex::sendBufMutex);
                if (sendBuf.empty()) {
                    continue;
                }
            }
            auto nowNodesConfig = RaftConfig::getNodes();
            while (true) {
                std::shared_ptr<NetData> netData = nullptr;
                {
                    std::lock_guard<std::mutex> lock(::ToyRaft::GlobalMutex::sendBufMutex);
                    if (sendBuf.empty()) {
                        break;
                    }
                    netData = sendBuf.front();
                    sendBuf.pop_front();
                }
                if (RaftConfig::getId() == netData->id_) {
                    continue;
                }
                if (nowNodesConfig.end() == nowNodesConfig.find(netData->id_)) {
                    continue;
                }
                std::string serverIPPort = nowNodesConfig[netData->id_]->innerIP_ + ":" +
                                           std::to_string(nowNodesConfig[netData->id_]->innerPort_);
                std::unique_ptr<::ToyRaft::SendAndReply::Stub> clientPtr(std::move(::ToyRaft::SendAndReply::NewStub(
                        grpc::CreateChannel(serverIPPort, grpc::InsecureChannelCredentials()))));
                ::ToyRaft::ServerSendBack sendBack;
                grpc::ClientContext context;
                ::grpc::Status sta = clientPtr->serverRaft(&context, netData->buf_, &sendBack);
                if (!sta.ok()) {
                    LOGDEBUG("error messge:%s", sta.error_message().c_str());
                } else {
                    LOGDEBUG("send OK");
                }
                clientPtr.reset(nullptr);
            }
        }
        return ret;
    }

    int RaftNet::realRecv() {
        int ret = 0;
        initInnerServer(RaftConfig::getInnerPort());
        return ret;
    }
} // namespace ToyRaft