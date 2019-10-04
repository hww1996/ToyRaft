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
    int Raft::handleRequestVote(std::shared_ptr<ToyRaft::RequestVote> requestVote) {
        int ret = 0;
        if (nullptr == requestVote) {
            return ret;
        }
        auto voteRspMsg = std::shared_ptr<RequestVoteResponse>(
                new RequestVoteResponse
        );
        // 以前的任期已经投过票了，所以不投给他
        if (term >= requestVote->term) {
            voteRspMsg->term = term;
            voteRspMsg->voteForMe = false;
        }
            // 假设他的任期更大，那么比较日志那个更新
        else {
            const RaftLog &lastCommit = log[commitIndex];
            // 首先比较的是最后提交的日志的任期
            // 投票请求的最后提交的日志的任期小于当前日志的最后任期
            // 那么不投票给他
            if (lastCommit.term > requestVote->lastLogTerm) {
                voteRspMsg->term = term;
                voteRspMsg->voteForMe = false;
            }
                // 投票请求的最后提交的日志的任期等于当前日志的最后任期
                // 那么比较应用到状态机的日志index
            else if (lastCommit.term == requestVote->lastLogTerm) {
                // 假如当前应用到状态机的index比投票请求的大
                // 那么不投票给他
                if (commitIndex > requestVote->lastCommitLogIndex) {
                    voteRspMsg->term = requestVote->term;
                    voteRspMsg->voteForMe = false;
                } else {
                    // 投票给候选者，并把自己的状态变为follower，
                    // 设置当前任期和投票给的人
                    becomeFollower(requestVote->term,
                                   requestVote->candidateId);
                    voteRspMsg->term = requestVote->term;
                    voteRspMsg->voteForMe = true;
                }
            }
        }
        std::shared_ptr<AllSend> requestVoteRsp = std::shared_ptr<AllSend>(new AllSend());
        requestVoteRsp->sendType = SendType::VOTERSP;
        requestVoteRsp->requestVoteResponse = voteRspMsg;
        ret = send(requestVoteRsp);
        return ret;
    }

    /**
     * @brief 接收到请求投票的response
     * @param requestVoteResponse
     * @return
     */
    int Raft::handleRequestVoteResponse(std::shared_ptr<ToyRaft::RequestVoteResponse> requestVoteResponse) {
        int ret = 0;
        if (Status::CANDIDATE != state) {
            return 0;
        }
        if (requestVoteResponse->term == term &&
            requestVoteResponse->voteForMe) {
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
    int Raft::handleRequestAppend(std::shared_ptr<ToyRaft::RequestAppend> requestAppend) {
        int ret = 0;
        std::shared_ptr<ToyRaft::RequestAppendResponse> appendRsp =
                std::shared_ptr<ToyRaft::RequestAppendResponse>(new ToyRaft::RequestAppendResponse);

        // 当term小于或者等于requestAppend的term的时候，那么直接成为follower,并处理appendLog请求
        if (term <= requestAppend->term) {
            becomeFollower(requestAppend->term, requestAppend->currentLeaderId);
            // 当前的commitIndex小于preLogIndex，那么直接返回错误
            if (commitIndex < requestAppend->preLogIndex) {
                appendRsp->term = term;
                appendRsp->commitIndex = -1;
                appendRsp->success = false;
            } else {
                std::vector<RaftLog> &appendEntries = requestAppend->entries;
                bool isMatch = false;
                // 判断是否匹配上
                // requestAppend->preLogIndex 为-1 一定是匹配上的
                if (-1 == requestAppend->preLogIndex) {
                    isMatch = true;
                }
                    // 任期一致，也匹配上
                else {
                    const RaftLog &preLog = log[requestAppend->preLogIndex];
                    if (requestAppend->preLogTerm == preLog.term) {
                        isMatch = true;
                    }
                }
                if (isMatch) {
                    int LogIndex = requestAppend->preLogIndex + 1;
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
                    appendRsp->term = term;
                    appendRsp->commitIndex = commitIndex;
                    appendRsp->success = true;
                } else {
                    appendRsp->term = term;
                    appendRsp->commitIndex = -1;
                    appendRsp->success = false;
                }
            }
        }
            // 当前的term大于requestAppend的term，
            // 那么说明这个leader是过期的，直接返回false，并告诉他出现异常
        else {
            appendRsp->term = term;
            appendRsp->commitIndex = -1;
            appendRsp->success = false;
        }
        std::shared_ptr<AllSend> requestAppendRsp = std::shared_ptr<AllSend>(new AllSend());
        appendRsp->sentBackId = id;
        requestAppendRsp->sendType = SendType::APPENDRSP;
        requestAppendRsp->requestAppendResponse = appendRsp;
        ret = send(requestAppendRsp);
        return ret;
    }

    /**
     * @brief 接收到了添加log的回复
     * @param requestAppendResponse
     * @return
     */
    int Raft::handleRequestAppendResponse(std::shared_ptr<ToyRaft::RequestAppendResponse> requestAppendResponse) {
        int ret = 0;
        if (Status::LEADER == state) {
            if (nodes.end() == nodes.find(requestAppendResponse->sentBackId)) {
                ret = -1;
            } else {
                auto node = nodes[requestAppendResponse->sentBackId];
                if (requestAppendResponse->success) {
                    node->matchIndex = requestAppendResponse->commitIndex;
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
