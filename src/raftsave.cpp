//
// Created by hww1996 on 2019/12/21.
//

#include <mutex>
#include <cstdio>

#include "raftsave.h"
#include "raftconfig.h"
#include "globalmutex.h"

#include "leveldb/options.h"

namespace ToyRaft {

    std::shared_ptr<RaftSave> RaftSave::raftSavePtr = nullptr;

    RaftSave::RaftSave() : db(nullptr) {
        leveldb::Options options;
        options.create_if_missing = true;
        leveldb::Status status = leveldb::DB::Open(options, RaftConfig::getRaftSavePath(), &db);
        assert(status.ok());
    }

    RaftSave::~RaftSave() {
        delete db;
    }

    std::shared_ptr<RaftSave> RaftSave::getInstance() {
        {
            std::lock_guard<std::mutex> lock(GlobalMutex::raftSaveMutex);
            if (nullptr == raftSavePtr) {
                raftSavePtr = std::shared_ptr<RaftSave>(new RaftSave);
            }
        }
        return raftSavePtr;
    }

    std::string RaftSave::getKey(int64_t key) {
        char c[100] = {0};
        sprintf(c, "%019lld", key);
        return std::string(c);
    }

    int RaftSave::saveData(int64_t start, const std::vector<std::string> &data) {
        int ret = 0;
        for (int i = 0; i < data.size(); i++) {
            std::string key = getKey(start + i);
            leveldb::Status s = db->Put(leveldb::WriteOptions(), key, data[i]);
            if (!s.ok()) {
                return -1;
            }
        }
        return ret;
    }

    int RaftSave::getData(int64_t start, size_t size, std::vector<std::string> &res) {
        int ret = 0;
        res.resize(size);
        for (int index = 0; index < size; index++) {
            std::string key = getKey(start + index);
            leveldb::Status s = db->Get(leveldb::ReadOptions(), key, &res[index]);
            if (!s.ok()) {
                if (s.IsNotFound()) {
                    res.resize(index);
                    break;
                } else {
                    return -1;
                }
            }
        }
        return ret;
    }

    int RaftSave::saveMeta(const std::string &value) {
        int ret = 0;
        std::string key = getKey(0x7fffffffffffffff);
        leveldb::Status s = db->Put(leveldb::WriteOptions(), key, value);
        if (!s.ok()) {
            return -1;
        }
        return ret;
    }

    int RaftSave::getMeta(std::string &value) {
        int ret = 0;
        std::string key = getKey(0x7fffffffffffffff);
        leveldb::Status s = db->Get(leveldb::ReadOptions(), key, &value);
        if (!s.ok()) {
            return -1;
        }
        return ret;
    }
} // namespace ToyRaft

