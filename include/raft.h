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

    struct Peers {
        int64_t id;
        int64_t nextIndex;
        int64_t matchIndex;
        bool isVoteForMe;

        Peers(int64_t nodeId, int64_t peersNextIndex, int64_t peersMatchIndex);
    };

    class Raft {
    public:
        Raft(bool votable = true);

        int tick();

    protected:
        int recvInnerMsg();

        // 发送vote请求
        int sendRequestVote();

        // 发送append请求
        int sendRequestAppend();

        // 状态改变
        int becomeLeader();

        int becomeFollower(int64_t term, int64_t leaderId);

        int becomeCandidate();

        // 处理投票
        int handleRequestVote(const ::ToyRaft::RequestVote &, int64_t sendFrom);

        int handleRequestVoteResponse(const ::ToyRaft::RequestVoteResponse &, int64_t sendFrom);

        // 处理日志复制
        int handleRequestAppend(const ::ToyRaft::RequestAppend &, int64_t sendFrom);

        int handleRequestAppendResponse(const ::ToyRaft::RequestAppendResponse &, int64_t sendFrom);

        int leaderCommit();

        int logToStable(int start, int size);

        int apply();

        // 外面的请求发送log过来
        int getFromOuterNet();

        // 更新集群
        int updatePeers();

        // 打包状态
        int packageStatus();

        //  恢复原来的log的状态，读取内容到内容中
        int recoverStatus();

        // 任期相关
        int64_t id;
        int64_t currentTerm;
        int64_t currentLeaderId;

        std::vector<::ToyRaft::RaftLog> log;
        int64_t commitIndex;
        int64_t lastAppliedIndex;

        Status state;

        int timeTick;

        std::unordered_map<int64_t, std::shared_ptr<Peers>> nodes;

        int electionTimeout;
        int heartBeatTimeout;

        bool canVote;
    };


} // namespace ToyRaft

#endif //TOYRAFT_RAFT_H
