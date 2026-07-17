# load_fpga_bitstream

Minimal standalone FPGA bitstream (`.rbf`) loader for the MiSTer /
DE10-Nano (Intel Cyclone V SoC HPS). Derived from
[Main_MiSTer](https://github.com/MiSTer-devel/Main_MiSTer)'s `fpga_io.cpp`
(GPLv3, see `LICENSE`).

It programs the FPGA through the SoC FPGA Manager:

1. Reads the `.rbf` (plain, or MiSTer-wrapped — the 16-byte `MiSTer` header
   is detected and skipped).
2. Asserts the framework core reset (GPO bit 30) and disables the
   HPS↔FPGA bridges.
3. Streams the bitstream into the FPGA Manager data port and polls
   through config-done / init / user mode.
4. Re-enables the bridges and releases the core reset.

## Building

```sh
./build.sh
```

Cross-compiles with the same ARM toolchain as minicast / GLdc / the
MiSTer kernel (`/opt/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf`).
Output: `build/load_fpga_bitstream`.

## Usage (on the MiSTer, as root)

```sh
./load_fpga_bitstream core.rbf
```

Note: this does not stop the running MiSTer main process — kill it first
if it's active, or it will keep poking the (now replaced) core.
