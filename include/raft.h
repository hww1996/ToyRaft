//
// Created by hww1996 on 2019/9/28.
//
#include <vector>
#include <memory>
#include <unordered_map>
#include "raft.pb.h"

#ifndef TOYRAFT_RAFT_H

namespace ToyRaft {
    enum Status {
        FOLLOWER,
        CANDIDATE,
        LEADER,
    };

    class Raft {
    public:
        int tick();

        // 从网络中获取数据
        int send(std::shared_ptr<AllSend>);

        int recvFromNet(::ToyRaft::AllSend &);

        int recv();

    private:
        // 构造vote请求
        std::shared_ptr<AllSend> constructRequestVote();

        // 构造append请求
        std::shared_ptr<AllSend> constructRequestAppend();

        // 状态改变
        int becomeLeader();

        int becomeFollower();

        int becomeFollower(int64_t term, int64_t voteFor);

        int becomeCandidate();

        // 处理数据
        int handleRequestVote(::ToyRaft::RequestVote&);

        int handleRequestVoteResponse(::ToyRaft::RequestVoteResponse&);

        int handleRequestAppend(::ToyRaft::RequestAppend&);

        int handleRequestAppendResponse(::ToyRaft::RequestAppendResponse&);

        int64_t id;
        int64_t term;
        int64_t votedFor;
        int64_t voteCount;

        std::vector<::ToyRaft::RaftLog> log;
        int64_t commitIndex;
        int64_t lastAppliedIndex;

        Status state;

        int heartBeatTick;
        int electionTick;

        std::unordered_map<int64_t, std::shared_ptr<ToyRaft::Raft>> nodes;
        int64_t nextIndex;
        int64_t matchIndex;

        int electionTimeout;
        int heartBeatTimeout;
    };


} // namespace ToyRaft

#define TOYRAFT_RAFT_H

#endif //TOYRAFT_RAFT_H
