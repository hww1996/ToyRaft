//
// Created by hww1996 on 2019/10/6.
//

#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

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

        static bool checkNodeExists(int64_t id);

        static std::string getRaftSavePath();

        static int flushConf(const std::string &b);

        static int checkConfig(const std::string &jsonData);

    private:
        enum RAFTCONFIG_ERR {
            NO_NODES = 1,
            NODES_NOT_ARRAY,
            NODES_MEM_NOT_OBJ,
            NO_NODES_MEM_ID,
            NODES_MEM_ID_NOT_NUM,
            NO_NODES_MEM_INNERIP,
            NODES_MEM_INNERIP_NOT_STR,
            NO_NODES_MEM_INNERPORT,
            NODES_MEM_INNERPORT_NOT_NUM,
            NO_NODES_MEM_OUTERIP,
            NODES_MEM_OUTERIP_NOT_STR,
            NO_NODES_MEM_OUTERPORT,
            NODES_MEM_OUTERPORT_NOT_NUM,
        };

        static void loadConfigWrap();

        static int loadDynamicConfig();

        static int loadStaticConfig();

        static int id_;

        static std::string raftConfigPath_;
        static std::string raftStaticConfigPath_;
        static std::string raftDynamicConfigPath_;
        static std::string raftSavePath_;
        static std::atomic<int> nowBufIndex;
        static std::vector<std::unordered_map<int, std::shared_ptr<NodeConfig> > > NodesConf;
    };
}// namespace ToyRaft

#endif //TOYRAFT_CONFIG_H
