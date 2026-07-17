# Cross-compile setup_hdmi for the MiSTer FPGA board's ARM/HPS
# (Cortex-A9) Linux. Modeled on minicast's tc.cmake and GLdc's
# toolchains/MiSTer.cmake; uses the same toolchain as the MiSTer kernel
# / minicast / GLdc builds.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(MISTER_TOOLCHAIN /opt/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf)

set(CMAKE_C_COMPILER   ${MISTER_TOOLCHAIN}/bin/arm-none-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${MISTER_TOOLCHAIN}/bin/arm-none-linux-gnueabihf-g++)

set(CMAKE_FIND_ROOT_PATH ${MISTER_TOOLCHAIN})

# search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# search headers and libraries in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
