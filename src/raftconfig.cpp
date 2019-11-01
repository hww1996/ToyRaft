//
// Created by hww1996 on 2019/10/6.
//

#include <fstream>
#include <cassert>
#include <thread>
#include <iterator>

#include <rapidjson/document.h>

#include "raftconfig.h"
#include "globalmutex.h"
#include "logger.h"

namespace ToyRaft {

    NodeConfig::NodeConfig(int64_t id, const std::string &ip, int port) : id_(id), ip_(ip), port_(port) {}

    int RaftConfig::outerPort_;
    int RaftConfig::innerPort_;

    int RaftConfig::id_;

    std::string RaftConfig::raftConfigPath_;

    std::atomic<int> RaftConfig::nowBufIndex;

    std::vector<std::unordered_map<int, std::shared_ptr<NodeConfig> > > RaftConfig::NodesConf(2,
                                                                                              std::unordered_map<int, std::shared_ptr<NodeConfig> >());

    RaftConfig::RaftConfig(const std::string &path) {
        raftConfigPath_ = path;
        nowBufIndex = 1;
        loadConfig();
        std::thread loadConfigThread(loadConfigWrap);
        loadConfigThread.detach();
    }

    std::unordered_map<int, std::shared_ptr<NodeConfig> > RaftConfig::getNodes() {
        return NodesConf[nowBufIndex.load()];
    }

    static void clearNodesConf(std::unordered_map<int, std::shared_ptr<NodeConfig> > &config) {
        LOGDEBUG("clear the config OK.");
        for (auto configIt = config.begin(); config.end() != configIt;) {
            config.erase(configIt++);
        }
    }

    void RaftConfig::loadConfigWrap() {
        LOGDEBUG("start load config");
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(3*86400));
            if (0 != loadConfig()) {
                break;
            }
        }
    }

    int RaftConfig::loadConfig() {
        LOGDEBUG("loading the config");
        int ret = 0;

        // 这里需要采取rename的方式写入文件，因为rename在操作系统层面是原子操作
        // 共享读影响
        std::fstream file;
        file.open(raftConfigPath_, std::fstream::in);
        std::istreambuf_iterator<char> beg(file), end;
        std::string jsonData(beg, end);
        file.close();

        LOGDEBUG("loading the file OK");
        LOGDEBUG("before change.nowBufIndex: %d", nowBufIndex.load());

        int secondConfIndex = 1 - nowBufIndex.load();

        auto &secondConf = NodesConf[secondConfIndex];
        
        LOGDEBUG("before cleaning.second conf length: %d", secondConf.size());

        clearNodesConf(secondConf);

        LOGDEBUG("after cleaning.second conf length: %d", secondConf.size());

        rapidjson::Document doc;
        doc.Parse(jsonData.c_str(), jsonData.size());

        assert(doc.HasMember("id"));
        const rapidjson::Value &nodeId = doc["id"];
        assert(nodeId.IsNumber());
        id_ = nodeId.GetInt();

        assert(doc.HasMember("outerListenPort"));
        const rapidjson::Value &outerListenPort = doc["outerListenPort"];
        assert(outerListenPort.IsNumber());
        outerPort_ = outerListenPort.GetInt();

        assert(doc.HasMember("innerListenPort"));
        const rapidjson::Value &innerListenPort = doc["innerListenPort"];
        assert(innerListenPort.IsNumber());
        innerPort_ = innerListenPort.GetInt();

        assert(doc.HasMember("nodes"));
        const rapidjson::Value &nodes = doc["nodes"];
        assert(nodes.IsArray());
        for (rapidjson::SizeType i = 0; i < nodes.Size(); i++) {

            assert(nodes[i].IsObject());
            assert(nodes[i].HasMember("id"));
            assert(nodes[i]["id"].IsNumber());
            assert(nodes[i].HasMember("ip"));
            assert(nodes[i]["ip"].IsString());
            assert(nodes[i].HasMember("port"));
            assert(nodes[i]["port"].IsNumber());
            int nowId = nodes[i]["id"].GetInt();
            secondConf[nowId] = std::make_shared<NodeConfig>(nodes[i]["id"].GetInt(), nodes[i]["ip"].GetString(),
                                                             nodes[i]["port"].GetInt());
        }

        nowBufIndex.fetch_xor(1);

        LOGDEBUG("after change.nowBufIndex: %d", nowBufIndex.load());

        LOGDEBUG("<<<<<<<<<<<");

        return ret;
    }

    int RaftConfig::getId() {
        return id_;
    }

    int RaftConfig::getInnerPort() {
        return innerPort_;
    }

    int RaftConfig::getOuterPort() {
        return outerPort_;
    }
}// namespace ToyRaft
