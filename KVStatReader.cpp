#include "KVStatReader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

KVStatReader::KVStatReader(const std::string& filename) : filepath(filename) {}

std::vector<KVStat> KVStatReader::readAll() {
    std::ifstream file(filepath);
    if (!file.is_open()) throw std::runtime_error("Failed to open file: " + filepath);

    std::vector<KVStat> stats;
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        KVStat stat;
        std::getline(ss, stat.key, '\t');
        std::getline(ss, token, '\t'); stat.size = std::stoi(token);
        std::getline(ss, token, '\t'); stat.frequency = std::stoi(token);
        stats.push_back(stat);
    }
    return stats;
}
