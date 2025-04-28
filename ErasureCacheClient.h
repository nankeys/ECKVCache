#pragma once
#include <string>
#include <vector>
#include <map>
#include <libmemcached/memcached.h>

class ErasureCacheClient {
public:
    ErasureCacheClient(const std::vector<std::string>& servers, int k, int n);
    ~ErasureCacheClient();
    bool put(const std::string& key, const std::vector<uint8_t>& value);
    std::vector<uint8_t> get(const std::string& key);
    int getK() const { return k; }
    int getN() const { return n; }
    int getM() const { return m; }

    const std::vector<memcached_st*>& getClients() const { return memc_pool; }

private:
    std::vector<memcached_st*> memc_pool;
    std::map<uint32_t, int> hashRing;
    int k, n, m;
    size_t block_size;

    memcached_st* getClient(const std::string& subkey);
    void initHashRing();
    uint32_t hash(const std::string& str) const;
    void encode(const std::vector<uint8_t>& value,
                std::vector<std::vector<uint8_t>>& data_blocks,
                std::vector<std::vector<uint8_t>>& parity_blocks);
};
