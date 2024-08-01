#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <chrono>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <condition_variable>
#include <semaphore.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include "ThreadPool.h"
#include "RaftRPC.h"
#include <grpc/grpc.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"
#define HEART_BEART_PERIOD 100000
/**
 * @brief 记录其他节点的ip和port
 * 
 */
struct PeersInfo{
    int m_peerId;
    std::string ip;
    int port;
};

/**
 * @brief 记录LogEntry的指令和任期号
 * 
 */
struct LogEntry{
    LogEntry(std::string cmd="",int term =-1):m_cmd(cmd),m_term(term){}
    std::string m_cmd;
    int m_term;
};
/**
 * @brief 快照相关信息
 * @param snapshot string，记录这块快照的数据内容
 * @param lastIncludeIndex 记录快照中最后一个日志的index
 * @param lastIncludeTerm 记录快照中最后一个日志的term
 * @param offset 记录快照在文件中的偏移量
 */
struct SnapShot{
    //TODO  要存储clientseq
    SnapShot(){}
    SnapShot(std::string ss,int li,int lt,int ofs):data(ss),lastIncludeIndex(li),lastIncludeTerm(lt),offset(ofs){}
    SnapShot(rf::SnapShot ss):data(ss.data()),lastIncludeIndex(ss.lastincludeindex()),lastIncludeTerm(ss.lastincludeterm()),offset(0){}
    std::string data;
    int lastIncludeIndex;
    int lastIncludeTerm;
    int offset;
};
/**
 * @brief 持久化内容
 * @param logs 持久化的log
 * @param cur_term 当前任期
 * @param voteFor 投票给谁了
 * @param shotsnap 快照数组 后续可以考虑合并
 */
struct Persister{
    std::vector<LogEntry> logs;
    int cur_term;
    int votedFor;
    //std::vector<SnapShot> snapshot;
    SnapShot snapshot;
};

/**
 * @brief 提交日志 将加入的日志的index和term 以及当前节点是不是leader返回
 * 
 */
struct StartRet{
    StartRet():m_cmdIndex(-1), m_curTerm(-1), isLeader(false){}
    int m_cmdIndex;
    int m_curTerm;
    bool isLeader;
};

/**
 * @brief 记录操作 
 * 
 */
struct Operation{
    std::string getCmd(){
        return op + " " + key + " " + value + " "+ std::to_string(clientId) + " " + std::to_string(requestId);;
    }
    std::string op;
    std::string key;
    std::string value;
    int clientId; //记录是哪个客户端记录的
    int requestId; //这个client发送的第几条请求
    int op_term; //操作写入raft层时的term
    int op_index; //操作写入raft层时的index
};

/**
 * @brief 向上层提交日志 //TODO 结构优化 
 * 
 */
struct ApplyMsg{
    bool isSnap; //标记是否为快照 true表示为快照 false为普通日志
    std::string cmd;
    int m_cmdIndex;
    int m_cmdTerm;
    Operation GetOperation();//解析cmd参数 //TODO 可以考虑把这个函数弄到Operation类中
    //下面为快照内容
    int lastIncludedIndex; //快照最后一个日志的index 
    int lastIncludedTerm; //快照最后一个日志的term
    std::string snapShot; //可以用protobuf中的map来存 然后序列化
    int offset; //记录在快照文件中的偏移量 这里提交到数据库层是为了能够确认快照的顺序（也可以根据lastindex和lastterm来确定）
};


class Raft{
public:
    Raft(std::string ip,int port);
    ~Raft();
    // //监听投票
    // void ListenForVote();
    // //监听Append
    // void ListenForAppend();
    //处理日志同步
    void ProcessEntriesLoop();
    //选举超时的loop
    void ElectionLoop();
    //发起重新选举 请求投票
    void CallRequestVote(int clientPeerId);
    //回复Vote
    rf::ResponseVote ReplyVote(const rf::RequestVote* request);
    //回复Append
    rf::AppendEntriesResponse ReplyAppend(const rf::AppendEntriesRequest* request);
    //回复InstallSnap
    rf::InstallSnapShotResponse ReplyInstallSnap(const rf::InstallSnapShotRequest*);
    //发起AppendRPC的线程
    void SendAppendEntries(int clientPeerId);
    //发起installsnapshot的线程
    void SendInstallSnapshot(int clientPeerId);
    //向上层应用日志的守护线程
    void ApplyLogLoop();
    //定义节点状态
    enum RAFT_STATE {LEADER=0,CANDIDATE,FOLLOWER};
    //初始化，预先记录其他节点id和ip：port，设定当前id
    void Init(std::vector<PeersInfo>& peers, int id); 
    //设置广播心跳时间
    void SetBellTime();
    //获取当前节点termId，是否leader
    std::pair<int,bool> GetState();
    //判断当前节点日志是否可以给CANDIDATE投票 传入任期号和日志index，在vote的时候调用
    bool CheckLogUptodate(int term, int index);
    //插入新日志
    void PushBackLog(LogEntry log);
    //提供接口给kvRaft调用判断日志大小是否超过了某个大小 由kvRaft来决定是否进行快照化
    bool ExceedLogSize(int size);
    //提供接口给kvRaft调用，将snapshot发送到Raft层
    void RecvSnapShot(std::string snapshot,int lastIncludedIndex);
    //持久化
    void SaveRaftState();
    //读取持久化状态
    void ReadRaftState();
    //持久化快照
    void SaveSnapShot();
    //读取快照
    bool ReadSnapShot();
    //从节点接收到installsnap调用，将快照应用到kvRaft层
    void installSnapShotTOkvServer(bool);
    //返回index对应的压缩过后的logs中的index
    int CompressLogidx(int index);
    //返回截断日志后的index
    int lastindex();
    //返回截断日志后的term
    int lastterm();
    //停止
    void Stop();
    //test函数 重新激活
    void Activate();
    //解析string cmd
    std::vector<LogEntry> GetCmdAndTerm(std::string text);
    //反序列化 读取持久化到文件中的内容
    bool Deserialize();
    //序列化 将log entry持久化到文件中
    void Serialize();
    //计算间隔时间
    int GetDuration(timeval last);
    //RPC服务loop线程
    void RPCLoop(std::string ip,int port);
    //用于接收cmd后向Raft层提交日志
    StartRet Start(Operation op);
    std::mutex apply_mtx;
    std::condition_variable apply_cond; //控制raft层与kvraft层的同步
    std::queue<ApplyMsg> m_msgs; //保存要提交到kvRaft层的log
private:
    std::vector<PeersInfo> m_peers; //记录的其他节点的id, ip和port
    Persister persister;
    int peer_id; //当前节点id
    bool m_stop;
    std::vector<LogEntry> m_logs; //当前内存中的log

    int m_curTerm; // 当前任期
    int m_voteFor; // 表示要投票给谁 因为有可能多个同时发起投票，而只能投票给一个，如果投过票了并且发起投票节点的任期id大于当前，则继续投票
    std::vector<int> m_nextIndex; //将要赋值给从节点的日志索引
    std::vector<int> m_matchIndex; //每个从节点匹配上leader的日志索引
    int m_lastApplied; //最后被应用到状态机的log entry index
    int m_commitIndex; //将被提交的log entry index
    int m_lastIncludedIndex; //最后一次快照的最后一条日志的index
    int m_lastIncludeTerm; //最后一次快照的最后一条日志的term
    int recvVotes; //如果是candidate的话，接收到的投票数量
    int finishedVote; //完成投票的数量
    int cur_peerId; //记录candidate请求vote到哪一个从机 或appendentry到哪一个从机

    RAFT_STATE m_state; //当前所处什么角色状态
    int m_leaderId; //认定的leader id
    struct timeval m_lastWakeTime; //记录follower最后一次接收到心跳的时间
    struct timeval m_lastBroadcastTime; //记录leader最后一次发送心跳的时间
    
    std::mutex mtx;
    //std::mutex apply_mtx; 
    std::mutex persist_mtx;
    std::condition_variable cond_;
    //std::condition_variable apply_cond; //控制raft层与kvraft层的同步
    std::atomic_bool wait_apply; //当为true时，Raft层可以读取
    // sem_t send_sem; //raft层applylog_thread通知kvraft层来读取applylog_queue中的日志信息
    // sem_t recv_sem; //kvraft层读取完毕后

    RaftGrpcImpl rpc_server;
    std::queue<ApplyMsg> applylog_queue;
    std::vector<std::unique_ptr<rf::RaftServerRpc::Stub>> stub_ptrs_;
    std::vector<int> dead;
    std::vector<std::shared_ptr<grpc_impl::Channel>> stub_channels_;
    std::thread rpc_thread;
    std::thread process_entry_thread; //同步log entry还有心跳包 loop
    std::thread election_loop_thread; //超时选举loop
    std::thread applylog_thread; //真正提交log loop
    ThreadPool_& thread_pool_; //vote和append任务的线程池
};