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
    request.set_clientid(this->clientId);
    request.set_requestid(this->getCurRequestId());
    int cur_leader = this->getCurLeader();
    for(const auto& info:groupinfo){
        (*request.mutable_groups())[info.first] = info.second;
    }
    for(auto c:request.groups()){
        std::cout<<c.first<<std::endl;
    }
    while(1){
        ::grpc::ClientContext context;
        grpc::Status status = this->stub_ptrs[cur_leader]->Join(&context,request,&response);
        if(status.ok()){
            if(response.iswrongleader()){
                cur_leader = this->getChangeLeader();
            }
            else{
                printf("Join finished, ip is [%s], port is [%d]\n",this->ServerInfo[cur_leader].ip.c_str(),this->ServerInfo[cur_leader].m_port);
                return;
            }
            usleep(100000);
        }
    }
}

Config ShardClerk::Query(int configid)
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
                printf("Query finished, ip is [%s], port is [%d]\n",this->ServerInfo[cur_leader].ip.c_str(),this->ServerInfo[cur_leader].m_port);
                break;
            }
            usleep(100000);
        }
    }
    this->config.configId = response.confignum();
    std::vector<int> newshard;
    for(auto s:response.shards()){
        newshard.push_back(s);
    }
    this->config.shards = newshard;
    for(const auto& s:response.groups()){
        std::vector<ShardInfo> rec;
        for(const auto& c:s.second.config()){
            ShardInfo info;
            info.ip =c.ip();
            info.port = c.port();
            rec.emplace_back(info);
        }
        this->config.groups[s.first]=rec;
    }
    return this->config;
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
                printf("Move finished, ip is [%s], port is [%d]\n",this->ServerInfo[cur_leader].ip.c_str(),this->ServerInfo[cur_leader].m_port);
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
                printf("Leave finished, ip is [%s], port is [%d]\n",this->ServerInfo[cur_leader].ip.c_str(),this->ServerInfo[cur_leader].m_port);
                return;
            }
            usleep(100000);
        }
    }
    for(auto gid:gids){
        this->config.groups.erase(gid);
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

void ShardClerk::ShowConfig()
{
    printf("group:\n");
    for(auto g:this->config.groups){
        std::cout<<g.first<<":"<<std::endl;
        for(auto info :g.second){
            std::cout<<info.ip<<":"<<info.port<<std::endl;
        }
    }

    printf("shards:\n");
    for(auto s:this->config.shards){
        std::cout<<s<<" ";
    }
    std::cout<<std::endl;
}

std::vector<MasterRPCInfo> GetMasterInfo(int n){
    std::vector<MasterRPCInfo> rec(n);
    int base_port = 1024;
    for(int i=0;i<n;i++){
        rec[i].ip = "192.168.203.128";
        rec[i].m_port = 1024 + i * 2 + 1;
    }
    return rec;
}


int main(){
    ShardClerk cli;
    std::vector<MasterRPCInfo> rec=GetMasterInfo(5);
    cli.Init(rec);
    std::unordered_map<int, shardkv::JoinConfigs> m;
    shardkv::JoinConfigs jcfg;
    for(int i=0;i<5;i++){
        auto tmp = jcfg.add_config();
        tmp->set_ip("192.168.128.203");
        tmp->set_port(2048+i);
    }
    m[1] = jcfg;
    for(auto c: jcfg.config()){
        std::cout<<c.ip()<<":"<<c.port()<<std::endl;
    }

    cli.Join(m);
    cli.Query(-1);
    cli.ShowConfig();
    std::unordered_map<int, shardkv::JoinConfigs> m2;
    shardkv::JoinConfigs jcfg2;
    for(int i=0;i<5;i++){
        auto tmp = jcfg2.add_config();
        tmp->set_ip("192.168.128.203");
        tmp->set_port(2200+i);
    }
    m2[2] = jcfg2;
    cli.Join(m2);
    cli.Query(-1);
    cli.ShowConfig();
    std::vector<int> l_gid={1};
    cli.Leave(l_gid);
    cli.Query(-1);
    cli.ShowConfig();
    return 0;
}