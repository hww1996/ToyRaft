//
// Created by apple on 2019/10/26.
//

#include <iostream>
#include <string>
#include <memory>

#include <grpc/grpc.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "logger.h"
#include "raftserver.pb.h"
#include "raftserver.grpc.pb.h"

void dealResponse(const ToyRaft::RaftServerMsg &raftServerMsg, std::string &sendName,
                  ToyRaft::RaftClientMsg::QueryType clientType) {
    switch (raftServerMsg.sendbacktype()) {
        case ToyRaft::RaftServerMsg::OK: {
            if (ToyRaft::RaftClientMsg::APPEND == clientType) {
                ToyRaft::LOGDEBUG("append the log OK.");
            } else if (ToyRaft::RaftClientMsg::QUERY == clientType) {
                ToyRaft::LOGDEBUG("query the log OK.");
                ToyRaft::ServerQueryMsg serverQueryMsg;
                serverQueryMsg.ParseFromString(raftServerMsg.serverbuf());
                ToyRaft::LOGDEBUG("get the raft commit:%d", serverQueryMsg.commitindex());
                auto &retLog = serverQueryMsg.appendlog();
                ToyRaft::LOGDEBUG("return log length is %d", retLog.size());
                for (int i = 0; i < retLog.size(); i++) {
                    ToyRaft::LOGDEBUG("index:%d.log content:%s", i, retLog[i].c_str());
                }
            } else if (ToyRaft::RaftClientMsg::MEMBER == clientType) {
                ToyRaft::LOGDEBUG("member change OK.");
            } else if (ToyRaft::RaftClientMsg::STATUS == clientType) {
                ToyRaft::ServerInnerStatusMsg serverInnerStatusMsg;
                serverInnerStatusMsg.ParseFromString(raftServerMsg.serverbuf());
                ToyRaft::LOGDEBUG("leaderId:%d,commitId:%d,state:%d,canvote:%d", serverInnerStatusMsg.currentleaderid(),
                                  serverInnerStatusMsg.commitindex(), serverInnerStatusMsg.state(),
                                  serverInnerStatusMsg.canvote());
            }
        }
            break;
        case ToyRaft::RaftServerMsg::REDIRECT: {

            ToyRaft::ServerRedirectMsg serverRedirectMsg;
            serverRedirectMsg.ParseFromString(raftServerMsg.serverbuf());
            std::string tempSendName = serverRedirectMsg.ip() + ":" + std::to_string(serverRedirectMsg.port());
            sendName.assign(tempSendName);
            ToyRaft::LOGDEBUG("meet redirect.now sendname: %s", sendName.c_str());
        }
            break;
        case ToyRaft::RaftServerMsg::RETRY: {
            ToyRaft::LOGDEBUG("meet retry.");
        }
            break;
        case ToyRaft::RaftServerMsg::UNKNOWN: {
            ToyRaft::LOGDEBUG("meet unknow.");
        }
            break;
        default:
            break;
    }
}

int main() {
    int sendType = 0;
    std::string sendName = "0.0.0.0:20087";
    while (true) {
        ToyRaft::LOGDEBUG("now ip:%s", sendName.c_str());
        if (std::cin.eof()) {
            break;
        }
        std::cin >> sendType;
        if (std::cin.eof()) {
            exit(0);
        }
        switch (sendType) {
            case 0: { // append
                std::cout << "append" << std::endl;
                int readSize;
                std::cin >> readSize;
                ToyRaft::ClientAppendMsg clientAppendMsg;
                for (int i = 0; i < readSize; i++) {
                    std::cin >> *clientAppendMsg.add_appendlog();
                    if (std::cin.eof()) {
                        exit(0);
                    }
                }

                ToyRaft::RaftClientMsg raftClientMsg;
                raftClientMsg.set_querytype(ToyRaft::RaftClientMsg::APPEND);
                std::string raftClientMsgBuf;
                clientAppendMsg.SerializeToString(&raftClientMsgBuf);
                raftClientMsg.set_clientbuf(raftClientMsgBuf.c_str(), raftClientMsgBuf.size());
                ToyRaft::RaftServerMsg raftServerMsg;
                std::unique_ptr<ToyRaft::OutSideService::Stub> clientPtr(std::move(ToyRaft::OutSideService::NewStub(
                        grpc::CreateChannel(sendName, grpc::InsecureChannelCredentials()))));
                grpc::ClientContext context;
                clientPtr->serverOutSide(&context, raftClientMsg, &raftServerMsg);
                clientPtr.reset(nullptr);
                dealResponse(raftServerMsg, sendName, ToyRaft::RaftClientMsg::APPEND);
            }
                break;
            case 1: { // query
                std::cout << "query" << std::endl;
                int start, end;
                std::cin >> start >> end;
                if (std::cin.eof()) {
                    exit(0);
                }
                ToyRaft::ClientQueryMsg clientQueryMsg;
                clientQueryMsg.set_startindex(start);
                clientQueryMsg.set_endindex(end);
                ToyRaft::RaftClientMsg raftClientMsg;
                raftClientMsg.set_querytype(ToyRaft::RaftClientMsg::QUERY);
                std::string raftClientMsgBuf;
                clientQueryMsg.SerializeToString(&raftClientMsgBuf);
                raftClientMsg.set_clientbuf(raftClientMsgBuf.c_str(), raftClientMsgBuf.size());
                ToyRaft::RaftServerMsg raftServerMsg;
                std::unique_ptr<ToyRaft::OutSideService::Stub> clientPtr(std::move(ToyRaft::OutSideService::NewStub(
                        grpc::CreateChannel(sendName, grpc::InsecureChannelCredentials()))));
                grpc::ClientContext context;
                clientPtr->serverOutSide(&context, raftClientMsg, &raftServerMsg);
                clientPtr.reset(nullptr);
                dealResponse(raftServerMsg, sendName, ToyRaft::RaftClientMsg::QUERY);
            }
                break;
            case 2: { // member change
                std::cout << "member change" << std::endl;
                int memberChangeType, id; // 0:Add 1:Remove
                std::cin >> memberChangeType >> id;
                if (std::cin.eof()) {
                    exit(0);
                }
                ToyRaft::ClientMemberChangeMsg clientMemberChangeMsg;
                if (0 == memberChangeType) {
                    std::cout << "add" << std::endl;
                    clientMemberChangeMsg.set_memberchangetype(ToyRaft::ClientMemberChangeMsg::ADD);
                    clientMemberChangeMsg.set_id(id);
                    std::string innerIP, outerIP;
                    int innerPort, outerPort;
                    std::cin >> innerIP >> outerIP;
                    if (std::cin.eof()) {
                        exit(0);
                    }
                    std::cin >> innerPort >> outerPort;
                    if (std::cin.eof()) {
                        exit(0);
                    }
                    clientMemberChangeMsg.set_innerip(innerIP);
                    clientMemberChangeMsg.set_innerport(innerPort);
                    clientMemberChangeMsg.set_outerip(outerIP);
                    clientMemberChangeMsg.set_outerport(outerPort);
                } else if (1 == memberChangeType) {
                    std::cout << "remove" << std::endl;
                    clientMemberChangeMsg.set_memberchangetype(ToyRaft::ClientMemberChangeMsg::REMOVE);
                    clientMemberChangeMsg.set_id(id);
                }
                ToyRaft::RaftClientMsg raftClientMsg;
                raftClientMsg.set_querytype(ToyRaft::RaftClientMsg::MEMBER);
                std::string raftClientMsgBuf;
                clientMemberChangeMsg.SerializeToString(&raftClientMsgBuf);
                raftClientMsg.set_clientbuf(raftClientMsgBuf.c_str(), raftClientMsgBuf.size());
                ToyRaft::RaftServerMsg raftServerMsg;
                std::unique_ptr<ToyRaft::OutSideService::Stub> clientPtr(std::move(ToyRaft::OutSideService::NewStub(
                        grpc::CreateChannel(sendName, grpc::InsecureChannelCredentials()))));
                grpc::ClientContext context;
                clientPtr->serverOutSide(&context, raftClientMsg, &raftServerMsg);
                clientPtr.reset(nullptr);
                dealResponse(raftServerMsg, sendName, ToyRaft::RaftClientMsg::MEMBER);
            }
                break;
            case 3: { // status query
                std::cout << "status query" << std::endl;
                ToyRaft::ClientQueryInnerStatusMsg clientQueryInnerStatusMsg;
                clientQueryInnerStatusMsg.set_id(0);
                ToyRaft::RaftClientMsg raftClientMsg;
                raftClientMsg.set_querytype(ToyRaft::RaftClientMsg::STATUS);
                std::string raftClientMsgBuf;
                clientQueryInnerStatusMsg.SerializeToString(&raftClientMsgBuf);
                raftClientMsg.set_clientbuf(raftClientMsgBuf.c_str(), raftClientMsgBuf.size());
                ToyRaft::RaftServerMsg raftServerMsg;
                std::unique_ptr<ToyRaft::OutSideService::Stub> clientPtr(std::move(ToyRaft::OutSideService::NewStub(
                        grpc::CreateChannel(sendName, grpc::InsecureChannelCredentials()))));
                grpc::ClientContext context;
                clientPtr->serverOutSide(&context, raftClientMsg, &raftServerMsg);
                clientPtr.reset(nullptr);
                dealResponse(raftServerMsg, sendName, ToyRaft::RaftClientMsg::STATUS);
            }
                break;
            default:
                break;
        }
    }
}