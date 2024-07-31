#include "raft.grpc.pb.h"
#include "raft.pb.h"
#include <iostream>
#include "client.h"
using namespace std;
// int main(){
//     rf::SnapShotInfo info;
//     std::string str=info.SerializeAsString();
//     cout<<str.size()<<endl;
//     return 0;
// }
int Client::Init(vector<pair<string,int>> servers)
{   
    int finished = 0;
    this->clientId = rand() % 10000 + 1;
    this->requestId = 0;
    int n = servers.size();
    this->clientfd = new int[n];
    this->leaderId = 0;
    this->connected.resize(n,0);
    for(int i=0;i<n;i++){
        clientfd[i] = socket(AF_INET, SOCK_STREAM, 0);
        //setnonblocking(clientfd[i]);
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(servers[i].second);
        server_addr.sin_addr.s_addr = inet_addr(servers[i].first.c_str());
        if(connect(clientfd[i], (struct sockaddr*)&server_addr, sizeof(server_addr))==-1){
            cout<<servers[i].second<<" connect false"<<endl;
            this->connected[i] = false;
            continue;
        }
        this->connected[i] = true;
        finished++;
    }
    this->servers_ = std::move(servers);
    return finished;
}

void Client::PutRequest(std::string key, std::string value)
{
    kvrf::Request request;
    kvrf::Response response;
    kvrf::PutRequest msg;
    kvrf::PutResponse res;
    request.set_type("set");
    msg.set_op("set");
    msg.set_key(key);
    msg.set_value(value);
    msg.set_clientid(clientId);
    msg.set_requestid(this->GetRequestId());
    int cur_leaderId = this->leaderId;
    request.set_request_msg(msg.SerializeAsString());
    std::string send_str = request.SerializeAsString();
    std::string recv_str;
    bool ret = this->SendRequest(send_str, recv_str);
    if(ret){
        printf("put success!\n");
    }
    else{
        printf("put wrong!\n");
    }
}

void Client::GetRequest(std::string key)
{
    kvrf::Request request;
    kvrf::GetRequest req;
    kvrf::Response response;
    kvrf::GetResponse res;
    request.set_type("get");
    req.set_key(key);
    req.set_clientid(this->clientId);
    req.set_requestid(this->GetRequestId());
    request.set_request_msg(req.SerializeAsString());
    std::string send_str = request.SerializeAsString();
    std::string recv_str;
    bool ret = this->SendRequest(send_str, recv_str);
    if(ret){
        res.ParseFromString(recv_str);
        if(!res.isexist()){
            printf("Key not exists!\n");
        }
        else{
            printf("get value: %s\n",res.value().c_str());
        }
    }
    else{
        printf("get wrong!\n");
    }
}
//获得与last的时间差 单位为微秒
int GetDuration(timeval last){
    struct timeval now;
    gettimeofday(&now, NULL);
    return ((now.tv_sec - last.tv_sec) * 1000000 + (now.tv_usec - last.tv_usec));
}


bool Client::SendRequest(std::string &send_str, std::string& recv_str)
{
    int cur_leaderId = this->leaderId;
    kvrf::Response response;
    struct timeval start;
    gettimeofday(&start, NULL);
    while(1){
        if(GetDuration(start) > 10 * 1000000){
            return false;
        }
        if(!this->connected[cur_leaderId]){
            //重连失败的话 更换leaderId
            if(this->reconnect(cur_leaderId)<0)
                cur_leaderId = this->GetChangeLeader();
        }
        else{
            int t = send(clientfd[cur_leaderId],send_str.c_str(),send_str.size(), 0);
            if(t<0){
                perror("send");
                close(clientfd[cur_leaderId]);
                this->connected[cur_leaderId] = false;
                cur_leaderId = this->GetChangeLeader();
            }
            else{
                //发送成功
                char buffer[1024];
                bzero(&buffer,sizeof(buffer));
                ssize_t nread = recv(clientfd[cur_leaderId],buffer,sizeof(buffer),0);
                //接受失败
                if(nread < 0){
                    perror("recv");
                    close(clientfd[cur_leaderId]);
                    this->connected[cur_leaderId] = false;
                    cur_leaderId = this->GetChangeLeader();
                }
                //接收成功
                else{
                    response.ParseFromArray(buffer,nread);
                    recv_str = response.response_msg();
                    //接收方不是leader
                    if(response.iswrongleader()){
                        cur_leaderId = this->GetChangeLeader();
                        usleep(1000);
                    }
                    else{
                        return true;
                    }
                }
            }
        }
    }
}

int Client::GetRequestId()
{
    return this->requestId++;
}

int Client::GetCurLeader()
{
    return this->leaderId;
}

int Client::GetChangeLeader()
{
    this->leaderId = (this->leaderId + 1)% servers_.size();
    return this->leaderId;
}

int Client::setnonblocking(int clientfd)
{
    int old_option=fcntl(clientfd,F_GETFL);
    if(old_option<0){
        printf("[%s:%d] fd[%d] getfl failed!",__FUNCTION__,__LINE__,clientfd);
        return -1;
    }
    int new_option=old_option|O_NONBLOCK;
    if(fcntl(clientfd,F_SETFL,new_option)<0){
        printf("[%s,%d] fd[%d] setfl failed!",__FUNCTION__,__LINE__,clientfd);
        return -1;
    }
    return old_option;
}

int Client::reconnect(int i)
{
    clientfd[i] = socket(AF_INET, SOCK_STREAM, 0);
    setnonblocking(clientfd[i]);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(servers_[i].second);
    server_addr.sin_addr.s_addr = inet_addr(servers_[i].first.c_str());
    if(connect(clientfd[i], (struct sockaddr*)&server_addr, sizeof(server_addr))==-1){
        cout<<servers_[i].second<<" connect false"<<endl;
        return -1;
    }
    this->connected[i] = true;
    return 0;
}
vector<pair<string,int>> GetPeers(int num){
    std::vector<pair<string,int>> peers(num);
    int base_port = 1024;
    for(int i = 0; i < num; i++){
        peers[i].first = "192.168.203.128";
        peers[i].second = base_port +  i * 2 + 1; //kvserver层的port
        // printf(" id : %d port1 : %d, port2 : %d\n", peers[i].m_peerId, peers[i].m_port.first, peers[i].m_port.second);
    }
    return peers;
}
int main(){
    Client cli1;
    Client cli2;
    Client cli3;
    Client cli4;
    vector<pair<string,int>> peers = GetPeers(5);
    int finished1 = cli1.Init(peers);
    int finished2 = cli2.Init(peers);
    int finished3 = cli3.Init(peers);
    int finished4 = cli4.Init(peers);
    int cur= 0;
    if(finished1 > 0){
        while(1){
            //put get
            cli1.PutRequest("abc",to_string(cur));
            cur++;
            cli1.GetRequest("abc");
            //get not exist
            cli1.GetRequest("def");
            //put 覆盖
            cli2.PutRequest("abc",to_string(cur));
            cur++;
            cli2.GetRequest("abc");
            //多客户端操作
            cli1.PutRequest("bcd",to_string(cur));
            cur++;
            cli2.PutRequest("akv",to_string(cur));
            cur++;
            cli3.PutRequest("sdaf",to_string(cur));
            cur++;
            cli4.PutRequest("qwe",to_string(cur));
            cur++;
            cli1.PutRequest("aasdf",to_string(cur));
            cur++;
            cli2.PutRequest("sdafs",to_string(cur));
            cur++;
            cli3.PutRequest("werqe",to_string(cur));
            cur++;
            cli4.PutRequest("pppp",to_string(cur));
            cur++;
            cli1.GetRequest("bcd");
            cli1.GetRequest("werqe");
            usleep(10000);
        }
    }
    else{
        printf("server wrong!\n");
    }
    return 0;
}