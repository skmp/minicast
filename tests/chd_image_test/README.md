# CHD reader check (manual)

Verifies that minicast's CHD reader (imgread/chd.cpp on libchdr) returns the
same disc layout and sector data as its GDI reader for the same dump.

1. `python3 mkgdi.py <dir>` writes a synthetic, sparse, full-size GD-ROM dump
   (3 tracks: data, audio, data) with recognizable sectors.
2. `chdman createcd -i <dir>/game.gdi -o <dir>/game.chd` (add `-c cdlz`,
   `-c cdzl`, `-c cdfl` or `-c cdzs` to exercise one codec).
3. Build `gdi_vs_chd.cpp` with `imgread/chd.cpp`, `imgread/gdi.cpp`,
   `imgread/imgread_common.cpp`, `deps/coreio/coreio.cpp` and the libchdr
   static libraries from the minicast build, using the minicast compile flags,
   then run it (natively on the DE10-Nano or under `qemu-arm-static`) from
   `<dir>`: `gdi_vs_chd game.gdi game.chd`.

It needs chdman, so it is not part of ctest.
