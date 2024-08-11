#include "RaftServer.h"
Raft::Raft():thread_pool_(ThreadPool_::GetThreadPool(5)){

}

Raft::~Raft()
{
    this->m_stop=true;
    election_loop_thread.join();
    process_entry_thread.join();
}

void Raft::Init(std::vector<PeersInfo> &peers, int id)
{
    m_peers = peers;
    peer_id = id;
    m_stop=false;
    m_state = FOLLOWER;
    m_curTerm = 0;
    m_leaderId = -1;
    m_voteFor = -1;
    dead.resize(m_peers.size(),0);
    gettimeofday(&m_lastWakeTime, NULL);

    recvVotes = 0;
    finishedVote = 0;
    cur_peerId = 0;

    m_lastApplied = 0;
    m_commitIndex = 0;
    m_lastIncludedIndex = 0;
    m_lastIncludeTerm = 0;
    m_nextIndex.resize(peers.size(), 1);
    m_matchIndex.resize(peers.size(), 0);

    ReadRaftState();
    this->installSnapShotTOkvServer(true);
    rpc_server.SetVoteCallBack(std::bind(&Raft::ReplyVote,this,std::placeholders::_1));
    rpc_server.SetAppendCallBack(std::bind(&Raft::ReplyAppend,this,std::placeholders::_1));
    rpc_server.SetInstallSnapShotCallBack(std::bind(&Raft::ReplyInstallSnap,this,std::placeholders::_1));
    for(int i=0;i<peers.size();i++){
        std::string ip_port=peers[i].ip+":"+std::to_string(peers[i].port);
        auto channel = grpc::CreateChannel(ip_port, grpc::InsecureChannelCredentials());
        stub_channels_.emplace_back(channel);
        stub_ptrs_.emplace_back(rf::RaftServerRpc::NewStub(channel));
    }
    rpc_thread = std::thread(&Raft::RPCLoop,this,peers[id].ip,peers[id].port);
    election_loop_thread=std::thread(&Raft::ElectionLoop,this);
    process_entry_thread=std::thread(&Raft::ProcessEntriesLoop,this);
    applylog_thread=std::thread(&Raft::ApplyLogLoop,this);
}

//这里是供vote成功后调用 立马发生一次appned 心跳为100000，只用让getduration的时候返回大于100000即可 这里让时间提前200000us，肯定能马上触发append
void Raft::SetBellTime()
{
    gettimeofday(&m_lastBroadcastTime, NULL);
    printf("before : %ld, %ld\n", m_lastBroadcastTime.tv_sec, m_lastBroadcastTime.tv_usec);
    if(m_lastBroadcastTime.tv_usec >= 200000){
        m_lastBroadcastTime.tv_usec -= 200000;
    }else{
        m_lastBroadcastTime.tv_sec -= 1;
        m_lastBroadcastTime.tv_usec += (1000000 - 200000);
    }
}

std::pair<int, bool> Raft::GetState()
{
    std::pair<int, bool> serverState;
    serverState.first = m_curTerm;
    serverState.second = (m_state == LEADER);
    return serverState;
}

int Raft::GetDuration(timeval last){
    struct timeval now;
    gettimeofday(&now, NULL);
    // printf("--------------------------------\n");
    // printf("now's sec : %ld, now's usec : %ld\n", now.tv_sec, now.tv_usec);
    // printf("last's sec : %ld, last's usec : %ld\n", last.tv_sec, last.tv_usec);
    // printf("%d\n", ((now.tv_sec - last.tv_sec) * 1000000 + (now.tv_usec - last.tv_usec)));
    // printf("--------------------------------\n");
    return ((now.tv_sec - last.tv_sec) * 1000000 + (now.tv_usec - last.tv_usec));
}

void Raft::RPCLoop(std::string ip,int port)
{
    std::string ip_port = ip+":"+std::to_string(port);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(ip_port, grpc::InsecureServerCredentials());

    builder.RegisterService(&rpc_server);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    server->Wait();
}

StartRet Raft::Start(Operation op)
{
    StartRet ret;
    std::lock_guard<std::mutex> lock(mtx);
    if(this->m_state != LEADER){
        ret.isLeader = false;
        return ret;
    }
    LogEntry log;
    log.m_cmd = op.getCmd();
    log.m_term = m_curTerm;
    PushBackLog(log);
    ret.m_cmdIndex = this->lastindex();
    ret.m_curTerm = m_curTerm;
    ret.isLeader = true;
    
    return ret;
}

std::vector<LogEntry> Raft::GetCmdAndTerm(std::string text)
{
    //传过来的日志格式为 cmd,term;
    std::vector<LogEntry> logs;
    int n = text.size();
    std::vector<std::string> str;
    std::string tmp = "";
    for(int i = 0; i < n; i++){
        if(text[i] != ';'){
            tmp += text[i];
        }else{
            if(tmp.size() != 0) str.push_back(tmp);
            tmp = "";
        }
    }
    for(int i = 0; i < str.size(); i++){
        tmp = "";
        int j = 0;
        for(; j < str[i].size(); j++){
            if(str[i][j] != ','){
                tmp += str[i][j];
            }else break;
        }
        std::string number(str[i].begin() + j + 1, str[i].end());
        int num = atoi(number.c_str());
        //std::cout<<"recv log cmd:"<<tmp<<std::endl;
        //std::cout<<"recv log term:"<<num<<std::endl;
        logs.push_back(LogEntry(tmp, num));
    }
    return logs;
}

// TODO 【后】日志组织格式可以更改一下
bool Raft::Deserialize(){
    std::string filename = "persister-" + std::to_string(peer_id);
    if(access(filename.c_str(), F_OK) == -1) return false;
    int fd = open(filename.c_str(), O_RDONLY);
    if(fd == -1){
        perror("open");
        return false;
    }
    int length = lseek(fd, 0, SEEK_END); //获取文件长度
    lseek(fd, 0, SEEK_SET); //重新将文件指针定位到文件开头
    char buf[length];
    bzero(buf, length);
    int len = read(fd, buf, length);
    if(len != length){
        perror("read");
        exit(-1);
    }
    std::string content(buf,len);
    std::vector<std::string> persist;
    std::string tmp = "";
    //记录的日志格式为 set key value. 前两行为cur_trem votedFor 这里是为了减少存储空间，同一任期下 一组日志用一个cur_term和votedFor记录
    for(int i = 0; i < content.size(); i++){
        if(content[i] != ';'){
            tmp += content[i];
        }else{
            if(tmp.size() != 0) 
                persist.push_back(tmp);
            tmp = "";
        }
    }
    if(tmp.size()!=0)
        persist.push_back(tmp);
    this->persister.cur_term = atoi(persist[0].c_str());
    this->persister.votedFor = atoi(persist[1].c_str());
    this->persister.snapshot.lastIncludeIndex = atoi(persist[2].c_str());
    this->persister.snapshot.lastIncludeTerm= atoi(persist[3].c_str());
    std::vector<std::string> log;
    std::vector<LogEntry> logs;
    tmp = "";
    for(int i = 4; i < persist.size(); i++){
        log.push_back(persist[i]);
    }

    for(int i = 0; i < log.size(); i++){
        tmp = "";
        int j = 0;
        for(; j < log[i].size(); j++){
            if(log[i][j] != ','){
                tmp += log[i][j];
            }else break;
        }
        std::string number(log[i].begin() + j + 1, log[i].end());
        int num = atoi(number.c_str());
        logs.push_back(LogEntry(tmp, num));
    }
    this->persister.logs = std::move(logs);
    return true;
}

//TODO 【后】 这里是每次序列化的时候都会遍历一遍 并且是重写 所以这里可以优化一下
void Raft::Serialize()
{
    //rf::InstallSnapShotRequest 
    std::string str;
    str += std::to_string(this->persister.cur_term) + ";" + std::to_string(this->persister.votedFor) + ";";
    str += std::to_string(this->m_lastIncludedIndex) + ";" + std::to_string(this->m_lastIncludeTerm) + ";";
    for(const auto& log : this->persister.logs){
        str += log.m_cmd + "," + std::to_string(log.m_term) + ";";
    }
    std::string filename = "persister-" + std::to_string(peer_id);
    int fd = open(filename.c_str(), O_WRONLY | O_CREAT |O_TRUNC, 0664);
    if(fd == -1){
        perror("open");
        exit(-1);
    }
    int len = write(fd, str.c_str(), str.size());
    close(fd);
}

bool Raft::CheckLogUptodate(int term, int index)
{
    std::lock_guard<std::mutex> lock(mtx);
    //只有当自己节点最后一条日志任期号小于等于的时候可以投票，或者任期号一样但index少一点 否则自己节点的日志要比CANDIDATE的更新，选择不投票
    int lastterm = this->lastterm();
    if(this->m_logs.size()==0){
        return true;
    }
    if(term>lastterm){
        return true;
    }
    if(term==lastterm && index >= this->lastindex()){
        return true;
    }
    return false;
}

void Raft::PushBackLog(LogEntry log)
{
    this->m_logs.emplace_back(log);
}
//TODO 【后】 1、重写日志的标准需要合理设定一下，并且为了避免无限重写（日志重写后大小不变，依然超出指定大小） 2、为了减少加锁的时间，建议给persister设置一个size的函数方便快速求大小
bool Raft::ExceedLogSize(int size)
{
    bool ret=false;
    int sum=0;
    std::lock_guard<std::mutex> lock(this->persist_mtx); //加锁是为了避免persister在更新，这边在读，然后发生了竞态
    for(int i=0;i<persister.logs.size();i++){
        sum += persister.logs[i].m_cmd.size();
    }
    //printf("[%d] [%d] current sum: %d\n",this->peer_id, this->m_state, sum);
    if(sum>=size){
        ret=true;
        printf("[%d] in Exceed the log size is %d B\n",peer_id,sum);
    }
    return ret;
}
//TODO 【无】
void Raft::RecvSnapShot(std::string snapshot, int lastIncludedIndex)
{
    
    {
        std::lock_guard<std::mutex> lck(this->mtx);
        //之前存过的快照已经包含了这次快照的最后的内容，则直接返回（为从机时，接收到了installsnapshot，数据比从机更新，但kvraft层依旧压缩日志了 为了避免快照被覆盖）
        if(lastIncludedIndex < this->m_lastIncludedIndex){
            return;
        }
        int compressLen = lastIncludedIndex - this->m_lastIncludedIndex;
        this->m_lastIncludeTerm = m_logs[CompressLogidx(lastIncludedIndex)].m_term;
        this->m_lastIncludedIndex = lastIncludedIndex;
        std::vector<LogEntry> tmpLogs;
        for(int i = compressLen;i<m_logs.size();i++){
            tmpLogs.push_back(m_logs[i]);
        }
        m_logs = tmpLogs;
    }
    {
        std::lock_guard<std::mutex> lck(persist_mtx);
        SnapShot ss(snapshot,lastIncludedIndex,this->m_lastIncludeTerm,0);
        persister.snapshot=std::move(ss);
        SaveRaftState();
        SaveSnapShot();
    }
}

//TODO 【无】
void Raft::SaveRaftState()
{
    persister.cur_term = m_curTerm;
    persister.votedFor = m_voteFor;
    persister.logs = m_logs;
    Serialize();
}
//TODO 【无】
void Raft::ReadRaftState()
{
    //只在初始化的时候调用，没必要加锁，因为run()在其之后才执行
    bool ret = this->Deserialize();
    if(!ret) return;
    this->m_curTerm = this->persister.cur_term;
    this->m_voteFor = this->persister.votedFor;

    for(const auto& log : this->persister.logs){
        PushBackLog(log);
    }
    printf(" [%d]'s term : %d, votefor : %d, logs.size() : %d\n", peer_id, m_curTerm, m_voteFor, m_logs.size());
}
//TODO 【无】 已经添加了protobuf序列化
void Raft::SaveSnapShot()
{
    std::string filename = "snapshot-" + std::to_string(peer_id);
    int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0664); //文件内容截断
    if(fd == -1){
        perror("open");
        exit(-1);
    }
    rf::SnapShot snapshot;
    snapshot.set_lastincludeindex(persister.snapshot.lastIncludeIndex);
    snapshot.set_lastincludeterm(persister.snapshot.lastIncludeTerm);
    snapshot.set_data(persister.snapshot.data);
    std::string snapshot_str = snapshot.SerializeAsString();
    int len = write(fd, snapshot_str.c_str(), snapshot_str.size());
    close(fd); 
}
//TODO 【无】
bool Raft::ReadSnapShot()
{
    std::string filename="snapshot-"+std::to_string(peer_id);
    if(access(filename.c_str(),F_OK) == -1){
        return false;
    }
    int fd = open(filename.c_str(),O_RDONLY);
    if(fd == -1){
        perror("open");
        return false;
    }
    int length = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    char buf[length];
    bzero(buf, length);
    int len = read(fd, buf, length);
    if(len != length){
        perror("read");
        exit(-1);
    }
    close(fd);
    rf::SnapShot snapshot;
    snapshot.ParseFromArray(buf, len);
    {
        std::lock_guard<std::mutex> lck(persist_mtx);
        persister.snapshot = snapshot;
    }
    return true;
}
//TODO 【无】用条件变量通知 is_init表示是否在为Raft中init函数调用
void Raft::installSnapShotTOkvServer(bool is_init)
{
    ApplyMsg msg;
    {
        if(is_init){
            bool ret = ReadSnapShot();
            if(!ret)
                return;
        }

        msg.isSnap = true;
        {
            std::lock_guard<std::mutex> lck(this->persist_mtx);
            msg.snapShot = persister.snapshot.data;
        }
        {
            std::lock_guard<std::mutex> lck(this->mtx);
            msg.lastIncludedIndex = this->m_lastIncludedIndex;
            msg.lastIncludedTerm = this->m_lastIncludeTerm;
            this->m_lastApplied = this->m_lastIncludedIndex;
        }
    }
    {
        //往m_msgs中放入msg，通过条件变量通知kvserver层来取 这里需要kvserver一次性将其读取然后处理
        std::lock_guard<std::mutex> lck(apply_mtx);
        // this->apply_cond.wait(lck,[this](){
        //     return this->wait_apply.load();
        // });
        this->m_msgs.push(msg);
        wait_apply=true;
        this->apply_cond.notify_one();
    }
}

int Raft::CompressLogidx(int index)
{
    return index-this->m_lastIncludedIndex-1;
}

int Raft::lastindex()
{
    return this->m_lastIncludedIndex+m_logs.size();
}

int Raft::lastterm()
{
    int lastterm = m_lastIncludeTerm;
    if(m_logs.size()!=0){
        lastterm = m_logs.back().m_term;
    }
    return lastterm;
}

void Raft::Stop()
{
    this->m_stop = true;
    printf("raft%d is dead\n", this->peer_id);
}
void Raft::Activate()
{
    this->m_stop = false;
    printf("raft%d is activate\n", peer_id);
}
// TODO 【后】 这里是定时心跳，因此只能while循环，或者使用条件变量设置超时时间
void Raft::ProcessEntriesLoop()
{
    while(!this->m_stop){
        usleep(1000);
        {
            std::lock_guard<std::mutex> lock(mtx);
            if(this->m_state!=LEADER){
                continue;
            }
            int during_time=this->GetDuration(this->m_lastBroadcastTime);
            //还没到心跳时间
            if(during_time<HEART_BEART_PERIOD){
                continue;
            }
            gettimeofday(&this->m_lastBroadcastTime, NULL);
        }
        for(auto server:this->m_peers){
            if(server.m_peerId==this->peer_id)
                continue;
            if(this->m_nextIndex[server.m_peerId] <= this->m_lastIncludedIndex){
                printf("【%d】 send snapshot!\n",server.m_peerId);
                thread_pool_.add_task(&Raft::SendInstallSnapshot,this,server.m_peerId);
            }
            else{
                thread_pool_.add_task(&Raft::SendAppendEntries,this,server.m_peerId);
            }
        }
    }
}

//TODO 【无】
void Raft::ElectionLoop()
{
    bool resetFlag = false;
    while(!this->m_stop){
        int timeOut = rand()%1000000 + 500000; //每次都要随机 这里单位为us 因此范围为[500ms,1500ms]
        while(1){
            usleep(1000);
            std::unique_lock<std::mutex> lock(mtx); //要用到条件变量 这里的锁得是unique_lock
            int during_time = GetDuration(m_lastWakeTime);
            //自己超时 发起选举

            if(m_state == FOLLOWER && during_time > timeOut){
                printf("[%d] timeout!\n",this->peer_id);
                m_state = CANDIDATE;
            }

            if(m_state == CANDIDATE && during_time > timeOut){
                printf(" %d attempt election at term %d, timeOut is %d,during_time is %d\n", peer_id, m_curTerm, timeOut,during_time);
                gettimeofday(&m_lastWakeTime, NULL);
                resetFlag = true;
                //m_curTerm++; //任期增加
                m_voteFor = peer_id; //自己给自己投票
                SaveRaftState();

                recvVotes = 1; 
                finishedVote = 1;
                cur_peerId = 0;

                //发起投票
                for(auto server:this->m_peers){
                    if(server.m_peerId==this->peer_id)
                        continue;
                    // ::grpc::ClientContext context;
                    // context.AddMetadata();
                    thread_pool_.add_task(&Raft::CallRequestVote,this,server.m_peerId);
                }
                //等待投票结束或者接收到过半的投票
                cond_.wait(lock,[this](){
                    return this->recvVotes>this->m_peers.size()/2||this->finishedVote==this->m_peers.size();
                });
                //投票过程中发现已经有节点当选了 转为了FOLLOWER
                if(this->m_state!=CANDIDATE){
                    continue;
                }
                //获取了过半投票 成为leader
                if(this->recvVotes > this->m_peers.size()/2){
                    this->m_state = LEADER;
                    this->m_curTerm ++;
                    for(int i=0;i<this->m_peers.size();i++){
                        this->m_nextIndex[i] = this->lastindex() + 1;
                        this->m_matchIndex[i] = 0; //目前不知道从节点匹配上的节点 全赋值为0
                    }
                    printf("[%d] become new leader at term %d \n",this->peer_id,this->m_curTerm);
                    //提前发起心跳 
                    this->SetBellTime();
                }
            }
            //重新选举过后，需要重置超时时间
            if(resetFlag){
                resetFlag = false;
                break;
            }
        }
    }
}

//TODO 【无】
void Raft::CallRequestVote(int clientPeerId)
{
    //std::cout<<"into callRequestvote"<<std::endl;
    rf::RequestVote request;
    rf::ResponseVote response;
    ::grpc::ClientContext context;
    {
        std::lock_guard<std::mutex> lock(mtx);
        request.set_candidateid(this->peer_id);
        request.set_lastlogindex(this->lastindex());
        request.set_lastlogterm(this->lastterm());
        request.set_term(this->m_curTerm + 1); //在当前任期上加一 而不是先++，否则选举失败会无限加加，节点重新上线的时候，会因为暂时联系不上而无限重新选举
        // int clientPeerId=this->cur_peerId;
    }
    stub_ptrs_[clientPeerId]->Vote(&context,request,&response);
    printf("recv client[%d] vote at term %d\n",clientPeerId,response.term());
    //完成投票，条件变量通知
    {
        std::lock_guard<std::mutex> lock(mtx);
        this->finishedVote++;
        cond_.notify_one();
        std::cout<<"vote finished "<<this->finishedVote<<std::endl;
        //如果回复的任期更大 则自己成为follower
        if(response.term()>this->m_curTerm + 1){
            printf("change state when call vote clientid[%d] at term[%d]\n",clientPeerId,response.term());
            this->m_state = FOLLOWER;
            this->m_curTerm=response.term();
            gettimeofday(&m_lastWakeTime, NULL);
            this->m_voteFor=-1;
            // cond_.notify_one();
            // //this->ReadRaftState();
            // return;
        }
        if(response.isaccepted()){
            this->recvVotes++;
        }
        //cond_.notify_one();
    }
}

rf::ResponseVote Raft::ReplyVote(const rf::RequestVote* request)
{
    rf::ResponseVote response;
    response.set_isaccepted(false);
    std::unique_lock<std::mutex> lock(mtx); //上锁的目的是为了互斥操作m_curTerm
    response.set_term(this->m_curTerm);
    //int candidateid = request->candidateid();
    // if(dead[candidateid]){
    //     std::string ip_port=m_peers[candidateid].ip+":"+std::to_string(m_peers[candidateid].port);
    //     stub_ptrs_[candidateid]=std::move(rf::RaftServerRpc::NewStub(stub_channels_[candidateid]));
    //     dead[candidateid] = 0;
    // }
    //如果当前任期比CANDIDATE的任期大 则直接拒绝
    if(this->m_curTerm>request->term()){
        return response;
    }
    //如果当前任期更小 如果自己是LEADER的话 则变为FOLLOWER 而如果是FOLLOWER则依旧保持身份
    if(this->m_curTerm<request->term()){
        printf("state change at RaftServer.cpp:584 request id[%d]\n",request->candidateid());
        this->m_state=FOLLOWER;
        this->m_curTerm=request->term(); //TODO 【后】还未投票成功就直接转变term的合法性 还需要思考一下
        this->m_voteFor = -1;
    }
    //还有种情况 是当前节点也发起了投票 新任期与另一个发起投票节点的任期一样 此时因为各自发起了投票 那么m_voteFor就是投给的自己 那么也就是直接拒绝了
    //如果没有投票或者已经投过了(?) 那么需要检查当前日志是不是比他更新 如果更新则选择不投票
    if(this->m_voteFor==-1||this->m_voteFor==request->candidateid()){
        lock.unlock();
        printf("request lastlogterm (%d) lastlogindex (%d)\n",request->lastlogterm(), request->lastlogindex());
        bool ret=CheckLogUptodate(request->lastlogterm(), request->lastlogindex());
        if(!ret)
            return response;
        response.set_isaccepted(true);
        lock.lock();
        this->m_voteFor = request->candidateid();
        printf("[%d] vote to [%d] at %d, duration is %d\n", peer_id, request->candidateid(), m_curTerm, GetDuration(m_lastWakeTime));
        gettimeofday(&m_lastWakeTime, NULL);
    }
    SaveRaftState();
    return response;
}

//TODO 【后】
rf::AppendEntriesResponse Raft::ReplyAppend(const rf::AppendEntriesRequest *request)
{
    //std::cout<<"FOLLOWER recv append logs:"<<request->m_sendlogs()<<std::endl;
    std::vector<LogEntry> recv_logs=GetCmdAndTerm(request->m_sendlogs());
    // std::cout<<"recv logs "<<recv_logs.size()<<std::endl;
    // std::cout<<"request prevlogindex "<<request->m_prevlogindex()<<std::endl;
    // std::cout<<"request prevlogterm "<<request->m_prevlogterm()<<std::endl;
    rf::AppendEntriesResponse response;
    std::lock_guard<std::mutex> lck(mtx);
    response.set_m_term(m_curTerm);
    response.set_m_success(false);
    response.set_m_conflict_index(-1);
    response.set_m_conflict_term(-1);
    
    //如果当前节点term大于请求中的term 表示当前已经选举出了新的LEADER，则将当前任期返回
    if(request->m_term()<m_curTerm){
        std::cout<<m_curTerm<<" bigger than "<<request->m_term()<<std::endl;
        return response;
    }
    else{
        //请求中的任期大于当前任期 则自己作为FOLLOWER 重置m_voteFor 
        if(request->m_term() > this->m_curTerm){
            this->m_voteFor = -1;
            SaveRaftState();
        }
        this->m_curTerm = request->m_term();
        this->m_state = FOLLOWER;
    }
    printf("[%d] recv append from [%d] at self term%d, send term %d, duration is %d\n",
            peer_id, request->m_leaderid(), m_curTerm, request->m_term(), GetDuration(m_lastWakeTime));
    gettimeofday(&m_lastWakeTime, NULL);

    int log_size=0;
    //当前节点存储日志为空 则全部复制
    std::cout<<"FOLLOWER "<<peer_id<<" log size:"<<this->m_logs.size()<<std::endl;
    //如果request的前一个log index比当前快照的最后一个还小 则说明快照有问题，让主服务器将所有日志的全部重发
    if(request->m_prevlogindex() < this->m_lastIncludedIndex){
        response.set_m_conflict_index(1);
        return response;
    }
    else if(request->m_prevlogindex() == this->m_lastIncludedIndex){
        //如果master发送过来的前一个日志的index刚好就是当前从机快照的最后一条日志 检查term是不是对的，如果不对则要将快照全量更新
        if(request->m_prevlogterm() != this->m_lastIncludeTerm){
            response.set_m_conflict_index(1);
            return response;
        }
    }
    else{//TODO 【问】为什么conflict_index要为lastindex+1 这里的情况为从机日志落后太多
        if(this->lastindex() < request->m_prevlogindex()){
            //如果当前从机的最后一条日志小于主机发送过来的日志index的前一条日志 说明差了几个日志，需要让主机发送lastindex()之后的日志
            response.set_m_conflict_index(this->lastindex() + 1);
            response.set_m_success(false);
            return response;
        }
        //下面的情况为当前从机的最后一条日志起码比prevlogindex大，判断对应index的是否冲突
        //冲突则需要发送冲突term的第一条日志 master接收到后先找到冲突term的最后一条日志，然后将这条日志以后的发送过来，
        //而如果没有冲突term，则找到刚好小于冲突term的最后一条日志, 然后将这条日志以后的发送过来
        //首先如果这次prevlogterm一样，那么日志肯定是相同的，而还是不一样，那么再继续往前找
        if(this->m_logs[this->CompressLogidx(request->m_prevlogindex())].m_term != request->m_prevlogterm()){
            response.set_m_conflict_term(this->m_logs[this->CompressLogidx(request->m_prevlogindex())].m_term);
            for(int index = this->m_lastIncludedIndex+1; index <= request->m_prevlogindex(); index++){
                if(this->m_logs[this->CompressLogidx(index)].m_term == response.m_conflict_term()){
                    response.set_m_conflict_index(index);
                    break;
                }
            }
            response.set_m_success(false);
            return response;
        }
    }
    //到这里那么就表示一样的 进行回退和复制
    int logsize = this->lastindex();
    //如果有多的日志那直接回退
    for(int i = request->m_prevlogindex(); i<logsize ;i++){
        this->m_logs.pop_back();
    }
    for(const auto& log: recv_logs){
        PushBackLog(log);
    }
    this->SaveRaftState();
    //如果leader已经提交的index已经比当前从机提交的index更大了 更新当前从机的commitindex
    int leadercommit = request->m_leadercommit();
    if(this->m_commitIndex < leadercommit){
        this->m_commitIndex = std::min(leadercommit,this->lastindex());
    }
    response.set_m_success(true);
    return response;

    // if(this->m_logs.size()==0){
    //     for(const auto& log:recv_logs){
    //         PushBackLog(log);
    //     }
    //     SaveRaftState();
    //     log_size=this->m_logs.size();
    //     //如果当前节点提交到的index是小于LEADER提交的index 则先提交到接近LEADER提交的index
    //     if(this->m_commitIndex < request->m_leadercommit()){
    //         this->m_commitIndex = std::min(request->m_leadercommit(),log_size);
    //     }
    //     response.set_m_success(true);
    //     return response;
    // }
    // //如果当前日志都没有request的前继日志index 则要返回冲突term index （这里只设置了conflict_index，conflict_term为-1 这是因为根本没有对应的index和term 只能这样返回 LEADER再根据这个conflict_index来发送）
    // if(this->m_logs.size() < request->m_prevlogindex()){
    //     std::cout<<"RaftServer.cpp:473 "<<response.m_conflict_term()<<std::endl;
    //     response.set_m_conflict_index(this->m_logs.size());
    //     response.set_m_success(false);
    //     return response;
    // }
    // //存在prevlogoindex 并且对应的任期不一样 需要返回冲突的term index
    // if(request->m_prevlogindex()>0 && this->m_logs[request->m_prevlogindex()-1].m_term!=request->m_prevlogterm()){
    //     std::cout<<"FOLLOWER: conflict term"<<this->m_logs[request->m_prevlogindex()-1].m_term<<std::endl;
    //     response.set_m_conflict_term(this->m_logs[request->m_prevlogindex()-1].m_term);
    //     //找到冲突的第一个index
    //     for(int index = 1;index <=request->m_prevlogindex();index++){
    //         if(this->m_logs[index].m_term==response.m_conflict_term()){
    //             response.set_m_conflict_index(index);
    //             break;
    //         }
    //     }
    //     response.set_m_success(false); //因为有冲突 没有append成功 还需要重新发送
    //     return response;
    // }

    // //如果任期一样 并且当前节点的m_logs.size()是大于LEADER的prevlogindex的 那么就需要回退
    // log_size = this->m_logs.size();
    // for(int i=request->m_prevlogindex();i<log_size;i++){
    //     this->m_logs.pop_back();
    // }
    // //将接收到的log全部存到当前内存中
    // for(const auto& log:recv_logs){
    //     PushBackLog(log);
    // }

    // SaveRaftState();
    // log_size = this->m_logs.size();
    // //更新commitindex
    // if(this->m_commitIndex < request->m_leadercommit()){
    //     this->m_commitIndex = std::min(request->m_leadercommit(), log_size);
    // }
    // for(auto a : m_logs) printf("%d ", a.m_term);
    // printf(" [%d] sync success\n", peer_id);
    // response.set_m_success(true);
    // return response;
}
//TODO 【问】如果当前从机快照有问题则是全部删除
rf::InstallSnapShotResponse Raft::ReplyInstallSnap(const rf::InstallSnapShotRequest* request)
{
    rf::InstallSnapShotResponse response;
    printf("recv install snapshot [%d],[%d]\n",request->lastincludeindex(),request->lastincludeterm());
    {
        std::lock_guard<std::mutex> lock(this->mtx);
        response.set_term(this->m_curTerm);
        if(request->term() < this->m_curTerm){
            return response;
        }
        else{
            if(request->term() > this->m_curTerm){
                this->m_voteFor = -1;
                SaveRaftState();
            }
            this->m_curTerm = request->term();
            this->m_state = FOLLOWER;
        }
        gettimeofday(&this->m_lastWakeTime, NULL);
        //如果发送过来的快照最后一个index是小于等于当前从机快照的index的
        if(request->lastincludeindex() <= this->m_lastIncludedIndex){
            return response;
        }
        else{
            //如果快照中的日志内容是当前从机的子集 
            if(request->lastincludeindex() < this->lastindex()){
                //如果当前从机中对应主机快照的最后一条日志的term与主机的term不一致 则直接清楚当前日志
                if(this->m_logs[this->CompressLogidx(this->lastindex())].m_term != request->lastincludeterm()){
                    this->m_logs.clear();
                }
                else{
                    //否则，将快照之前的日志从m_logs中删除
                    std::vector<LogEntry> tmpLog(this->m_logs.begin() + this->CompressLogidx(request->lastincludeindex()) + 1, m_logs.end());
                    this->m_logs = tmpLog;
                }
            }
            else{
                this->m_logs.clear();
            }
        }
        this->m_lastIncludedIndex = request->lastincludeindex();
        this->m_lastIncludeTerm = request->lastincludeterm();
    }
    {
        std::lock_guard<std::mutex> lock(this->persist_mtx);
        this->persister.snapshot.data = request->data(0);
        
        this->SaveRaftState();
        this->SaveSnapShot();
    }
    this->installSnapShotTOkvServer(false);
    return response;

}

//TODO 【后】解决下一个TODO的问题
void Raft::SendAppendEntries(int clientPeerId)
{
    rf::AppendEntriesRequest request;
    rf::AppendEntriesResponse response;
    int send_size = 0;
    //为防止因为stub没有连接上对应的client 这里设置一下default值
    // response.set_m_conflict_index(-1);
    // response.set_m_conflict_term(-1);
    //std::cout<<"default AppendEntriesResponse conflict index:"<<response.m_conflict_index()<<std::endl;
    ::grpc::ClientContext context;
    {
        std::lock_guard<std::mutex> lock(mtx);
    
        request.set_m_term(this->m_curTerm);
        request.set_m_leaderid(this->peer_id);
        request.set_m_prevlogindex(this->m_nextIndex[clientPeerId]-1);
        request.set_m_leadercommit(this->m_commitIndex);
        //将未同步的log全部发送 而如果是刚vote结束，这里发起append，则会因为把nextIndex全部都赋值为m_logs.size() + 1 这里就发送的空包
        std::string send_logs;
        std::cout<<"Leader term:"<<request.m_term()<<std::endl;
        std::cout<<"Leader log size:"<<this->m_logs.size()<<std::endl;
        std::cout<<"request prevlogindex :"<<request.m_prevlogindex()<<" "<<this->m_nextIndex[clientPeerId]-1<<std::endl;
        for(int i=this->CompressLogidx(request.m_prevlogindex()) + 1; i < this->m_logs.size(); i++){
            send_logs+=(this->m_logs[i].m_cmd + "," + std::to_string(this->m_logs[i].m_term) + ";");
            send_size++;
        }
        std::cout<<"Leader send log size:"<<send_size<<std::endl;
        request.set_m_sendlogs(send_logs);

        //如果前一个日志的index等于快照的最后一条日志的index 那直接前继term就为lastincludedterm 这两个初始值本来也是0，为空的时候也是一样的逻辑
        if(request.m_prevlogindex() == this->m_lastIncludedIndex){
            // request.set_m_prevlogterm(0);
            // //如果有log，则设置之前log的任期，这里设置为第一个log的term ？？？？不太懂这里为啥要这样设计
            // if(this->m_logs.size()>0){
            //     request.set_m_prevlogterm(m_logs[0].m_term);
            // }
            request.set_m_prevlogterm(this->m_lastIncludeTerm);
        }
        else{
            request.set_m_prevlogterm(this->m_logs[this->CompressLogidx(request.m_prevlogindex())].m_term);
        }
        printf("[%d] -> [%d]'s prevLogIndex : %d, prevLogTerm : %d\n", this->peer_id, clientPeerId, request.m_prevlogindex(), request.m_prevlogterm());
    }
    grpc::Status stub_status=stub_ptrs_[clientPeerId]->AppendEntries(&context,request,&response);

    if(!stub_status.ok()){
        int retry_times = 3;
        // std::string ip_port=m_peers[clientPeerId].ip+":"+std::to_string(m_peers[clientPeerId].port);
        // auto channel = grpc::CreateChannel(ip_port, grpc::InsecureChannelCredentials());
        // std::unique_ptr<rf::RaftServerRpc::Stub> tmp = rf::RaftServerRpc::NewStub(channel);
        while(!stub_status.ok()&&retry_times--){
            printf("[%d] retry \n",clientPeerId);
            ::grpc::ClientContext context;
            context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(5)); 
            stub_ptrs_[clientPeerId]=std::move(rf::RaftServerRpc::NewStub(stub_channels_[clientPeerId]));
            stub_status=stub_ptrs_[clientPeerId]->AppendEntries(&context,request,&response);
            //stub_status=tmp->AppendEntries(&context,request,&response);
            
        }
        if(!stub_status.ok()){
            printf("client peerid[%d] fail to connect!\n",clientPeerId);
            dead[clientPeerId] = 1;
            return;
        }
    }
    {
        printf("stub[%d] send ok\n",clientPeerId);
        std::cout<<"conflict index:"<<response.m_conflict_index()<<std::endl;
        std::cout<<"conflict term:"<<response.m_conflict_term()<<std::endl;
        std::cout<<"response success :"<<response.m_success()<<std::endl;
        std::cout<<"response term: "<<response.m_term()<<std::endl;
        std::lock_guard<std::mutex> lock(mtx);
        //如果在append的过程中，发生了脑裂，之后网络恢复，另外的主机发起了投票，成为了新的主机，导致当前主机的term变化
        if(this->m_curTerm != request.m_term()){
            return;
        }
        //这里是发生网络分区后 恢复，如果另一个分区中选出了新的leader 这时候当前leader发送append的时候会被返回最新的term 发现自己的任期小则说明已经选出了新的leader
        //这里也存在问题，考虑到response的节点是才上线 然后发起了投票，接收到leader回复后更新了自己的term，然后又超时了，更新term++，然后导致当前leader转为follower
        if(response.m_term() > this->m_curTerm){
            printf("change state because of [%d] response term [%d]\n",clientPeerId,response.m_term());
            this->m_state = FOLLOWER;
            this->m_curTerm=response.m_term();
            this->m_voteFor=-1;
            this->SaveRaftState();
            return;
        }
        //发送成功后 更新nextIndex加上已发送的log的长度 matchIndex
        if(response.m_success()){
            //this->m_nextIndex[clientPeerId] += this->GetCmdAndTerm(request.m_sendlogs()).size();
            this->m_nextIndex[clientPeerId] += send_size; //
            this->m_matchIndex[clientPeerId] = this->m_nextIndex[clientPeerId]-1;
            this->m_matchIndex[this->peer_id] = this->lastindex();//自己节点也要算进去，因此记录一下当前节点的最后一条日志索引
            
            std::vector<int> tmp=this->m_matchIndex;
            sort(tmp.begin(),tmp.end());
            //如果超过一半节点已经缓存的index是大于当前commitindex，并且该index的任期还是当前任期则commit 更新commitindex
            int realMajorityMatchIndex = tmp[tmp.size() / 2];
            //第一个条件只有等到将日志发送到了一半以上的从机后从会成立 //TODO【问】 第二个条件暂时没有理解为什么小于等于最后一次快照的lastindex，就更新commitindex
            if(realMajorityMatchIndex > this->m_commitIndex && (realMajorityMatchIndex <= this->m_lastIncludedIndex||this->m_logs[this->CompressLogidx(realMajorityMatchIndex)].m_term == this->m_curTerm)){
                this->m_commitIndex = realMajorityMatchIndex;
            }
        }
        else{//不成功的话 会返回冲突的term
            if(response.m_conflict_term()!=-1){
                int conflict_index= -1;
                //向前找到回复的冲突的term 这里是找到冲突任期的最后一条日志 这里就是尝试 如果下次返回来还是冲突的话 m_conflict_term就是-1 
                for(int index = request.m_prevlogindex(); index > this->m_lastIncludedIndex; index--){
                    if(this->m_logs[this->CompressLogidx(index)].m_term <= response.m_conflict_term()){
                        conflict_index=index;
                        break;
                    }
                }
                //能找到冲突的term 先设置为冲突任期最后一条日志的下一条
                if(conflict_index!=-1){
                    this->m_nextIndex[clientPeerId] = conflict_index + 1;
                }
                else{//如果没有找到 则用回复的conflict_index
                    this->m_nextIndex[clientPeerId] = response.m_conflict_index();
                }
            }
            else{//如果FOLLOWER的log中没有perv_index，FOLLOWER会返回他log最后一条的index
                this->m_nextIndex[clientPeerId] = response.m_conflict_index() + 1;
            }
        }
        this->SaveRaftState();
    }
}
//TODO 【后】实现发起installsnapshot的rpc调用 
void Raft::SendInstallSnapshot(int clientPeerId)
{
    rf::InstallSnapShotRequest request;
    rf::InstallSnapShotResponse response;
    ::grpc::ClientContext context;
    {
        std::lock_guard<std::mutex> lock(this->mtx);
        request.set_lastincludeindex(this->m_lastIncludedIndex);
        request.set_lastincludeterm(this->m_lastIncludeTerm);
        request.set_leaderid(this->peer_id);
        request.set_term(this->m_curTerm);
        this->ReadSnapShot();
        request.add_data(this->persister.snapshot.data);
    }
    grpc::Status stub_status = stub_ptrs_[clientPeerId]->InstallSnapShot(&context,request,&response);
    {
        std::lock_guard<std::mutex> lock(this->mtx);
        if(this->m_curTerm != response.term()){
            return;
        }
        if(this->m_curTerm < response.term()){
            this->m_state = FOLLOWER;
            this->m_curTerm=response.term();
            this->m_voteFor=-1;
            this->SaveRaftState();
            return;
        }
        this->m_nextIndex[clientPeerId] = this->lastindex() + 1;
        this->m_matchIndex[clientPeerId] = request.lastincludeindex();
        this->m_matchIndex[this->peer_id] = this->lastindex();
        std::vector<int> tmpIndex = this->m_matchIndex;
        sort(tmpIndex.begin(), tmpIndex.end());
        int realMajorityMatchIndex = tmpIndex[tmpIndex.size() / 2];
        if(realMajorityMatchIndex > this->m_commitIndex && (realMajorityMatchIndex <= this->m_lastIncludedIndex || this->m_logs[this->CompressLogidx(realMajorityMatchIndex)].m_term == this->m_curTerm)){
            this->m_commitIndex = realMajorityMatchIndex;
        }
    }
}
/*TODO【无】
主要逻辑就是遍历m_lastApplied到m_commitindex的日志 保存到Raft.m_msgs数组上，条件变量通知kvRaft层读取保存到database中
*/
void Raft::ApplyLogLoop()
{
    while(1){
        while(!this->m_stop){
            usleep(10000);
            std::vector<ApplyMsg> msgs;
            {
                //将上次apply到已经commit的日志取出放到msgs数组里
                std::lock_guard<std::mutex> lock(this->mtx);
                while(this->m_lastApplied < this->m_commitIndex){
                    this->m_lastApplied ++;
                    int appliedIdx = this->CompressLogidx(this->m_lastApplied);
                    ApplyMsg msg;
                    msg.cmd = this->m_logs[appliedIdx].m_cmd;
                    msg.isSnap = false;
                    msg.m_cmdTerm = this->m_logs[appliedIdx].m_term;
                    msg.m_cmdIndex = this->m_lastApplied;
                    msgs.push_back(msg);
                }
            }
            {
                //将msgs中要提交的日志放到m_msgs队列中，唤醒kvserver中的线程进行读取
                std::lock_guard<std::mutex> lock(this->apply_mtx);
                for(int i = 0; i<msgs.size(); i++){
                    this->m_msgs.push(msgs[i]);
                }
            }
            this->apply_cond.notify_one();
        }
        usleep(10000);
    }
}

Operation ApplyMsg::GetOperation()
{
    Operation operation;
    std::vector<std::string> str;
    std::string tmp;
    for(int i = 0; i < this->cmd.size(); i++){
        if(cmd[i] != ' '){
            tmp += cmd[i];
        }else{
            if(tmp.size() != 0) str.push_back(tmp);
            tmp = "";
        }
    }
    if(tmp.size() != 0){
        str.push_back(tmp);
    }
    operation.op = str[0];
    operation.key = str[1];
    if(operation.op == "get"){
        operation.clientId = atoi(str[2].c_str());
        operation.requestId = atoi(str[3].c_str());
    }
    else{
        operation.value = str[2];
        operation.clientId = atoi(str[3].c_str());
        operation.requestId = atoi(str[4].c_str());
        operation.args = str[5];
    }
    operation.op_term = this->m_cmdTerm;
    operation.op_index = this->m_cmdIndex;
    return operation;
}
