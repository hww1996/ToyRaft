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
    std::string RaftConfig::raftStaticConfigPath_;
    std::string RaftConfig::raftDynamicConfigPath_;
    std::string RaftConfig::raftSavePath_;

    std::atomic<int> RaftConfig::nowBufIndex;

    std::vector<std::unordered_map<int, std::shared_ptr<NodeConfig> > > RaftConfig::NodesConf(2,
                                                                                              std::unordered_map<int, std::shared_ptr<NodeConfig> >());

    static void loadJsonData(const std::string &Path, rapidjson::Document &doc) {
        std::fstream file;
        file.open(Path, std::fstream::in);
        std::istreambuf_iterator<char> beg(file), end;
        std::string jsonData(beg, end);
        file.close();
        doc.Parse(jsonData.c_str(), jsonData.size());
    }

    RaftConfig::RaftConfig(const std::string &path) {
        raftConfigPath_ = path;
        nowBufIndex = 1;
        loadStaticConfig();
        loadDynamicConfig();
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
            if (0 != loadDynamicConfig()) {
                break;
            }
        }
    }

    int RaftConfig::loadStaticConfig() {
        int ret = 0;
        rapidjson::Document configDoc;
        loadJsonData(raftConfigPath_, configDoc);
        raftStaticConfigPath_ = configDoc["static_config_file"].GetString();
        raftDynamicConfigPath_ = configDoc["dynamic_config_file"].GetString();

        // 静态文件load
        rapidjson::Document staticConfig;
        loadJsonData(raftStaticConfigPath_, staticConfig);
        assert(staticConfig.HasMember("id"));
        const rapidjson::Value &nodeId = staticConfig["id"];
        assert(nodeId.IsNumber());
        id_ = nodeId.GetInt();
        assert(staticConfig.HasMember("save_path"));
        const rapidjson::Value &savePath = staticConfig["save_path"];
        assert(savePath.IsString());
        raftSavePath_ = savePath.GetString();
        return ret;
    }

    int RaftConfig::loadDynamicConfig() {
        int ret = 0;

        // 这里需要采取rename的方式写入文件，因为rename在操作系统层面是原子操作
        // 共享读影响

        // 动态文件load

        rapidjson::Document dynamicConfig;
        loadJsonData(raftDynamicConfigPath_, dynamicConfig);
        int secondConfIndex = 1 - nowBufIndex.load();

        auto &secondConf = NodesConf[secondConfIndex];

        clearNodesConf(secondConf);


        assert(dynamicConfig.HasMember("nodes"));
        const rapidjson::Value &nodes = dynamicConfig["nodes"];
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
        return rename(tempFileName.c_str(), raftDynamicConfigPath_.c_str());
    }

    int RaftConfig::checkConfig(const std::string &jsonData) {
        rapidjson::Document doc;
        doc.Parse(jsonData.c_str(), jsonData.size());

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

    std::string RaftConfig::getRaftSavePath() {
        return raftSavePath_;
    }
}// namespace ToyRaft
