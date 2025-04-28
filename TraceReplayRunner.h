#pragma once
#include <string>
#include <unordered_map>
#include "ErasureCacheClient.h"
#include "KVStatReader.h"

class TraceReplayRunner {
public:
    TraceReplayRunner(const std::string& traceFile,
                      const std::string& statFile,
                      ErasureCacheClient& cacheClient);
    void run();

private:
    std::string tracePath;
    std::string statPath;
    ErasureCacheClient& client;
    std::unordered_map<std::string, KVStat> statMap;

    void loadStat();
    void cleanup();
};
