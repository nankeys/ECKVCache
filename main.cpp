#include "KVStatReader.h"
#include "ErasureCacheClient.h"
#include "TraceReplayRunner.h"
#include <iostream>

int main() {
    int k = 6;
    int n = 9;

    try {
        std::vector<std::string> servers = {"192.168.1.110", "192.168.1.108", "192.168.1.113", "192.168.1.111", "192.168.1.127", "192.168.1.139", "192.168.1.125", "192.168.1.126", "192.168.1.143", "192.168.1.112"};
        ErasureCacheClient client(servers, k, n);
        TraceReplayRunner runner("trace.txt", "stat.txt", client);
        runner.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
