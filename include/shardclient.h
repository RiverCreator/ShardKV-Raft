#include "kvclerk.h"
#include "common.h"
#include "kvserver.grpc.pb.h"
#include "kvserver.pb.h"
#include <vector>
#include <string>
class ShardClient{
public:
    ShardClient(std::vector<MasterRPCInfo>& ser);
    std::string get(std::string key);
    void putappend(std::string op, std::string key,std::string value);
    void Join(std::unordered_map<int,std::vector<ShardInfo>>& m);
    void Leave(std::vector<int>& gids);
    void Move(int shardid,int gid);
    Config Query(int cid);
    int getCurRequestId();
    int getCurLeader(int gid);
    int getChangeLeader(int gid);
private:
    ShardClerk client;
    Config config;
    std::mutex mtx;
    std::unordered_map<int,int> gid2leader;
    int clientId;
    int requestId;
    
};