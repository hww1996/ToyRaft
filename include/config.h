//
// Created by hww1996 on 2019/10/6.
//

#include <unordered_map>
#include <vector>
#include <string>

#ifndef TOYRAFT_CONFIG_H
#define TOYRAFT_CONFIG_H

namespace ToyRaft {

    struct NodeConfig {
        int64_t id_;
        std::string ip_;
        int port_;

        NodeConfig(int64_t id, const std::string &ip, int port);

        NodeConfig &operator=(const NodeConfig &nodeConfig);

        NodeConfig(const NodeConfig &nodeConfig);
    };

    class NodesConfig {
    public:
        NodesConfig(const std::string &path);

        std::unordered_map<int, std::shared_ptr<NodeConfig> > get();

        int loadConfig();

    private:
        std::string configPath_;
        int nowBuf;
        std::vector<std::unordered_map<int, std::shared_ptr<NodeConfig> > > NodesConf;
    };

    class ServerConfig {
    public:
        ServerConfig(const std::string &path);

        int getPort();

        int getId();

    private:
        int loadConfig();

        std::string configPath_;
        int port_;
        int id_;
    };

}// namespace ToyRaft

#endif //TOYRAFT_CONFIG_H
