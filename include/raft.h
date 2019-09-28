//
// Created by hww1996 on 2019/9/28.
//
#include <vector>
#include <memory>
#include <unordered_map>

#ifndef TOYRAFT_RAFT_H

namespace ToyRaft{

class RequestVote{
private:
    int term;
    int candidateId;
    int lastLogTerm;
    int lastCommitLogIndex;
};

class RequestVoteResponse{
private:
    int term;
    bool voteForMe;
};

typedef char* LogType;

class RequestAppend{
private:
    int term;
    int preLogTerm;
    int preLogIndex;
    int leaderCommit;
    std::vector<LogType> entries;
};

class RequestAppendResponse{
private:
    int term;
    bool success;
    int currentIndex;
    int firstAppliedIndex;
};

enum Status{
    FOLLOWER,
    CANDIDATE,
    LEADER,
};

class Server{
private:
    int id;
    int Term;
    int votedFor;

    std::vector<LogType> log;
    int commitIndex;
    int lastAppliedIndex;

    Status state;

    /* 计时器，周期函数每次执行时会递增改值 */
    int timeoutElapsed;

    std::unordered_map<std::unique_ptr<Server>> nodes;
    std::vector<int> nodeApplied;
    std::vector<int> nodeCommited;

    int electionTimeout;
    int requestTimeout;

    int  currentLeader;
};


} // namespace ToyRaft

#define TOYRAFT_RAFT_H

#endif //TOYRAFT_RAFT_H
