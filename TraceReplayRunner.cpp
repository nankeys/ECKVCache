#include "TraceReplayRunner.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <chrono>
#include <random>

TraceReplayRunner::TraceReplayRunner(const std::string& traceFile,
                                     const std::string& statFile,
                                     ErasureCacheClient& cacheClient)
    : tracePath(traceFile), statPath(statFile), client(cacheClient) {
    cleanup();
    loadStat();
}

void TraceReplayRunner::loadStat() {
    KVStatReader reader(statPath);
    bool flag;
    std::cout << "2222222222222222" << std::endl;
    auto stats = reader.readAll();
    std::cout << "1111111111111111" << std::endl;
    for (const auto& s : stats) {
        statMap[s.key] = s;

        // 模拟每个 key 对应一条4KB*k的数据并写入 memcached
        std::vector<uint8_t> value(1024 * client.getK());
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, 255);
        for (auto& byte : value) {
            byte = static_cast<uint8_t>(dist(rng));
        }
        flag = client.put(s.key, value);
        if(!flag){
            std::cerr << "Fail to set blocks." << std::endl;
            exit(1);
        }
    }
    std::cout << "Load stat successfully!" << std::endl;
}

void TraceReplayRunner::run() {
    std::ifstream trace(tracePath);
    if (!trace.is_open()) {
        std::cerr << "Cannot open trace file: " << tracePath << std::endl;
        return;
    }

    std::string line;
    int total = 0, success = 0;
    auto start_total = std::chrono::steady_clock::now();

    while (std::getline(trace, line)) {
	std::stringstream ss(line);
        std::string timestamp, key;

        std::getline(ss, timestamp, ','); // 先读时间戳
        std::getline(ss, key, ',');        // 再读key
        

        total++;
        auto start = std::chrono::steady_clock::now();
        auto recovered = client.get(key);
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (!recovered.empty()) {
            success++;
            std::cout << "[SUCCESS] Key: " << key << ", Size: " << recovered.size()
                      << ", Time: " << ms << "ms\n";
        } else {
            std::cout << "[FAIL] Key: " << key << ", Time: " << ms << "ms\n";
        }
        if(key == "user1573987489603120213") break;
    }

    auto end_total = std::chrono::steady_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_total - start_total).count();
    std::cout << "\nTrace replay complete. Total: " << total
              << ", Success: " << success
              << ", Duration: " << total_ms << "ms\n";
              
    //cleanup();
}

void TraceReplayRunner::cleanup() {
    std::cout << "Flushing all data from Memcached...\n";

    for (auto memc : client.getClients()) {
        memcached_flush(memc, 0);
    }

    std::cout << "Flush complete.\n";
}