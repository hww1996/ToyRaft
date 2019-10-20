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
    };

    class RaftConfig {
    public:
        RaftConfig(const std::string &path);

        static std::unordered_map<int, std::shared_ptr<NodeConfig> > getNodes();

        static int getInnerPort();

        static int getOuterPort();

        static int getId();

    private:
        static void loadConfigWrap();

        static int loadConfig();

        static int innerPort_;
        static int outerPort_;
        static int id_;

        static std::string raftConfigPath_;
        static std::atomic<int> nowBuf;
        static std::vector<std::unordered_map<int, std::shared_ptr<NodeConfig> > > NodesConf;
    };
}// namespace ToyRaft

#endif //TOYRAFT_CONFIG_H
