//
// Created by apple on 2019/9/28.
//

#include "raft.h"

namespace ToyRaft {
    /**
     *  只有leader和candidate有权限发送
     *  每个角色都有权限接收
     *
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

        if (LEADER == state) {
            if (heartBeatTick >= heartBeatTimeout) {
                //ret = send();
                if (ret != 0) {
                    return ret;
                }
            }
        } else if (CANDIDATE == state) {
            //ret = send();
            if (ret != 0) {
                return ret;
            }
        }

        return ret;
    }

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
            const RaftLog &lastLog = log.back();
            // 首先比较的是最后应用的日志的任期
            // 投票请求的最后应用的日志的任期小于当前日志的最后任期
            // 那么不投票给他
            if (lastLog.term > requestVote->lastLogTerm) {
                voteRspMsg->term = term;
                voteRspMsg->voteForMe = false;
            }
                // 投票请求的最后应用的日志的任期等于当前日志的最后任期
                // 那么比较应用到状态机的日志index
            else if (lastLog.term == requestVote->lastLogTerm) {
                // 假如当前应用到状态机的index比投票请求的大
                // 那么不投票给他
                if (lastAppliedIndex > requestVote->lastAppliedLogIndex) {
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
        ret = send(requestVoteRsp);
        return ret;
    }

    int Raft::handleRequestVoteResponse(std::shared_ptr<ToyRaft::RequestVoteResponse> requestVoteResponse) {
        int ret = 0;
        if (CANDIDATE != state) {
            return 0;
        }
        if (requestVoteResponse->term == term &&
            requestVoteResponse->voteForMe) {
            voteCount++;
        }
        if (voteCount > (nodes.size() / 2 + 1)) {
            becomeLeader();
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
                case REQVOTE:
                    ret = handleRequestVote(allSend->requestVote);
                    break;
                case VOTERSP:
                    ret = handleRequestVoteResponse(allSend->requestVoteResponse);
                    break;
                case REQAPPEND:
                    ret = handleRequestAppend(allSend->requestAppend);
                    break;
                case APPENDRSP:
                    ret = handleRequestAppendResponse(allSend->requestAppendResponse);
                    break;
                default:
                    break;
            }

        }

        return ret;
    }
} // namespace ToyRaft
