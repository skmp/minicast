cmake_minimum_required(VERSION 3.16)

# Host tests for emulator components. Buildable two ways:
#  - standalone, with the host toolchain (sanitizers on):
#      cmake -S tests -B tests/build
#      cmake --build tests/build
#      ctest --test-dir tests/build
#  - as part of the cross build (compile-checks the headers for the target)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(minicast_tests CXX)
    enable_testing()
endif()

set(dyna ${CMAKE_CURRENT_SOURCE_DIR}/../libswirl/hw/sh4/dyna)

# regalloc.h can't be included without the full emulator headers; generate a
# copy with the emu includes stripped and pair it with the mocks in mockshil.h
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/regalloc_under_test.h
    COMMAND ${CMAKE_COMMAND}
        -DIN=${dyna}/regalloc.h
        -DOUT=${CMAKE_CURRENT_BINARY_DIR}/regalloc_under_test.h
        -P ${CMAKE_CURRENT_SOURCE_DIR}/strip_emu_includes.cmake
    DEPENDS ${dyna}/regalloc.h ${CMAKE_CURRENT_SOURCE_DIR}/strip_emu_includes.cmake
    COMMENT "Generating regalloc_under_test.h from regalloc.h"
)

add_executable(regalloc_fuzz
    regalloc_fuzz.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/regalloc_under_test.h
)
set_property(TARGET regalloc_fuzz PROPERTY CXX_STANDARD 17)
target_include_directories(regalloc_fuzz PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(NOT CMAKE_CROSSCOMPILING)
    target_compile_options(regalloc_fuzz PRIVATE -O1 -g -fsanitize=address,undefined)
    target_link_options(regalloc_fuzz PRIVATE -fsanitize=address,undefined)
endif()

# args: <base seed> <iterations per pool/flag combination>
add_test(NAME regalloc_fuzz COMMAND regalloc_fuzz 1 5000)
# the allocator intentionally keeps the last block's spans until the next
# DoAlloc; that reads as a leak at process exit
set_tests_properties(regalloc_fuzz PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
