#include "ErasureCacheClient.h"
#include <isa-l.h>
#include <random>
#include <cstring>
#include <iostream>
#include <openssl/md5.h>
#include <fcntl.h>
#include <sys/select.h>
#include <cerrno>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

uint32_t ErasureCacheClient::hash(const std::string& str) const {
    std::hash<std::string> hasher;
    return static_cast<uint32_t>(hasher(str));
}

void ErasureCacheClient::initHashRing() {
    int virtual_nodes = 100;
    for (int i = 0; i < memc_pool.size(); ++i) {
        for (int v = 0; v < virtual_nodes; ++v) {
            std::string node_id = std::to_string(i) + "#" + std::to_string(v);
            hashRing[hash(node_id)] = i;
        }
    }
}

ErasureCacheClient::ErasureCacheClient(const std::vector<std::string>& servers, int k, int n)
    : k(k), n(n), m(n - k){
    for (const auto& server : servers) {
        server_addr.emplace_back(server);
        memcached_st* memc = memcached_create(NULL);
        memcached_server_add(memc, server.c_str(), 11211);
        memc_pool.push_back(memc);
    }
    initHashRing();
}

ErasureCacheClient::~ErasureCacheClient() {
    for (auto memc : memc_pool) {
        memcached_free(memc);
    }
}

memcached_st* ErasureCacheClient::getClient(const std::string& subkey) {
    uint32_t h = hash(subkey);
    auto it = hashRing.lower_bound(h);
    if (it == hashRing.end()) {
        it = hashRing.begin(); // wrap around
    }
    int index = it->second;
    //std::cout << "subkey = " << subkey << ", client = " << index << std::endl;
    return memc_pool[index];
}

void ErasureCacheClient::encode(const std::vector<uint8_t>& value,
                                std::vector<std::vector<uint8_t>>& data_blocks,
                                std::vector<std::vector<uint8_t>>& parity_blocks) {
    std::vector<uint8_t*> data_ptrs(k);
    std::vector<uint8_t*> parity_ptrs(m);
    
    for (int i = 0; i < k; ++i) {
        std::copy(value.begin() + i * block_size, value.begin() + (i + 1) * block_size, data_blocks[i].begin());
        data_ptrs[i] = data_blocks[i].data();
    }
    for (int i = 0; i < m; ++i) {
        parity_ptrs[i] = parity_blocks[i].data();
    }

    std::vector<uint8_t> encode_matrix(n * k);
    std::vector<uint8_t> g_tbls(k * m * 32);

    gf_gen_rs_matrix(encode_matrix.data(), n, k);
    ec_init_tables(k, m, &encode_matrix[k * k], g_tbls.data());
    ec_encode_data(block_size, k, m, g_tbls.data(), data_ptrs.data(), parity_ptrs.data());
}

bool ErasureCacheClient::put(const std::string& key, const std::vector<uint8_t>& value) {
    block_size = value.size() / k;

    if (value.size() != k * block_size) return false;

    std::vector<std::vector<uint8_t>> data_blocks(k, std::vector<uint8_t>(block_size));
    std::vector<std::vector<uint8_t>> parity_blocks(m, std::vector<uint8_t>(block_size));

    encode(value, data_blocks, parity_blocks);
    

    for (int i = 0; i < k; ++i) {
        std::string subkey = key + "." + std::to_string(i);
        memcached_st* memc = getClient(subkey);
        memcached_set(memc, subkey.c_str(), subkey.size(),
                      reinterpret_cast<char*>(data_blocks[i].data()), block_size, 0, 0);
    }
    for (int i = 0; i < m; ++i) {
        std::string subkey = key + "." + std::to_string(k + i);
        memcached_st* memc = getClient(subkey);
        memcached_set(memc, subkey.c_str(), subkey.size(),
                      reinterpret_cast<char*>(parity_blocks[i].data()), block_size, 0, 0);
    }
    return true;
}

bool is_node_alive(const std::string& ip, int port, int timeout_ms = 500) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    // 设置非阻塞
    fcntl(sock, F_SETFL, O_NONBLOCK);

    struct sockaddr_in serv_addr {};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr);

    int result = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (result == 0) {
        close(sock);
        return true;  // 立即连接成功
    } else if (errno != EINPROGRESS) {
        close(sock);
        return false;  // 非阻塞连接失败
    }

    // 使用 select 等待连接完成
    fd_set wait_set;
    FD_ZERO(&wait_set);
    FD_SET(sock, &wait_set);
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    result = select(sock + 1, NULL, &wait_set, NULL, &timeout);
    if (result <= 0) {
        close(sock);
        return false;  // 超时或错误
    }

    // 检查 socket 错误
    int so_error;
    socklen_t len = sizeof(so_error);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);

    close(sock);
    return so_error == 0;
}

void ErasureCacheClient::check_node_alive() {
    std::cout << "server num = " << server_addr.size() << std::endl;
    for(int i = 0; i < server_addr.size(); i++) {
        int index = i;
        std::cout << "index = " << index << std::endl;
        const auto ip = server_addr[index];
        std::cout << "ip = " << ip << std::endl;
        node_alive_cache[index] = is_node_alive(ip, 11211);
        std::cout << "node_alive_cache = " << node_alive_cache[index] << std::endl;
        sleep(0.5);
    }
}

std::vector<uint8_t> ErasureCacheClient::get(const std::string& key) {
    std::vector<std::vector<uint8_t>> blocks(n);
    std::vector<uint8_t*> block_ptrs(n, nullptr);
    std::vector<int> present;
    std::vector<int> erasures;

    // 尝试获取所有分块
    for (int i = 0; i < n; ++i) {
        std::string subkey = key + "." + std::to_string(i);
        size_t len = 0;
        uint32_t flags = 0;
        memcached_return_t rc;

        memcached_st* memc = getClient(subkey);
        char* data = memcached_get(memc, subkey.c_str(), subkey.size(), &len, &flags, &rc);
        if (rc == MEMCACHED_SUCCESS && len == block_size) {
            blocks[i] = std::vector<uint8_t>(data, data + block_size);
            block_ptrs[i] = blocks[i].data();
            present.push_back(i);
        } else {
            erasures.push_back(i);
        }
        if (data) free(data);
    }

    if (present.size() < size_t(k)) {
        std::cerr << "Not enough blocks to recover key: " << present.size() << ", key = " << key << std::endl;
        return {};
    }

    // 生成 Reed-Solomon 编码矩阵
    std::vector<uint8_t> encode_matrix(n * k);
    gf_gen_rs_matrix(encode_matrix.data(), n, k);

    // 1. 构造解码矩阵
    std::vector<uint8_t> decode_matrix(k * k);
    std::vector<uint8_t*> available_data(k);
    std::vector<int> available_indices;

    for (int i = 0; i < k; ++i) {
        int idx = present[i];
        available_indices.push_back(idx);
        available_data[i] = block_ptrs[idx];
        memcpy(&decode_matrix[i * k], &encode_matrix[idx * k], k);
    }

    // 2. 求解逆矩阵
    std::vector<uint8_t> invert_matrix(k * k);
    if (gf_invert_matrix(decode_matrix.data(), invert_matrix.data(), k) < 0) {
        std::cerr << "Matrix inversion failed." << std::endl;
        return {};
    }

    // 3. 构造恢复矩阵
    std::vector<uint8_t> recovery_matrix(m * k);
    int r = 0;
    for (int i : erasures) {
        if (i < k) { // 只恢复原始数据块
            memcpy(&recovery_matrix[r * k], &encode_matrix[i * k], k);
            r++;
        }
    }

    // 4. 构造恢复表并进行恢复
    std::vector<uint8_t> g_tbls(k * m * 32);
    ec_init_tables(k, r, recovery_matrix.data(), g_tbls.data());

    std::vector<std::vector<uint8_t>> recovered_blocks(r, std::vector<uint8_t>(block_size));
    std::vector<uint8_t*> recovered_ptrs(r);
    for (int i = 0; i < r; ++i) recovered_ptrs[i] = recovered_blocks[i].data();

    ec_encode_data(block_size, k, r, g_tbls.data(), available_data.data(), recovered_ptrs.data());

    // 5. 拼接完整数据
    std::vector<uint8_t> full_value;
    int recovered_idx = 0;
    for (int i = 0; i < k; ++i) {
        if (block_ptrs[i]) {
            full_value.insert(full_value.end(), block_ptrs[i], block_ptrs[i] + block_size);
        } else {
            full_value.insert(full_value.end(), recovered_ptrs[recovered_idx], recovered_ptrs[recovered_idx] + block_size);
            recovered_idx++;
        }
    }

    return full_value;
}
