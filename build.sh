export PATH=$PATH:/opt/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin
export CC='/opt/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc'
[ -d build ] || cmake -DCMAKE_TOOLCHAIN_FILE=`pwd`/tc.cmake -S faux/ -B build/
cmake --build build --parallel

