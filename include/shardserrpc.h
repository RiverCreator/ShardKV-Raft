#include <functional>
#include <kvserver.grpc.pb.h>

class ShardKVServerRPCImp :public kvrf::KvRaftServiceRpc::Service{
public:
    ShardKVServerRPCImp();
    virtual ~ShardKVServerRPCImp();
    ::grpc::Status Get(::grpc::ServerContext* context,const kvrf::GetRequest* request, kvrf::GetResponse* response);
    ::grpc::Status Put(::grpc::ServerContext* context,const kvrf::PutRequest* request, kvrf::PutResponse* response);
    ::grpc::Status Migrate(::grpc::ServerContext* context,const kvrf::MigrationRequest* request, kvrf::MigrationResponse* response);
    ::grpc::Status Ack(::grpc::ServerContext* context,const kvrf::AckRequest* request, kvrf::AckResponse* response);
    void setgetcb(std::function<kvrf::GetResponse(const kvrf::GetRequest*)> fn);
    void setputcb(std::function<kvrf::PutResponse(const kvrf::PutRequest*)> fn);
    void setmigratecb(std::function<kvrf::MigrationResponse(const kvrf::MigrationRequest*)> fn);
    void setackcb(std::function<kvrf::AckResponse(const kvrf::AckRequest*)> fn);
private:
    std::function<kvrf::GetResponse(const kvrf::GetRequest*)> getcb;
    std::function<kvrf::PutResponse(const kvrf::PutRequest*)> putcb;
    std::function<kvrf::MigrationResponse(const kvrf::MigrationRequest*)> migratecb;
    std::function<kvrf::AckResponse(const kvrf::AckRequest*)> ackcb;
};