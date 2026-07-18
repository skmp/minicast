/*
	This file is part of libswirl

	MiSTer/polly2 support glue: emulator/renderer stats OSD drawn into the
	SPG top border band (polly2 FB_TOP, 640x30 shown as 1280x60).
*/
#pragma once
#include "types.h"

/* speed_pct: emulated speed in % (100 = full speed); vblank/renders are
 * rates over the caller's report window; display_mode: short format
 * string, e.g. "480i NTSC". */
void ReportEmulatorStats(double speed_pct, double vblank_per_sec,
                         const char* display_mode, double renders_per_sec);

/* Call once per rendered frame with that frame's wait time (ms) and
 * polly2 cycle count (clk_sys). Min/max are tracked internally over the
 * window since the last UpdateStatsOSD, so OSD reports are always
 * aligned with the draw cadence. */
void ReportRendererStats(double wait_ms, u32 cycles);

/* Redraws the OSD band from the latest reported stats; call right after
 * os_SetWindowText. Lazily maps the band FB and enables FB_TOP on first
 * use; a no-op when polly2 isn't reachable or the bitstream predates
 * border bands (REVISION < 2). */
void UpdateStatsOSD();
