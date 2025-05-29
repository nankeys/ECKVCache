
#include "TraceReplayRunner.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <chrono>
#include <random>
#include <stdio.h>
#include <algorithm>
#include <unordered_set>

TraceReplayRunner::TraceReplayRunner(const std::string& traceFile,
                                     const std::string& statFile,
                                     ErasureCacheClient& cacheClient)
    : tracePath(traceFile), statPath(statFile), client(cacheClient) {
    cleanup();
    getchar();
    std::cout << "正在写入。。。" << std::endl;
    loadStat();
}

void TraceReplayRunner::loadStat() {
    KVStatReader reader(statPath);
    bool flag;
    auto stats = reader.readAll();

    // 排序选择热数据
    std::sort(stats.begin(), stats.end(), [](const KVStat& a, const KVStat& b) {
        return a.frequency > b.frequency;
    });

    int hot_count = stats.size() * 0.2;
    for (int i = 0; i < hot_count; ++i) {
        hot_keys.insert(stats[i].key);
    }

    for (const auto& s : stats) {
        statMap[s.key] = s;

        int valsize = (s.size % client.getK() == 0) ? (s.size / client.getK()) : (s.size / client.getK() + 1);
        std::vector<uint8_t> value(valsize * client.getK());
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

void TraceReplayRunner::run_1st() {
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
        std::getline(ss, timestamp, ',');
        std::getline(ss, key, ',');

        if(total == 100) break;
        total++;

        // if (hot_keys.count(key)) {
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
        // } else {
        //     cold_pending_keys.push_back(key);
        // }
    }

    auto end_total = std::chrono::steady_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_total - start_total).count();
    std::cout << "\nTrace replay complete. Total: " << total
              << ", Success: " << success
              << ", Duration: " << total_ms << "ms\n";

    //repairColdData();
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
        std::getline(ss, timestamp, ',');
        std::getline(ss, key, ',');

        if(total == 100) break;
        total++;

        if (hot_keys.count(key)) {
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
        } else {
            cold_pending_keys.push_back(key);
        }
    }

    auto end_total = std::chrono::steady_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_total - start_total).count();
    std::cout << "\nTrace replay complete. Total: " << total
              << ", Success: " << success
              << ", Duration: " << total_ms << "ms\n";

    repairColdData();
}

void TraceReplayRunner::repairColdData() {
    std::cout << "\n[Repairing Cold Data During Idle Time...]\n";
    int repaired = 0;

    for (const auto& key : cold_pending_keys) {
        auto recovered = client.get(key);
        if (!recovered.empty()) {
            repaired++;
            std::cout << "[COLD-RECOVERED] Key: " << key << ", Size: " << recovered.size() << "\n";
        } else {
            std::cout << "[COLD-FAILED] Key: " << key << "\n";
        }
    }

    std::cout << "[Cold Repair Complete] Total: " << cold_pending_keys.size()
              << ", Success: " << repaired << "\n";
    cold_pending_keys.clear();
}

static memcached_return_t stat_printer(const memcached_instance_st *server,
    const char *key, size_t key_length,
    const char *value, size_t value_length,
    void *context)
{
(void)server;
(void)context;
(void)key;
(void)key_length;
(void)value;
(void)value_length;

return MEMCACHED_SUCCESS;
}

void TraceReplayRunner::cleanup() {
    std::cout << "Flushing all data from Memcached...\n";
    for (auto memc : client.getClients()) {
        memcached_flush(memc, 0);
        memcached_stat_execute(memc, "reset", stat_printer, NULL);
    }
    std::cout << "Flush complete.\n";
}
