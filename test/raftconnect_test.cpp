//
// Created by apple on 2019/10/23.
//

#include <iostream>
#include <string>

#include "raftconnect.h"
#include "raftconfig.h"
#include "logger.h"
#include "raft.pb.h"

int main(int argc, char **argv) {
    if (2 != argc) {
        std::cout << "usage: <script> <path to config>" << std::endl;
        exit(-1);
    }
    ToyRaft::RaftConfig c(argv[1]);
    ToyRaft::RaftNet rn;
    std::string buf;
    int id;
    while (true) {
        std::cin >> id;
        std::cin >> buf;
        if (id == 0) {
            break;
        }
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
        auto log = requestAppend.add_entries();
        log->set_term(-1);
        log->set_type(ToyRaft::RaftLog::APPEND);
        log->set_buf(buf);
        allSend.set_allocated_requestappend(&requestAppend);
        auto nodes = ToyRaft::RaftConfig::getNodes();
        for (auto nodeIt = nodes.begin(); nodeIt != nodes.end(); ++nodeIt) {
            ToyRaft::RaftNet::sendToNet(nodeIt->first, allSend);
        }
        int ret = 0;
        ToyRaft::AllSend result;
        while (0 != ToyRaft::RaftNet::recvFromNet(&result)) {
            auto entriesArray = result.requestappend().entries();
            for (int i = 0; i < entriesArray.size(); i++) {
                ToyRaft::LOGDEBUG("id:%d, index:%d, content:%s ", result.sendfrom(), i, entriesArray[i].buf().c_str());
            }
        }
    }
    ToyRaft::LOGDEBUG("I am exit.");
    return 0;
}