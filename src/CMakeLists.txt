cmake_minimum_required(VERSION 3.2)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
set(_GRPC_GRPCPP gRPC::grpc++)

get_filename_component(PARENT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
project(raft)
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
set(SRC_LIST kvRaft.cpp raft.grpc.pb.cc raft.pb.cc kvserver.grpc.pb.cc kvserver.pb.cc RaftRPC.cpp RaftServer.cpp ThreadPool.cpp)
set(CLI_SRC_LIST client.cpp)
#set(SRC_LIST client.cpp raft.grpc.pb.cc raft.pb.cc)
add_executable(raft ${SRC_LIST})
add_executable(cli ${CLI_SRC_LIST})
target_link_libraries(raft pthread raft_proto mytcp mytool ${_REFLECTION}
${_GRPC_GRPCPP}
${_PROTOBUF_LIBPROTOBUF})
target_link_libraries(cli pthread raft_proto)