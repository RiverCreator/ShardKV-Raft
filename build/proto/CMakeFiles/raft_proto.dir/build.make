# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/river/work/Raft/Raft_LAB3

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/river/work/Raft/Raft_LAB3/build

# Include any dependencies generated for this target.
include proto/CMakeFiles/raft_proto.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include proto/CMakeFiles/raft_proto.dir/compiler_depend.make

# Include the progress variables for this target.
include proto/CMakeFiles/raft_proto.dir/progress.make

# Include the compile flags for this target's objects.
include proto/CMakeFiles/raft_proto.dir/flags.make

proto/kvserver.pb.h: ../proto/kvserver.proto
proto/kvserver.pb.h: /usr/local/bin/protoc-3.14.0.0
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/river/work/Raft/Raft_LAB3/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Running cpp protocol buffer compiler on kvserver.proto"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/local/bin/protoc-3.14.0.0 --cpp_out /home/river/work/Raft/Raft_LAB3/build/proto -I /home/river/work/Raft/Raft_LAB3/proto /home/river/work/Raft/Raft_LAB3/proto/kvserver.proto

proto/kvserver.pb.cc: proto/kvserver.pb.h
	@$(CMAKE_COMMAND) -E touch_nocreate proto/kvserver.pb.cc

proto/raft.pb.h: ../proto/raft.proto
proto/raft.pb.h: /usr/local/bin/protoc-3.14.0.0
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/river/work/Raft/Raft_LAB3/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Running cpp protocol buffer compiler on raft.proto"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/local/bin/protoc-3.14.0.0 --cpp_out /home/river/work/Raft/Raft_LAB3/build/proto -I /home/river/work/Raft/Raft_LAB3/proto /home/river/work/Raft/Raft_LAB3/proto/raft.proto

proto/raft.pb.cc: proto/raft.pb.h
	@$(CMAKE_COMMAND) -E touch_nocreate proto/raft.pb.cc

proto/shardkv.pb.h: ../proto/shardkv.proto
proto/shardkv.pb.h: /usr/local/bin/protoc-3.14.0.0
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/river/work/Raft/Raft_LAB3/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Running cpp protocol buffer compiler on shardkv.proto"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/local/bin/protoc-3.14.0.0 --cpp_out /home/river/work/Raft/Raft_LAB3/build/proto -I /home/river/work/Raft/Raft_LAB3/proto /home/river/work/Raft/Raft_LAB3/proto/shardkv.proto

proto/shardkv.pb.cc: proto/shardkv.pb.h
	@$(CMAKE_COMMAND) -E touch_nocreate proto/shardkv.pb.cc

proto/kvserver.grpc.pb.h: ../proto/kvserver.proto
proto/kvserver.grpc.pb.h: /usr/local/bin/protoc-3.14.0.0
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/river/work/Raft/Raft_LAB3/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Running grpc protocol buffer compiler on kvserver.proto"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/local/bin/protoc-3.14.0.0 --grpc_out /home/river/work/Raft/Raft_LAB3/build/proto --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin -I /home/river/work/Raft/Raft_LAB3/proto /home/river/work/Raft/Raft_LAB3/proto/kvserver.proto

proto/kvserver.grpc.pb.cc: proto/kvserver.grpc.pb.h
	@$(CMAKE_COMMAND) -E touch_nocreate proto/kvserver.grpc.pb.cc

proto/raft.grpc.pb.h: ../proto/raft.proto
proto/raft.grpc.pb.h: /usr/local/bin/protoc-3.14.0.0
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/river/work/Raft/Raft_LAB3/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Running grpc protocol buffer compiler on raft.proto"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/local/bin/protoc-3.14.0.0 --grpc_out /home/river/work/Raft/Raft_LAB3/build/proto --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin -I /home/river/work/Raft/Raft_LAB3/proto /home/river/work/Raft/Raft_LAB3/proto/raft.proto

proto/raft.grpc.pb.cc: proto/raft.grpc.pb.h
	@$(CMAKE_COMMAND) -E touch_nocreate proto/raft.grpc.pb.cc

proto/shardkv.grpc.pb.h: ../proto/shardkv.proto
proto/shardkv.grpc.pb.h: /usr/local/bin/protoc-3.14.0.0
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/river/work/Raft/Raft_LAB3/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Running grpc protocol buffer compiler on shardkv.proto"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/local/bin/protoc-3.14.0.0 --grpc_out /home/river/work/Raft/Raft_LAB3/build/proto --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin -I /home/river/work/Raft/Raft_LAB3/proto /home/river/work/Raft/Raft_LAB3/proto/shardkv.proto

proto/shardkv.grpc.pb.cc: proto/shardkv.grpc.pb.h
	@$(CMAKE_COMMAND) -E touch_nocreate proto/shardkv.grpc.pb.cc

proto/CMakeFiles/raft_proto.dir/kvserver.pb.o: proto/CMakeFiles/raft_proto.dir/flags.make
proto/CMakeFiles/raft_proto.dir/kvserver.pb.o: proto/kvserver.pb.cc
proto/CMakeFiles/raft_proto.dir/kvserver.pb.o: proto/CMakeFiles/raft_proto.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/river/work/Raft/Raft_LAB3/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building CXX object proto/CMakeFiles/raft_proto.dir/kvserver.pb.o"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT proto/CMakeFiles/raft_proto.dir/kvserver.pb.o -MF CMakeFiles/raft_proto.dir/kvserver.pb.o.d -o CMakeFiles/raft_proto.dir/kvserver.pb.o -c /home/river/work/Raft/Raft_LAB3/build/proto/kvserver.pb.cc

proto/CMakeFiles/raft_proto.dir/kvserver.pb.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/raft_proto.dir/kvserver.pb.i"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/river/work/Raft/Raft_LAB3/build/proto/kvserver.pb.cc > CMakeFiles/raft_proto.dir/kvserver.pb.i

proto/CMakeFiles/raft_proto.dir/kvserver.pb.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/raft_proto.dir/kvserver.pb.s"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/river/work/Raft/Raft_LAB3/build/proto/kvserver.pb.cc -o CMakeFiles/raft_proto.dir/kvserver.pb.s

proto/CMakeFiles/raft_proto.dir/raft.pb.o: proto/CMakeFiles/raft_proto.dir/flags.make
proto/CMakeFiles/raft_proto.dir/raft.pb.o: proto/raft.pb.cc
proto/CMakeFiles/raft_proto.dir/raft.pb.o: proto/CMakeFiles/raft_proto.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/river/work/Raft/Raft_LAB3/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Building CXX object proto/CMakeFiles/raft_proto.dir/raft.pb.o"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT proto/CMakeFiles/raft_proto.dir/raft.pb.o -MF CMakeFiles/raft_proto.dir/raft.pb.o.d -o CMakeFiles/raft_proto.dir/raft.pb.o -c /home/river/work/Raft/Raft_LAB3/build/proto/raft.pb.cc

proto/CMakeFiles/raft_proto.dir/raft.pb.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/raft_proto.dir/raft.pb.i"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/river/work/Raft/Raft_LAB3/build/proto/raft.pb.cc > CMakeFiles/raft_proto.dir/raft.pb.i

proto/CMakeFiles/raft_proto.dir/raft.pb.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/raft_proto.dir/raft.pb.s"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/river/work/Raft/Raft_LAB3/build/proto/raft.pb.cc -o CMakeFiles/raft_proto.dir/raft.pb.s

proto/CMakeFiles/raft_proto.dir/shardkv.pb.o: proto/CMakeFiles/raft_proto.dir/flags.make
proto/CMakeFiles/raft_proto.dir/shardkv.pb.o: proto/shardkv.pb.cc
proto/CMakeFiles/raft_proto.dir/shardkv.pb.o: proto/CMakeFiles/raft_proto.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/river/work/Raft/Raft_LAB3/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "Building CXX object proto/CMakeFiles/raft_proto.dir/shardkv.pb.o"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT proto/CMakeFiles/raft_proto.dir/shardkv.pb.o -MF CMakeFiles/raft_proto.dir/shardkv.pb.o.d -o CMakeFiles/raft_proto.dir/shardkv.pb.o -c /home/river/work/Raft/Raft_LAB3/build/proto/shardkv.pb.cc

proto/CMakeFiles/raft_proto.dir/shardkv.pb.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/raft_proto.dir/shardkv.pb.i"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/river/work/Raft/Raft_LAB3/build/proto/shardkv.pb.cc > CMakeFiles/raft_proto.dir/shardkv.pb.i

proto/CMakeFiles/raft_proto.dir/shardkv.pb.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/raft_proto.dir/shardkv.pb.s"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/river/work/Raft/Raft_LAB3/build/proto/shardkv.pb.cc -o CMakeFiles/raft_proto.dir/shardkv.pb.s

proto/CMakeFiles/raft_proto.dir/kvserver.grpc.pb.o: proto/CMakeFiles/raft_proto.dir/flags.make
proto/CMakeFiles/raft_proto.dir/kvserver.grpc.pb.o: proto/kvserver.grpc.pb.cc
proto/CMakeFiles/raft_proto.dir/kvserver.grpc.pb.o: proto/CMakeFiles/raft_proto.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/river/work/Raft/Raft_LAB3/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_10) "Building CXX object proto/CMakeFiles/raft_proto.dir/kvserver.grpc.pb.o"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT proto/CMakeFiles/raft_proto.dir/kvserver.grpc.pb.o -MF CMakeFiles/raft_proto.dir/kvserver.grpc.pb.o.d -o CMakeFiles/raft_proto.dir/kvserver.grpc.pb.o -c /home/river/work/Raft/Raft_LAB3/build/proto/kvserver.grpc.pb.cc

proto/CMakeFiles/raft_proto.dir/kvserver.grpc.pb.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/raft_proto.dir/kvserver.grpc.pb.i"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/river/work/Raft/Raft_LAB3/build/proto/kvserver.grpc.pb.cc > CMakeFiles/raft_proto.dir/kvserver.grpc.pb.i

proto/CMakeFiles/raft_proto.dir/kvserver.grpc.pb.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/raft_proto.dir/kvserver.grpc.pb.s"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/river/work/Raft/Raft_LAB3/build/proto/kvserver.grpc.pb.cc -o CMakeFiles/raft_proto.dir/kvserver.grpc.pb.s

proto/CMakeFiles/raft_proto.dir/raft.grpc.pb.o: proto/CMakeFiles/raft_proto.dir/flags.make
proto/CMakeFiles/raft_proto.dir/raft.grpc.pb.o: proto/raft.grpc.pb.cc
proto/CMakeFiles/raft_proto.dir/raft.grpc.pb.o: proto/CMakeFiles/raft_proto.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/river/work/Raft/Raft_LAB3/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_11) "Building CXX object proto/CMakeFiles/raft_proto.dir/raft.grpc.pb.o"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT proto/CMakeFiles/raft_proto.dir/raft.grpc.pb.o -MF CMakeFiles/raft_proto.dir/raft.grpc.pb.o.d -o CMakeFiles/raft_proto.dir/raft.grpc.pb.o -c /home/river/work/Raft/Raft_LAB3/build/proto/raft.grpc.pb.cc

proto/CMakeFiles/raft_proto.dir/raft.grpc.pb.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/raft_proto.dir/raft.grpc.pb.i"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/river/work/Raft/Raft_LAB3/build/proto/raft.grpc.pb.cc > CMakeFiles/raft_proto.dir/raft.grpc.pb.i

proto/CMakeFiles/raft_proto.dir/raft.grpc.pb.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/raft_proto.dir/raft.grpc.pb.s"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/river/work/Raft/Raft_LAB3/build/proto/raft.grpc.pb.cc -o CMakeFiles/raft_proto.dir/raft.grpc.pb.s

proto/CMakeFiles/raft_proto.dir/shardkv.grpc.pb.o: proto/CMakeFiles/raft_proto.dir/flags.make
proto/CMakeFiles/raft_proto.dir/shardkv.grpc.pb.o: proto/shardkv.grpc.pb.cc
proto/CMakeFiles/raft_proto.dir/shardkv.grpc.pb.o: proto/CMakeFiles/raft_proto.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/river/work/Raft/Raft_LAB3/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_12) "Building CXX object proto/CMakeFiles/raft_proto.dir/shardkv.grpc.pb.o"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT proto/CMakeFiles/raft_proto.dir/shardkv.grpc.pb.o -MF CMakeFiles/raft_proto.dir/shardkv.grpc.pb.o.d -o CMakeFiles/raft_proto.dir/shardkv.grpc.pb.o -c /home/river/work/Raft/Raft_LAB3/build/proto/shardkv.grpc.pb.cc

proto/CMakeFiles/raft_proto.dir/shardkv.grpc.pb.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/raft_proto.dir/shardkv.grpc.pb.i"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/river/work/Raft/Raft_LAB3/build/proto/shardkv.grpc.pb.cc > CMakeFiles/raft_proto.dir/shardkv.grpc.pb.i

proto/CMakeFiles/raft_proto.dir/shardkv.grpc.pb.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/raft_proto.dir/shardkv.grpc.pb.s"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/river/work/Raft/Raft_LAB3/build/proto/shardkv.grpc.pb.cc -o CMakeFiles/raft_proto.dir/shardkv.grpc.pb.s

# Object files for target raft_proto
raft_proto_OBJECTS = \
"CMakeFiles/raft_proto.dir/kvserver.pb.o" \
"CMakeFiles/raft_proto.dir/raft.pb.o" \
"CMakeFiles/raft_proto.dir/shardkv.pb.o" \
"CMakeFiles/raft_proto.dir/kvserver.grpc.pb.o" \
"CMakeFiles/raft_proto.dir/raft.grpc.pb.o" \
"CMakeFiles/raft_proto.dir/shardkv.grpc.pb.o"

# External object files for target raft_proto
raft_proto_EXTERNAL_OBJECTS =

proto/libraft_proto.a: proto/CMakeFiles/raft_proto.dir/kvserver.pb.o
proto/libraft_proto.a: proto/CMakeFiles/raft_proto.dir/raft.pb.o
proto/libraft_proto.a: proto/CMakeFiles/raft_proto.dir/shardkv.pb.o
proto/libraft_proto.a: proto/CMakeFiles/raft_proto.dir/kvserver.grpc.pb.o
proto/libraft_proto.a: proto/CMakeFiles/raft_proto.dir/raft.grpc.pb.o
proto/libraft_proto.a: proto/CMakeFiles/raft_proto.dir/shardkv.grpc.pb.o
proto/libraft_proto.a: proto/CMakeFiles/raft_proto.dir/build.make
proto/libraft_proto.a: proto/CMakeFiles/raft_proto.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/river/work/Raft/Raft_LAB3/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_13) "Linking CXX static library libraft_proto.a"
	cd /home/river/work/Raft/Raft_LAB3/build/proto && $(CMAKE_COMMAND) -P CMakeFiles/raft_proto.dir/cmake_clean_target.cmake
	cd /home/river/work/Raft/Raft_LAB3/build/proto && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/raft_proto.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
proto/CMakeFiles/raft_proto.dir/build: proto/libraft_proto.a
.PHONY : proto/CMakeFiles/raft_proto.dir/build

proto/CMakeFiles/raft_proto.dir/clean:
	cd /home/river/work/Raft/Raft_LAB3/build/proto && $(CMAKE_COMMAND) -P CMakeFiles/raft_proto.dir/cmake_clean.cmake
.PHONY : proto/CMakeFiles/raft_proto.dir/clean

proto/CMakeFiles/raft_proto.dir/depend: proto/kvserver.grpc.pb.cc
proto/CMakeFiles/raft_proto.dir/depend: proto/kvserver.grpc.pb.h
proto/CMakeFiles/raft_proto.dir/depend: proto/kvserver.pb.cc
proto/CMakeFiles/raft_proto.dir/depend: proto/kvserver.pb.h
proto/CMakeFiles/raft_proto.dir/depend: proto/raft.grpc.pb.cc
proto/CMakeFiles/raft_proto.dir/depend: proto/raft.grpc.pb.h
proto/CMakeFiles/raft_proto.dir/depend: proto/raft.pb.cc
proto/CMakeFiles/raft_proto.dir/depend: proto/raft.pb.h
proto/CMakeFiles/raft_proto.dir/depend: proto/shardkv.grpc.pb.cc
proto/CMakeFiles/raft_proto.dir/depend: proto/shardkv.grpc.pb.h
proto/CMakeFiles/raft_proto.dir/depend: proto/shardkv.pb.cc
proto/CMakeFiles/raft_proto.dir/depend: proto/shardkv.pb.h
	cd /home/river/work/Raft/Raft_LAB3/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/river/work/Raft/Raft_LAB3 /home/river/work/Raft/Raft_LAB3/proto /home/river/work/Raft/Raft_LAB3/build /home/river/work/Raft/Raft_LAB3/build/proto /home/river/work/Raft/Raft_LAB3/build/proto/CMakeFiles/raft_proto.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : proto/CMakeFiles/raft_proto.dir/depend

