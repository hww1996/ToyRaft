// use etcd paper test 5-1
// Created by hww1996 on 2019/11/17.
//
#include <iostream>

#include "raftconfig.h"
#include "raft.h"
#include "logger.h"

class TestRaft : public ToyRaft::Raft {
public:
    int TestFollowerUpdateTermFromMessage() {
        testUpdateTermFromMessage(ToyRaft::Status::FOLLOWER);
        return 0;
    }
    int TestCandidateUpdateTermFromMessage() {
        testUpdateTermFromMessage(ToyRaft::Status::CANDIDATE);
        return 0;
    }
    int TestLeaderUpdateTermFromMessage() {
        testUpdateTermFromMessage(ToyRaft::Status::LEADER);
        return 0;
    }
    int TestRejectStaleTermMessage() {
        currentTerm = 3;
        ToyRaft::RequestAppend requestAppend;
        requestAppend.set_term(currentTerm - 1);
        requestAppend.set_currentleaderid(2);
        requestAppend.set_leadercommit(-1);
        requestAppend.set_prelogindex(-1);
        requestAppend.set_prelogterm(-1);
        return handleRequestAppend(requestAppend, 2);
    }
private:
    int testUpdateTermFromMessage(ToyRaft::Status sta) {
        int ret = 0;
        switch(sta) {
            case ToyRaft::Status::FOLLOWER:
                becomeFollower(1,2);
                break;
            case ToyRaft::Status::CANDIDATE:
                becomeCandidate();
                break;
            case ToyRaft::Status::LEADER:
                becomeLeader();
                break;
        }
        currentTerm = 1;
        ToyRaft::RequestAppend requestAppend;
        requestAppend.set_term(2);
        requestAppend.set_currentleaderid(2);
        requestAppend.set_leadercommit(-1);
        requestAppend.set_prelogindex(-1);
        requestAppend.set_prelogterm(-1);
        handleRequestAppend(requestAppend, 2);
        ToyRaft::LOGDEBUG("After: term:%d, status:%d", currentTerm, state);
        return ret;
    }
};

int main(int argc, char **argv) {
    if (2 != argc) {
        std::cout << "usage: <script> <path to config>" << std::endl;
        exit(-1);
    }
    ToyRaft::RaftConfig cs(argv[1]);
    TestRaft raft;
    raft.TestFollowerUpdateTermFromMessage();
    raft.TestCandidateUpdateTermFromMessage();
    raft.TestLeaderUpdateTermFromMessage();
    raft.TestRejectStaleTermMessage();
}