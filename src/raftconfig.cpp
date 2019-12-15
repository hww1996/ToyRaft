//
// Created by hww1996 on 2019/10/6.
//

#include <fstream>
#include <cassert>
#include <thread>
#include <iterator>
#include <cstdio>

#include <rapidjson/document.h>

#include "raftconfig.h"
#include "globalmutex.h"
#include "logger.h"

namespace ToyRaft {

    NodeConfig::NodeConfig(int64_t id, const std::string &innerIP, int innerPort, const std::string &outerIP,
                           int outerPort) : id_(id), innerIP_(innerIP), innerPort_(innerPort), outerIP_(outerIP),
                                            outerPort_(outerPort) {}


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
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if (0 != loadConfig()) {
                break;
            }
        }
    }

    int RaftConfig::loadConfig() {
        int ret = 0;

        // 这里需要采取rename的方式写入文件，因为rename在操作系统层面是原子操作
        // 共享读影响
        std::fstream file;
        file.open(raftConfigPath_, std::fstream::in);
        std::istreambuf_iterator<char> beg(file), end;
        std::string jsonData(beg, end);
        file.close();

        int secondConfIndex = 1 - nowBufIndex.load();

        auto &secondConf = NodesConf[secondConfIndex];

        clearNodesConf(secondConf);

        rapidjson::Document doc;
        doc.Parse(jsonData.c_str(), jsonData.size());

        assert(doc.HasMember("id"));
        const rapidjson::Value &nodeId = doc["id"];
        assert(nodeId.IsNumber());
        id_ = nodeId.GetInt();

        assert(doc.HasMember("nodes"));
        const rapidjson::Value &nodes = doc["nodes"];
        assert(nodes.IsArray());
        for (rapidjson::SizeType i = 0; i < nodes.Size(); i++) {

            assert(nodes[i].IsObject());
            assert(nodes[i].HasMember("id"));
            assert(nodes[i]["id"].IsNumber());
            assert(nodes[i].HasMember("innerIP"));
            assert(nodes[i]["innerIP"].IsString());
            assert(nodes[i].HasMember("innerPort"));
            assert(nodes[i]["innerPort"].IsNumber());
            assert(nodes[i].HasMember("outerIP"));
            assert(nodes[i]["outerIP"].IsString());
            assert(nodes[i].HasMember("outerPort"));
            assert(nodes[i]["outerPort"].IsNumber());
            int nowId = nodes[i]["id"].GetInt();
            secondConf[nowId] = std::make_shared<NodeConfig>(nodes[i]["id"].GetInt(), nodes[i]["innerIP"].GetString(),
                                                             nodes[i]["innerPort"].GetInt(),
                                                             nodes[i]["outerIP"].GetString(),
                                                             nodes[i]["outerPort"].GetInt());
        }

        nowBufIndex.fetch_xor(1);

        return ret;
    }

    int RaftConfig::getId() {
        return id_;
    }

    std::string RaftConfig::getInnerIP() {
        return NodesConf[nowBufIndex.load()][id_]->innerIP_;
    }

    int RaftConfig::getInnerPort() {
        return NodesConf[nowBufIndex.load()][id_]->innerPort_;
    }

    std::string RaftConfig::getOuterIP() {
        return NodesConf[nowBufIndex.load()][id_]->outerIP_;
    }

    int RaftConfig::getOuterPort() {
        return NodesConf[nowBufIndex.load()][id_]->outerPort_;
    }

    bool RaftConfig::checkNodeExists(int64_t id) {
        auto conf = NodesConf[nowBufIndex.load()];
        return conf.end() != conf.find(id);
    }

    int RaftConfig::flushConf(const std::string &b) {
        std::string tempFileName = ".temp.newconfig";
        std::fstream file;
        file.open(tempFileName, std::fstream::out | std::fstream::binary);
        file << b;
        file.close();
        return rename(tempFileName.c_str(), raftConfigPath_.c_str());
    }

    int RaftConfig::checkConfig(const std::string &jsonData) {
        rapidjson::Document doc;
        doc.Parse(jsonData.c_str(), jsonData.size());

        if (!doc.HasMember("id")) {
            return RAFTCONFIG_ERR::NO_ID;
        }
        const rapidjson::Value &nodeId = doc["id"];
        if (!nodeId.IsNumber()) {
            return RAFTCONFIG_ERR::ID_NOT_NUM;
        }

        if (!doc.HasMember("nodes")) {
            return RAFTCONFIG_ERR::NO_NODES;
        }
        const rapidjson::Value &nodes = doc["nodes"];
        if (!nodes.IsArray()) {
            return RAFTCONFIG_ERR::NODES_NOT_ARRAY;
        }
        for (rapidjson::SizeType i = 0; i < nodes.Size(); i++) {
            if (!nodes[i].IsObject()) {
                return RAFTCONFIG_ERR::NODES_MEM_NOT_OBJ;
            }
            if (!nodes[i].HasMember("id")) {
                return RAFTCONFIG_ERR::NO_NODES_MEM_ID;
            }
            if (!nodes[i]["id"].IsNumber()) {
                return RAFTCONFIG_ERR::NODES_MEM_ID_NOT_NUM;
            }
            if (!nodes[i].HasMember("innerIP")) {
                return RAFTCONFIG_ERR::NO_NODES_MEM_INNERIP;
            }
            if (!nodes[i]["innerIP"].IsString()) {
                return RAFTCONFIG_ERR::NODES_MEM_INNERIP_NOT_STR;
            }
            if (!nodes[i].HasMember("innerPort")) {
                return RAFTCONFIG_ERR::NO_NODES_MEM_INNERPORT;
            }
            if (!nodes[i]["innerPort"].IsNumber()) {
                return RAFTCONFIG_ERR::NODES_MEM_INNERPORT_NOT_NUM;
            }
            if (!nodes[i].HasMember("outerIP")) {
                return RAFTCONFIG_ERR::NO_NODES_MEM_OUTERIP;
            }
            if (!nodes[i]["outerIP"].IsString()) {
                return RAFTCONFIG_ERR::NODES_MEM_OUTERIP_NOT_STR;
            }
            if (!nodes[i].HasMember("outerPort")) {
                return RAFTCONFIG_ERR::NO_NODES_MEM_OUTERPORT;
            }
            if (!nodes[i]["outerPort"].IsNumber()) {
                return RAFTCONFIG_ERR::NODES_MEM_OUTERPORT_NOT_NUM;
            }
        }
        return 0;
    }
}// namespace ToyRaft
