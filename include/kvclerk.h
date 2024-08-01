#include <mutex>
#include <vector>
#include <string>
#include "shardkv.grpc.pb.h"
#include "shardkv.pb.h"

struct MasterRPCInfo{
    std::string ip;
    std::vector<int> m_ports;
};
class ShardClerk{
public:
    void Init(std::vector<MasterRPCInfo>& info);
    shardkv::JoinResponse Join(shardkv::JoinRequest req);
    shardkv::QueryResponse Query(shardkv::QueryRequest req);
    shardkv::MoveResponse Move(shardkv::MoveRequest req);
    
    int getCurRequestId();
    int getCurLeader();
    int getChangeLeader();
private:
    std::vector<MasterRPCInfo>  ServerInfo;
    std::mutex mtx;
    int leaderId;
    int clientId;
    int requestId;
};