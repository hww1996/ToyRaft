//
// Created by hww1996 on 2019/9/28.
//

#include "raft.h"

namespace ToyRaft {
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
                //ret = send();
                if (ret != 0) {
                    return ret;
                }
            }
        } else if (Status::CANDIDATE == state) {
            //ret = send();
            if (ret != 0) {
                return ret;
            }
        }

        return ret;
    }

    std::shared_ptr<AllSend> constructRequestVote() {
        std::shared_ptr<AllSend> requestVote(new AllSend);
        return requestVote;
    }

    /**
     * @brief 接收到投票请求
     * @param requestVote
     * @return 错误码
     */
    int Raft::handleRequestVote(::ToyRaft::RequestVote& requestVote) {
        int ret = 0;
        ::ToyRaft::RequestVoteResponse voteRspMsg;
        // 以前的任期已经投过票了，所以不投给他
        if (term >= requestVote.term()) {
            voteRspMsg.set_term(term);
            voteRspMsg.set_voteforme(false);
        }
            // 假设他的任期更大，那么比较日志那个更新
        else {
            const ::ToyRaft::RaftLog &lastCommit = log[commitIndex];
            // 首先比较的是最后提交的日志的任期
            // 投票请求的最后提交的日志的任期小于当前日志的最后任期
            // 那么不投票给他
            if (lastCommit.term() > requestVote.lastlogterm()) {
                voteRspMsg.set_term(term);
                voteRspMsg.set_voteforme(false);
            }
                // 投票请求的最后提交的日志的任期等于当前日志的最后任期
                // 那么比较应用到状态机的日志index
            else if (lastCommit.term() == requestVote.lastlogterm()) {
                // 假如当前应用到状态机的index比投票请求的大
                // 那么不投票给他
                if (commitIndex > requestVote.lastcommitlogindex()) {
                    voteRspMsg.set_term(term);
                    voteRspMsg.set_voteforme(false);
                } else {
                    // 投票给候选者，并把自己的状态变为follower，
                    // 设置当前任期和投票给的人
                    becomeFollower(requestVote.term(),
                                   requestVote.candidateid());
                    voteRspMsg.set_term(term);
                    voteRspMsg.set_voteforme(true);
                }
            }
            // 投票请求的最后提交的日志的大于等于当前日志的最后任期
            else{
                // 投票给候选者，并把自己的状态变为follower，
                // 设置当前任期和投票给的人
                becomeFollower(requestVote.term(),
                               requestVote.candidateid());
                voteRspMsg.set_term(term);
                voteRspMsg.set_voteforme(true);
            }
        }
        ::ToyRaft::AllSend requestVoteRsp;
        requestVoteRsp.set_sendtype(::ToyRaft::AllSend::VOTERSP);
        requestVoteRsp.set_allocated_requestvoteresponse(&voteRspMsg);
        // ret = send(requestVoteRsp);
        return ret;
    }

    /**
     * @brief 接收到请求投票的response
     * @param requestVoteResponse
     * @return
     */
    int Raft::handleRequestVoteResponse(::ToyRaft::RequestVoteResponse& requestVoteResponse) {
        int ret = 0;
        if (Status::CANDIDATE != state) {
            return 0;
        }
        if (requestVoteResponse.term() == term &&
            requestVoteResponse.voteforme()) {
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
    int Raft::handleRequestAppend(::ToyRaft::RequestAppend& requestAppend) {
        int ret = 0;
        ::ToyRaft::RequestAppendResponse appendRsp;

        // 当term小于或者等于requestAppend的term的时候，那么直接成为follower,并处理appendLog请求
        if (term <= requestAppend.term()) {
            becomeFollower(requestAppend.term(), requestAppend.currentleaderid());
            // 当前的commitIndex小于preLogIndex，那么直接返回错误
            if (commitIndex < requestAppend.prelogindex()) {
                appendRsp.set_term(term);
                appendRsp.set_commitindex(-1);
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
                    while (LogIndex < log.size() &&
                           entriesIndex < appendEntries.size()
                            ) {
                        log[LogIndex++] = appendEntries[entriesIndex++];
                    }
                    while (entriesIndex < appendEntries.size()) {
                        log.push_back(appendEntries[entriesIndex++]);
                        LogIndex++;
                    }
                    commitIndex = LogIndex - 1;
                    appendRsp.set_term(term);
                    appendRsp.set_commitindex(commitIndex);
                    appendRsp.set_success(true);
                } else {
                    appendRsp.set_term(term);
                    appendRsp.set_commitindex(-1);
                    appendRsp.set_success(false);
                }
            }
        }
            // 当前的term大于requestAppend的term，
            // 那么说明这个leader是过期的，直接返回false，并告诉他出现异常
        else {
            appendRsp.set_term(term);
            appendRsp.set_commitindex(-1);
            appendRsp.set_success(false);
        }
        ::ToyRaft::AllSend requestAppendRsp;
        appendRsp.set_sentbackid(id);
        requestAppendRsp.set_sendtype(::ToyRaft::AllSend::APPENDRSP);
        requestAppendRsp.set_allocated_requestappendresponse(&appendRsp);
        //ret = send(requestAppendRsp);
        return ret;
    }

    /**
     * @brief 接收到了添加log的回复
     * @param requestAppendResponse
     * @return
     */
    int Raft::handleRequestAppendResponse(::ToyRaft::RequestAppendResponse& requestAppendResponse) {
        int ret = 0;
        if (Status::LEADER == state) {
            if (nodes.end() == nodes.find(requestAppendResponse.sentbackid())) {
                ret = -1;
            } else {
                auto node = nodes[requestAppendResponse.sentbackid()];
                if (requestAppendResponse.success()) {
                    node->matchIndex = requestAppendResponse.commitindex();
                    node->nextIndex = node->matchIndex + 1;
                } else {
                    if (0 != node->nextIndex) {
                        node->nextIndex--;
                    }
                }
            }
            // 查找需要提交的commit
            std::unordered_map<int, int> commitCount;
            int allFollowerCount = nodes.size() - 1;
            bool findCommit = false;
            int needCommitIndex = -1;
            for (const auto &follower : nodes) {
                int nowFollowerCommit = follower.second->commitIndex;
                if (commitCount.end() == commitCount.find(nowFollowerCommit)) {
                    commitCount[nowFollowerCommit] = 0;
                }
                commitCount[nowFollowerCommit]++;
                if (commitCount[nowFollowerCommit] >= (allFollowerCount / 2 + 1)) {
                    findCommit = true;
                    needCommitIndex = nowFollowerCommit;
                }
            }
            if (findCommit) {
                commitIndex = needCommitIndex;
            }
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
            std::shared_ptr<AllSend> allSend = recvFromNet();
            if (nullptr == allSend) {
                break;
            }
            switch (allSend->sendType) {
                case SendType::REQVOTE:
                    ret = handleRequestVote(allSend->requestVote);
                    break;
                case SendType::VOTERSP:
                    ret = handleRequestVoteResponse(allSend->requestVoteResponse);
                    break;
                case SendType::REQAPPEND:
                    ret = handleRequestAppend(allSend->requestAppend);
                    break;
                case SendType::APPENDRSP:
                    ret = handleRequestAppendResponse(allSend->requestAppendResponse);
                    break;
                default:
                    break;
            }
        }
        return ret;
    }
} // namespace ToyRaft
