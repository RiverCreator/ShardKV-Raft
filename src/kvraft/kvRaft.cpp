#include "kvRaft.h"
#include <sstream>
KVServer::KVServer(std::vector<KVServerInfo> &kvInfo, int me, int snapshotthresh):server(kvInfo[me].kv_ip,kvInfo[me].kv_port,5)
{
    server.setonmessagecb(std::bind(&KVServer::HandleMessage,this, std::placeholders::_1, std::placeholders::_2));
    this->m_ip = kvInfo[me].kv_ip;
    this->m_port = kvInfo[me].kv_port;
    this->m_id = me;
    this->peers = this->getRaftPort(kvInfo);
    this->m_snapshotthresh = snapshotthresh;
}

OpContext::OpContext(Operation &op)
{
    this->op = op;
    this->isIgnored = false;
    this->ready = NOT_READY;
}

void KVServer::Init()
{    
    this->m_raft.Init(peers, this->m_id);
    this->m_lastAppliedIndex = 0;
    this->m_database.clear();
    this->m_clientSeqMap.clear();
    apply_loop = std::thread(&KVServer::ApplyLoop,this);
    snapshot_loop = std::thread(&KVServer::CheckSnapShotLoop,this);
    server_loop = std::thread(&KVServer::start,this);
}

void KVServer::HandleMessage(spConnection conn, std::string &message)
{
    Operation op;
    bool ret = ParseRequest(message, op);
    kvrf::Response response;
    if(!ret){
        response.set_type("bug");
        response.set_response_msg("wrong request");
    }
    else{
        if(op.op == "get"){
            response = this->GetImpl(op);
        }
        else if(op.op == "set"){
            response = this->PutImpl(op);
        }
        else if(op.op == "del"){

        }
    }
    std::string reply = response.SerializeAsString();
    conn->send(reply, reply.size());
}
//TODO 可以添加管道机制，多个请求一次发送过来
bool KVServer::ParseRequest(std::string message, Operation& res)
{
    
    kvrf::Request request;
    request.ParseFromString(message);
    if(request.type() == "get"){
        kvrf::GetRequest get_request;
        get_request.ParseFromString(request.request_msg());
        res.op = "get";
        res.key = get_request.key();
        res.clientId = get_request.clientid();
        res.requestId = get_request.requestid();
        return true;
    }
    else if(request.type() == "set"){
        kvrf::PutRequest put_request;
        put_request.ParseFromString(request.request_msg());
        res.op = put_request.op();
        res.key = put_request.key();
        res.value = put_request.value();
        res.clientId = put_request.clientid();
        res.requestId = put_request.requestid();
        return true;
    }
    else if(request.type() == "del"){
        return true;
    }
    else{
        return false;
    }
}

std::vector<PeersInfo> KVServer::getRaftPort(std::vector<KVServerInfo> &kvInfo)
{
    std::vector<PeersInfo> peers;
    int n = kvInfo.size();
    for(int i = 0; i < n; i++){
        peers.emplace_back(kvInfo[i].peersInfo);
    }
    return peers;
}

std::string KVServer::getSnapShot()
{
    rf::SnapShotInfo info;
    {
        std::lock_guard<std::mutex> lock(this->db_mtx);
        for(const auto& ele : m_database){
            //snapshot += ele.first + " " + ele.second + ".";
            (*info.mutable_data())[ele.first]=ele.second;
        }
    }
    {
        std::lock_guard<std::mutex> lock(this->seq_mtx);
        for(const auto& ele : m_clientSeqMap){
            (*info.mutable_clientseq())[ele.first] = ele.second;
        }
    }
    printf("[ip:%s port:%d] get snapshot\n",this->m_ip.c_str(),this->m_port);
    //printf("[ip:%s port:%d] snapshot is %s",this->m_ip.c_str(),this->m_port, snapshot.c_str());
    std::string snapshot = info.SerializeAsString();
    return snapshot;
}

void KVServer::RecoveryFromSnapShot(std::string snapshot)
{
    rf::SnapShotInfo info;
    info.ParseFromString(snapshot);
    {
        std::lock_guard<std::mutex> lock(this->db_mtx);
        for(const auto& entry : info.data()){
            this->m_database[entry.first] = entry.second;
        }
    }
    {
        std::lock_guard<std::mutex> lock(this->seq_mtx);
        for(const auto& entry : info.clientseq()){
            this->m_clientSeqMap[entry.first] = entry.second;
        }
    }
    // printf("-----------------databegin---------------------------\n");
    // for(auto a : this->m_database){
    //     printf("data-> key is %s, value is %s\n", a.first.c_str(), a.second.c_str());
    // }
    // printf("-----------------requSeqbegin---------------------------\n");
    // for(auto a : this->m_clientSeqMap){
    //     printf("data-> key is %d, value is %d\n", a.first, a.second);
    // }
}

kvrf::Response KVServer::PutImpl(Operation &op)
{
    kvrf::Response res;
    kvrf::PutResponse response;
    res.set_type("set");
    StartRet ret = this->m_raft.Start(op);
    op.op_term = ret.m_curTerm;
    op.op_index = ret.m_cmdIndex;
    if(!ret.isLeader){
        response.set_ok(false);
        res.set_iswrongleader(true);
        res.set_response_msg(response.SerializeAsString());
        return res;
    }
    OpContext context(op);
    {
        std::lock_guard<std::mutex> lock(this->ctx_mtx);
        this->m_requestMap[op.op_index] = &context;
    }
    {
        std::unique_lock<std::mutex> lock(context.mtx_);
        context.cond_.wait(lock,[&context](){
            return context.ready!=OpContext::NOT_READY;
        });
    }
    LOG_WAR("set ok?");
    if(context.state&state::LEADER_WRONG){
        response.set_ok(false);
        res.set_iswrongleader(true);
        res.set_response_msg(response.SerializeAsString());
        return res;
    }else{
        //ignore
    }
    {
        std::lock_guard<std::mutex> lock(this->ctx_mtx);
        this->m_requestMap.erase(op.op_index);
    }
    response.set_ok(true);
    res.set_iswrongleader(false);
    res.set_response_msg(response.SerializeAsString());
    return res;
}

kvrf::Response KVServer::GetImpl(Operation &op)
{
    kvrf::Response res;
    kvrf::GetResponse response;
    res.set_type("get");
    response.set_isexist(false);
    {
        std::lock_guard<std::mutex> lock(this->db_mtx);
        if(this->m_database.count(op.key)){
            response.set_isexist(true);
            response.set_value(this->m_database[op.key]);
        }
    }
    StartRet ret = this->m_raft.Start(op);
    op.op_term = ret.m_curTerm;
    op.op_index = ret.m_cmdIndex;
    if(!ret.isLeader){
        res.set_iswrongleader(true);
        res.set_response_msg(response.SerializeAsString());
        return res;
    }
    OpContext context(op);
    {
        std::lock_guard<std::mutex> lock(this->ctx_mtx);
        this->m_requestMap[op.op_index] = &context;
    }
    {
        std::unique_lock<std::mutex> lock(context.mtx_);
        context.cond_.wait(lock,[&context](){
            return context.ready!=OpContext::NOT_READY;
        });
    }
    if(context.state&state::LEADER_WRONG){
        res.set_iswrongleader(true);
    }else if(context.state&state::KEY_WRONG){
        response.set_isexist(false);
    }else{
        response.set_value(context.op.value);
    }
    {
        std::lock_guard<std::mutex> lock(this->ctx_mtx);
        this->m_requestMap.erase(op.op_index);
    }
    res.set_response_msg(response.SerializeAsString());
    return res;
}   

bool KVServer::IsLeader()
{
    return this->m_raft.GetState().second;
}

void KVServer::start()
{
    this->server.start();
}

std::string KVServer::test(std::string key)
{
    return this->m_database[key];
}

void KVServer::killraft()
{
    this->m_raft.Stop();
}

void KVServer::activateraft()
{
    this->m_raft.Activate();
    
}

void KVServer::ApplyLoop()
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
                //为快照
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
            else{
                //为普通日志
                Operation op = msg.GetOperation();
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
                    std::lock_guard<std::mutex> lock(this->ctx_mtx);
                    if(this->m_requestMap.count(index)){
                        opctx = this->m_requestMap[index];
                        opexist = true;
                    }
                }
                {

                    std::thread::id id = std::this_thread::get_id();
                    std::ostringstream oss;
                    oss<<id;
                    std::string id_str = oss.str();
                    LOG_INFO("op:%s:%d [%s]",op.op.c_str(),op.op_index,id_str.c_str());
                    LOG_INFO("isleader ?[%d] [%s]",this->m_raft.GetState().second,id_str.c_str());
                    //sleep(2);
                    if(opctx != nullptr){
                        LOG_INFO("%d",opctx);
                        std::lock_guard<std::mutex> lock(opctx->mtx_);
                        if(opctx&&opctx->op.op_term != op.op_term){
                            opctx->state |= state::LEADER_WRONG;
                        }
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
                //TODO 【后】加入数据库的其他操作 hset，hadd等
                if(op.op == "set"){
                    //这里一种第一种情况为当前为从机，刚开始同步，还没有之前的记录，则直接记录
                    //第二种情况为当之前请求的最大requestId比当前操作的requestId小时，才写入操作，否则表示已经写入了数据库，为了保持幂等性
                    if(!seqexist || prevclientrequestId < op.requestId){
                        LOG_WAR("slave syn!\n");
                        LOG_WAR("seq exist? [%d]",seqexist);
                        LOG_WAR("prevclientrequestId:[%d],op.requestId:[%d]",prevclientrequestId,op.requestId);
                        LOG_WAR("client exist? [%d]",opexist);
                        {
                            std::lock_guard<std::mutex> lock(this->db_mtx);
                            this->m_database[op.key] = op.value;
                        }
                    }else if(opexist){
                        LOG_WAR("opexit!\n");
                        {
                            std::lock_guard<std::mutex> lock(opctx->mtx_);
                            opctx->isIgnored = true;
                        }
                    }
                }
                else if(op.op == "get"){
                    {
                        std::lock_guard<std::mutex> lock(this->db_mtx);
                        if(this->m_database.count(op.key) && opctx != nullptr){
                            std::lock_guard<std::mutex> lock(opctx->mtx_);
                            opctx->op.value = this->m_database[op.key];
                        }
                        else{
                            if(opctx != nullptr){
                                std::lock_guard<std::mutex> lock(opctx->mtx_);
                                opctx->state|=state::KEY_WRONG;
                            }
                        }
                    }
                }
                else{
                    //其他操作
                }
                if(opexist){
                    LOG_WAR("notify!\n");
                    opctx->ready = OpContext::OK;
                    opctx->cond_.notify_one();
                }
            }
        }
    }
}

void KVServer::CheckSnapShotLoop()
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

//test 函数 生成kvserver即raft层的ip和port， 这里采用的是一个kvserver对应一个raft，raft层rpcserver采用的单个port监听，
//kvraft层使用的tcpserver，也是只用了一个port监听
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

int main(int argc,char* argv[]){
    std::vector<KVServerInfo> servers = getKvServerPort(5);
    srand((unsigned)time(NULL));
    KVServer* kv[5];
    for(int i = 0; i < 5; i++){
        kv[i] = new KVServer(servers, i, 1024);
        kv[i]->Init();
        printf("Init ok\n");
    }

    //--------------------------------------test---------------------------------------------
    sleep(3);
    for(int i = 0; i < 5; i++){
        printf("server%d's key : abc -> value is %s\n", i, kv[i]->test("abc").c_str());
    }
    sleep(5);
    int i = 0;
    while(1){
        i = rand() % 5;
        if(!kv[i]->IsLeader()){
            kv[i]->killraft();          //先让某个不是leader的raft宕机，不接受leader的appendEntriesRPC，让日志落后于leader的快照状态
            break;
        }
    }
    sleep(3);
    kv[i]->activateraft();              //重新激活对应的raft，在raft层发起installRPC请求，且向对应落后的kvServer安装从raft的leader处获得的快照
    //--------------------------------------test---------------------------------------------
    while(1);
    return 0;
}