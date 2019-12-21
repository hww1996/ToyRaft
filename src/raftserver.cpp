//
// Created by hww1996 on 2019/10/13.
//
#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "raftserver.h"
#include "globalmutex.h"
#include "raft.h"
#include "raftconnect.h"
#include "logger.h"
#include "raftsave.h"

namespace ToyRaft {
    std::deque<::ToyRaft::RaftClientMsg> RaftServer::requestBuf;

    static void makeConfigData(std::unordered_map<int, std::shared_ptr<NodeConfig> > &config, std::string &data) {
        // 将config变成json，并放到append的log中。
        rapidjson::Value ans(rapidjson::kObjectType);
        rapidjson::Value raftNodes(rapidjson::kArrayType);
        rapidjson::Document document;
        rapidjson::Document::AllocatorType &alloc = document.GetAllocator();
        for (auto it = config.begin(); config.end() != it; ++it) {
            rapidjson::Value raftNodeItem(rapidjson::kObjectType);
            raftNodeItem.AddMember("id", rapidjson::Value().SetInt(it->first), alloc);
            raftNodeItem.AddMember("innerIP", rapidjson::Value().SetString(it->second->innerIP_.c_str(),
                                                                           it->second->innerIP_.size(), alloc), alloc);
            raftNodeItem.AddMember("innerPort", rapidjson::Value().SetInt(it->second->innerPort_), alloc);
            raftNodeItem.AddMember("outerIP", rapidjson::Value().SetString(it->second->outerIP_.c_str(),
                                                                           it->second->outerIP_.size(), alloc), alloc);
            raftNodeItem.AddMember("outerPort", rapidjson::Value().SetInt(it->second->outerPort_), alloc);
            raftNodes.PushBack(raftNodeItem, alloc);
        }
        ans.AddMember("nodes", raftNodes, alloc);
        rapidjson::StringBuffer buff;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buff);
        ans.Accept(writer);
        std::string tempJsonString = buff.GetString();
        data.assign(tempJsonString);
        LOGDEBUG("make the json OK.");
    }

    static int getStatus(int64_t &leaderId, Status &state, int64_t &commitIndex, bool &canVote) {
        std::string statusJson;
        int ret = RaftSave::getInstance()->getMeta(statusJson);
        if (0 != ret) {
            return ret;
        }
        rapidjson::Document doc;
        doc.Parse(statusJson.c_str(), statusJson.size());
        leaderId = doc["leaderId"].GetInt();
        state = Status(doc["status"].GetInt());
        commitIndex = doc["commitIndex"].GetInt();
        canVote = doc["canVote"].GetBool();
        return ret;
    }

    ::grpc::Status
    OuterServiceImpl::serverOutSide(::grpc::ServerContext *context, const ::ToyRaft::RaftClientMsg *request,
                                    ::ToyRaft::RaftServerMsg *response) {
        ::grpc::Status sta = ::grpc::Status::OK;
        int64_t currentLeaderId = -1;
        Status state = FOLLOWER;
        int64_t commitIndex = -1;
        bool canVote = false;
        int ret = getStatus(currentLeaderId, state, commitIndex, canVote);
        if (0 != ret) {
            response->set_sendbacktype(RaftServerMsg::RETRY);
            return sta;
        }
        switch (request->querytype()) {
            case ::ToyRaft::RaftClientMsg::APPEND: {
                switch (state) {
                    case FOLLOWER: {
                        auto nodes = RaftConfig::getNodes();
                        response->set_sendbacktype(RaftServerMsg::REDIRECT);
                        ServerRedirectMsg sendRedirectMsg;
                        if (-1 == currentLeaderId) {
                            sendRedirectMsg.set_ip(RaftConfig::getOuterIP());
                            sendRedirectMsg.set_port(RaftConfig::getOuterPort());
                        } else {
                            sendRedirectMsg.set_ip(nodes[currentLeaderId]->outerIP_);
                            sendRedirectMsg.set_port(nodes[currentLeaderId]->outerPort_);
                        }

                        LOGDEBUG("server ip:%s,port:%d", sendRedirectMsg.ip().c_str(), sendRedirectMsg.port());

                        std::string sendRedirectMsgBuf;
                        sendRedirectMsg.SerializeToString(&sendRedirectMsgBuf);
                        response->set_serverbuf(sendRedirectMsgBuf.c_str(), sendRedirectMsgBuf.size());
                        return sta;
                    }
                        break;
                    case CANDIDATE: {
                        response->set_sendbacktype(RaftServerMsg::RETRY);
                        return sta;
                    }
                        break;
                    case LEADER: {
                        LOGDEBUG("push to the requestBuf.");
                        {
                            std::lock_guard<std::mutex> lock(GlobalMutex::requestMutex);
                            RaftServer::requestBuf.push_back(*request);
                        }
                        response->set_sendbacktype(RaftServerMsg::OK);
                        return sta;
                    }
                        break;
                    default: {
                        response->set_sendbacktype(RaftServerMsg::UNKNOWN);
                        return sta;
                    }
                        break;
                }
            }
                break;
            case ::ToyRaft::RaftClientMsg::QUERY: {
                ClientQueryMsg clientQueryMsg;
                clientQueryMsg.ParseFromString(request->clientbuf());

                ServerQueryMsg serverQueryMsg;
                ret = RaftServer::getReadBuffer(serverQueryMsg, clientQueryMsg.startindex(), clientQueryMsg.endindex(),
                                                commitIndex);
                if (0 != ret) {
                    response->set_sendbacktype(RaftServerMsg::RETRY);
                    return sta;
                }

                response->set_sendbacktype(RaftServerMsg::OK);
                std::string serverQueryMsgBuf;
                serverQueryMsg.SerializeToString(&serverQueryMsgBuf);
                response->set_serverbuf(serverQueryMsgBuf.c_str(), serverQueryMsgBuf.size());
            }
                break;
            case ::ToyRaft::RaftClientMsg::MEMBER: {
                LOGDEBUG("in the member.");
                switch (state) {
                    case FOLLOWER: {
                        auto nodes = RaftConfig::getNodes();
                        response->set_sendbacktype(RaftServerMsg::REDIRECT);
                        ServerRedirectMsg sendRedirectMsg;
                        if (-1 == currentLeaderId) {
                            sendRedirectMsg.set_ip(RaftConfig::getOuterIP());
                            sendRedirectMsg.set_port(RaftConfig::getOuterPort());
                        } else {
                            sendRedirectMsg.set_ip(nodes[currentLeaderId]->outerIP_);
                            sendRedirectMsg.set_port(nodes[currentLeaderId]->outerPort_);
                        }

                        LOGDEBUG("server ip:%s,port:%d", sendRedirectMsg.ip().c_str(), sendRedirectMsg.port());

                        std::string sendRedirectMsgBuf;
                        sendRedirectMsg.SerializeToString(&sendRedirectMsgBuf);
                        response->set_serverbuf(sendRedirectMsgBuf.c_str(), sendRedirectMsgBuf.size());
                        return sta;
                    }
                        break;
                    case CANDIDATE: {
                        response->set_sendbacktype(RaftServerMsg::RETRY);
                        return sta;
                    }
                        break;
                    case LEADER: {
                        ClientMemberChangeMsg clientMemberChangeMsg;
                        clientMemberChangeMsg.ParseFromString(request->clientbuf());
                        auto configNodes = RaftConfig::getNodes();
                        auto changeId = clientMemberChangeMsg.id();
                        switch (clientMemberChangeMsg.memberchangetype()) {
                            case ClientMemberChangeMsg::ADD: {
                                LOGDEBUG("in the add.");
                                if (configNodes.find(changeId) != configNodes.end()) {
                                    response->set_sendbacktype(RaftServerMsg::UNKNOWN);
                                    return sta;
                                }
                                configNodes[changeId] = std::make_shared<NodeConfig>(changeId,
                                                                                     clientMemberChangeMsg.innerip(),
                                                                                     clientMemberChangeMsg.innerport(),
                                                                                     clientMemberChangeMsg.outerip(),
                                                                                     clientMemberChangeMsg.outerport());
                            }
                                break;
                            case ClientMemberChangeMsg::REMOVE: {
                                LOGDEBUG("in the remove.");
                                if (configNodes.find(changeId) == configNodes.end()) {
                                    response->set_sendbacktype(RaftServerMsg::UNKNOWN);
                                    return sta;
                                }
                                configNodes.erase(changeId);
                            }
                                break;
                            default: {
                                response->set_sendbacktype(RaftServerMsg::UNKNOWN);
                                return sta;
                            }
                                break;
                        }
                        LOGDEBUG("begin append.");
                        std::string serializeData;
                        makeConfigData(configNodes, serializeData);
                        ToyRaft::ClientAppendMsg clientAppendMsg;
                        clientAppendMsg.add_appendlog()->assign(serializeData.c_str(), serializeData.size());
                        std::string raftClientMsgBuf;
                        clientAppendMsg.SerializeToString(&raftClientMsgBuf);
                        RaftClientMsg raftClientMsg;
                        raftClientMsg.set_querytype(ToyRaft::RaftClientMsg::MEMBER);
                        raftClientMsg.set_clientbuf(raftClientMsgBuf.c_str(), raftClientMsgBuf.size());
                        LOGDEBUG("append OK.");
                        {
                            std::lock_guard<std::mutex> lock(GlobalMutex::requestMutex);
                            RaftServer::requestBuf.push_back(raftClientMsg);
                        }
                        response->set_sendbacktype(RaftServerMsg::OK);
                        return sta;
                    }
                        break;
                    default: {
                        response->set_sendbacktype(RaftServerMsg::UNKNOWN);
                        return sta;
                    }
                        break;
                }
            }
                break;
            case ::ToyRaft::RaftClientMsg::STATUS: {
                ServerInnerStatusMsg serverInnerStatusMsg;
                serverInnerStatusMsg.set_currentleaderid(currentLeaderId);
                serverInnerStatusMsg.set_commitindex(commitIndex);
                serverInnerStatusMsg.set_state(state);
                serverInnerStatusMsg.set_canvote(canVote);
                response->set_sendbacktype(RaftServerMsg::OK);
                std::string serverInnerStatusMsgBuf;
                serverInnerStatusMsg.SerializeToString(&serverInnerStatusMsgBuf);
                response->set_serverbuf(serverInnerStatusMsgBuf.c_str(), serverInnerStatusMsgBuf.size());
            }
                break;
            default:
                response->set_sendbacktype(RaftServerMsg::UNKNOWN);
                break;
        }
        return sta;
    }


    RaftServer::RaftServer(const std::string &raftConfigPath) {
        RaftConfig cs(raftConfigPath);
        RaftNet r;
        std::thread t(recvFromNet);
        t.detach();
    }

    int RaftServer::serverForever(bool newJoin) {
        int ret = 0;
        Raft raft(!newJoin);
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            ret = raft.tick();
            if (0 != ret) {
                break;
            }
        }
        return ret;
    }

    static void initOuterServer(const std::string &IP, int port) {
        std::string server_address = IP + ":";
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
        initOuterServer(RaftConfig::getOuterIP(), RaftConfig::getOuterPort());
        return ret;
    }

    int RaftServer::getNetLogs(std::vector<ToyRaft::RaftLog> &netLog) {
        int ret = 0;
        while (true) {
            ClientAppendMsg clientAppendMsg;
            RaftClientMsg::QueryType queryType;
            {
                std::lock_guard<std::mutex> lock(GlobalMutex::requestMutex);
                LOGDEBUG("requestBuf size:%d", requestBuf.size());
                if (requestBuf.empty()) {
                    break;
                }
                clientAppendMsg.ParseFromString(requestBuf.front().clientbuf());
                queryType = requestBuf.front().querytype();
                requestBuf.pop_front();
            }
            auto Logs = clientAppendMsg.appendlog();
            LOGDEBUG("log size:%d", requestBuf.size());
            for (auto &Log : Logs) {
                RaftLog raftLog;
                if (ToyRaft::RaftClientMsg::MEMBER == queryType) {
                    raftLog.set_type(::ToyRaft::RaftLog::MEMBER);
                } else {
                    raftLog.set_type(::ToyRaft::RaftLog::APPEND);
                }
                raftLog.set_buf(Log.c_str(), Log.size());
                netLog.push_back(raftLog);
            }
        }
        return ret;
    }

    int RaftServer::getReadBuffer(::ToyRaft::ServerQueryMsg &serverQueryMsg, int from, int to, int commit) {
        int ret = 0;
        if (from < 0) {
            return -1;
        }
        if (from >= to) {
            return -2;
        }
        if (to > commit + 1) {
            to = commit + 1;
        }
        std::vector<std::string> ans;
        ret = RaftSave::getInstance()->getData(from, to - from, ans);
        for (int i = 0; i < ans.size(); i++) {
            serverQueryMsg.add_appendlog(ans[i].c_str(), ans[i].size());
        }
        serverQueryMsg.set_commitindex(commit);
        return ret;
    }
} // namespace ToyRaft

