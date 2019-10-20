cmake_minimum_required(VERSION 3.10)
project(ToyRaft)

set(CMAKE_CXX_STANDARD 11)

find_package(RapidJSON REQUIRED)

find_package(Protobuf REQUIRED)

aux_source_directory(src toyraft_src)

set(Protobuf_USE_STATIC_LIBS ON)

file(GLOB GLOB_PROTOS "proto/*.proto")
set(GRPC_CPP_PLUGIN /usr/local/bin/grpc_cpp_plugin)

foreach (protofile ${GLOB_PROTOS})

    get_filename_component(proto_include_h ${protofile} DIRECTORY)

    get_filename_component(proto_src_cc ${protofile} NAME_WE)

    list(APPEND proto_include ${proto_include_h})
    list(REMOVE_DUPLICATES proto_include)
    list(APPEND proto_src ${proto_include_h}/${proto_src_cc}.pb.cc)
    list(APPEND grpc_src ${proto_include_h}/${proto_src_cc}.grpc.pb.cc)

    execute_process(COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            -I${proto_include_h} --cpp_out=${proto_include_h} ${proto_src_cc}.proto)
    execute_process(COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            -I${proto_include_h} --grpc_out=${proto_include_h}
            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
            ${proto_src_cc}.proto)
endforeach ()

include_directories(include ${PROTOBUF_INCLUDE_DIRS} ${proto_include} ${RapidJSON_INCLUDE_DIR})

add_library(ToyRaft STATIC ${toyraft_src} ${proto_src} ${grpc_src})

target_link_libraries(ToyRaft pthread ${PROTOBUF_LIBRARIES} grpc)

if(COMPILED_TEST)
    add_executable(logger_test test/logger_test.cpp)
    target_link_libraries(logger_test ToyRaft)
endif()
