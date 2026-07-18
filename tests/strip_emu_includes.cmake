# Generates a copy of an emulator header with the emu-wide includes stripped,
# so it can be compiled against the test mocks (see mockshil.h).
# Usage: cmake -DIN=<header> -DOUT=<out> -P strip_emu_includes.cmake
file(READ "${IN}" content)
string(REPLACE "#include \"license/bsd\"" "" content "${content}")
string(REPLACE "#include <types.h>" "" content "${content}")
string(REPLACE "#include \"shil.h\"" "" content "${content}")
file(WRITE "${OUT}" "${content}")
