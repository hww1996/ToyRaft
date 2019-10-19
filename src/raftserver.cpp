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
        if (0 != ret) {
            response->set_sendbacktype(RaftServerMsg::RETRY);
            return sta;
        }
        switch (request->querytype()) {
            case ::ToyRaft::RaftClientMsg::APPEND:
                switch (state) {
                    case FOLLOWER: {
                        auto nodes = RaftConfig::getNodes();
                        response->set_sendbacktype(RaftServerMsg::REDIRECT);
                        ServerRedirectMsg sendRedirectMsg;
                        sendRedirectMsg.set_ip(nodes[currentLeaderId]->ip_);
                        sendRedirectMsg.set_port(nodes[currentLeaderId]->port_);
                        response->set_allocated_serverredirectmsg(&sendRedirectMsg);
                        return sta;
                    }
                    case CANDIDATE: {
                        response->set_sendbacktype(RaftServerMsg::RETRY);
                        return sta;
                    }
                    case LEADER: {
                        {
                            std::lock_guard<std::mutex> lock(GlobalMutex::requestMutex);
                            RaftServer::requestBuf.push_back(*request);
                        }
                        response->set_sendbacktype(RaftServerMsg::OK);
                        return sta;
                    }
                    default: {
                        response->set_sendbacktype(RaftServerMsg::UNKNOWN);
                        return sta;
                    }
                }
            case ::ToyRaft::RaftClientMsg::QUERY: {
                ServerQueryMsg serverQueryMsg;
                auto &clientQueryMsg = request->clientquerymsg();
                ret = RaftServer::getReadBuffer(serverQueryMsg, clientQueryMsg.startindex(), clientQueryMsg.endindex(),
                                                commitIndex);
                if (0 != ret) {
                    response->set_sendbacktype(RaftServerMsg::RETRY);
                    return sta;
                }
                response->set_sendbacktype(RaftServerMsg::OK);
                response->set_allocated_serverquerymsg(&serverQueryMsg);
            }
            default:
                response->set_sendbacktype(RaftServerMsg::UNKNOWN);
                break;
        }
        return sta;
    }

    std::deque<::ToyRaft::RaftClientMsg> RaftServer::requestBuf;
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
            while (!requestBuf.empty()) {
                auto Logs = requestBuf.front().clientappendmsg().appendlog();
                for (auto &Log : Logs) {
                    netLog.emplace_back(Log.c_str(), Log.size());
                }
                requestBuf.pop_front();
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

    int RaftServer::getReadBuffer(::ToyRaft::ServerQueryMsg &serverQueryMsg, int from, int to, int commit) {
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
                serverQueryMsg.add_appendlog(readBuffer[i].c_str(), readBuffer[i].size());
            }
        }
        return ret;
    }
} // namespace ToyRaft

