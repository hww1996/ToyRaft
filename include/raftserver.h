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

        int recvFromNet();

        static ::ToyRaft::NodesConfig &getNodesConfig();
        static ::ToyRaft::ServerConfig &getServerConfig();

    private:
        static ::ToyRaft::NodesConfig nodesConfig;
        static std::string NodesConfigPath_;

        static ::ToyRaft::ServerConfig serverConfig;
        static std::string ServerConfigPath_;

        static std::deque<::ToyRaft::RaftClientMsg> request;
        static std::deque<::ToyRaft::RaftServerMsg> response;
    };
} // namespace ToyRaft

#endif //TOYRAFT_RAFTSERVER_H
