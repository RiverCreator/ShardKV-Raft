#include "shardkv.grpc.pb.h"
#include <iostream>
#include <functional>
class ShardKvRPCImp : public shardkv::ShardKVRpc::Service{
public:
    ShardKvRPCImp();
    virtual ~ShardKvRPCImp();
    ::grpc::Status Join(::grpc::ServerContext* context, const shardkv::JoinRequest* request, shardkv::JoinResponse* response);
    ::grpc::Status Move(::grpc::ServerContext* context, const shardkv::MoveRequest* request, shardkv::MoveResponse* response);
    ::grpc::Status Leave(::grpc::ServerContext* context, const shardkv::LeaveRequest* request, shardkv::LeaveResponse* response);
    ::grpc::Status Query(::grpc::ServerContext* context, const shardkv::QueryRequest* request, shardkv::QueryResponse* response);
    void setjoincb(std::function<shardkv::JoinResponse(const shardkv::JoinRequest*)> fn);
    void setmovecb(std::function<shardkv::MoveResponse(const shardkv::MoveRequest*)> fn);
    void setleavecb(std::function<shardkv::LeaveResponse(const shardkv::LeaveRequest*)> fn);
    void setquerycb(std::function<shardkv::QueryResponse(const shardkv::QueryRequest*)> fn);
private:
    std::function<shardkv::JoinResponse(const shardkv::JoinRequest*)> joincb;
    std::function<shardkv::MoveResponse(const shardkv::MoveRequest*)> movecb;
    std::function<shardkv::LeaveResponse(const shardkv::LeaveRequest*)> leavecb;
    std::function<shardkv::QueryResponse(const shardkv::QueryRequest*)> querycb;
};