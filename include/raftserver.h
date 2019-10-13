//
// Created by apple on 2019/10/12.
//

#ifndef TOYRAFT_RAFTSERVER_H
#define TOYRAFT_RAFTSERVER_H

#include <deque>
#include <string>

#include "globalmutext.h"
#include "config.h"
#include "raftserver.pb.h"

namespace ToyRaft {
    class RaftServer {
    public:
        RaftServer(const std::string &nodesConfigPath, const std::string &serverConfigPath);

        int serverForever();

        static int recvFromNet();

        static int pushReadBuffer(int start, int commit, const std::vector<::ToyRaft::RaftLog> &log);

    private:
        static std::string NodesConfigPath_;
        static std::string ServerConfigPath_;
        static std::deque<::ToyRaft::RaftClientMsg> request;
        static std::vector<std::string> ReadBuffer;
    };
} // namespace ToyRaft

#endif //TOYRAFT_RAFTSERVER_H
