//
// Created by hww1996 on 2019/9/28.
//
#include <vector>
#include <memory>
#include <unordered_map>
#include "raft.pb.h"

#ifndef TOYRAFT_RAFT_H
#define TOYRAFT_RAFT_H

namespace ToyRaft {
    enum Status {
        FOLLOWER, CANDIDATE, LEADER,
    };

    class Raft {
    public:
        int tick();

    private:
        int recv();

        // 发送vote请求
        int sendRequestVote();

        // 发送append请求
        int sendRequestAppend();

        // 状态改变
        int becomeLeader();

        int becomeFollower(int64_t term, int64_t leaderId);

        int becomeCandidate();

        // 处理投票
        int handleRequestVote(const ::ToyRaft::RequestVote &);

        int handleRequestVoteResponse(const ::ToyRaft::RequestVoteResponse &);

        // 处理日志复制
        int handleRequestAppend(const ::ToyRaft::RequestAppend &);

        int handleRequestAppendResponse(const ::ToyRaft::RequestAppendResponse &);

        int commit();

        // 任期相关
        int64_t id;
        int64_t currentTerm;
        int64_t currentLeaderId;
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

#endif //TOYRAFT_RAFT_H
