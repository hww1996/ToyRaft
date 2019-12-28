//
// Created by hww1996 on 2019/9/28.
//

#include <cstdlib>
#include <ctime>
#include <memory>
#include <cmath>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "raft.h"
#include "raftconnect.h"
#include "raftconfig.h"
#include "raftserver.h"
#include "globalmutex.h"
#include "raftsave.h"
#include "logger.h"

namespace ToyRaft {
    static int64_t min(int64_t a, int64_t b) {
        return a > b ? b : a;
    }

    Peers::Peers(int64_t nodeId, int64_t peersNextIndex, int64_t peersMatchIndex) : id(nodeId),
                                                                                    nextIndex(peersNextIndex),
                                                                                    matchIndex(peersMatchIndex),
                                                                                    isVoteForMe(false) {}

    Raft::Raft(bool votable) : log(std::vector<::ToyRaft::RaftLog>()),
                               nodes(std::unordered_map<int64_t, std::shared_ptr<Peers>>()) {
        auto nodesConfigMap = RaftConfig::getNodes();
        id = RaftConfig::getId();
        currentTerm = -1;
        currentLeaderId = -1;
        commitIndex = -1;
        lastAppliedIndex = -1;
        state = Status::FOLLOWER;

        timeTick = 0;

        canVote = votable;

        for (auto nodesConfigIt = nodesConfigMap.begin(); nodesConfigMap.end() != nodesConfigIt; ++nodesConfigIt) {
            nodes[nodesConfigIt->first] = std::make_shared<Peers>(nodesConfigIt->second->id_, 0, -1);
        }

        heartBeatTimeout = 1;
        time_t seed = time(NULL);
        srand(static_cast<unsigned int>(seed));
        int standerElectionTimeOut = heartBeatTimeout * 10;
        electionTimeout = standerElectionTimeOut + rand() % standerElectionTimeOut;
        recoverStatus();
        packageStatus();
    }


    /**
     * @brief 只有leader和candidate有权限发送，每个角色都有权限接收
     * @return 返回错误码
     */
    int Raft::tick() {
        LOGDEBUG(
                "leader:%d,status:%d,leaderCommit:%d,lastapplied:%d,electionTimeout:%d,timeTick:%d,term:%d,log size:%d",
                currentLeaderId, state, commitIndex, lastAppliedIndex, electionTimeout, timeTick, currentTerm,
                log.size());
        int ret = 0;

        if (!RaftConfig::checkNodeExists(id)) {
            LOGDEBUG("not in the config any more.exit.");
            return 1;
        }

        if (commitIndex > lastAppliedIndex) {
            ret = apply();
        }
        if (canVote) {
            timeTick++;
        }
        if (timeTick >= electionTimeout) {
            ret = becomeCandidate();
            if (0 != ret) {
                return ret;
            }
        }

        if (Status::LEADER == state) {
            timeTick = 0;
            leaderCommit();
            ret = sendRequestAppend();
            if (0 != ret) {
                return ret;
            }
        }

        // 每隔一段时间接收一次消息
        ret = recvInnerMsg();
        if (0 != ret) {
            return ret;
        }
        packageStatus();
        updatePeers();
        return ret;
    }

    int Raft::becomeLeader() {
        LOGDEBUG("become leader");
        int ret = 0;
        if (Status::CANDIDATE == state) {
            state = Status::LEADER;
            for (auto nodeIt = nodes.begin(); nodes.end() != nodeIt; ++nodeIt) {
                if (nodeIt->second->id != id) {
                    nodeIt->second->matchIndex = -1;
                    nodeIt->second->nextIndex = log.size();
                }
            }
            ret = sendRequestAppend();
        }
        return ret;
    }

    int Raft::becomeCandidate() {
        if (!canVote) {
            LOGERROR("can not become candidate.");
            return 0;
        }
        LOGDEBUG("become candidate");
        int ret = 0;
        currentTerm++;

        state = Status::CANDIDATE;

        currentLeaderId = id;

        for (auto nodesIt = nodes.begin(); nodes.end() != nodesIt; ++nodesIt) {
            nodesIt->second->isVoteForMe = false;
        }
        nodes[id]->isVoteForMe = true;

        time_t seed = time(nullptr);
        srand(static_cast<unsigned int>(seed));
        int standerElectionTimeOut = heartBeatTimeout * 10;
        timeTick = 0;
        electionTimeout = standerElectionTimeOut + rand() % standerElectionTimeOut;
        ret = sendRequestVote();
        return ret;
    }

    int Raft::becomeFollower(int64_t term, int64_t leaderId) {
        LOGDEBUG("become follower");
        int ret = 0;
        state = Status::FOLLOWER;
        currentLeaderId = leaderId;
        currentTerm = term;
        timeTick = 0;
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
            int lastLogIndex = static_cast<int>(log.size()) - 1;
            requestVote.set_lastlogindex(lastLogIndex);
            if (-1 == lastLogIndex) {
                requestVote.set_lastlogterm(-1);
            } else {
                requestVote.set_lastlogterm(log[lastLogIndex].term());
            }
            for (auto nodeIter = nodes.begin(); nodes.end() != nodeIter; ++nodeIter) {
                if (nodeIter->second->id != id) {
                    ::ToyRaft::AllSend allSend;
                    allSend.set_sendfrom(id);
                    allSend.set_sendtype(::ToyRaft::AllSend::REQVOTE);
                    std::string requestVoteBuf;
                    requestVote.SerializeToString(&requestVoteBuf);
                    allSend.set_sendbuf(requestVoteBuf.c_str(), requestVoteBuf.size());
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

    int Raft::getFromOuterNet() {
        int ret = 0;
        std::vector<RaftLog> res;
        ret = RaftServer::getNetLogs(res);
        if (0 != ret) {
            LOGERROR("get outer log error.ret %d", ret);
            return ret;
        }
        LOGDEBUG("Outer log size :%d", res.size());
        for (int i = 0; i < res.size(); i++) {
            res[i].set_term(currentTerm);
            log.push_back(res[i]);
        }
        return ret;
    }

    /**
     * @brief 请求复制log
     * @return
     */
    int Raft::sendRequestAppend() {
        int ret = 0;
        ret = getFromOuterNet();
        if (Status::LEADER != state) {
            return -1;
        } else {
            for (auto nodeIter = nodes.begin(); nodes.end() != nodeIter; ++nodeIter) {
                if (nodeIter->second->id != id) {
                    ::ToyRaft::RequestAppend requestAppend;
                    for (int i = nodeIter->second->nextIndex; i < log.size(); i++) {
                        auto needAppendLog = requestAppend.add_entries();
                        needAppendLog->set_term(log[i].term());
                        needAppendLog->set_type(log[i].type());
                        needAppendLog->set_buf(log[i].buf().c_str(), log[i].buf().size());
                    }
                    requestAppend.set_term(currentTerm);
                    requestAppend.set_currentleaderid(id);
                    requestAppend.set_leadercommit(commitIndex);
                    requestAppend.set_prelogindex(nodeIter->second->nextIndex - 1);
                    if (log.empty()) {
                        requestAppend.set_prelogterm(-1);
                    } else {
                        requestAppend.set_prelogterm(log[nodeIter->second->nextIndex - 1].term());
                    }
                    ::ToyRaft::AllSend allSend;
                    allSend.set_sendfrom(id);
                    allSend.set_sendtype(::ToyRaft::AllSend::REQAPPEND);
                    std::string requestAppendBuf;
                    requestAppend.SerializeToString(&requestAppendBuf);
                    allSend.set_sendbuf(requestAppendBuf.c_str(), requestAppendBuf.size());
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
    int Raft::handleRequestVote(const ::ToyRaft::RequestVote &requestVote, int64_t sendFrom) {
        int ret = 0;
        ::ToyRaft::RequestVoteResponse voteRspMsg;
        do {
            // 以前的任期已经投过票了，所以不投给他
            if (currentTerm >= requestVote.term()) {
                LOGDEBUG("the request vote term is smaller.");
                voteRspMsg.set_term(currentTerm);
                voteRspMsg.set_voteforme(false);
            }
                // 假设他的任期更大，那么比较日志那个更新
            else {
                if (log.empty()) {
                    LOGDEBUG("the log is empty.");
                    becomeFollower(requestVote.term(), requestVote.candidateid());
                    voteRspMsg.set_term(currentTerm);
                    voteRspMsg.set_voteforme(true);
                    break;
                }
                const ::ToyRaft::RaftLog &lastAppendLog = log.back();
                // 首先比较的是最后提交的日志的任期
                // 投票请求的最后提交的日志的任期小于当前日志的最后任期
                // 那么不投票给他
                LOGDEBUG("last log term:%d. requestVote term:%d.", lastAppendLog.term(), requestVote.lastlogterm());
                if (lastAppendLog.term() > requestVote.lastlogterm()) {
                    LOGDEBUG("election term is smaller.");
                    voteRspMsg.set_term(currentTerm);
                    voteRspMsg.set_voteforme(false);
                }
                    // 投票请求的最后提交的日志的任期等于当前日志的最后任期
                    // 那么比较应用到状态机的日志index
                else if (lastAppendLog.term() == requestVote.lastlogterm()) {
                    LOGDEBUG("election term is equal.");
                    // 假如当前应用到状态机的index比投票请求的大
                    // 那么不投票给他
                    if (static_cast<int>(log.size() - 1) > requestVote.lastlogindex()) {
                        LOGDEBUG("log's term is bigger.");
                        voteRspMsg.set_term(currentTerm);
                        voteRspMsg.set_voteforme(false);
                    } else {
                        // 投票给候选者，并把自己的状态变为follower，
                        // 设置当前任期和投票给的人
                        LOGDEBUG("log's term is smaller.");
                        becomeFollower(requestVote.term(), requestVote.candidateid());
                        voteRspMsg.set_term(currentTerm);
                        voteRspMsg.set_voteforme(true);
                    }
                }
                    // 投票请求的最后提交的日志的大于等于当前日志的最后任期
                else {
                    // 投票给候选者，并把自己的状态变为follower，
                    // 设置当前任期和投票给的人
                    LOGDEBUG("election term is bigger.");
                    becomeFollower(requestVote.term(), requestVote.candidateid());
                    voteRspMsg.set_term(currentTerm);
                    voteRspMsg.set_voteforme(true);
                }
            }
        } while (false);
        ::ToyRaft::AllSend requestVoteRsp;
        requestVoteRsp.set_sendfrom(id);
        requestVoteRsp.set_sendtype(::ToyRaft::AllSend::VOTERSP);
        std::string voteRspMsgBuf;
        voteRspMsg.SerializeToString(&voteRspMsgBuf);
        requestVoteRsp.set_sendbuf(voteRspMsgBuf.c_str(), voteRspMsgBuf.size());
        ret = RaftNet::sendToNet(requestVote.candidateid(), requestVoteRsp);
        return ret;
    }

    /**
     * @brief 接收到请求投票的response
     * @param requestVoteResponse
     * @return
     */
    int Raft::handleRequestVoteResponse(const ::ToyRaft::RequestVoteResponse &requestVoteResponse, int64_t sendFrom) {
        int ret = 0;
        if (Status::CANDIDATE != state) {
            return 0;
        }
        LOGDEBUG("recv %d the return vote:%d", sendFrom, requestVoteResponse.voteforme());
        if (requestVoteResponse.term() == currentTerm && requestVoteResponse.voteforme()) {
            nodes[sendFrom]->isVoteForMe = true;
        }
        int voteCount = 0;
        for (auto nodesIt = nodes.begin(); nodes.end() != nodesIt; ++nodesIt) {
            if (nodesIt->second->isVoteForMe) {
                voteCount++;
            }
        }
        LOGDEBUG("voteCount:%d", voteCount);
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
    int Raft::handleRequestAppend(const ::ToyRaft::RequestAppend &requestAppend, int64_t sendFrom) {
        int ret = 0;
        ::ToyRaft::RequestAppendResponse appendRsp;
        canVote = true;

        // 当currentTerm小于或者等于requestAppend的term的时候，那么直接成为follower,并处理appendLog请求
        if (currentTerm <= requestAppend.term()) {
            becomeFollower(requestAppend.term(), requestAppend.currentleaderid());
            // 当前的log的最后的index小于preLogIndex，那么直接返回错误
            LOGDEBUG("pre log index:%d", requestAppend.prelogindex());
            if (static_cast<int>(log.size()) - 1 < requestAppend.prelogindex()) {
                appendRsp.set_term(currentTerm);
                appendRsp.set_lastlogindex(static_cast<int>(log.size()) - 1);
                appendRsp.set_success(false);
            } else {
                auto &appendEntries = requestAppend.entries();
                bool isMatch = false;
                // 判断是否匹配上

                if (requestAppend.prelogindex() < commitIndex) { // 已经提交的不能再被覆盖
                    isMatch = false;
                } else if (-1 == requestAppend.prelogindex()) { // requestAppend->preLogIndex 为-1 一定是匹配上的
                    isMatch = true;
                } else { // 任期一致，也匹配上
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
                    int64_t  lastCommit = commitIndex;
                    commitIndex = min(requestAppend.leadercommit(), static_cast<int>(log.size()) - 1);
                    ret = logToStable(lastCommit + 1, commitIndex - lastCommit);
                    if (commitIndex > lastAppliedIndex) {
                        ret = apply();
                    }
                    ret = packageStatus();
                    appendRsp.set_term(currentTerm);
                    appendRsp.set_lastlogindex(static_cast<int>(log.size()) - 1);
                    appendRsp.set_success(true);
                } else {
                    appendRsp.set_term(currentTerm);
                    appendRsp.set_lastlogindex(static_cast<int>(log.size()) - 1);
                    appendRsp.set_success(false);
                }
            }
        }
            // 当前的term大于requestAppend的term，
            // 那么说明这个leader是过期的，直接返回false，并告诉他出现异常
        else {
            LOGDEBUG("Term is smaller.");
            appendRsp.set_term(currentTerm);
            appendRsp.set_lastlogindex(static_cast<int>(log.size()) - 1);
            appendRsp.set_success(false);
        }
        ::ToyRaft::AllSend requestAppendRsp;
        appendRsp.set_sentbackid(id);
        requestAppendRsp.set_sendfrom(id);
        requestAppendRsp.set_sendtype(::ToyRaft::AllSend::APPENDRSP);
        std::string appendRspBuf;
        appendRsp.SerializeToString(&appendRspBuf);
        requestAppendRsp.set_sendbuf(appendRspBuf.c_str(), appendRspBuf.size());
        ret = RaftNet::sendToNet(requestAppend.currentleaderid(), requestAppendRsp);
        return ret;
    }

    /**
     * @brief 接收到了添加log的回复
     * @param requestAppendResponse
     * @return
     */
    int
    Raft::handleRequestAppendResponse(const ::ToyRaft::RequestAppendResponse &requestAppendResponse, int64_t sendFrom) {
        int ret = 0;
        if (Status::LEADER == state) {
            if (nodes.end() == nodes.find(requestAppendResponse.sentbackid())) {
                ret = -1;
            } else {
                auto &node = nodes[requestAppendResponse.sentbackid()];
                if (requestAppendResponse.success()) {
                    node->matchIndex = requestAppendResponse.lastlogindex();
                    node->nextIndex = node->matchIndex + 1;
                } else {
                    size_t peersLastLog = requestAppendResponse.lastlogindex() + 1;
                    size_t selfLastLog = log.size();
                    if (requestAppendResponse.lastlogindex() + 1 <= log.size()) {
                        node->nextIndex = peersLastLog > selfLastLog ? selfLastLog : peersLastLog;
                    }
                    else if (0 != node->nextIndex) {
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
    int Raft::leaderCommit() {
        int ret = 0;
        std::vector<int> commitCount(log.size() - 1 - commitIndex, 1);
        for (auto nodeIt = nodes.begin(); nodes.end() != nodeIt; ++nodeIt) {
            if (nodeIt->second->id != id) {
                if (-1 != nodeIt->second->matchIndex) {
                    for (int i = 0; i < nodeIt->second->matchIndex - commitIndex; i++) {
                        commitCount[i]++;
                    }
                }
            }
        }
        int i = static_cast<int>(commitCount.size()) - 1;
        for (; i >= 0; i--) {
            if ((nodes.size() / 2 + 1) <= commitCount[i]) {
                break;
            }
        }
        int64_t lastCommit = commitIndex;
        if (i >= 0 && log[commitIndex + i + 1].term() == currentTerm) {
            commitIndex = commitIndex + i + 1;
        }
        ret = logToStable(lastCommit + 1, commitIndex - lastCommit);
        if (commitIndex > lastAppliedIndex) {
            ret = apply();
        }
        ret = packageStatus();
        return ret;
    }

    int Raft::apply() {
        int ret = 0;
        lastAppliedIndex++;
        auto &appliedLog = log[lastAppliedIndex];
        LOGERROR("applied type:%d", appliedLog.type());
        if (ToyRaft::RaftLog::MEMBER == appliedLog.type()) {
            const std::string &jsonData = appliedLog.buf();
            ret = RaftConfig::checkConfig(jsonData);
            if (0 != ret) {
                LOGERROR("new config error.ret %d", ret);
                return ret;
            }
            return RaftConfig::flushConf(jsonData);
        }
        return ret;
    }

    int Raft::logToStable(int start, int size) {
        std::vector<std::string> saveLog(size);
        int logIndex = start;
        for (int i = 0; i < size; i++) {
            std::string logIndexSerialize;
            log[logIndex++].SerializeToString(&logIndexSerialize);
            saveLog[i].assign(logIndexSerialize);
        }
        return RaftSave::getInstance()->saveData(start, saveLog);
    }

    int Raft::updatePeers() {
        auto nodesConfigMap = RaftConfig::getNodes();
        if (nodesConfigMap.size() == nodes.size()) {
            return 0;
        } else if (nodesConfigMap.size() > nodes.size()) {
            auto addId = id;
            for (auto nodesConfigMapIt = nodesConfigMap.begin();
                 nodesConfigMapIt != nodesConfigMap.end(); ++nodesConfigMapIt) {
                if (nodes.find(nodesConfigMapIt->first) == nodes.end()) {
                    addId = nodesConfigMapIt->first;
                    break;
                }
            }
            LOGDEBUG("add a node.id:%d", addId);
            if (nodes.find(addId) != nodes.end()) {
                return 0;
            }
            nodes[addId] = std::make_shared<Peers>(addId, 0, -1);
        } else {
            auto removeId = id;
            for (auto nodesIt = nodes.begin(); nodesIt != nodes.end(); ++nodesIt) {
                if (nodesConfigMap.find(nodesIt->first) == nodesConfigMap.end()) {
                    removeId = nodesIt->first;
                    break;
                }
            }
            LOGDEBUG("remove a node.id:%d", removeId);
            if (nodes.find(removeId) == nodes.end()) {
                return 0;
            }
            nodes.erase(removeId);
        }
        return 0;
    }

    int Raft::packageStatus() {
        rapidjson::Value ans(rapidjson::kObjectType);
        rapidjson::Document document;
        rapidjson::Document::AllocatorType &alloc = document.GetAllocator();
        ans.AddMember("leaderId", rapidjson::Value().SetInt(currentLeaderId), alloc);
        ans.AddMember("status", rapidjson::Value().SetInt(state), alloc);
        ans.AddMember("commitIndex", rapidjson::Value().SetInt(commitIndex), alloc);
        ans.AddMember("applied", rapidjson::Value().SetInt(lastAppliedIndex), alloc);
        ans.AddMember("canVote", rapidjson::Value().SetBool(canVote), alloc);
        rapidjson::StringBuffer buff;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buff);
        ans.Accept(writer);
        std::string tempJsonString = buff.GetString();
        return RaftSave::getInstance()->saveMeta(tempJsonString);
    }

    int Raft::recoverStatus() {
        std::string statusStr;
        int ret = RaftSave::getInstance()->getMeta(statusStr);
        if (ret < 0) {
            return 0;
        }
        rapidjson::Document doc;
        doc.Parse(statusStr.c_str(), statusStr.size());
        commitIndex = doc["commitIndex"].GetInt();
        lastAppliedIndex = doc["applied"].GetInt();
        std::vector<std::string> arr;
        RaftSave::getInstance()->getData(0, commitIndex + 1, arr);
        log.resize(arr.size());
        commitIndex = arr.size() - 1;
        if (lastAppliedIndex > commitIndex) {
            lastAppliedIndex = commitIndex;
        }
        for (int i = 0; i < arr.size(); i++) {
            log[i].ParseFromString(arr[i]);
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
    int Raft::recvInnerMsg() {
        int ret = 0;
        while (true) {
            ::ToyRaft::AllSend allSend;
            ret = RaftNet::recvFromNet(&allSend);
            if (0 >= ret) {
                break;
            }
            // 将不在config里面的消息都丢弃
            if (!RaftConfig::checkNodeExists(allSend.sendfrom())) {
                LOGDEBUG("meet id not not in the config.");
                continue;
            }
            switch (allSend.sendtype()) {
                case ::ToyRaft::AllSend::REQVOTE: {
                    LOGDEBUG("recv the reqvote.");
                    RequestVote requestVote;
                    requestVote.ParseFromString(allSend.sendbuf());
                    ret = handleRequestVote(requestVote, allSend.sendfrom());
                }
                    break;
                case ::ToyRaft::AllSend::VOTERSP: {
                    LOGDEBUG("recv the votersp.");
                    RequestVoteResponse requestVoteResponse;
                    requestVoteResponse.ParseFromString(allSend.sendbuf());
                    ret = handleRequestVoteResponse(requestVoteResponse, allSend.sendfrom());
                }
                    break;
                case ::ToyRaft::AllSend::REQAPPEND: {
                    LOGDEBUG("recv the reqappend.");
                    RequestAppend requestAppend;
                    requestAppend.ParseFromString(allSend.sendbuf());
                    ret = handleRequestAppend(requestAppend, allSend.sendfrom());
                }
                    break;
                case ::ToyRaft::AllSend::APPENDRSP: {
                    LOGDEBUG("recv the appendrsp.");
                    RequestAppendResponse requestAppendResponse;
                    requestAppendResponse.ParseFromString(allSend.sendbuf());
                    ret = handleRequestAppendResponse(requestAppendResponse, allSend.sendfrom());
                }
                    break;
                default:
                    break;
            }
        }
        return ret;
    }
} // namespace ToyRaft
