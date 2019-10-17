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
#include "raftserver.grpc.pb.h"

namespace ToyRaft {
    class OuterServiceImpl : public ::ToyRaft::OutSideService::Service {
        ::grpc::Status serverOutSide(::grpc::ServerContext *context, const ::ToyRaft::RaftClientMsg *request,
                                     ::ToyRaft::RaftServerMsg *response);
    };

    class RaftServer {
    public:
        RaftServer(const std::string &nodesConfigPath, const std::string &serverConfigPath);

        int serverForever();

    private:

        static int recvFromNet();

        static int getNetLogs(std::vector<std::string> &netLog);

        static int pushReadBuffer(const std::vector<::ToyRaft::RaftLog> &log);

        /**
         * [from,to)
         * @param buf
         * @param from
         * @param to
         * @return
         */
        static int getReadBuffer(std::vector<std::string> &buf, int from, int to, int commit);

        static std::string nodesConfigPath_;
        static std::string serverConfigPath_;
        static std::deque<::ToyRaft::RaftClientMsg> request;
        static std::vector<std::string> readBuffer;
        friend class Raft;
        friend class OuterServiceImpl;
    };
} // namespace ToyRaft

#endif //TOYRAFT_RAFTSERVER_H
