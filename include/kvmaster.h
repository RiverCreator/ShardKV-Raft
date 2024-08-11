#pragma once
#include "RaftServer.h"
#include "shardkv.grpc.pb.h"
#include "shardkv.pb.h"
#include "kvrpc.h"
#include "common.h"
#include <thread>
//实现kvmaster 主要是管理配置文件的 整体框架与kvraft差不多，无日志快照
class ShardMaster;
//存储raft所有节点的ip和port
// class KVServerInfo{
// public:
//     PeersInfo peersInfo; //Raft层的ip和port
//     std::string kv_ip; 
//     int kv_port;
// };
//保存客户端RPC调用时请求的上下文信息
// class OpContext{
// public:
//     OpContext(Operation& op);
//     std::condition_variable cond_; //条件变量用来applyloop中来通知对应的get和put线程来表示日志写入成功可以返回给客户端
//     std::mutex mtx_;
//     Operation op;
//     Config config; //记录config
//     bool isIgnored; //维护幂等性，客户端可能会重复发送消息过来，如果发现重复发送一样的requestId，则忽略，并返回给客户端
//     enum OP_STATE  {OK = 0, NOT_READY};
//     OP_STATE ready; //如果get操作 是OK的话则op中的value即是返回的value
//     state::state state; //记录错误信息
// };

class ShardMaster{
public:
    void Init(std::vector<KVServerInfo>& kvInfo, int me);
    void RPCLoop(std::string ip,int port);
    void ApplyLoop();
    //提交join move leave操作到raft层进行同步 返回值表示是否为错误leader
    bool StartOperation(Operation op);
    //applyloop中实际调用的函数
    void OpHandler(Operation op);
    //join 和 leave后需要对分片进行负载均衡
    void BalanceShard(Config& config);
    int GetConfigSize();
    shardkv::JoinResponse Join(const shardkv::JoinRequest* request);
    shardkv::MoveResponse Move(const shardkv::MoveRequest* request);
    shardkv::LeaveResponse Leave(const shardkv::LeaveRequest* request);
    shardkv::QueryResponse Query(const shardkv::QueryRequest* request);

private:
    Raft m_raft;
    int m_id; //集群中的编号
    std::string ip;
    int m_port;
    HashRing hring; //一致性哈希
    std::vector<Config> configs; //每一期的config都存储起来
    std::mutex ctx_mtx;
    std::mutex seq_mtx;
    std::mutex cfg_mtx;
    ShardKvRPCImp rpc_server;
    std::unordered_map<int, int> m_clientSeqMap;    //只记录特定客户端已提交的最大请求ID
    std::unordered_map<int, OpContext*> m_requestMap;  //记录当前RPC对应的上下文
    std::thread rpcloop;
    std::thread applylogloop;
};