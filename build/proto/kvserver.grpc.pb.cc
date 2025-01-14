// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: kvserver.proto

#include "kvserver.pb.h"
#include "kvserver.grpc.pb.h"

#include <functional>
#include <grpcpp/impl/codegen/async_stream.h>
#include <grpcpp/impl/codegen/async_unary_call.h>
#include <grpcpp/impl/codegen/channel_interface.h>
#include <grpcpp/impl/codegen/client_unary_call.h>
#include <grpcpp/impl/codegen/client_callback.h>
#include <grpcpp/impl/codegen/message_allocator.h>
#include <grpcpp/impl/codegen/method_handler.h>
#include <grpcpp/impl/codegen/rpc_service_method.h>
#include <grpcpp/impl/codegen/server_callback.h>
#include <grpcpp/impl/codegen/server_callback_handlers.h>
#include <grpcpp/impl/codegen/server_context.h>
#include <grpcpp/impl/codegen/service_type.h>
#include <grpcpp/impl/codegen/sync_stream.h>
namespace kvrf {

static const char* KvRaftServiceRpc_method_names[] = {
  "/kvrf.KvRaftServiceRpc/Put",
  "/kvrf.KvRaftServiceRpc/Get",
};

std::unique_ptr< KvRaftServiceRpc::Stub> KvRaftServiceRpc::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< KvRaftServiceRpc::Stub> stub(new KvRaftServiceRpc::Stub(channel));
  return stub;
}

KvRaftServiceRpc::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_Put_(KvRaftServiceRpc_method_names[0], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_Get_(KvRaftServiceRpc_method_names[1], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status KvRaftServiceRpc::Stub::Put(::grpc::ClientContext* context, const ::kvrf::PutRequest& request, ::kvrf::PutResponse* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_Put_, context, request, response);
}

void KvRaftServiceRpc::Stub::experimental_async::Put(::grpc::ClientContext* context, const ::kvrf::PutRequest* request, ::kvrf::PutResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Put_, context, request, response, std::move(f));
}

void KvRaftServiceRpc::Stub::experimental_async::Put(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::kvrf::PutResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Put_, context, request, response, std::move(f));
}

void KvRaftServiceRpc::Stub::experimental_async::Put(::grpc::ClientContext* context, const ::kvrf::PutRequest* request, ::kvrf::PutResponse* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_Put_, context, request, response, reactor);
}

void KvRaftServiceRpc::Stub::experimental_async::Put(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::kvrf::PutResponse* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_Put_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::kvrf::PutResponse>* KvRaftServiceRpc::Stub::AsyncPutRaw(::grpc::ClientContext* context, const ::kvrf::PutRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::kvrf::PutResponse>::Create(channel_.get(), cq, rpcmethod_Put_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::kvrf::PutResponse>* KvRaftServiceRpc::Stub::PrepareAsyncPutRaw(::grpc::ClientContext* context, const ::kvrf::PutRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::kvrf::PutResponse>::Create(channel_.get(), cq, rpcmethod_Put_, context, request, false);
}

::grpc::Status KvRaftServiceRpc::Stub::Get(::grpc::ClientContext* context, const ::kvrf::GetRequest& request, ::kvrf::GetResponse* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_Get_, context, request, response);
}

void KvRaftServiceRpc::Stub::experimental_async::Get(::grpc::ClientContext* context, const ::kvrf::GetRequest* request, ::kvrf::GetResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Get_, context, request, response, std::move(f));
}

void KvRaftServiceRpc::Stub::experimental_async::Get(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::kvrf::GetResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Get_, context, request, response, std::move(f));
}

void KvRaftServiceRpc::Stub::experimental_async::Get(::grpc::ClientContext* context, const ::kvrf::GetRequest* request, ::kvrf::GetResponse* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_Get_, context, request, response, reactor);
}

void KvRaftServiceRpc::Stub::experimental_async::Get(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::kvrf::GetResponse* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_Get_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::kvrf::GetResponse>* KvRaftServiceRpc::Stub::AsyncGetRaw(::grpc::ClientContext* context, const ::kvrf::GetRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::kvrf::GetResponse>::Create(channel_.get(), cq, rpcmethod_Get_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::kvrf::GetResponse>* KvRaftServiceRpc::Stub::PrepareAsyncGetRaw(::grpc::ClientContext* context, const ::kvrf::GetRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::kvrf::GetResponse>::Create(channel_.get(), cq, rpcmethod_Get_, context, request, false);
}

KvRaftServiceRpc::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      KvRaftServiceRpc_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< KvRaftServiceRpc::Service, ::kvrf::PutRequest, ::kvrf::PutResponse>(
          [](KvRaftServiceRpc::Service* service,
             ::grpc_impl::ServerContext* ctx,
             const ::kvrf::PutRequest* req,
             ::kvrf::PutResponse* resp) {
               return service->Put(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      KvRaftServiceRpc_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< KvRaftServiceRpc::Service, ::kvrf::GetRequest, ::kvrf::GetResponse>(
          [](KvRaftServiceRpc::Service* service,
             ::grpc_impl::ServerContext* ctx,
             const ::kvrf::GetRequest* req,
             ::kvrf::GetResponse* resp) {
               return service->Get(ctx, req, resp);
             }, this)));
}

KvRaftServiceRpc::Service::~Service() {
}

::grpc::Status KvRaftServiceRpc::Service::Put(::grpc::ServerContext* context, const ::kvrf::PutRequest* request, ::kvrf::PutResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status KvRaftServiceRpc::Service::Get(::grpc::ServerContext* context, const ::kvrf::GetRequest* request, ::kvrf::GetResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace kvrf

