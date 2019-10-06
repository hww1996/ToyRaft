//
// Created by hww1996 on 2019/9/28.
//

#include <cstdlib>
#include <ctime>

#include "raft.h"
#include "networking.h"

namespace ToyRaft {
    static int64_t min(int64_t a, int64_t b) {
        return a > b ? b : a;
    }

    /**
     * @brief 只有leader和candidate有权限发送，每个角色都有权限接收
     * @return 返回错误码
     */
    int Raft::tick() {

        heartBeatTick++;
        electionTick++;

        int ret = 0;

        if (electionTick >= electionTimeout) {
            ret = becomeCandidate();
            if (0 != ret) {
                return ret;
            }
        }

        // 每隔一段时间接收一次消息
        ret = recv();
        if (0 != ret) {
            return ret;
        }

        if (Status::LEADER == state) {
            if (heartBeatTick >= heartBeatTimeout) {
                commit();
                ret = sendRequestAppend();
                if (0 != ret) {
                    return ret;
                }
            }
        } else if (Status::CANDIDATE == state) {
            ret = sendRequestVote();
            if (0 != ret) {
                return ret;
            }
        }

        return ret;
    }

    int Raft::becomeLeader() {
        int ret = 0;
        if (Status::CANDIDATE == state) {
            state = Status::LEADER;
            for (auto nodeIt = nodes.begin(); nodes.end() != nodeIt; ++nodeIt) {
                if (nodeIt->second->id != id) {
                    nodeIt->second->matchIndex = -1;
                    nodeIt->second->nextIndex = lastAppliedIndex + 1;
                }
            }
            ret = sendRequestAppend();
        }
        return ret;
    }

    int Raft::becomeCandidate() {
        int ret = 0;
        currentTerm++;

        state = Status::CANDIDATE;

        currentLeaderId = id;

        voteCount = 1;

        time_t seed = time(NULL);
        srand(static_cast<unsigned int>(seed));
        int64_t standerElectionTimeOut = heartBeatTimeout * 10;
        electionTimeout = standerElectionTimeOut + rand() % standerElectionTimeOut;
        ret = sendRequestVote();
        return ret;
    }

    int Raft::becomeFollower(int64_t term, int64_t leaderId) {
        int ret = 0;
        state = Status::FOLLOWER;
        currentLeaderId = leaderId;
        currentTerm = term;
        return ret;
    }

    /**
     * @brief 请求投票
     * @return
     */
    int Raft::sendRequestVote() {
        int ret = 0;
        if (Status::CANDIDATE == state) {
            ::ToyRaft::RequestVote requestVote;
            requestVote.set_term(currentTerm);
            requestVote.set_candidateid(id);
            requestVote.set_lastlogterm(lastAppliedIndex);
            if (-1 == lastAppliedIndex) {
                requestVote.set_lastlogterm(-1);
            } else {
                requestVote.set_lastlogterm(log[lastAppliedIndex].term());
            }
            for (auto nodeIter = nodes.begin(); nodes.end() != nodeIter; ++nodeIter) {
                if (nodeIter->second->id != id) {
                    ::ToyRaft::AllSend allSend;
                    allSend.set_sendtype(::ToyRaft::AllSend::REQVOTE);
                    allSend.set_allocated_requestvote(&requestVote);
                    ret = RaftNet::sendToNet(nodeIter->second->id, allSend);
                }
                if (0 != ret) {
                    return ret;
                }
            }
        } else {
            return -1;
        }
        return ret;
    }

    /**
     * @brief 请求复制log
     * @return
     */
    int Raft::sendRequestAppend() {
        int ret = 0;
        if (Status::LEADER != state) {
            return -1;
        } else {
            for (auto nodeIter = nodes.begin(); nodes.end() != nodeIter; ++nodeIter) {
                if (nodeIter->second->id != id) {
                    ::ToyRaft::RequestAppend requestAppend;
                    for (int i = nodeIter->second->nextIndex; i <= lastAppliedIndex; i++) {
                        auto needAppendLog = requestAppend.add_entries();
                        needAppendLog->set_term(currentTerm);
                        needAppendLog->set_type(::ToyRaft::RaftLog::APPEND);
                        needAppendLog->set_buf(log[i].buf());
                    }
                    requestAppend.set_term(currentTerm);
                    requestAppend.set_currentleaderid(id);
                    requestAppend.set_leadercommit(commitIndex);
                    requestAppend.set_prelogindex(nodeIter->second->matchIndex);
                    if (requestAppend.prelogindex() == -1) {
                        requestAppend.set_prelogterm(-1);
                    } else {
                        requestAppend.set_prelogterm(log[nodeIter->second->matchIndex].term());
                    }
                    ::ToyRaft::AllSend allSend;
                    allSend.set_sendtype(::ToyRaft::AllSend::REQAPPEND);
                    allSend.set_allocated_requestappend(&requestAppend);
                    ret = RaftNet::sendToNet(nodeIter->second->id, allSend);
                }
                if (0 != ret) {
                    return ret;
                }
            }
        }
        return ret;
    }

    /**
     * @brief 接收到投票请求
     * @param requestVote
     * @return 错误码
     */
    int Raft::handleRequestVote(const ::ToyRaft::RequestVote &requestVote) {
        int ret = 0;
        ::ToyRaft::RequestVoteResponse voteRspMsg;
        // 以前的任期已经投过票了，所以不投给他
        if (currentTerm >= requestVote.term()) {
            voteRspMsg.set_term(currentTerm);
            voteRspMsg.set_voteforme(false);
        }
            // 假设他的任期更大，那么比较日志那个更新
        else {
            const ::ToyRaft::RaftLog &lastApplied = log[lastAppliedIndex];
            // 首先比较的是最后提交的日志的任期
            // 投票请求的最后提交的日志的任期小于当前日志的最后任期
            // 那么不投票给他
            if (lastApplied.term() > requestVote.lastlogterm()) {
                voteRspMsg.set_term(currentTerm);
                voteRspMsg.set_voteforme(false);
            }
                // 投票请求的最后提交的日志的任期等于当前日志的最后任期
                // 那么比较应用到状态机的日志index
            else if (lastApplied.term() == requestVote.lastlogterm()) {
                // 假如当前应用到状态机的index比投票请求的大
                // 那么不投票给他
                if (lastAppliedIndex > requestVote.lastlogindex()) {
                    voteRspMsg.set_term(currentTerm);
                    voteRspMsg.set_voteforme(false);
                } else {
                    // 投票给候选者，并把自己的状态变为follower，
                    // 设置当前任期和投票给的人
                    becomeFollower(requestVote.term(), requestVote.candidateid());
                    voteRspMsg.set_term(currentTerm);
                    voteRspMsg.set_voteforme(true);
                }
            }
                // 投票请求的最后提交的日志的大于等于当前日志的最后任期
            else {
                // 投票给候选者，并把自己的状态变为follower，
                // 设置当前任期和投票给的人
                becomeFollower(requestVote.term(), requestVote.candidateid());
                voteRspMsg.set_term(currentTerm);
                voteRspMsg.set_voteforme(true);
            }
        }
        ::ToyRaft::AllSend requestVoteRsp;
        requestVoteRsp.set_sendtype(::ToyRaft::AllSend::VOTERSP);
        requestVoteRsp.set_allocated_requestvoteresponse(&voteRspMsg);
        ret = RaftNet::sendToNet(requestVote.candidateid(), requestVoteRsp);
        return ret;
    }

    /**
     * @brief 接收到请求投票的response
     * @param requestVoteResponse
     * @return
     */
    int Raft::handleRequestVoteResponse(const ::ToyRaft::RequestVoteResponse &requestVoteResponse) {
        int ret = 0;
        if (Status::CANDIDATE != state) {
            return 0;
        }
        if (requestVoteResponse.term() == currentTerm && requestVoteResponse.voteforme()) {
            voteCount++;
        }
        if (voteCount >= (nodes.size() / 2 + 1)) {
            becomeLeader();
        }
        return ret;
    }

    /**
     * @brief 接收到了entries append请求
     * @param requestAppend
     * @return
     */
    int Raft::handleRequestAppend(const ::ToyRaft::RequestAppend &requestAppend) {
        int ret = 0;
        ::ToyRaft::RequestAppendResponse appendRsp;

        // 当currentTerm小于或者等于requestAppend的term的时候，那么直接成为follower,并处理appendLog请求
        if (currentTerm <= requestAppend.term()) {
            becomeFollower(requestAppend.term(), requestAppend.currentleaderid());
            // 当前的lastAppliedIndex小于preLogIndex，那么直接返回错误
            if (lastAppliedIndex < requestAppend.prelogindex()) {
                appendRsp.set_term(currentTerm);
                appendRsp.set_appliedindex(lastAppliedIndex);
                appendRsp.set_success(false);
            } else {
                auto &appendEntries = requestAppend.entries();
                bool isMatch = false;
                // 判断是否匹配上
                // requestAppend->preLogIndex 为-1 一定是匹配上的
                if (-1 == requestAppend.prelogindex()) {
                    isMatch = true;
                }
                    // 任期一致，也匹配上
                else {
                    const ::ToyRaft::RaftLog &preLog = log[requestAppend.prelogindex()];
                    if (requestAppend.prelogterm() == preLog.term()) {
                        isMatch = true;
                    }
                }
                if (isMatch) {
                    int64_t LogIndex = requestAppend.prelogindex() + 1;
                    int entriesIndex = 0;
                    while (LogIndex < log.size() && entriesIndex < appendEntries.size()) {
                        log[LogIndex++] = appendEntries[entriesIndex++];
                    }
                    while (entriesIndex < appendEntries.size()) {
                        log.push_back(appendEntries[entriesIndex++]);
                        LogIndex++;
                    }
                    lastAppliedIndex = LogIndex - 1;
                    commitIndex = min(requestAppend.leadercommit(), commitIndex);
                    appendRsp.set_term(currentTerm);
                    appendRsp.set_appliedindex(lastAppliedIndex);
                    appendRsp.set_success(true);
                } else {
                    appendRsp.set_term(currentTerm);
                    appendRsp.set_appliedindex(lastAppliedIndex);
                    appendRsp.set_success(false);
                }
            }
        }
            // 当前的term大于requestAppend的term，
            // 那么说明这个leader是过期的，直接返回false，并告诉他出现异常
        else {
            appendRsp.set_term(currentTerm);
            appendRsp.set_appliedindex(lastAppliedIndex);
            appendRsp.set_success(false);
        }
        ::ToyRaft::AllSend requestAppendRsp;
        appendRsp.set_sentbackid(id);
        requestAppendRsp.set_sendtype(::ToyRaft::AllSend::APPENDRSP);
        requestAppendRsp.set_allocated_requestappendresponse(&appendRsp);
        ret = RaftNet::sendToNet(requestAppend.currentleaderid(), requestAppendRsp);
        return ret;
    }

    /**
     * @brief 接收到了添加log的回复
     * @param requestAppendResponse
     * @return
     */
    int Raft::handleRequestAppendResponse(const ::ToyRaft::RequestAppendResponse &requestAppendResponse) {
        int ret = 0;
        if (Status::LEADER == state) {
            if (nodes.end() == nodes.find(requestAppendResponse.sentbackid())) {
                ret = -1;
            } else {
                auto node = nodes[requestAppendResponse.sentbackid()];
                if (requestAppendResponse.success()) {
                    node->matchIndex = requestAppendResponse.appliedindex();
                    node->nextIndex = node->matchIndex + 1;
                } else {
                    if (0 != node->nextIndex) {
                        node->nextIndex--;
                    }
                }
            }
        }
        return ret;
    }

    /**
     * @brief 提交还未提交的日志
     * @return
     */
    int Raft::commit() {
        int ret = 0;
        std::vector<int> commitCount(lastAppliedIndex - commitIndex, 1);
        for (auto nodeIt = nodes.begin(); nodes.end() != nodeIt; ++nodeIt) {
            if (nodeIt->second->id != id) {
                if (-1 != nodeIt->second->matchIndex) {
                    for (int i = 0; i < nodeIt->second->matchIndex - commitIndex; i++) {
                        commitCount[i]++;
                    }
                }
            }
        }
        int i = commitCount.size() - 1;
        for (; i >= 0; i--) {
            if ((nodes.size() / 2 + 1) <= commitCount[i]) {
                break;
            }
        }
        if (i >= 0) {
            commitIndex = commitIndex + i + 1;
        }
        return ret;
    }

    /**
     *
     * 总共存在种接收的信息：
     *  1. 投票请求信息（人人都能接收,用于更新状态）
     *  2. 投票请求回复信息（只有candidate能收）
     *  2. 心跳信息（人人都能收，用于更新状态）
     *  4. 心跳回复信息（leader能收）
     *
     * @return
     */
    int Raft::recv() {
        int ret = 0;
        while (true) {
            ::ToyRaft::AllSend allSend;
            ret = RaftNet::recvFromNet(allSend);
            if (0 >= ret) {
                break;
            }
            switch (allSend.sendtype()) {
                case ::ToyRaft::AllSend::REQVOTE:
                    ret = handleRequestVote(allSend.requestvote());
                    break;
                case ::ToyRaft::AllSend::VOTERSP:
                    ret = handleRequestVoteResponse(allSend.requestvoteresponse());
                    break;
                case ::ToyRaft::AllSend::REQAPPEND:
                    ret = handleRequestAppend(allSend.requestappend());
                    break;
                case ::ToyRaft::AllSend::APPENDRSP:
                    ret = handleRequestAppendResponse(allSend.requestappendresponse());
                    break;
                default:
                    break;
            }
        }
        return ret;
    }
} // namespace ToyRaft
