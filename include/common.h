#include <vector>
#include <unordered_map>
#include<string>
struct ShardInfo{
    std::string ip;
    int port;
};
class Config{
public:
    Config(int NShards = 10){
        configId = 0;
        shards.resize(NShards, 0);
        groups.clear();
    }
    int configId; //表示是第几个配置
    std::vector<int> shards; //表示分片的关系
    std::unordered_map<int, std::vector<ShardInfo>> groups;  //表示gid和对应的集群中所有主机的ip和port
};
/**
 * @brief 记录操作 
 * 
 */
struct Operation{
    std::string getCmd(){
        return op + " " + key + " " + value + " "+ std::to_string(clientId) + " " + std::to_string(requestId);;
    }
    std::string op;
    std::string key;
    std::string value;
    void* args;// 记录join move leave等的参数
    int clientId; //记录是哪个客户端记录的
    int requestId; //这个client发送的第几条请求
    int op_term; //操作写入raft层时的term
    int op_index; //操作写入raft层时的index
};