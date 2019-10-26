//
// Created by apple on 2019/10/23.
//

#include <iostream>
#include <string>
#include <thread>

#include "raftconnect.h"
#include "raftconfig.h"
#include "logger.h"
#include "raft.pb.h"

void threadFunc() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        ToyRaft::AllSend result;
        while (0 != ToyRaft::RaftNet::recvFromNet(&result)) {
            ToyRaft::RequestAppend requestAppendRecv;
            requestAppendRecv.ParseFromString(result.sendbuf());
            auto entriesArray = requestAppendRecv.entries();
            if (0 == entriesArray.size()) {
                ToyRaft::LOGNOTICE("id:%d, sendType:%d", result.sendfrom(), result.sendtype());
            }
            for (int i = 0; i < entriesArray.size(); i++) {
                ToyRaft::LOGNOTICE("id:%d, sendType:%d, index:%d, content:%s ", result.sendfrom(), result.sendtype(), i,
                                   entriesArray[i].buf().c_str());
            }
        }
    }
}

int main(int argc, char **argv) {
    if (2 != argc) {
        std::cout << "usage: <script> <path to config>" << std::endl;
        exit(-1);
    }
    ToyRaft::RaftConfig c(argv[1]);
    ToyRaft::RaftNet rn;
    std::thread t(threadFunc);
    t.detach();
    std::string buf;
    int getType = 0;
    while (true) {
        std::cin >> getType;
        if (std::cin.eof()) {
            break;
        }
        std::cin >> buf;
        if (std::cin.eof()) {
            break;
        }
        ToyRaft::AllSend allSend;
        allSend.set_sendfrom(ToyRaft::RaftConfig::getId());
        allSend.set_sendtype(ToyRaft::AllSend::REQAPPEND);
        ToyRaft::RequestAppend requestAppend;
        requestAppend.set_term(-1);
        requestAppend.set_currentleaderid(ToyRaft::RaftConfig::getId());
        requestAppend.set_prelogindex(-1);
        requestAppend.set_prelogterm(-1);
        requestAppend.set_leadercommit(-1);

        if (getType == 1) {
            ToyRaft::LOGDEBUG("don't need empty.");
            auto log = requestAppend.add_entries();
            log->set_term(-1);
            log->set_type(ToyRaft::RaftLog::APPEND);
            log->set_buf(buf);
        }

        std::string requestAppendBuf;
        requestAppend.SerializeToString(&requestAppendBuf);
        allSend.set_sendbuf(requestAppendBuf.c_str(), requestAppendBuf.size());

        auto nodes = ToyRaft::RaftConfig::getNodes();
        for (auto nodeIt = nodes.begin(); nodeIt != nodes.end(); ++nodeIt) {
            ToyRaft::RaftNet::sendToNet(nodeIt->first, allSend);
        }
    }
    ToyRaft::LOGDEBUG("I am exit.");
    return 0;
}