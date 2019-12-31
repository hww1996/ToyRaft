//
// Created by hww1996 on 2019/12/21.
//

#include <string>
#include <vector>
#include <memory>

#include "leveldb/db.h"

#ifndef TOYRAFT_RAFTSAVE_H
#define TOYRAFT_RAFTSAVE_H

namespace ToyRaft {
    class RaftSave {
    public:
        static std::shared_ptr<RaftSave> getInstance();

        int saveData(int64_t start, const std::vector<std::string> &data);

        int getData(int64_t start, size_t size, std::vector<std::string> &res);

        int saveMeta(const std::string &value);

        int getMeta(std::string &value);

        ~RaftSave();

    private:
        RaftSave();

        RaftSave(RaftSave &) = delete;

        RaftSave &operator=(RaftSave &) = delete;

        std::string getKey(int64_t key);

        leveldb::DB *db;

        static std::shared_ptr<RaftSave> raftSavePtr;
    };
} // namespace ToyRaft

#endif //TOYRAFT_RAFTSAVE_H
