cmake_minimum_required(VERSION 3.10)
project(ToyRaft)

set(CMAKE_CXX_STANDARD 11)

find_package(Protobuf REQUIRED)

aux_source_directory(src toyraft_src)

set(Protobuf_USE_STATIC_LIBS ON)

if (Protobuf_FOUND)

    file(GLOB BAR_PROTOS "proto/*.proto")

    foreach (protofile ${BAR_PROTOS})

        get_filename_component(proto_include_h ${protofile} DIRECTORY)

        get_filename_component(proto_src_cc ${protofile} NAME_WE)

        list(APPEND proto_include ${proto_include_h})
        list(REMOVE_DUPLICATES proto_include)
        list(APPEND proto_src ${proto_include_h}/${proto_src_cc}.pb.cc)

        execute_process(COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                -I${proto_include_h} --cpp_out=${proto_include_h} ${proto_src_cc}.proto)
    endforeach ()

    include_directories(include ${PROTOBUF_INCLUDE_DIRS} ${proto_include})

    add_library(ToyRaft STATIC ${toyraft_src} ${proto_src})

    target_link_libraries(ToyRaft ${PROTOBUF_LIBRARIES})
else ()
    message(FATAL_ERROR "FATAL: libprotobuf not found.")
endif ()
