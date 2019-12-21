//
// Created by hww1996 on 2019/10/12.
//

#include <deque>
#include <string>
#include <vector>

#include "globalmutex.h"
#include "raftconfig.h"
#include "raftserver.pb.h"
#include "raft.pb.h"
#include "raftserver.grpc.pb.h"

#ifndef TOYRAFT_RAFTSERVER_H
#define TOYRAFT_RAFTSERVER_H

namespace ToyRaft {
    class OuterServiceImpl : public ::ToyRaft::OutSideService::Service {
    public:
        ::grpc::Status serverOutSide(::grpc::ServerContext *context, const ::ToyRaft::RaftClientMsg *request,
                                     ::ToyRaft::RaftServerMsg *response);
    };

    class RaftServer {
    public:
        RaftServer(const std::string &raftConfigPath);

        int serverForever(bool newJoin = false);

    private:

        static int recvFromNet();

        static int getNetLogs(std::vector<ToyRaft::RaftLog> &netLog);

        static int getReadBuffer(::ToyRaft::ServerQueryMsg &serverQueryMsg, int from, int to, int commit);

        static std::deque<::ToyRaft::RaftClientMsg> requestBuf;

        friend class Raft;

        friend class OuterServiceImpl;
    };
} // namespace ToyRaft

#endif //TOYRAFT_RAFTSERVER_H
