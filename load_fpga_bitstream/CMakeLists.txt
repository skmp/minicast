cmake_minimum_required(VERSION 3.10)
project(load_fpga_bitstream CXX)

set(CMAKE_CXX_STANDARD 14)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(load_fpga_bitstream main.cpp)
target_compile_options(load_fpga_bitstream PRIVATE -Wall -Wextra -Wno-strict-aliasing)
