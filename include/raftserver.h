//
// Created by apple on 2019/10/12.
//

#ifndef TOYRAFT_RAFTSERVER_H
#define TOYRAFT_RAFTSERVER_H

#include <deque>
#include <string>
#include <vector>

#include "globalmutext.h"
#include "config.h"
#include "raftserver.pb.h"
#include "raft.pb.h"

namespace ToyRaft {
    class RaftServer {
    public:
        RaftServer(const std::string &nodesConfigPath, const std::string &serverConfigPath);

        int serverForever();

        static int recvFromNet(std::vector<std::string> &netLog);

        static int pushReadBuffer(int start, int commit, const std::vector<::ToyRaft::RaftLog> &log);

        /**
         * [from,to)
         * @param buf
         * @param from
         * @param to
         * @return
         */
        static int getReadBuffer(std::vector<std::string> &buf, int from, int to);

    private:
        static std::string nodesConfigPath_;
        static std::string serverConfigPath_;
        static std::deque<::ToyRaft::RaftClientMsg> request;
        static std::vector<std::string> readBuffer;
        static int commit_;
    };
} // namespace ToyRaft

#endif //TOYRAFT_RAFTSERVER_H
