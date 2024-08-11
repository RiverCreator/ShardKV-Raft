find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(c-ares CONFIG)
# find_package(Threads)
# Set the CMake policy for CMP0003
if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)
#
# Protobuf/Grpc source files
#
# set(SHARD_SRCS
#     shardkv.proto
# )
set(PROTO_FILES kvserver.proto raft.proto shardkv.proto)

# set(GENERATED_PROTO_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})
# #
# # Add Library target with protobuf sources
# #
# add_library(shard_proto ${SHARD_SRCS})
# target_include_directories(shard_proto PUBLIC ${GENERATED_PROTO_DIR})
# target_link_libraries(shard_proto 
#     PUBLIC
#         protobuf::libprotobuf
#         gRPC::grpc
#         gRPC::grpc++
# )
# target_include_directories(shard_proto PUBLIC
# ${PROTOBUF_INCLUDE_DIRS} 
# ${CMAKE_CURRENT_BINARY_DIR}
# )

get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
# protobuf_generate(TARGET shard_proto LANGUAGE cpp)
# protobuf_generate(TARGET shard_proto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")


add_library(raft_proto ${PROTO_FILES})

target_link_libraries(raft_proto 
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::grpc++
)
message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(raft_proto PUBLIC
    ${PROTOBUF_INCLUDE_DIRS} 
    ${CMAKE_CURRENT_BINARY_DIR}
)

#
# Compile protobuf and grpc files in mointor proto target to cpp
#
message(STATUS "plugin:${grpc_cpp_plugin_location}")

protobuf_generate(TARGET raft_proto LANGUAGE cpp)
protobuf_generate(TARGET raft_proto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")
