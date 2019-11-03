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
        std::string innerIP_;
        int innerPort_;
        std::string outerIP_;
        int outerPort_;

        NodeConfig(int64_t id, const std::string &innerIP, int innerPort, const std::string &outerIP, int outerPort);
    };

    class RaftConfig {
    public:
        RaftConfig(const std::string &path);

        static std::unordered_map<int, std::shared_ptr<NodeConfig> > getNodes();

        static std::string getInnerIP();

        static int getInnerPort();

        static std::string getOuterIP();

        static int getOuterPort();

        static int getId();

    private:
        static void loadConfigWrap();

        static int loadConfig();

        static int id_;

        static std::string raftConfigPath_;
        static std::atomic<int> nowBufIndex;
        static std::vector<std::unordered_map<int, std::shared_ptr<NodeConfig> > > NodesConf;
    };
}// namespace ToyRaft

#endif //TOYRAFT_CONFIG_H
