//
// Created by hww1996 on 2019/10/26.
//

#include <iostream>

#include "raftserver.h"

int main(int argc, char **argv) {
    if (2 != argc) {
        std::cout << "usage: <script> <path to config>" << std::endl;
        exit(-1);
    }
    ToyRaft::RaftServer raftServer(argv[1]);
    raftServer.serverForever();
    return 0;
}