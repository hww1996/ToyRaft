//
// Created by hww1996 on 2019/9/28.
//
#include <vector>
#include <memory>
#include <unordered_map>

#ifndef TOYRAFT_RAFT_H

namespace ToyRaft {
    enum SendType {
        REQVOTE,
        VOTERSP,
        REQAPPEND,
        APPENDRSP,
    };

    struct RequestVote {
        int term;
        int candidateId;
        int lastLogTerm;
        int lastCommitLogIndex;
    };

    struct RequestVoteResponse {
        int term;
        bool voteForMe;
    };

    enum LogType {
        APPEND
    };

    struct RaftLog {
        int term;
        LogType type;
        char *buf;
        int len;
    };

    struct RequestAppend {
        int term;
        int currentLeaderId;
        int preLogTerm;
        int preLogIndex;
        int leaderCommit;
        std::vector<RaftLog> entries;
    };

    struct RequestAppendResponse {
        int term;
        int sentBackId;
        int commitIndex;
        bool success;
    };

    struct AllSend {
        SendType sendType;
        std::shared_ptr<RequestVote> requestVote;
        std::shared_ptr<RequestVoteResponse> requestVoteResponse;
        std::shared_ptr<RequestAppend> requestAppend;
        std::shared_ptr<RequestAppendResponse> requestAppendResponse;

        AllSend() : requestVote(nullptr), requestVoteResponse(nullptr),
                    requestAppend(nullptr), requestAppendResponse(nullptr) {}
    };

    enum Status {
        FOLLOWER,
        CANDIDATE,
        LEADER,
    };

    class Raft {
    public:
        int tick();

        // 状态改变
        int becomeLeader();

        int becomeFollower();

        int becomeFollower(int term, int voteFor);

        int becomeCandidate();

        // 从网络中获取数据
        int send(std::shared_ptr<AllSend>);

        std::shared_ptr<AllSend> recvFromNet();

        int recv();

        // 处理数据
        int handleRequestVote(std::shared_ptr<RequestVote>);

        int handleRequestVoteResponse(std::shared_ptr<RequestVoteResponse>);

        int handleRequestAppend(std::shared_ptr<RequestAppend>);

        int handleRequestAppendResponse(std::shared_ptr<RequestAppendResponse>);

    private:
        int id;
        int term;
        int votedFor;
        int voteCount;

        std::vector<RaftLog> log;
        int commitIndex;
        int lastAppliedIndex;

        Status state;

        int heartBeatTick;
        int electionTick;

        std::unordered_map<int, std::shared_ptr<ToyRaft::Raft>> nodes;
        nextIndex;
        matchIndex;

        int electionTimeout;
        int heartBeatTimeout;

        int currentLeader;
    };


} // namespace ToyRaft

#define TOYRAFT_RAFT_H

#endif //TOYRAFT_RAFT_H
