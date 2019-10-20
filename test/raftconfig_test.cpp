//
// Created by hww1996 on 2019/10/20.
//
#include <iostream>
#include <thread>
#include <chrono>

#include "raftconfig.h"

int main() {
    ToyRaft::RaftConfig r("../conf/raft.json");
    std::cout << "ID:" << ToyRaft::RaftConfig::getId() << std::endl;
    std::cout << "OuterPort:" << ToyRaft::RaftConfig::getOuterPort() << std::endl;
    std::cout << "InnerPort:" << ToyRaft::RaftConfig::getInnerPort() << std::endl;
    auto nodes = ToyRaft::RaftConfig::getNodes();
    for (auto It = nodes.begin(); nodes.end() != It; ++It) {
        std::cout << "Peers id:" << It->second->id_ << " ip:" << It->second->ip_ << " port:" << It->second->port_
                  << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::seconds(23));
    std::cout << "ID:" << ToyRaft::RaftConfig::getId() << std::endl;
    std::cout << "OuterPort:" << ToyRaft::RaftConfig::getOuterPort() << std::endl;
    std::cout << "InnerPort:" << ToyRaft::RaftConfig::getInnerPort() << std::endl;
    nodes = ToyRaft::RaftConfig::getNodes();
    for (auto It = nodes.begin(); nodes.end() != It; ++It) {
        std::cout << "Peers id:" << It->second->id_ << " ip:" << It->second->ip_ << " port:" << It->second->port_
                  << std::endl;
    }
    return 0;
}

