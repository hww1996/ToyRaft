cmake_minimum_required(VERSION 3.10)
project(ToyRaft)

set(CMAKE_CXX_STANDARD 11)

include_directories(include)

aux_source_directory(src toyraft_src)


add_library(ToyRaft STATIC ${toyraft_src})