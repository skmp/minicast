# Extracts rw_related()/constprop() verbatim from shil.cpp so the test compiles
# the real source text rather than a hand-copied version that could drift.
# Usage: cmake -DIN=<shil.cpp> -DOUT=<out> -P extract_constprop.cmake
file(READ "${IN}" content)

# from the start of rw_related up to (not including) the next function after
# constprop -- these two are contiguous and self-contained
string(FIND "${content}" "void rw_related(RuntimeBlockInfo* blk)" _begin)
string(FIND "${content}" "//read_v4m3z1" _end)

if(_begin EQUAL -1 OR _end EQUAL -1 OR _end LESS _begin)
    message(FATAL_ERROR
        "could not locate rw_related()..constprop() in ${IN} -- did the "
        "function names or their order change? Update extract_constprop.cmake.")
endif()

math(EXPR _len "${_end} - ${_begin}")
string(SUBSTRING "${content}" ${_begin} ${_len} extracted)

file(WRITE "${OUT}" "// GENERATED from shil.cpp -- do not edit.\n#include \"constprop_mocks.h\"\n\n${extracted}")
