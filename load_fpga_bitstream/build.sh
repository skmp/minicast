#!/bin/sh -e
# Build load_fpga_bitstream for the MiSTer FPGA board's ARM/HPS Linux.
# Modeled on GLdc's build_mister.sh / minicast's run_cmake.sh.
#
# Requires the ARM cross toolchain at
# /opt/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf
# (same one minicast, GLdc and the MiSTer kernel use).

cd "$(dirname "$0")"

cmake -DCMAKE_TOOLCHAIN_FILE="$(pwd)/toolchain.cmake" -S . -B build
cmake --build build --parallel
