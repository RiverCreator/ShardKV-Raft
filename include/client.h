#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <random>
#include <chrono>
#include <fcntl.h>
#include "kvserver.grpc.pb.h"
#include "kvserver.pb.h"
using namespace std;
class Client{
public:
    int Init(std::vector<pair<std::string, int>> servers);
    void PutRequest(std::string key, std::string value);
    void GetRequest(std::string key);
    bool SendRequest(std::string& msg, std::string& recv_str);
    int GetRequestId();
    int GetCurLeader();
    int GetChangeLeader();
    int setnonblocking(int clientfd);
    int reconnect(int idx); //重新连接第idx个服务器
private:
    int* clientfd; //客户端socket
    int requestId; //当前客户端请求的id 递增
    int clientId; //客户端id
    int leaderId; //当前客户端认为的leaderid
    std::vector<std::pair<std::string, int>> servers_;
    std::vector<bool> connected;
};