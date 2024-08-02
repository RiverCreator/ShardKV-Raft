#include "kvrpc.h"

::grpc::Status ShardKvRPCImp::Join(::grpc::ServerContext *context, const shardkv::JoinRequest *request, shardkv::JoinResponse *response)
{
    auto res = joincb(request);
    *response = res;
    return grpc::Status::OK;
}

::grpc::Status ShardKvRPCImp::Move(::grpc::ServerContext *context, const shardkv::MoveRequest *request, shardkv::MoveResponse *response)
{
    auto res = movecb(request);
    *response = res;
    return grpc::Status::OK;
}

::grpc::Status ShardKvRPCImp::Leave(::grpc::ServerContext *context, const shardkv::LeaveRequest *request, shardkv::LeaveResponse *response)
{
    auto res = leavecb(request);
    *response = res;
    return grpc::Status::OK;
}

::grpc::Status ShardKvRPCImp::Query(::grpc::ServerContext *context, const shardkv::QueryRequest *request, shardkv::QueryResponse *response)
{
    auto res =querycb(request);
    *response = res;
    return grpc::Status::OK;
}

void ShardKvRPCImp::setjoincb(std::function<shardkv::JoinResponse(const shardkv::JoinRequest *)> fn)
{
    joincb = fn;
}

void ShardKvRPCImp::setmovecb(std::function<shardkv::MoveResponse(const shardkv::MoveRequest *)> fn)
{
    movecb = fn;
}

void ShardKvRPCImp::setleavecb(std::function<shardkv::LeaveResponse(const shardkv::LeaveRequest *)> fn)
{
    leavecb = fn;
}

void ShardKvRPCImp::setquerycb(std::function<shardkv::QueryResponse(const shardkv::QueryRequest *)> fn)
{
    querycb = fn;
}
