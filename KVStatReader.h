#pragma once
#include <string>
#include <vector>
#include "KVStat.h"

class KVStatReader {
public:
    explicit KVStatReader(const std::string& filename);
    std::vector<KVStat> readAll();
private:
    std::string filepath;
};
