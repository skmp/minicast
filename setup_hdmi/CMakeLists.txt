cmake_minimum_required(VERSION 3.10)
project(setup_hdmi CXX)

set(CMAKE_CXX_STANDARD 14)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(setup_hdmi main.cpp smbus.cpp)
target_compile_options(setup_hdmi PRIVATE -Wall -Wextra -Wno-strict-aliasing)
