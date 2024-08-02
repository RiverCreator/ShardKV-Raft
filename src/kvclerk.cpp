#include "kvclerk.h"

void ShardClerk::Init(std::vector<MasterRPCInfo> &info)
{
    this->ServerInfo = info;
    this->clientId = rand() % 100000;
    this->requestId = 0;
    this->leaderId = rand() % info.size();
    for(int i=0;i<info.size();i++){
        std::string ip_port=info[i].ip+":"+std::to_string(info[i].m_port);
        auto channel = grpc::CreateChannel(ip_port, grpc::InsecureChannelCredentials());
        stub_channels.emplace_back(channel);
        stub_ptrs.emplace_back(shardkv::ShardKVRpc::NewStub(channel));
    }
}

int GetDuration(timeval last){
    struct timeval now;
    gettimeofday(&now, NULL);
    return ((now.tv_sec - last.tv_sec) * 1000000 + (now.tv_usec - last.tv_usec));
}

void ShardClerk::Join(std::unordered_map<int, shardkv::JoinConfigs>& groupinfo)
{   
    shardkv::JoinRequest request;
    shardkv::JoinResponse response;
    ::grpc::ClientContext context;
    request.set_clientid(this->clientId);
    request.set_requestid(this->getCurRequestId());
    int cur_leader = this->getCurLeader();
    for(const auto& info:groupinfo){
        (*request.mutable_groups())[info.first] = info.second;
    }
    while(1){
        grpc::Status status = this->stub_ptrs[cur_leader]->Join(&context,request,&response);
        if(status.ok()){
            if(response.iswrongleader()){
                cur_leader = this->getChangeLeader();
            }
            else{
                printf("Join finished, ip is [%s], port is [%d]",this->ServerInfo[cur_leader].ip.c_str(),this->ServerInfo[cur_leader].m_port);
                return;
            }
            usleep(100000);
        }
    }
}

shardkv::QueryResponse ShardClerk::Query(int configid)
{
    shardkv::QueryRequest request;
    shardkv::QueryResponse response;
    ::grpc::ClientContext context;
    request.set_configid(configid);
    request.set_clientid(this->clientId);
    request.set_requestid(this->getCurRequestId());
    int cur_leader = this->getCurLeader();
    //struct timeval start;
    //gettimeofday(&start, NULL);
    while(1){
        // if(GetDuration(start) > 10 * 1000000){
        //     return false;
        // }
        grpc::Status status = this->stub_ptrs[cur_leader]->Query(&context,request,&response);
        if(status.ok()){
            if(response.iswrongleader()){
                cur_leader = this->getChangeLeader();
            }
            else{
                printf("Query finished, ip is [%s], port is [%d]",this->ServerInfo[cur_leader].ip.c_str(),this->ServerInfo[cur_leader].m_port);
                break;
            }
            usleep(100000);
        }
    }
    this->config.configId = response.confignum();
    for(auto s:response.shards()){
        this->config.shards.push_back(s);
    }
    for(const auto& s:response.groups()){
        for(const auto& c:s.second.config()){
            this->config.groups[s.first].emplace_back(c.ip(),c.port());
        }
    }
    return response;
}

void ShardClerk::Move(int shardid, int gid)
{
    shardkv::MoveRequest request;
    ::grpc::ClientContext context;
    shardkv::MoveResponse response;
    request.set_clientid(this->clientId);
    request.set_requestid(this->requestId);
    request.set_gid(gid);
    request.set_shardid(shardid);
    int cur_leader = this->getCurLeader();
    while(1){
        // if(GetDuration(start) > 10 * 1000000){
        //     return false;
        // }
        grpc::Status status = this->stub_ptrs[cur_leader]->Move(&context,request,&response);
        if(status.ok()){
            if(response.iswrongleader()){
                cur_leader = this->getChangeLeader();
            }
            else{
                printf("Move finished, ip is [%s], port is [%d]",this->ServerInfo[cur_leader].ip.c_str(),this->ServerInfo[cur_leader].m_port);
                return;
            }
            usleep(100000);
        }
    }
}


void ShardClerk::Leave(std::vector<int>& gids)
{
    shardkv::LeaveRequest request;
    ::grpc::ClientContext context;
    shardkv::LeaveResponse response;
    request.set_clientid(this->clientId);
    request.set_requestid(this->requestId);
    for(int i = 0;i < gids.size();i++){
        request.add_groupids(gids[i]);
    }
    int cur_leader = this->getCurLeader();
    while(1){
        // if(GetDuration(start) > 10 * 1000000){
        //     return false;
        // }
        grpc::Status status = this->stub_ptrs[cur_leader]->Leave(&context,request,&response);
        if(status.ok()){
            if(response.iswrongleader()){
                cur_leader = this->getChangeLeader();
            }
            else{
                printf("Leave finished, ip is [%s], port is [%d]",this->ServerInfo[cur_leader].ip.c_str(),this->ServerInfo[cur_leader].m_port);
                return;
            }
            usleep(100000);
        }
    }
}

int ShardClerk::getCurRequestId()
{
    std::lock_guard<std::mutex> lock(this->mtx);
    return this->requestId++;
}

int ShardClerk::getCurLeader()
{
    std::lock_guard<std::mutex> lock(this->mtx);
    return this->leaderId;
}

int ShardClerk::getChangeLeader()
{
    std::lock_guard<std::mutex> lock(this->mtx);
    this->leaderId = (this->leaderId + 1) % this->ServerInfo.size();
    return leaderId;
}
