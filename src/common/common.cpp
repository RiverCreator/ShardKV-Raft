#include "common.h"
namespace state{
    char KEY_WRONG = 0x01;
    char SYN_WRONG = 0x02;
    char LEADER_WRONG = 0x04;
    char GROUP_WRONG = 0x08;
}
Config::Config()
{
    configId = 0;
    shards.resize(NShards, 0);
    groups.clear();
}

std::string Operation::getCmd()
{
     return op + " " + key + " " + value + " "+ std::to_string(clientId) + " " + std::to_string(requestId) +" " + this->args;
}

OpContext::OpContext(Operation &op)
{
    this->op = op;
    this->isIgnored = false;
    this->ready = NOT_READY;
    
}

uint32_t murmurhash(const void* key, int len, uint32_t seed) {
    const uint8_t* data = (const uint8_t*)key;
    const int nblocks = len / 4;
    uint32_t h1 = seed;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);
    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;

        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }

    const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);
    uint32_t k1 = 0;

    switch (len & 3) {
    case 3: k1 ^= tail[2] << 16;
    case 2: k1 ^= tail[1] << 8;
    case 1: k1 ^= tail[0];
        k1 *= c1; k1 = (k1 << 15) | (k1 >> 17); k1 *= c2; h1 ^= k1;
    };

    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

HashRing::HashRing(int num, int nshard):virtualNodesNum(num)
{
    shards.resize(nshard,0);
}

void HashRing::AddNode(int gid)
{
    std::string node = std::to_string(gid);
    for(int i=0;i<virtualNodesNum;i++){
        std::string vrnode = node + "#" + std::to_string(i);
        size_t hashk = murmurhash(vrnode.c_str(),vrnode.size());
        virtualmap[vrnode] = gid;
        ring[hashk] = vrnode;
    }
}

void HashRing::RemoveNode(int gid)
{
    std::string node = std::to_string(gid);
    //删除节点
    for(int i=0;i<virtualNodesNum;i++){
        std::string vrnode = node + "#" + std::to_string(i);
        size_t hashk = murmurhash(vrnode.c_str(),vrnode.size());
        ring.erase(hashk);
        virtualmap.erase(vrnode);
    }
    //将在这个gid上的分片rehash映射到新的虚拟节点上
    for(int i=0;i<shards.size();i++){
        if(shards[i]==gid){
            std::string mapnode = "shard:"+ std::to_string(i);
            std::string virnode = GetNode(mapnode);
            shards[i] = virtualmap[virnode];
        }
    }
}

void HashRing::rehashshard()
{
    for(int i=0;i<shards.size();i++){
        std::string mapnode = "shard:"+ std::to_string(i);
        std::string virnode = GetNode(mapnode);
        shards[i] = virtualmap[virnode];
    }
}

std::string HashRing::GetNode(const std::string str)
{
    size_t hashk = murmurhash(str.c_str(),str.size());
    auto it = ring.lower_bound(hashk);
    if (it == ring.end()) {
        it = ring.begin();
    }
    return it->second;
}

std::vector<int> HashRing::Getshard()
{
    return shards;
}


