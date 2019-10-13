//
// Created by hww1996 on 2019/10/6.
//

#include <fstream>
#include <cassert>
#include <thread>

#include <rapidjson/document.h>

#include "config.h"
#include "globalmutext.h"

namespace ToyRaft {

    NodeConfig::NodeConfig(int64_t id, const std::string &ip, int port) : id_(id), ip_(ip), port_(port) {}

    NodeConfig::NodeConfig(const ToyRaft::NodeConfig &nodeConfig) {
        if (this == &nodeConfig)
            return;
        this->id_ = nodeConfig.id_;
        this->ip_ = nodeConfig.ip_;
        this->port_ = nodeConfig.port_;
    }

    NodeConfig &NodeConfig::operator=(const ToyRaft::NodeConfig &nodeConfig) {
        if (this == &nodeConfig)
            return *this;
        this->id_ = nodeConfig.id_;
        this->ip_ = nodeConfig.ip_;
        this->port_ = nodeConfig.port_;
        return *this;
    }

    std::vector<std::unordered_map<int, std::shared_ptr<NodeConfig> > > NodesConfig::NodesConf(2,
                                                                                               std::unordered_map<int, std::shared_ptr<NodeConfig> >());
    std::string NodesConfig::configPath_;

    int NodesConfig::nowBuf;

    NodesConfig::NodesConfig(const std::string &path) {
        configPath_ = path;
        loadConfig();
        std::thread loadConfigThread(loadConfig);
        loadConfigThread.detach();
    }

    std::unordered_map<int, std::shared_ptr<NodeConfig> > NodesConfig::get() {
        return NodesConf[nowBuf];
    }

    static void clearNodesConf(std::unordered_map<int, std::shared_ptr<NodeConfig> > &config) {
        for (auto configIt = config.begin(); config.end() != configIt;) {
            config.erase(configIt++);
        }
    }

    int NodesConfig::loadConfig() {
        int ret = 0;
        std::string jsonData;
        // 这里需要采取rename的方式写入文件，因为rename在操作系统层面是原子操作
        // 共享读影响
        std::fstream file;
        file.open(configPath_, std::fstream::in);
        file >> jsonData;
        file.close();

        int secondConfIndex = 1 - nowBuf;

        auto &secondConf = NodesConf[secondConfIndex];

        clearNodesConf(secondConf);

        rapidjson::Document doc;
        doc.Parse(jsonData.c_str(), jsonData.size());

        assert(doc.HasMember("nodes"));
        const rapidjson::Value &nodes = doc["nodes"];
        assert(nodes.IsArray());
        for (rapidjson::SizeType i = 0; i < nodes.Size(); i++) {

            assert(nodes[i].IsObject());
            assert(nodes[i].HasMember("id"));
            assert(nodes[i]["id"].IsNumber());
            assert(nodes[i].HasMember("ip"));
            assert(nodes[i]["ip"].IsNumber());
            assert(nodes[i].HasMember("port"));
            assert(nodes[i]["port"].IsNumber());
            int nowId = nodes[i]["id"].GetInt();
            secondConf[nowId] = std::shared_ptr<NodeConfig>(
                    new NodeConfig(nodes[i]["id"].GetInt(), nodes[i]["ip"].GetString(), nodes[i]["port"].GetInt()));
        }

        nowBuf = secondConfIndex;

        return ret;
    }

    ServerConfig::ServerConfig(const std::string &path) : configPath_(path) {
        loadConfig();
    }

    int ServerConfig::getId() {
        return id_;
    }

    int ServerConfig::getInnerPort() {
        return innerPort_;
    }

    int ServerConfig::getOuterPort() {
        return outerPort_;
    }

    int ServerConfig::loadConfig() {
        int ret = 0;
        std::string jsonData;
        std::fstream file;
        file.open(configPath_, std::fstream::in);
        file >> jsonData;
        file.close();

        rapidjson::Document doc;
        doc.Parse(jsonData.c_str(), jsonData.size());

        assert(doc.HasMember("innerListenPort"));
        const rapidjson::Value &innerListenPort = doc["innerListenPort"];
        assert(innerListenPort.IsNumber());
        innerPort_ = innerListenPort.GetInt();

        assert(doc.HasMember("outerListenPort"));
        const rapidjson::Value &outerListenPort = doc["outerListenPort"];
        assert(outerListenPort.IsNumber());
        outerPort_ = outerListenPort.GetInt();

        assert(doc.HasMember("id"));
        const rapidjson::Value &nodeId = doc["id"];
        assert(nodeId.IsNumber());
        id_ = nodeId.GetInt();

        return ret;
    }
}// namespace ToyRaft
