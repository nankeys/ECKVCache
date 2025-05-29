
#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ErasureCacheClient.h"
#include "KVStatReader.h"

class TraceReplayRunner {
public:
    TraceReplayRunner(const std::string& traceFile,
                      const std::string& statFile,
                      ErasureCacheClient& cacheClient);
    void run();
    void run_1st();

private:
    std::string tracePath;
    std::string statPath;
    ErasureCacheClient& client;
    std::unordered_map<std::string, KVStat> statMap;

    std::unordered_set<std::string> hot_keys;       // 热数据key集合
    std::vector<std::string> cold_pending_keys;     // 冷数据待修复列表

    void loadStat();
    void cleanup();
    void repairColdData(); // 新增：空闲时冷数据恢复
};
