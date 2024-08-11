cmake_minimum_required(VERSION 3.2)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
set(_GRPC_GRPCPP gRPC::grpc++)

get_filename_component(PARENT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
get_filename_component(ROOT_FOLDER ${PARENT_FOLDER} DIRECTORY)
project(shardkv)

file(GLOB RAFT_SRC "${PARENT_FOLDER}/raft/*.cpp")
file(GLOB COMMON_SRC "${PARENT_FOLDER}/common/*.cpp")
file(GLOB PROTO_SRC "${PARENT_FOLDER}/proto/*.cpp")

message(STATUS "RAFT_SRC: ${RAFT_SRC}")
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
set(SRC_LIST shardserver.cpp shardserrpc.cpp kvclerk.cpp ${COMMON_SRC} ${RAFT_SRC} ${PROTO_SRC})
set(CLI_SRC_LIST shardclient.cpp kvclerk.cpp ${COMMON_SRC} ${PROTO_SRC})
set(SHARD_MASTER_SRC kvmaster.cpp kvrpc.cpp ${RAFT_SRC} ${COMMON_SRC} ${PROTO_SRC})
set(CLERK_SRC kvclerk.cpp ${COMMON_SRC} ${PROTO_SRC})

add_executable(shardserver ${SRC_LIST})
add_executable(shardclient ${CLI_SRC_LIST})
add_executable(master ${SHARD_MASTER_SRC})
add_executable(clerk ${CLERK_SRC})

target_link_libraries(shardserver pthread raft_proto ${_REFLECTION}
${_GRPC_GRPCPP}
${_PROTOBUF_LIBPROTOBUF})

target_link_libraries(shardclient pthread raft_proto ${_REFLECTION}
${_GRPC_GRPCPP}
${_PROTOBUF_LIBPROTOBUF})

target_link_libraries(master pthread raft_proto mytool)

target_link_libraries(clerk raft_proto)