#include "kvmaster.h"
#include <mylog/Logger.h>
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
    this->configs.resize(1); //初始配置为0 没有分片分配到任何group上
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
    while(1){
        std::vector<ApplyMsg> msgs;    
        {
            std::unique_lock<std::mutex> lock(this->m_raft.apply_mtx);
            //wait在阻塞时会释放锁
            this->m_raft.apply_cond.wait(lock,[this](){
                return !this->m_raft.m_msgs.empty();
            });
            while(!this->m_raft.m_msgs.empty()){
                msgs.emplace_back(this->m_raft.m_msgs.front());
                this->m_raft.m_msgs.pop();
            }
        }
        int n=msgs.size();
        for(auto msg:msgs){
            Operation op = msg.GetOperation();
            int index = msg.m_cmdIndex;
            OpContext* opctx = nullptr;
            bool opexist = false;
            bool seqexist = false;
            int prevclientrequestId = INT_MAX; //客户端已提交的的最大id
            {
                std::lock_guard<std::mutex> lock(this->ctx_mtx);
                if(this->m_requestMap.count(index)){
                    opctx = this->m_requestMap[index];
                    if(opctx->op.op_term != op.op_term){
                        opctx->state|=state::LEADER_WRONG;
                    }
                    opexist = true;
                }
            }
            {
                std::lock_guard<std::mutex> lock(this->seq_mtx);
                if(this->m_clientSeqMap.count(op.clientId)){
                    seqexist = true;
                    prevclientrequestId = m_clientSeqMap[op.clientId];
                }
                this->m_clientSeqMap[op.clientId] = op.requestId;
            }
            //处理
            if(op.op == "join" || op.op == "move"||op.op == "leave"){
                printf("[%d]'s prevIdx is %d, opIdx is %d, isSeqExist is %d, cliendID is %d, op is %s\n",
                this->m_id, prevclientrequestId, op.op_index, seqexist, op.clientId, op.op.c_str());
                if(!seqexist || prevclientrequestId < op.requestId){
                    this->OpHandler(op);
                }
            }
            else{
                //为query
                shardkv::QueryRequest req;
                req.ParseFromString(op.args);
                int queryid = req.configid();
                if(opctx !=nullptr){
                    std::lock_guard<std::mutex>  lock(cfg_mtx);
                    if(queryid >= this->configs.size()||queryid < 0){
                        opctx->config = this->configs[this->configs.size() - 1];
                    }else{
                        opctx->config = this->configs[queryid];
                    }
                }
            }
            if(opexist){
                opctx->ready = OpContext::OK;
                opctx->cond_.notify_one();
            }
        }
    }
}

bool ShardMaster::StartOperation(Operation op)
{
    StartRet ret =this->m_raft.Start(op);
    op.op_term = ret.m_curTerm;
    op.op_index = ret.m_cmdIndex;
    if(!ret.isLeader){
        return true;
    }
    OpContext ctx(op);
    {
        std::lock_guard<std::mutex> lock(this->ctx_mtx);
        this->m_requestMap[ret.m_cmdIndex] = &ctx;
    }
    {   //TODO 超时 换成wait_for，
        std::unique_lock<std::mutex> lock(ctx.mtx_);
        ctx.cond_.wait(lock,[&ctx](){
            return ctx.ready!=OpContext::NOT_READY;
        });
    }
    if(ctx.state&state::LEADER_WRONG){
        return true;
    }else{
        //ignore
    }
    {
        std::lock_guard<std::mutex> lock(this->ctx_mtx);
        this->m_requestMap.erase(op.op_index);
    }
    return false;
}

void ShardMaster::OpHandler(Operation op)
{
    Config config = this->configs.back();
    config.configId++;
    if(op.op == "join"){

        shardkv::JoinRequest req;
        req.ParseFromString(op.args);
        for(auto it:req.groups()){
            int gid = it.first;
            LOG_INFO<<gid;
            auto joininfo = it.second;
            std::vector<ShardInfo> rec;
            for(auto it2:joininfo.config()){
                ShardInfo tmp;
                tmp.ip = it2.ip();
                tmp.port = it2.port();
                rec.push_back(tmp);
            }
            this->hring.AddNode(gid); //往hash ring上添加节点
            config.groups[gid] = rec;
        }
        //空的则不做处理
        if(config.groups.empty()){
            return;
        }
        this->hring.rehashshard();
        config.shards = this->hring.Getshard();
        //this->BalanceShard(config); //对分片进行负载均衡
        this->configs.emplace_back(config);
        return;
    }
    else if(op.op == "move"){
        shardkv::MoveRequest req;
        req.ParseFromString(op.args);
        config.shards[req.shardid()] = req.gid();
        configs.push_back(config); 
    }
    else if(op.op =="leave"){
        shardkv::LeaveRequest req;
        req.ParseFromString(op.args);
        std::unordered_set<int> rec;
        for(auto gid :req.groupids()){
            rec.insert(gid);
            config.groups.erase(gid);
            this->hring.RemoveNode(gid);
        }

        if(config.groups.empty()){
            config.groups.clear();
            config.shards.resize(NShards, 0);
            return;
        }
        config.shards = this->hring.Getshard();
        configs.push_back(config);
        return;
    }
}

int ShardMaster::GetConfigSize()
{
    std::lock_guard<std::mutex> lock(this->cfg_mtx);
    return configs.size();
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
    op.args = request->SerializeAsString();
    //op.args_len = op.args.size();
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
    op.args = request->SerializeAsString();
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
    op.args = request->SerializeAsString();
    response.set_iswrongleader(StartOperation(op));
    return response;
}

shardkv::QueryResponse ShardMaster::Query(const shardkv::QueryRequest *request)
{
    shardkv::QueryResponse response;
    response.set_iswrongleader(false);
    Operation op;
    op.op = "query";
    op.key = "0";
    op.value = "0";
    op.clientId = request->clientid();
    op.requestId = request->requestid();
    op.args = request->SerializeAsString();
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
//test函数
std::vector<KVServerInfo> getKvServerPort(int num){
    std::vector<KVServerInfo> peers(num);
    int base_port = 1024;
    for(int i = 0; i < num; i++){
        peers[i].peersInfo.m_peerId = i; //raft层的id
        peers[i].peersInfo.port = base_port + i * 2; //raft层的port
        peers[i].peersInfo.ip = "192.168.203.128";
        peers[i].kv_ip = "192.168.203.128";
        peers[i].kv_port = base_port +  i * 2 + 1; //kvserver层的port
        // printf(" id : %d port1 : %d, port2 : %d\n", peers[i].m_peerId, peers[i].m_port.first, peers[i].m_port.second);
    }
    return peers;
}

int main(){
    auto kvinfo = getKvServerPort(5);
    ShardMaster* master[5];
    for(int i=0;i<5;i++){
        master[i] = new ShardMaster();
        master[i]->Init(kvinfo,i);
    }
    while(1);
    return 0;
}