//
// Created by hww1996 on 2019/10/13.
//
#include "raftserver.h"
#include "globalmutext.h"
#include "raft.h"
#include "raftconnect.h"

namespace ToyRaft {
    ::grpc::Status
    OuterServiceImpl::serverOutSide(::grpc::ServerContext *context, const ::ToyRaft::RaftClientMsg *request,
                                    ::ToyRaft::RaftServerMsg *response) {
        ::grpc::Status sta = ::grpc::Status::OK;
        int64_t currentLeaderId = -1;
        Status state = FOLLOWER;
        int64_t commitIndex = -1;
        int ret = OuterRaftStatus::get(currentLeaderId, state, commitIndex);

        switch (request->querytype()) {
            case ::ToyRaft::RaftClientMsg::APPEND:
                switch (state) {
                    case FOLLOWER:
                        auto nodes = RaftConfig::getNodes();
                        response->set_sendbacktype(RaftServerMsg::REDIRECT);
                        ServerRedirectMsg sendRedirectMsg;
                        sendRedirectMsg.set_ip(nodes[currentLeaderId]->ip_);
                        sendRedirectMsg.set_port(nodes[currentLeaderId]->port_);
                        response->set_allocated_serverredirectmsg(&sendRedirectMsg);
                        return sta;
                    case CANDIDATE:
                        response->set_sendbacktype(RaftServerMsg::RETRY);
                        return sta;
                    case LEADER:
                        auto needAppendLog = request->clientappendmsg().appendlog();
                        for (auto &i : needAppendLog) {
                            {
                                std::lock_guard<std::mutex> lock(GlobalMutex::requestMutex);
                                RaftServer::request.emplace_back(i.c_str(), i.size());
                            }
                        }
                        response->set_sendbacktype(RaftServerMsg::OK);
                        return sta;
                    default:
                        response->set_sendbacktype(RaftServerMsg::UNKNOWN);
                        return sta;
                }
            case ::ToyRaft::RaftClientMsg::QUERY:
                break;
            default:
                response->set_sendbacktype(RaftServerMsg::UNKNOWN);
                return sta;
        }
        return sta;
    }

    std::deque<::ToyRaft::RaftClientMsg> RaftServer::request;
    std::vector<std::string> RaftServer::readBuffer;

    RaftServer::RaftServer(const std::string &raftConfigPath) : raftConfigPath_(raftConfigPath) {
        RaftConfig cs(raftConfigPath);
        RaftNet r;
        std::thread t(recvFromNet);
        t.detach();
    }

    static void initOuterServer(int port) {
        std::string server_address = "0.0.0.0:";
        server_address += std::to_string(port);
        ::ToyRaft::OuterServiceImpl service;

        ::grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
        server->Wait();
    }

    int RaftServer::recvFromNet() {
        int ret = 0;
        initOuterServer(RaftConfig::getOuterPort());
        return ret;
    }

    int RaftServer::getNetLogs(std::vector<std::string> &netLog) {
        int ret = 0;
        {
            std::lock_guard<std::mutex> lock(GlobalMutex::requestMutex);
            while (!request.empty()) {
                auto Logs = request.front().clientappendmsg().appendlog();
                for (auto &Log : Logs) {
                    netLog.emplace_back(Log.c_str(), Log.size());
                }
                request.pop_front();
            }
        }
        return ret;
    }

    int RaftServer::serverForever() {
        int ret = 0;
        Raft raft(raftConfigPath_);
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ret = raft.tick();
            if (ret != 0) {
                break;
            }
        }
        return ret;
    }

    int RaftServer::pushReadBuffer(const std::vector<::ToyRaft::RaftLog> &log) {
        int ret = 0;
        {
            std::lock_guard<std::mutex> lock(GlobalMutex::readBufferMutex);
            int index = 0;
            for (; index < readBuffer.size(); index++) {
                readBuffer[index].assign(log[index].buf().c_str(), log[index].buf().size());
            }
            for (; index < log.size(); index++) {
                readBuffer.emplace_back(log[index].buf().c_str(), log[index].buf().size());
            }
        }
        return ret;
    }

    int RaftServer::getReadBuffer(std::vector<std::string> &buf, int from, int to, int commit) {
        int ret = 0;
        {
            std::lock_guard<std::mutex> lock(GlobalMutex::readBufferMutex);
            if (from < 0) {
                return -1;
            }
            if (from >= to) {
                return -2;
            }
            if (to > commit + 1) {
                to = commit + 1;
            }
            for (int i = from; i < to; i++) {
                buf.emplace_back(readBuffer[i].c_str(), readBuffer[i].size());
            }
        }
        return ret;
    }
} // namespace ToyRaft

