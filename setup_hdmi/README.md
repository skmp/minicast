# setup_hdmi

Minimal standalone ADV7513 HDMI transmitter setup for the MiSTer /
DE10-Nano, fixed at 1920x1080. Derived from
[Main_MiSTer](https://github.com/MiSTer-devel/Main_MiSTer)'s `video.cpp`
(`hdmi_config_init` / `hdmi_config_set_csc` / `hdmi_config_set_mode`) and
`smbus.cpp` (GPLv3, see `LICENSE`).

Configures the chip for exactly what the simplex `sys_top.v`/`SPG.sv`
fabric produces:

- 1920x1080 CEA-861 (**VIC 16**), 2200x1125 totals, **both syncs
  positive** (unlike stock MiSTer modes, no sync inversion in reg 0x17)
  at the fixed 148.5 / 148.352 MHz `pll_hdmi` pixel clock.
- 24-bit RGB 444 input, full-range identity CSC, no pixel repetition.
- HPD forced high, interrupts disabled (nothing is listening).
- I2S audio, 48 kHz, 16-bit, N=6144 / CTS=74250.

## Building

```sh
./build.sh
```

Cross-compiles with the same ARM toolchain as minicast / GLdc / the
MiSTer kernel. Output: `build/setup_hdmi`.

## Usage (on the MiSTer, as root)

```sh
./setup_hdmi [--dvi] [--limited] [--game]
```

- `--dvi` — DVI mode instead of HDMI (no infoframes/audio on the wire)
- `--limited` — limited-range RGB (16-235) via the CSC + AVI quantization bits
- `--game` — flag content as game / IT content in the AVI InfoFrame

The ADV7513's I2C bus is the HPS I2C peripheral routed **through the FPGA
fabric** to the HDMI pins (`cyclonev_hps_interface_peripheral_i2c` in
`sys_top.v`), so run this *after* `load_fpga_bitstream` — with no (or a
wrong) bitstream loaded the chip is unreachable and the tool reports
"ADV7513 not found". Order: load bitstream, then `setup_hdmi`.
