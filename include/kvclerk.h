#pragma once
#include <mutex>
#include <vector>
#include <string>
#include <thread>
#include "shardkv.grpc.pb.h"
#include "shardkv.pb.h"
#include "common.h"
#include <grpc/grpc.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/grpcpp.h>
#include <sys/time.h>
// struct MasterRPCInfo{
//     std::string ip;
//     int m_port;
//     //std::vector<int> m_ports; //这里每个server有多个port来接收rpc请求 //TODO 这里因为其就是在一个进程上开多个线程来监听rpc请求 且grpc底层为epoll 多线程监听请求，这里设置多个port的必要性存疑
// };

class ShardClerk{
public:
    //输入参数为shardmaster集群各个主机的ip和port
    void Init(std::vector<MasterRPCInfo>& info);
    //添加配置信息， joinrequest中groups为一个map，key为gid，value是一个数组，表示配置信息 这里用的是编号，实际应该可以用ip和port来表示有多少个就表示这个group有多少个主机
    void Join(std::unordered_map<int, shardkv::JoinConfigs>& groupinfo);
    //查询配置 queryrequest中就一个gid，然后返回对应的配置信息 包括分片与集群的对应关系以及每个集群的信息
    Config Query(int configid);
    //移动指定分片（shard）到哪个group下
    void Move(int shardid, int gid);
    //让指定gids离开
    void Leave(std::vector<int>& gids);
    int getCurRequestId();
    int getCurLeader();
    int getChangeLeader();
    void ShowConfig();
private:
    std::vector<MasterRPCInfo>  ServerInfo;
    std::mutex mtx;
    int leaderId;
    int clientId;
    int requestId;
    //std::unordered_map<int,std::unique_ptr<shardkv::ShardKVRpc::Stub>> stub_maps;
    std::vector<std::unique_ptr<shardkv::ShardKVRpc::Stub>> stub_ptrs; //因为可能会和每个server通信，因此先建立连接
    //std::unordered_map<int,std::shared_ptr<grpc_impl::Channel>> stub_channels;
    std::vector<std::shared_ptr<grpc_impl::Channel>> stub_channels;
    Config config;
};