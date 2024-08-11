#include "shardclient.h"

ShardClient::ShardClient(std::vector<MasterRPCInfo> &ser)
{
    this->client.Init(ser);
    this->clientId = rand()%10000+1;
    this->requestId = 0;
    this->gid2leader.clear();
}

int key2shard(std::string key){
    int shard = 0;
    if(key.size() > 0){
        shard = key[0] - 'a';
    }
    shard = shard % NShards;
    return shard;
}

std::string ShardClient::get(std::string key)
{
    kvrf::GetRequest request;
    kvrf::GetResponse response;
    request.set_key(key);
    request.set_clientid(this->clientId);
    request.set_requestid(getCurRequestId());
    int shardid = key2shard(key);
    int groupid = this->config.shards[shardid];
    int curLeaderId = this->getCurLeader(groupid);
    while(1){
        if(this->config.groups.count(groupid)){
            std::string ip_port=this->config.groups[groupid][curLeaderId].ip+":"+std::to_string(this->config.groups[groupid][curLeaderId].port);
            auto channel = grpc::CreateChannel(ip_port, grpc::InsecureChannelCredentials());
            auto stub_ptr=kvrf::KvRaftServiceRpc::NewStub(channel);
            ::grpc::ClientContext context;
            grpc::Status status = stub_ptr->Get(&context,request,&response);
            if(status.ok()){
                if(response.iswrongleader()){
                    curLeaderId = this->getChangeLeader(groupid);
                }
                else{
                    if(response.isexist()){
                        printf("value:%s\n",response.value());
                        return response.value();
                    }
                    else{
                        printf("key not exists\n");
                        return "";
                    }
                }
            }
        }
        this->config = client.Query(-1);
        usleep(100000);
    }
}

void ShardClient::putappend(std::string op, std::string key, std::string value)
{
    kvrf::PutRequest request;
    kvrf::PutResponse response;
    request.set_op(op);
    request.set_key(key);
    request.set_value(value);
    request.set_clientid(this->clientId);
    request.set_requestid(this->getCurRequestId());
    int shardid = key2shard(key);
    int groupid = this->config.shards[shardid];
    int curLeaderId = this->getCurLeader(groupid);
    while(1){
        groupid = this->config.shards[shardid];
        if(config.groups.count(groupid)){
            std::string ip_port=this->config.groups[groupid][curLeaderId].ip+":"+std::to_string(this->config.groups[groupid][curLeaderId].port);
            auto channel = grpc::CreateChannel(ip_port, grpc::InsecureChannelCredentials());
            auto stub_ptr=kvrf::KvRaftServiceRpc::NewStub(channel);
            ::grpc::ClientContext context;
            grpc::Status status = stub_ptr->Put(&context,request,&response);
            if(status.ok()){
                if(response.iswrongleader()){
                    curLeaderId = this->getChangeLeader(groupid);
                }
                else{
                    if(response.ok()){
                        printf("put ok!\n");
                    }
                    else{
                        printf("put failed!\n");
                    }
                    return;
                }
            }
        }
        this->config = client.Query(-1);
        usleep(100000);
    }
}

void ShardClient::Join(std::unordered_map<int, std::vector<ShardInfo>> &m)
{
    std::unordered_map<int, shardkv::JoinConfigs> req;
    for(auto it :m){
        shardkv::JoinConfigs jcfg;
        for(int i=0;i<it.second.size();i++){
            auto tmp = jcfg.add_config();
            tmp->set_ip(it.second[i].ip);
            tmp->set_port(it.second[i].port);
        }
        req[it.first] = jcfg;
    }
    client.Join(req);
}

void ShardClient::Leave(std::vector<int> &gids)
{
    client.Leave(gids);
}

void ShardClient::Move(int shardid, int gid)
{
    client.Move(shardid,gid);
}

Config ShardClient::Query(int cid)
{
    Config cfg = client.Query(cid);
    return cfg;
}

int ShardClient::getCurRequestId()
{
    std::lock_guard<std::mutex> lock(this->mtx);
    return this->requestId++;
}

int ShardClient::getCurLeader(int gid)
{
    std::lock_guard<std::mutex> lock(this->mtx);
    return this->gid2leader[gid];
}

int ShardClient::getChangeLeader(int gid)
{
    std::lock_guard<std::mutex> lock(this->mtx);
    this->gid2leader[gid] = (this->gid2leader[gid] + 1) % config.groups[gid].size();
    return this->gid2leader[gid];
}
