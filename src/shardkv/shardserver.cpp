#include "shardserver.h"

ShardKVServer::ShardKVServer():thread_pool(ThreadPool_::GetThreadPool(5)){}

void ShardKVServer::Init(std::vector<KVServerInfo> &kvinfo, int me, int maxshotsnap, std::vector<MasterRPCInfo> &masters)
{
    this->m_id = me;
    this->ip = kvinfo[me].kv_ip;
    this->port = kvinfo[me].kv_port;
    std::vector<PeersInfo> peers = this->getRaftPort(kvinfo);
    this->m_snapshotthresh = maxshotsnap;
    this->m_lastAppliedIndex = 0;
    this->m_pullfinished = 0;
    this->requestId = 0;
    this->clerk.Init(masters);
    this->m_raft.Init(peers, me);
    this->m_database.clear();
    this->m_clientSeqMap.clear();
    this->m_requestMap.clear();

    rpc_server.setackcb(std::bind(&ShardKVServer::Ack,this,std::placeholders::_1));
    rpc_server.setgetcb(std::bind(&ShardKVServer::Get,this,std::placeholders::_1));
    rpc_server.setmigratecb(std::bind(&ShardKVServer::Migrate,this,std::placeholders::_1));
    rpc_server.setputcb(std::bind(&ShardKVServer::Put,this,std::placeholders::_1));

    ThreadPool_::GetThreadPool(5);
    this->rpcloop = std::thread(&ShardKVServer::RPCLoop,this);
    this->applyloop = std::thread(&ShardKVServer::ApplyLoop,this);
    this->snaploop = std::thread(&ShardKVServer::SnapShotLoop,this);
    this->ackgarbageloop = std::thread(&ShardKVServer::AckGarbageLoop,this);
    this->pullshardloop = std::thread(&ShardKVServer::PullShardLoop,this);
    this->updatecfgloop = std::thread(&ShardKVServer::UpdateConfigLoop,this);
    
}

std::vector<PeersInfo> ShardKVServer::getRaftPort(std::vector<KVServerInfo> &kvInfo)
{
    std::vector<PeersInfo> peers;
    int n = kvInfo.size();
    for(int i = 0; i < n; i++){
        peers.emplace_back(kvInfo[i].peersInfo);
    }
    return peers;
}

std::string ShardKVServer::SerilizeConfig(Config config)
{
    shardkv::Config cfg;
    cfg.set_confignum(config.configId);
    for(const auto& c:config.shards){
        cfg.add_shards(c);
    }
    for(const auto& g:config.groups){
        shardkv::JoinConfigs ser_info;
        for(const auto& info:g.second){
            auto t = ser_info.add_config();
            t->set_ip(info.ip);
            t->set_port(info.port);
        }
        (*cfg.mutable_groups())[g.first] =ser_info;
    }
    return cfg.SerializeAsString();
}

Config ShardKVServer::DeserilizeConfig(std::string &str)
{
    shardkv::Config cfg;
    cfg.ParseFromString(str);
    Config res;
    res.configId = cfg.confignum();
    int i=0;
    for(const auto& s:cfg.shards()){
        res.shards[i++]=s;
    }
    for(const auto& g:cfg.groups()){
        std::vector<ShardInfo> rec;
        for(const auto& info:g.second.config()){
            rec.emplace_back(info.ip(),info.port());
        }
        res.groups[g.first] = std::move(rec);
    }
    return res;
}

int ShardKVServer::getRequestId()
{
    std::lock_guard<std::mutex> lock(this->requestid_mtx);
    return requestId++;
}

void ShardKVServer::RPCLoop()
{
    std::string ip_port = this->ip+":"+std::to_string(this->port);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(ip_port, grpc::InsecureServerCredentials());

    builder.RegisterService(&rpc_server);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    server->Wait();
}

void ShardKVServer::ApplyLoop()
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
        for(auto msg : msgs){
            if(msg.isSnap){
                this->SnapshotHandler(msg);
            }
            else{
                //为普通日志
                Operation op = msg.GetOperation();
                int index = msg.m_cmdIndex;
                {
                    std::lock_guard<std::mutex> lock(this->apidx_mtx);
                    this->m_lastAppliedIndex = index;
                }
                if(op.op == "UC"){
                    //update config 不需要opctx来对updateconfig loop通知
                    Config config = this->DeserilizeConfig(op.value);
                    this->ConfigHandler(config);
                }
                else if(op.op == "US"){
                    //update shard 需要根据拉取的数据进行更新
                    kvrf::MigrationResponse res;
                    res.ParseFromString(op.value);
                    this->MigrationHandler(res);
                }
                else if(op.op == "CAK"){
                    //client ack 删除unacked中的记录，避免重复发送与占用内存
                    this->EraseUnackedHandler(stoi(op.key),stoi(op.value));
                }
                else{
                    //否则则需要通知回原本线程
                    int index = msg.m_cmdIndex;
                    {
                        std::lock_guard<std::mutex> lock(this->apidx_mtx);
                        this->m_lastAppliedIndex = index;
                    }
                    OpContext* opctx = nullptr;
                    bool opexist = false;
                    bool seqexist = false;
                    int prevclientrequestId = INT_MAX; //客户端已提交的的最大id
                    {
                        //从requestMap中取出context 并且根据传入raft层时的term和进入raft层后term变化与否，判断是否发生了重新了选主
                        std::lock_guard<std::mutex> lock(this->ctx_mtx);
                        if(this->m_requestMap.count(index)){
                            opctx = this->m_requestMap[index];
                            opexist = true;
                            if(opctx->op.op_term != op.op_term){
                                opctx->state&=state::LEADER_WRONG;
                            }
                        }
                    }
                    {
                        //根据客户端id找到其之前提交成功的最大requestId 
                        std::lock_guard<std::mutex> lock(this->seq_mtx);
                        if(this->m_clientSeqMap.count(op.clientId)){
                            seqexist = true;
                            prevclientrequestId = m_clientSeqMap[op.clientId];
                        }
                        this->m_clientSeqMap[op.clientId] = op.requestId;
                    }

                    //以上为共同代码 
                    if(op.op == "SCK"){
                        // server ack 接收到shard接收方的ack 删除outshards中的缓存
                        int cfgNum = stoi(op.key);
                        this->EraseOutShardHandler(cfgNum, op.requestId);
                    }
                    else if(op.op == "put" || op.op == "append"){
                        this->PutAppendHandler(op, opctx, opexist, seqexist, prevclientrequestId);
                        
                    }
                    else if(op.op == "get"){
                        this->GetHandler(op, opctx);
                    }
                    if(opexist){
                        opctx->ready = OpContext::OK;
                        opctx->cond_.notify_one();
                    }
                }
            }
        }
    }
}


void ShardKVServer::SnapShotLoop()
{
    while(1){
        int lastIncludedIndex;
        std::string snapshot;
        if(this->m_snapshotthresh != -1&&this->m_raft.ExceedLogSize(this->m_snapshotthresh)){
            snapshot = this->getSnapShot();
            {
                std::lock_guard<std::mutex> lock(this->apidx_mtx);
                lastIncludedIndex = this->m_lastAppliedIndex;
            }
        }
        if(snapshot.size()!=0){
            this->m_raft.RecvSnapShot(snapshot,lastIncludedIndex);
            printf("[%d] called recvsnapShot size is %d, lastapply is %d\n",this->m_id, snapshot.size(),this->m_lastAppliedIndex);
        }
        usleep(10000);
    }
}

void ShardKVServer::UpdateConfigLoop()
{
    while(1){
        bool isleader = this->m_raft.GetState().second;
        int comein_size;
        {
            std::lock_guard<std::mutex> lock(this->comein_mtx);
            //如果不是leader就继续continue 如果还有需要pullshard，此时暂时先不更新config 等待shards pull完成，
            //此时即使已经有config更新完成 在migrate时也会因为configid比对面group的configid低而返回拒绝
            comein_size = comeinshards.size();
        }
        if(!isleader || comein_size > 0){
            usleep(50000);
            continue;
        }
        int nextcfgid;
        {
            std::lock_guard<std::mutex> lock(this->cfg_mtx);
            nextcfgid = this->config.configId + 1; //为了确保每个config的变更都执行到
        }
        Config newconfig = this->clerk.Query(nextcfgid);
        std::string str = this->SerilizeConfig(newconfig);
        //判断一下 确保是config需要更新 start更新到大多数从机后，即会更新config和comeinshards和outshards
        if(newconfig.configId == nextcfgid){
            Operation op;
            op.op = "UC";
            op.key = "0";
            op.value = str;
            op.args = "0";
            op.requestId = this->getRequestId();
            op.clientId = 0;
            op.op_index = -1; //不需要记录 因为不会重复发生
            op.op_term = -1;
            this->m_raft.Start(op);
        }
        usleep(50000);
    }
}

void ShardKVServer::PullShardLoop()
{
    while(1){
        bool isleader = this->m_raft.GetState().second;
        int comein_size;
        {
            std::lock_guard<std::mutex> lock(this->comein_mtx);
            comein_size = this->comeinshards.size();
        }
        if(!isleader || comein_size==0){
            usleep(50000);
            continue;
        }
        std::vector<std::pair<int,int>> rec;
        {
            std::lock_guard<std::mutex> lock(this->comein_mtx);
            for(auto p:comeinshards){
                rec.emplace_back(p.first,p.second);
            }
        }
        for(int i=0;i<comein_size;i++){
            thread_pool.add_task(std::bind(&ShardKVServer::PullShard,this,rec[i].first,rec[i].second));
        }
        {
            std::unique_lock<std::mutex> lock(this->pullfin_mtx);
            this->pull_cond.wait(lock,[this,comein_size](){
                return this->m_pullfinished == comein_size;
            });
        }
        usleep(80000);
    }
}

void ShardKVServer::AckGarbageLoop()
{
    while(1){
        bool isleader = this->m_raft.GetState().second;
        int unack_size = 0;
        {
            std::lock_guard<std::mutex> lock(this->unack_mtx);
            unack_size = this->unacked.size();
        }
        if(!isleader || unack_size == 0){
            usleep(100000);
            continue;
        }
        //遍历所有unacked 发送ack
        std::unordered_map<int,std::unordered_set<int>> tmp_unacked;
        {
            std::lock_guard<std::mutex> lock(this->unack_mtx);
            tmp_unacked = this->unacked;
        }
        for(auto it:tmp_unacked){
            this->m_ackfinished = 0;
            thread_pool.add_task(&ShardKVServer::SendAskAck,this,it.first,it.second);
            {
                //条件变量这里等待发送任务完成，一个原因是如果不等待，下次又进入循环了，之前的ack还没发完，这里又重复调用sendaskack，导致重复发送
                std::unique_lock<std::mutex> lock(this->ackfin_mtx);
                ack_cond.wait(lock,[this,it](){
                    return this->m_ackfinished == it.second.size();
                });
            }
        }
        usleep(100000);
    }
}

kvrf::GetResponse ShardKVServer::Get(const kvrf::GetRequest * request)
{
    kvrf::GetResponse response;
    if(!this->IsMatchShard(request->key())){
        response.set_iswronggroup(true);
        return response;
    }
    response.set_iswronggroup(false);
    if(!this->m_raft.GetState().second){
        response.set_iswrongleader(true);
        return response;
    }
    Operation operation;
    operation.op = "get";
    operation.key = request->key();
    operation.clientId = request->clientid();
    operation.requestId = request->requestid();
    operation.args = "0";
    StartRet ret = this->m_raft.Start(operation);
    operation.op_index =  ret.m_cmdIndex;
    operation.op_term = ret.m_curTerm;

    OpContext opctx(operation);
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
    }else if(opctx.state&state::KEY_WRONG){
        response.set_isexist(false);
    }else{
        response.set_isexist(true);
        response.set_value(opctx.op.value);
    }
    {
        std::lock_guard<std::mutex> lock(this->ctx_mtx);
        if(this->m_requestMap.count(operation.op_index)){
            if(this->m_requestMap[operation.op_index] == &opctx){
                this->m_requestMap.erase(operation.op_index);
            }
        }
    }
    return response;
}

kvrf::PutResponse ShardKVServer::Put(const kvrf::PutRequest * request)
{
    kvrf::PutResponse response;
    response.set_iswronggroup(false);
    response.set_iswrongleader(false);
    if(!this->IsMatchShard(request->key())){
        response.set_iswronggroup(true);
        return response;
    }
    if(!this->m_raft.GetState().second){
        response.set_iswrongleader(true);
        return response;
    }
    Operation operation;
    operation.op = "put";
    operation.key = request->key();
    operation.value = request->value();
    operation.clientId = request->clientid();
    operation.requestId = request->requestid();
    operation.args = "0";
    StartRet ret = this->m_raft.Start(operation);
    operation.op_index =  ret.m_cmdIndex;
    operation.op_term = ret.m_curTerm;

    OpContext opctx(operation);
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
    }else{

        response.set_ok(true);
    }
    {
        std::lock_guard<std::mutex> lock(this->ctx_mtx);
        if(this->m_requestMap.count(operation.op_index)){
            if(this->m_requestMap[operation.op_index] == &opctx){
                this->m_requestMap.erase(operation.op_index);
            }
        }
    }
    return response;
}

kvrf::MigrationResponse ShardKVServer::Migrate(const kvrf::MigrationRequest * request)
{
    kvrf::MigrationResponse response;
    if(!this->m_raft.GetState().second){
        response.set_err("WL"); //WrongLeader
        return response;
    }
    //请求的cfgid 比 当前的cfgid大，说明当前group还没有来得及更新配置
    int cfgid;
    {
        std::lock_guard<std::mutex> lock(this->cfg_mtx);
        cfgid = this->config.configId;
    }
    if(request->configid() > cfgid){
        response.set_err("NR"); //NotReady
        return response;
    }
    //如果是小于的话 要发送的数据是记录在outshards中的，可以直接读取后返回 
    //等于的话，有两种方案：
    //1、updateconfig的handler函数中，可以先更新config，然后再往outshards中填充要转移的数据，这里就先临时读取一部分要发送的数据
    //避免一次性发送太多数据
    //2、需要要保证更新outshards完成，再更新config，这样当等于的时候，要发送的数据一定全部存到了outshards中，
    std::unordered_map<std::string,std::string> tmp_outshards;
    {
        std::lock_guard<std::mutex> lock(this->outshard_mtx);
        tmp_outshards = outshards[request->configid()][request->shardid()];
    }
    for(const auto& data : tmp_outshards){
        (*response.mutable_database())[data.first] = data.second;
    }
    {
        //还要将m_clientSeqMap发送回去，避免接收到分片的group重复执行某些指令
        std::lock_guard<std::mutex> lock(this->seq_mtx);
        for(const auto& it : this->m_clientSeqMap){
            (*response.mutable_clientreq())[it.first]=it.second;
        }
    }
    response.set_shardid(request->shardid());
    response.set_configid(request->configid());
    return response;
}

kvrf::AckResponse ShardKVServer::Ack(const kvrf::AckRequest * request)
{
    kvrf::AckResponse response;
    if(!this->m_raft.GetState().second){
        response.set_error("WL"); //Wrong leader
        return response;
    }
    {
        std::lock_guard<std::mutex> lock(this->outshard_mtx);
        if(!this->outshards[request->configid()].count(request->shardid())){
            //如果outshards中没有 可能是上一次处理时返回response的时候，因为网络问题，调用方没有接收到response，导致重发
            //但是当前group已经提交成功了日志，然后删除了outshard中对应的shard
            //也有可能是更新配置过程中，outshards还没有更新，但是能发ack过来，一定是接收到了migrate传输的数据，而migrate要传输数据则对应的shard一定存在于outshards中
            //因此只能是网络问题重发了ack，因此返回成功
            response.set_received(true);
            response.set_error("OK");
            return response;
        }
    }
    response.set_error("OK");
    Operation op;
    op.op = "SAK"; //server ACK 这个所作为rpc server端 接收到ack后需要将outshards中缓存数据删除
    op.key = request->configid();
    op.value = request->shardid();
    op.requestId = this->getRequestId();
    op.clientId = 0;
    op.args = "0";
    auto ret = this->m_raft.Start(op);
    op.op_index = ret.m_cmdIndex;
    op.op_term = ret.m_curTerm;
    if(!ret.isLeader){ //重新判断的目的是避免start时宕机了（与其他group成员连接不上），导致重新选举
        response.set_error("WL");
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
        response.set_error("WL");
    }else{
        response.set_error("OK");
        response.set_received(true);
    }
    {
        std::lock_guard<std::mutex> lock(this->ctx_mtx);
        //判断多次的原因 是避免raft层发生日志回滚，导致当前日志丢失，但是发生日志回滚时一定不是leader，上面也返回了wrongleader的错误
        //假设之前是leader，然后重新选举后不是leader了，那么这时候有可能是回滚之后，有新的操作日志到来，index重新到了之前的op_index
        //那么如果直接删除的话就会出错，因此这里判断一下
        if(this->m_requestMap.count(op.op_index)){
            if(this->m_requestMap[op.op_index] == &opctx){
                this->m_requestMap.erase(op.op_index);
            }
        }
    }
    return response;
}

void ShardKVServer::SendAskAck(int cfgid, std::unordered_set<int> shards)
{
    Config config = this->clerk.Query(cfgid);
    for(auto shard:shards){
        int gid = config.shards[shard];
        kvrf::AckRequest request;
        request.set_configid(cfgid);
        request.set_shardid(shard);
        for(auto& ser:config.groups[gid]){
            std::string ip_port = ser.ip + ":" + std::to_string(ser.port);
            auto channel = grpc::CreateChannel(ip_port, grpc::InsecureChannelCredentials());
            auto stub_ptr=kvrf::KvRaftServiceRpc::NewStub(channel);
             ::grpc::ClientContext context;
            kvrf::AckResponse response;
            grpc::Status status = stub_ptr->Ack(&context,request,&response);
            if(status.ok() && response.error()=="OK"){
                //将unacked中对应去掉
                std::lock_guard<std::mutex> lock(this->unack_mtx);
                this->unacked[cfgid].erase(shard);
                if(this->unacked[cfgid].empty()){
                    this->unacked.erase(cfgid);
                }
                //需要跟其他server也进行同步 然后删除unacked中对应 不需要等待完成unacked都是已经同步完成的shard 
                Operation op;
                op.op ="CAK"; //client ack update shard 
                op.key = std::to_string(cfgid); //这里转换为string 传输int类型，有些浪费资源，这里可以优化一下
                op.value = std::to_string(shard);
                op.args = "0";
                op.requestId = this->getRequestId();
                op.clientId = 0;
                auto ret = this->m_raft.Start(op);
                op.op_index = ret.m_cmdIndex;
                op.op_term = ret.m_curTerm;
                break;
            }
        }
        {
            //这里无论成没成功，这里都要finish++，即使没有成功，也要
            std::lock_guard<std::mutex> lock(this->ackfin_mtx);
            m_ackfinished++;
        }
    }
    this->ack_cond.notify_one();
}

void ShardKVServer::PullShard(int shardid, int cfgid)
{
    kvrf::MigrationRequest request;
    kvrf::MigrationResponse response;
    request.set_configid(cfgid);
    request.set_shardid(shardid);
    int pregid = this->preconfig.shards[shardid]; //因为更新了配置 需要从shard之前所在的group请求shard
    //轮询向对应group下的server进行访问
    for(const auto& server:this->preconfig.groups[pregid]){
        std::string ip_port = server.ip + ":" + std::to_string(server.port);
        auto channel = grpc::CreateChannel(ip_port, grpc::InsecureChannelCredentials());
        auto stub_ptr=kvrf::KvRaftServiceRpc::NewStub(channel);
        ::grpc::ClientContext context;
        grpc::Status status = stub_ptr->Migrate(&context,request,&response);
        if(status.ok()&&response.err()=="OK"){
            //拉取是需要自己更新shard的 因此要start(op) 这里不用opcontext的原因是 只是内部同步这个日志信息，
            //首先是即使没有同步到大多数的server，当前挂掉了，新上任的pullloop也会因为更新了config 去拉取新的对应的shard
            //leader这里也不需要等待一半以上同步完成，接着执行就行了
            Operation op;
            op.op = "US"; //update shard
            op.key = "0";
            op.value = response.SerializeAsString();
            op.args = "0";
            op.requestId = this->getRequestId();
            op.clientId = 0;
            op.op_index = -1;
            op.op_term = -1;
            this->m_raft.Start(op); //等将该操作同步到大多数从机后，再applyloop中读取指令后再删除comeinshards对应的shard
            op.op_index = -1;
            op.op_term = -1;
            {
                std::lock_guard<std::mutex> lock(this->pullfin_mtx);
                this->m_pullfinished ++;
            }
            this->pull_cond.notify_one();
            break;
        }
    }
}

void ShardKVServer::PutAppendHandler(Operation op,OpContext* ctx,bool opexist,bool seqexist,int prevRequestId)
{
    int shard = this->key2shard(op.key);
    {
        std::lock_guard<std::mutex> lock(this->avail_mtx);
        if(!this->m_availshards.count(shard)){
            ctx->state |= state::GROUP_WRONG;
            return;
        }
    }
    if(!seqexist || prevRequestId < op.requestId){
        std::lock_guard<std::mutex> lock(this->db_mtx);
        if(op.op == "put"){
            this->m_database[op.key] = op.value;
        }
        else if(op.op == "append"){
            this->m_database[op.key].append(op.value); 
        }
    }
    else if(opexist){
        //否则说明之前请求过一样的 
        std::lock_guard<std::mutex> lock(ctx->mtx_);
        ctx->isIgnored = true;
    }
}

void ShardKVServer::GetHandler(Operation op, OpContext *ctx)
{
    if(ctx == nullptr)
        return;
    std::lock_guard<std::mutex> lock(this->db_mtx);
    //如果数据库中存在key
    if(this->m_database.count(op.key)){
        std::lock_guard<std::mutex> lock(ctx->mtx_);
        ctx->op.value = this->m_database[op.key];
    }
    else{
        //不存在
        std::lock_guard<std::mutex> lock(ctx->mtx_);
        ctx->state|=state::KEY_WRONG;
    }
}

void ShardKVServer::SnapshotHandler(ApplyMsg msg)
{
    if(msg.snapShot.size()==0){
        {
            std::lock_guard<std::mutex> lock(this->db_mtx);
            this->m_database.clear();
        }
        {
            std::lock_guard<std::mutex> lock(this->seq_mtx);
            this->m_clientSeqMap.clear();
        }
    }
    else{
        this->RecoveryFromSnapShot(msg.snapShot);
        {
            std::lock_guard<std::mutex> lock(this->apidx_mtx);
            this->m_lastAppliedIndex = msg.lastIncludedIndex;
        }
    }
}

void ShardKVServer::ConfigHandler(Config cfg)
{
    Config old_cfg;
    std::unordered_set<int> outshards_tmp;
    {
        //cfg为比当前config更新的config 所以这里如果比当前config的cfgid小于等于，那说明已经提交过了，忽略掉
        std::lock_guard<std::mutex> lock(this->cfg_mtx);
        if(cfg.configId <= this->config.configId){
            return;
        }
        old_cfg = this->config;
        this->config = cfg; //更新了新配置
    }
    {
        std::lock_guard<std::mutex> lock_guard(this->avail_mtx);
        outshards_tmp = std::move(this->m_availshards);
        //this->m_availshards.clear();
        for(int i = 0; i < config.shards.size() ; i++){
            if(config.shards[i] != this->m_groupid){
                continue;
            }
            //如果新配置中分给当前group的shard已经在当前group中了 或者old group为初始group 则要将所有配置中归为当前group的分片都要加入到当前
            if(outshards_tmp.count(i) || old_cfg.configId == 0){
                outshards_tmp.erase(i);
                this->m_availshards.insert(i);
            }
            else{
                //如果没有在当前 则需要去pull
                std::lock_guard<std::mutex> lock(this->comein_mtx);
                //这里记录的是老配置文件id，要拉取的数据切片要从老配置中获得其所在的group 传old_cfg的cfgid也是为了告诉对方是根据这个cfg来pull shard的
                this->comeinshards[i] = old_cfg.configId;
            }
        }
    }
    //如果要发送出去的shard个数大于0
    if(outshards_tmp.size() > 0){
        for(const auto& shard : outshards_tmp){
            std::unordered_map<std::string,std::string> shard_data;
            std::unordered_map<std::string, std::string> dataBackUp; //用一个临时变量存储数据库数据
            {
                std::lock_guard<std::mutex> lock_guard(this->db_mtx);
                dataBackUp = this->m_database;
            }
            //这样遍历数据效率较低 //TODO 后面考虑直接将数据库变为shardid->sharddata sharddata为一个kv键值对map 可以更快找到对应shard的数据
            for(const auto& data : dataBackUp){
                if(this->key2shard(data.first) == shard){
                    shard_data.insert(data);
                    {
                        std::lock_guard<std::mutex> lock_guard(this->db_mtx);
                        this->m_database.erase(data.first);
                    }
                }
            }
            {
                std::lock_guard<std::mutex> lock(this->outshard_mtx);
                this->outshards[old_cfg.configId][shard] = std::move(shard_data);
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(this->cfg_mtx);
        this->preconfig = old_cfg;
    }
}

void ShardKVServer::MigrationHandler(kvrf::MigrationResponse& response)
{
    {
        std::lock_guard<std::mutex> lock(this->cfg_mtx);
        //只有是preconfig的才能更新
        if(response.configid() != this->config.configId - 1){
            return;
        }
    }
    {
        std::lock_guard<std::mutex> lock(this->comein_mtx);
        this->comeinshards.erase(response.shardid());
    }
    {
        std::lock_guard<std::mutex> lock(this->avail_mtx);
        //如果当前已拥有的shards中没有回复的shard 这里是为了避免重复migrate
        if(!this->m_availshards.count(response.shardid())){
            for(const auto& data : response.database()){
                std::lock_guard<std::mutex> lock(this->db_mtx);
                this->m_database[data.first] = data.second;
            }
            for(const auto& seq : response.clientreq()){
                std::lock_guard<std::mutex> lock(this->seq_mtx);
                this->m_clientSeqMap[seq.first] = std::max(this->m_clientSeqMap[seq.first], seq.second);//记录对应clientId最大的requestId
            }
            this->m_availshards.insert(response.shardid());
            {
                std::lock_guard<std::mutex> lock(this->unack_mtx);
                this->unacked[response.configid()].insert(response.shardid()); //需要添加unacked中 发送ack给对应的group
            }
        }
    }
}

void ShardKVServer::EraseOutShardHandler(int cfgid, int shardid)
{
    std::lock_guard<std::mutex> lock(this->outshard_mtx);
    if(outshards.count(cfgid)){
        outshards[cfgid].erase(shardid);
        if(outshards[cfgid].empty()){
            outshards.erase(cfgid);
        }
    }
}

void ShardKVServer::RecoveryFromSnapShot(std::string snapshot)
{
    rf::SnapShotInfo info;
    info.ParseFromString(snapshot);
    //database
    if(info.data().size()==0){
        {
            std::lock_guard<std::mutex> lock(this->db_mtx);
            this->m_database.clear();
        }
    }
    {
        std::lock_guard<std::mutex> lock(this->db_mtx);
        for(const auto& data : info.data()){
            this->m_database[data.first] = data.second;
        }
    }
    //clientseq
    {
        std::lock_guard<std::mutex> lock(this->seq_mtx);
        for(const auto& entry : info.clientseq()){
            this->m_clientSeqMap[entry.first] = entry.second;
        }
    }
    //comeinshards
    {
        std::lock_guard<std::mutex> lock(this->comein_mtx);
        for(const auto& it : info.comeinshards()){
            this->comeinshards[it.first] = it.second;
        }
    }
    //outshards
    {
        std::lock_guard<std::mutex> lock(this->outshard_mtx);
        for(const auto& it : info.outshards()){
            for(const auto& it2 : it.second.data()){
                for(const auto& it3 : it2.second.data()){
                    this->outshards[it.first][it2.first][it3.first] = it3.second;
                }
            }
        }
    }
    //avalishards
    {
        std::lock_guard<std::mutex> lock(this->avail_mtx);
        for(const auto& it : info.avalishards()){
            this->m_availshards.insert(it);
        }
    }
    //unacked
    {
        std::lock_guard<std::mutex> lock(this->unack_mtx);
        for(const auto& it : info.unacked()){
            for(const auto& it2 : it.second.data()){
                this->unacked[it.first].insert(it2);
            }
        }
    }
    //config
    {
        std::lock_guard<std::mutex> lock(this->cfg_mtx);
        this->config.configId = info.config().confignum();
        int i=0;
        for(const auto& s:info.config().shards()){
            this->config.shards[i++]=s;
        }
        for(const auto& g:info.config().groups()){
            std::vector<ShardInfo> rec;
            for(const auto& info:g.second.config()){
                rec.emplace_back(info.ip(),info.port());
            }
            this->config.groups[g.first] = std::move(rec);
        }
    }
}

void ShardKVServer::EraseUnackedHandler(int cfgid, int shardid)
{
    std::lock_guard<std::mutex> lock(this->unack_mtx);
    this->unacked[cfgid].erase(shardid);
    if(this->unacked[cfgid].empty()){
        this->unacked.erase(cfgid);
    }
}

int ShardKVServer::key2shard(std::string key){
    int shard = 0;
    if(key.size() > 0){
        shard = key[0] - 'a';
    }
    shard = shard % NShards;
    return shard;
}

bool ShardKVServer::IsMatchShard(std::string key)
{
    std::lock_guard<std::mutex> lock(this->cfg_mtx);
    return this->m_groupid == this->config.shards[key2shard(key)];
}

std::string ShardKVServer::getSnapShot()
{
    rf::SnapShotInfo info;
    //database 
    {
        std::lock_guard<std::mutex> lock(this->db_mtx);
        for(const auto& ele : m_database){
            //snapshot += ele.first + " " + ele.second + ".";
            (*info.mutable_data())[ele.first]=ele.second;
        }
    }
    //clientseq
    {
        std::lock_guard<std::mutex> lock(this->seq_mtx);
        for(const auto& ele : m_clientSeqMap){
            (*info.mutable_clientseq())[ele.first] = ele.second;
        }
    }

    // shardkv.Config config = 7; //当前配置
    //comeinshards
    {
        std::lock_guard<std::mutex> lock(this->comein_mtx);
        for(const auto& it:this->comeinshards){
            (*info.mutable_comeinshards())[it.first] = it.second;
        }
    }
    //outshards
    {
        std::lock_guard<std::mutex> lock(this->outshard_mtx);
        for(const auto& it : this->outshards){
            rf::ShardsData tmp;
            for(const auto& it2 : it.second){
                rf::ShardData data;
                for(const auto& it3 : it2.second){
                    (*data.mutable_data())[it3.first] = it3.second;
                }
                (*tmp.mutable_data())[it2.first] = data;
            }
            (*info.mutable_outshards())[it.first] = tmp;
        }
    }
    //avail_shards
    {
        std::lock_guard<std::mutex> lock(this->avail_mtx);
        for(const auto& it : this->m_availshards){
            info.add_avalishards(it);
        }
    }
    //unacked
    {
        std::lock_guard<std::mutex> lock(this->unack_mtx);
        for(const auto& it : this->unacked){
            rf::UnAckShards t;
            for(const auto& it2 : it.second){
                t.add_data(it2);
            }
            (*info.mutable_unacked())[it.first] = t;
        }
    }
    //config
    {
        std::lock_guard<std::mutex> lock(this->cfg_mtx);
        shardkv::Config* cfg = info.mutable_config();
        cfg->set_confignum(this->config.configId);
        for(const auto& c:this->config.shards){
            cfg->add_shards(c);
        }
        for(const auto& g:this->config.groups){
            shardkv::JoinConfigs ser_info;
            for(const auto& info:g.second){
                auto t = ser_info.add_config();
                t->set_ip(info.ip);
                t->set_port(info.port);
            }
            (*(cfg->mutable_groups()))[g.first] =ser_info;
        }
    }
    std::string ans = info.SerializeAsString();
    return ans;
}

