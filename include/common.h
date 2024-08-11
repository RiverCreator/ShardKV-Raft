#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <map>
#include <condition_variable>
#define NShards 10
namespace state{
    typedef char state;
    extern char KEY_WRONG;
    extern char SYN_WRONG;
    extern char LEADER_WRONG;
    extern char GROUP_WRONG;
};

struct PeersInfo{
    int m_peerId;
    std::string ip;
    int port;
};

class KVServerInfo{
public:
    PeersInfo peersInfo; //Raft层的ip和port
    std::string kv_ip; 
    int kv_port;
};

struct ShardInfo{
    ShardInfo(){};
    ShardInfo(std::string ip_,int port_):ip(ip_),port(port_){}
    std::string ip;
    int port;
};

struct MasterRPCInfo{
    std::string ip;
    int m_port;
    //std::vector<int> m_ports; //这里每个server有多个port来接收rpc请求 //TODO 这里因为其就是在一个进程上开多个线程来监听rpc请求 且grpc底层为epoll 多线程监听请求，这里设置多个port的必要性存疑
};

class Config{
public:
    Config();
    int configId; //表示是第几个配置
    std::vector<int> shards; //表示分片的关系
    std::unordered_map<int, std::vector<ShardInfo>> groups;  //表示gid和对应的集群中所有主机的ip和port
};
/**
 * @brief 记录操作 
 * 
 */
struct Operation{
    std::string getCmd();
    std::string op;
    std::string key;
    std::string value;
    std::string args;// 记录join move leave等的参数
    int clientId; //记录是哪个客户端记录的
    int requestId; //这个client发送的第几条请求
    int op_term; //操作写入raft层时的term
    int op_index; //操作写入raft层时的index
};

//保存客户端RPC调用时请求的上下文信息
class OpContext{
public:
    OpContext(Operation& op);
    std::condition_variable cond_; //条件变量用来applyloop中来通知对应的get和put线程来表示日志写入成功可以返回给客户端
    std::mutex mtx_;
    Operation op;
    //bool isWrongLeader; //表示是否是错误的leader
    Config config; //记录config
    bool isIgnored; //维护幂等性，客户端可能会重复发送消息过来，如果发现重复发送一样的requestId，则忽略，并返回给客户端
    enum OP_STATE  {OK = 0, NOT_READY};
    OP_STATE ready; //如果get操作 是OK的话则op中的value即是返回的value
    state::state state; //记录错误信息
};


uint32_t murmurhash(const void* key, int len, uint32_t seed = 0x9747b28c);
class HashRing{
public:
    HashRing(int num=5,int nshard=NShards);
    void AddNode(int gid);
    void RemoveNode(int gid);
    void rehashshard();

    std::string GetNode(const std::string str);
    std::vector<int> Getshard();
private:
    std::vector<int> shards;//分片 shards[i]表示的是第i个分片属于哪个gid
    std::map<size_t,std::string> ring;//由hash值映射到对应的节点上
    std::unordered_map<std::string,int> virtualmap; //虚拟节点映射到真正物理节点的gid
    int virtualNodesNum; //虚拟节点个数
};