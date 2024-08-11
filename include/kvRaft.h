#include "RaftServer.h"
#include "kvserver.grpc.pb.h"
#include "kvserver.pb.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <mytcp/TcpServer.h>
#include <mylog/Logger.h>
/*TODO 
主要：简单的kv数据库
1、包含进行快照化（遍历数据库保存每个key的value）然后发送到raft层进行快照存储
2、读取raft层接收到的快照并根据快照恢复
3、基于TcpServer来实现服务器功能，Raft作为类成员变量
4、实现put和get的逻辑，put的时候需要将日志发送到raft层（raft.start函数），get则是直接返回客户端对应的value
*/
class KVServer;
//存储raft所有节点的ip和port

class KVServer{
public:
    KVServer(std::vector<KVServerInfo>& kvInfo, int me, int SnapShotThresh);
    //提供put get的rpc调用 //TODO 【无】可以考虑换成epoll监听
    //void RPCLoop(std::string ip,int port);
    //loop线程监听Raft层上传的日志
    void ApplyLoop();
    //检查日志是否过期的loop线程
    void CheckSnapShotLoop();
    //初始化
    void Init();
    void HandleMessage(spConnection conn, std::string& message);
    //解析请求
    bool ParseRequest(std::string message,Operation& op);
    // kvrf::GetResponse Get(kvrf::GetRequest);
    // kvrf::PutResponse Put(kvrf::PutRequest);

    //从kvinfo里获取Raft节点所有ip和port
    std::vector<PeersInfo> getRaftPort(std::vector<KVServerInfo>& kvInfo);
    //将KvServer的数据库信息转为shotsnap
    std::string getSnapShot();
    //从raft层获取snapshot后恢复数据
    void RecoveryFromSnapShot(std::string snapshot);
    //TODO test函数
    //put操作
    kvrf::Response PutImpl(Operation& op);
    //get操作 返回的value存到op的value中
    kvrf::Response GetImpl(Operation& op);
    //查看raft层的role
    bool IsLeader();
    //启动server循环
    void start();
    //-----test 函数-----
    std::string test(std::string key);
    //关闭raft层
    void killraft();
    //重新激活raft层
    void activateraft();
private:

    int m_port; //当前KVraft的port
    std::string m_ip; //当前ip
    Raft m_raft; //Raft层
    TcpServer server;
    std::mutex ctx_mtx;
    std::mutex db_mtx;
    std::mutex seq_mtx;
    std::mutex apidx_mtx;
    std::condition_variable op_cond; //用于通知get和put操作有操作已经同步完成，可以返回结果给客户端
    int m_id;
    int m_snapshotthresh;  //超过这个大小就快照
    int m_lastAppliedIndex; //最后提交的日志index
    std::vector<PeersInfo> peers;
    //std::thread rpc_server;
    std::thread apply_loop;
    std::thread snapshot_loop;
    std::thread server_loop;
    std::unordered_map<std::string, std::string> m_database;  //模拟数据库
    std::unordered_map<int, int> m_clientSeqMap;    //只记录特定客户端已提交的最大请求ID
    std::unordered_map<int, OpContext*> m_requestMap;  //记录当前RPC对应的上下文

};
