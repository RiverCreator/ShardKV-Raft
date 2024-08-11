#pragma once
#include "RaftServer.h"
#include "shardserrpc.h"
#include "kvserver.grpc.pb.h"
#include "kvserver.pb.h"
#include "kvclerk.h"
#include <atomic>
#include "common.h"
class ShardKVServer{
public:
    ShardKVServer();
    void Init(std::vector<KVServerInfo>& peers,int me, int maxshotsnap,std::vector<MasterRPCInfo>& masters);
    
    //loop functions
    void RPCLoop();
    void ApplyLoop();
    void SnapShotLoop();
    void UpdateConfigLoop(); //向master发起query请求更新config的loop
    void PullShardLoop();
    void AckGarbageLoop(); //确认切片被对应的group接收到

    //rpc functions
    kvrf::GetResponse Get(const kvrf::GetRequest*);
    kvrf::PutResponse Put(const kvrf::PutRequest*);
    kvrf::MigrationResponse Migrate(const kvrf::MigrationRequest*);
    kvrf::AckResponse Ack(const kvrf::AckRequest*);

    //send rpc to other groups
    //确认切片移动后，接收方是否接收到了 接收到了以后再删除数据
    void SendAskAck(int cfgid, std::unordered_set<int> shards);
    //去对应的group拉取分配给自己的shard
    void PullShard(int shardid,int cfgid);

    //handler
    //处理put和append逻辑
    void PutAppendHandler(Operation op,OpContext* ctx,bool opexist,bool seqexist,int prevRequestIdx);
    //处理get逻辑
    void GetHandler(Operation op,OpContext* ctx);
    //处理接收到快照同步时的处理逻辑
    void SnapshotHandler(ApplyMsg msg);
    //config同步，并且决定出要传出的shard的数据
    void ConfigHandler(Config config);
    //根据MigrationResponse更新数据库
    void MigrationHandler(kvrf::MigrationResponse& response);
    //当Ack确认到指定group接受到了分片后删除自己的分片
    void EraseOutShardHandler(int cfgid, int shardid);
    //从raft层获取snapshot后恢复数据
    void RecoveryFromSnapShot(std::string snapshot);
    //删除unacked中已经发送了ack的shard
    void EraseUnackedHandler(int cfgifd,int shardid);

    //aux function
    //判断是否是key对应的shard是不是在当前group下
    bool IsMatchShard(std::string key);
    //将KvServer的数据库信息转为shotsnap
    std::string getSnapShot();
    std::vector<PeersInfo> getRaftPort(std::vector<KVServerInfo> &kvInfo);
    //将Config转换为shardkv::Config
    std::string SerilizeConfig(Config config);
    //string转换回去
    Config DeserilizeConfig(std::string& str);
    int getRequestId();
    int key2shard(std::string key);
private:
    Raft m_raft;
    ShardClerk clerk;
    ShardKVServerRPCImp rpc_server;
    int m_id;
    std::string ip;
    int port;
    int m_groupid;
    Config preconfig;
    Config config;
    int m_snapshotthresh;  //超过这个大小就快照
    int m_lastAppliedIndex; //最后提交的日志index
    int m_pullfinished;
    int m_ackfinished;
    int requestId;
    std::mutex requestid_mtx; 
    std::mutex ctx_mtx;
    std::mutex db_mtx;
    std::mutex seq_mtx; //m_clientSeqMap的互斥访问锁
    std::mutex apidx_mtx;
    std::mutex cfg_mtx;
    std::mutex comein_mtx;
    std::mutex pullfin_mtx;
    std::mutex ackfin_mtx;
    std::mutex unack_mtx;
    std::mutex outshard_mtx;
    std::mutex avail_mtx; //m_availshards
    std::condition_variable pull_cond;
    std::condition_variable ack_cond;
    std::unordered_map<std::string, std::string> m_database;  //模拟数据库
    std::unordered_map<int, int> m_clientSeqMap;    //只记录特定客户端已提交的最大请求ID key为clientid，value为requestid
    std::unordered_map<int, OpContext*> m_requestMap;  //记录当前RPC对应的上下文
    
    std::unordered_map<int,int> comeinshards; //记录要pull的shardid与configid
    std::unordered_set<int> m_availshards; //记录当前group下包含可用的shard
    std::unordered_map<int,std::unordered_set<int>> unacked; //记录接收到migrate reply后收到的数据，记录是为了发送回ack确保收到数据 key为cfgid value为shardid的集合
    std::unordered_map<int,std::unordered_map<int,std::unordered_map<std::string,std::string>>> outshards; //记录每个cfgid要发送出去的shard，key为shardid，value为database
    //loop thread
    ThreadPool_& thread_pool;
    std::thread rpcloop;
    std::thread applyloop;
    std::thread snaploop;
    std::thread updatecfgloop;
    std::thread pullshardloop;
    std::thread ackgarbageloop;
};