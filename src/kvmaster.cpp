#include "kvmaster.h"

std::vector<PeersInfo> getRaftPort(std::vector<KVServerInfo> &kvInfo)
{
    std::vector<PeersInfo> peers;
    int n = kvInfo.size();
    for(int i = 0; i < n; i++){
        peers.emplace_back(kvInfo[i].peersInfo);
    }
    return peers;
}
void ShardMaster::Init(std::vector<KVServerInfo> &kvInfo, int me)
{
    std::vector<PeersInfo> peers = getRaftPort(kvInfo);
    this->m_id = me;
    this->ip = kvInfo[me].kv_ip;
    this->m_port = kvInfo[me].kv_port;
    this->configs.resize(1);
    this->m_raft.Init(peers, me);
    this->m_clientSeqMap.clear();
    this->m_requestMap.clear();
    rpc_server.setjoincb(std::bind(&ShardMaster::Join,this,std::placeholders::_1));
    rpc_server.setleavecb(std::bind(&ShardMaster::Leave,this,std::placeholders::_1));
    rpc_server.setmovecb(std::bind(&ShardMaster::Move,this,std::placeholders::_1));
    rpc_server.setquerycb(std::bind(&ShardMaster::Query,this,std::placeholders::_1));
    rpcloop = std::thread(&ShardMaster::RPCLoop,this,kvInfo[me].kv_ip,kvInfo[me].kv_port);
    applylogloop = std::thread(&ShardMaster::ApplyLoop,this);
}

void ShardMaster::RPCLoop(std::string ip, int port)
{
    std::string ip_port = ip+":"+std::to_string(port);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(ip_port, grpc::InsecureServerCredentials());

    builder.RegisterService(&rpc_server);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    server->Wait();
}

void ShardMaster::ApplyLoop()
{

}

bool ShardMaster::StartOperation(Operation op)
{
    
}

void ShardMaster::OpHandler(Operation op)
{
}

void ShardMaster::BalanceShard(Config &config)
{
}

int ShardMaster::GetConfigSize()
{
    return 0;
}

shardkv::JoinResponse ShardMaster::Join(const shardkv::JoinRequest *request)
{
    shardkv::JoinResponse response;
    response.set_iswrongleader(false);
    Operation op;
    op.op = "join";
    op.key = "0";
    op.value = "0";
    op.clientId = request->clientid();
    op.requestId = request->requestid();
    
    op.args = (void*)request;
    //auto t = &(request->groups());
    //google::protobuf::Map<google::protobuf::int32, shardkv::JoinConfigs> rec = *((google::protobuf::Map<google::protobuf::int32, shardkv::JoinConfigs>*)op.args);

    // for(const auto s:request->groups()){
    //     for(const auto c:s.second.config()){
    //         ShardInfo info;
    //         info.ip = c.ip();
    //         info.port = c.port();
    //         op.shard_info[s.first].push_back(info);
    //     }
    // }
    response.set_iswrongleader(StartOperation(op));
    return response;
}

shardkv::MoveResponse ShardMaster::Move(const shardkv::MoveRequest *request)
{
    shardkv::MoveResponse response;
    response.set_iswrongleader(false);
    Operation op;
    op.op = "move";
    op.key = "0";
    op.value = "0";
    op.clientId = request->clientid();
    op.requestId = request->requestid();
    op.args = (void*)request;
    response.set_iswrongleader(StartOperation(op));
    return response;
}

shardkv::LeaveResponse ShardMaster::Leave(const shardkv::LeaveRequest *request)
{
    shardkv::LeaveResponse response;
    response.set_iswrongleader(false);
    Operation op;
    op.op = "leave";
    op.key = "0";
    op.value = "0";
    op.clientId = request->clientid();
    op.requestId = request->requestid();
    op.args = (void*)request;
    response.set_iswrongleader(StartOperation(op));
    return response;
}

shardkv::QueryResponse ShardMaster::Query(const shardkv::QueryRequest *request)
{
    shardkv::QueryResponse response;
    response.set_iswrongleader(false);
    Operation op;
    op.op = "leave";
    op.key = "0";
    op.value = "0";
    op.clientId = request->clientid();
    op.requestId = request->requestid();
    op.args = (void*)request;
    StartRet ret= this->m_raft.Start(op);
    op.op_term = ret.m_curTerm;
    op.op_index = ret.m_cmdIndex;
    if(!ret.isLeader){
        response.set_iswrongleader(true);
        return response;
    }
    OpContext opctx(op);
    {
        std::lock_guard<std::mutex> lock(this->ctx_mtx);
        this->m_requestMap[ret.m_cmdIndex] = &opctx;
    }
    {
        std::unique_lock<std::mutex> lock(opctx.mtx_);
        opctx.cond_.wait(lock,[&opctx](){
            return opctx.ready!=OpContext::NOT_READY;
        });
    }
    if(opctx.state&state::LEADER_WRONG){
        response.set_iswrongleader(true);
        return response;
    }else{
        //ignore
    }
    {
        std::lock_guard<std::mutex> lock(this->ctx_mtx);
        this->m_requestMap.erase(op.op_index);
    }
    response.set_iswrongleader(false);
    response.set_confignum(opctx.config.configId);
    for(auto it:opctx.config.groups){
        for(auto c:it.second){
            shardkv::GroupConfig* tmp=(*response.mutable_groups())[it.first].add_config();
            tmp->set_ip(c.ip);
            tmp->set_port(c.port);
        }
    }
    for(auto shard :opctx.config.shards){
        response.add_shards(shard);
    }
    return response;
}
