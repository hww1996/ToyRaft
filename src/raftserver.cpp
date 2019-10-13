//
// Created by hww1996 on 2019/10/13.
//
#include "raftserver.h"
#include "globalmutext.h"
#include "raft.h"
#include "networking.h"

namespace ToyRaft {
    std::string RaftServer::nodesConfigPath_;
    std::string RaftServer::serverConfigPath_;
    std::deque<::ToyRaft::RaftClientMsg> RaftServer::request;
    std::vector<std::string> RaftServer::readBuffer;
    int RaftServer::commit_ = 0;

    RaftServer::RaftServer(const std::string &nodesConfigPath, const std::string &serverConfigPath) {
        nodesConfigPath_ = nodesConfigPath;
        serverConfigPath_ = serverConfigPath;
        RaftNet r(serverConfigPath_);
    }

    int RaftServer::serverForever() {
        int ret = 0;
        Raft raft(serverConfigPath_);
        while(true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ret = raft.tick();
            if(ret != 0) {
                break;
            }
        }
        return ret;
    }

    int RaftServer::pushReadBuffer(int start, int commit, const std::vector<::ToyRaft::RaftLog> &log) {
        int ret = 0;
        {
            std::lock_guard<std::mutex> lock(GlobalMutex::readBufferMutex);
            int index = start;
            commit_ = commit;
            for (; index < readBuffer.size(); index++) {
                readBuffer[index].assign(log[index].buf().c_str(), log[index].buf().size());
            }
            for (; index < log.size(); index++) {
                readBuffer.emplace_back(log[index].buf().c_str(), log[index].buf().size());
            }
        }
        return ret;
    }

    int RaftServer::getReadBuffer(std::vector<std::string> &buf, int from, int to) {
        int ret = 0;
        {
            std::lock_guard<std::mutex> lock(GlobalMutex::readBufferMutex);
            if (from < 0) {
                return -1;
            }
            if (from >= to) {
                return -2;
            }
            if (to > commit_ + 1) {
                to = commit_ + 1;
            }
            for (int i = from; i < to; i++) {
                buf.emplace_back(readBuffer[i].c_str(), readBuffer[i].size());
            }
        }
        return ret;
    }
} // namespace ToyRaft

