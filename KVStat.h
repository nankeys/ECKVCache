#pragma once
#include <string>

struct KVStat {
    std::string key;
    int size;       // in bytes
    int frequency;  // access count
};
