# Minimum CMake required
cmake_minimum_required(VERSION 3.15)

# Project
project(20F_EE324_Assign4)

set (CMAKE_CXX_STANDARD 11)

# Protobuf
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${protobuf_VERSION}")

# Protobuf-compiler
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)

# gRPC
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")
set(_GRPC_GRPCPP gRPC::grpc++)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

# Proto file
get_filename_component(hw_proto "assign4.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

get_filename_component(hw_proto_1 "master_slave.proto" ABSOLUTE)
get_filename_component(hw_proto_path_1 "${hw_proto}" PATH)

get_filename_component(hw_proto_2 "peer.proto" ABSOLUTE)
get_filename_component(hw_proto_path_2 "${hw_proto}" PATH)

# Generated sources
set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/assign4.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/assign4.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/assign4.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/assign4.grpc.pb.h")


# Generated sources
set(hw_proto_srcs_1 "${CMAKE_CURRENT_BINARY_DIR}/master_slave.pb.cc")
set(hw_proto_hdrs_1 "${CMAKE_CURRENT_BINARY_DIR}/master_slave.pb.h")
set(hw_grpc_srcs_1 "${CMAKE_CURRENT_BINARY_DIR}/master_slave.grpc.pb.cc")
set(hw_grpc_hdrs_1 "${CMAKE_CURRENT_BINARY_DIR}/master_slave.grpc.pb.h")


# Generated sources
set(hw_proto_srcs_2 "${CMAKE_CURRENT_BINARY_DIR}/peer.pb.cc")
set(hw_proto_hdrs_2 "${CMAKE_CURRENT_BINARY_DIR}/peer.pb.h")
set(hw_grpc_srcs_2 "${CMAKE_CURRENT_BINARY_DIR}/peer.grpc.pb.cc")
set(hw_grpc_hdrs_2 "${CMAKE_CURRENT_BINARY_DIR}/peer.grpc.pb.h")




add_custom_command(
      OUTPUT "${hw_proto_srcs}" "${hw_proto_srcs_1}" "${hw_proto_srcs_2}" "${hw_proto_hdrs}" "${hw_proto_hdrs_1}" "${hw_proto_hdrs_2}" "${hw_grpc_srcs}" "${hw_grpc_srcs_1}" "${hw_grpc_srcs_2}" "${hw_grpc_hdrs}" 
"${hw_grpc_hdrs_1}"  "${hw_grpc_hdrs_2}"   
   COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${hw_proto_path}" -I "${hw_proto_path_1}" -I "${hw_proto_path_2}" 
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${hw_proto}" "${hw_proto_1}" "${hw_proto_2}" 
      DEPENDS "${hw_proto}" "${hw_proto_1}" "${hw_proto_2}")


# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# Targets (client|server)
foreach(_target
  client super child)
  add_executable(${_target} "${_target}.cpp"
    ${hw_proto_srcs}  ${hw_proto_srcs_1}  ${hw_proto_srcs_2}
    ${hw_grpc_srcs} ${hw_grpc_srcs_1} ${hw_grpc_srcs_2})
  target_link_libraries(${_target}
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()
