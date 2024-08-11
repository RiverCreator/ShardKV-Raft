#include "shardserrpc.h"

ShardKVServerRPCImp::ShardKVServerRPCImp(){}

ShardKVServerRPCImp::~ShardKVServerRPCImp(){}

::grpc::Status ShardKVServerRPCImp::Get(::grpc::ServerContext *context, const kvrf::GetRequest *request, kvrf::GetResponse *response)
{
    auto res = getcb(request);
    *response  = res;
    return grpc::Status::OK;
}

::grpc::Status ShardKVServerRPCImp::Put(::grpc::ServerContext *context, const kvrf::PutRequest *request, kvrf::PutResponse *response)
{
    auto res = putcb(request);
    * response = res;
    return grpc::Status::OK;
}

::grpc::Status ShardKVServerRPCImp::Migrate(::grpc::ServerContext *context, const kvrf::MigrationRequest *request, kvrf::MigrationResponse *response)
{
    auto res = migratecb(request);
    * response = res;
    return grpc::Status::OK;
}

::grpc::Status ShardKVServerRPCImp::Ack(::grpc::ServerContext *context, const kvrf::AckRequest *request, kvrf::AckResponse *response)
{
    auto res = ackcb(request);
    * response = res;
    return grpc::Status::OK;
}

void ShardKVServerRPCImp::setgetcb(std::function<kvrf::GetResponse(const kvrf::GetRequest *)> fn)
{
    this->getcb = fn;
}

void ShardKVServerRPCImp::setputcb(std::function<kvrf::PutResponse(const kvrf::PutRequest *)> fn)
{
    this->putcb = fn;
}

void ShardKVServerRPCImp::setmigratecb(std::function<kvrf::MigrationResponse(const kvrf::MigrationRequest *)> fn)
{
    this->migratecb = fn;
}

void ShardKVServerRPCImp::setackcb(std::function<kvrf::AckResponse(const kvrf::AckRequest *)> fn)
{
    this->ackcb = fn;
}
