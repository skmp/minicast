/*
	This file is part of libswirl

	Stats OSD in the polly2 SPG top border band: a 640x30 RGB565 linear
	framebuffer in DDR right after the VRAM window, displayed 2x-doubled
	as a 1280x60 band above the game image (see polly2-rtl FB_TOP).

	ReportEmulatorStats() stores the numbers; ReportRendererStats() is
	called once per rendered frame and min/max accumulate here, so each
	OSD update shows the extremes of exactly the window since the last
	draw. UpdateStatsOSD() redraws the band and resets that window:

	  S <speed%> V <vblank/s> R <renders/s>  W <max wait ms> R <max render ms>  <mode>

	R converts the window's max render cycles to ms using the core clock
	the CLK register reads back at draw time (75/90/100/112.5 MHz).

	Drawn with a built-in 8x8 font (5x7 glyphs); the 2x band doubling makes
	it 16x16 on screen. This TU maps its own polly2 MMIO view (polly2_mmio
	is per-TU static, same pattern as AudioStream_Mister).
*/
#include "types.h"
#include "mister_support.h"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "../../../../polly2-rtl/driver/polly2_mmio.h"

/* Physical DDR home of the band FB: the first 128-byte-aligned byte after
 * the 16MB VRAM window at 0x32000000 (HW_FPGA_VRAM_OFST + SPAN in
 * _vmem.cpp) - outside everything the core or Linux touches. */
#define STATS_FB_PHYS 0x33000000u
#define STATS_FB_MAP  (64u * 1024u)   /* page-multiple >= 1280*30 bytes */

static volatile u16* stats_fb;        /* 640x30, stride 640 pixels */

static struct {
	double speed_pct, vbs, rps;
	char   mode[24];
	// per-OSD-window accumulators, merged by ReportRendererStats() each
	// frame and reset by UpdateStatsOSD() after drawing
	double wait_min_ms, wait_max_ms;
	u32    cycles_min, cycles_max, cycles_total, frames;
} stats = { 0, 0, 0, {0}, 1e12, 0, ~0u, 0 };

static void reset_render_window()
{
	stats.wait_min_ms = 1e12;
	stats.wait_max_ms = 0;
	stats.cycles_min  = ~0u;
	stats.cycles_max  = 0;
	stats.cycles_total = 0;
	stats.frames = 0;
}

// ---------------------------------------------------------------- font ---
// 5x7 glyphs in 8x8 cells, MSB = leftmost pixel, rows top to bottom.

static const u8 font_digit[10][8] = {
	{0x70,0x88,0x98,0xA8,0xC8,0x88,0x70,0x00},  // 0
	{0x20,0x60,0x20,0x20,0x20,0x20,0x70,0x00},  // 1
	{0x70,0x88,0x08,0x30,0x40,0x80,0xF8,0x00},  // 2
	{0x70,0x88,0x08,0x30,0x08,0x88,0x70,0x00},  // 3
	{0x10,0x30,0x50,0x90,0xF8,0x10,0x10,0x00},  // 4
	{0xF8,0x80,0xF0,0x08,0x08,0x88,0x70,0x00},  // 5
	{0x30,0x40,0x80,0xF0,0x88,0x88,0x70,0x00},  // 6
	{0xF8,0x08,0x10,0x20,0x40,0x40,0x40,0x00},  // 7
	{0x70,0x88,0x88,0x70,0x88,0x88,0x70,0x00},  // 8
	{0x70,0x88,0x88,0x78,0x08,0x10,0x60,0x00},  // 9
};

static const u8 font_upper[26][8] = {
	{0x70,0x88,0x88,0xF8,0x88,0x88,0x88,0x00},  // A
	{0xF0,0x88,0x88,0xF0,0x88,0x88,0xF0,0x00},  // B
	{0x70,0x88,0x80,0x80,0x80,0x88,0x70,0x00},  // C
	{0xF0,0x88,0x88,0x88,0x88,0x88,0xF0,0x00},  // D
	{0xF8,0x80,0x80,0xF0,0x80,0x80,0xF8,0x00},  // E
	{0xF8,0x80,0x80,0xF0,0x80,0x80,0x80,0x00},  // F
	{0x70,0x88,0x80,0xB8,0x88,0x88,0x78,0x00},  // G
	{0x88,0x88,0x88,0xF8,0x88,0x88,0x88,0x00},  // H
	{0x70,0x20,0x20,0x20,0x20,0x20,0x70,0x00},  // I
	{0x38,0x10,0x10,0x10,0x10,0x90,0x60,0x00},  // J
	{0x88,0x90,0xA0,0xC0,0xA0,0x90,0x88,0x00},  // K
	{0x80,0x80,0x80,0x80,0x80,0x80,0xF8,0x00},  // L
	{0x88,0xD8,0xA8,0xA8,0x88,0x88,0x88,0x00},  // M
	{0x88,0xC8,0xA8,0x98,0x88,0x88,0x88,0x00},  // N
	{0x70,0x88,0x88,0x88,0x88,0x88,0x70,0x00},  // O
	{0xF0,0x88,0x88,0xF0,0x80,0x80,0x80,0x00},  // P
	{0x70,0x88,0x88,0x88,0xA8,0x90,0x68,0x00},  // Q
	{0xF0,0x88,0x88,0xF0,0xA0,0x90,0x88,0x00},  // R
	{0x78,0x80,0x80,0x70,0x08,0x08,0xF0,0x00},  // S
	{0xF8,0x20,0x20,0x20,0x20,0x20,0x20,0x00},  // T
	{0x88,0x88,0x88,0x88,0x88,0x88,0x70,0x00},  // U
	{0x88,0x88,0x88,0x88,0x88,0x50,0x20,0x00},  // V
	{0x88,0x88,0x88,0xA8,0xA8,0xA8,0x50,0x00},  // W
	{0x88,0x88,0x50,0x20,0x50,0x88,0x88,0x00},  // X
	{0x88,0x88,0x50,0x20,0x20,0x20,0x20,0x00},  // Y
	{0xF8,0x08,0x10,0x20,0x40,0x80,0xF8,0x00},  // Z
};

static const u8 font_pct[8]   = {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00};  // %
static const u8 font_dot[8]   = {0x00,0x00,0x00,0x00,0x00,0x60,0x60,0x00};  // .
static const u8 font_slash[8] = {0x04,0x08,0x10,0x20,0x40,0x80,0x00,0x00};  // /
static const u8 font_colon[8] = {0x00,0x60,0x60,0x00,0x60,0x60,0x00,0x00};  // :
static const u8 font_minus[8] = {0x00,0x00,0x00,0xF8,0x00,0x00,0x00,0x00};  // -
static const u8 font_plus[8]  = {0x00,0x20,0x20,0xF8,0x20,0x20,0x00,0x00};  // +
static const u8 font_blank[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static const u8* glyph(char c)
{
	if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
	if (c >= '0' && c <= '9') return font_digit[c - '0'];
	if (c >= 'A' && c <= 'Z') return font_upper[c - 'A'];
	switch (c) {
		case '%': return font_pct;
		case '.': return font_dot;
		case '/': return font_slash;
		case ':': return font_colon;
		case '-': return font_minus;
		case '+': return font_plus;
		default:  return font_blank;
	}
}

// ------------------------------------------------------------- drawing ---

static void draw_text(int x, int y, const char* text, u16 color)
{
	for (; *text && x <= (int)POLLY2_BAND_W - 8; text++, x += 8) {
		const u8* g = glyph(*text);
		for (int r = 0; r < 8; r++) {
			volatile u16* line = stats_fb + (y + r) * POLLY2_BAND_W + x;
			for (int b = 0; b < 8; b++)
				if (g[r] & (0x80u >> b)) line[b] = color;
		}
	}
}

static bool osd_init()
{
	static int state;   /* 0 = untried, 1 = ok, -1 = unavailable */
	if (state) return state > 0;
	state = -1;

	if (!polly2_mmio && polly2_mmio_init() != 0) {
		printf("mister_support: no polly2 MMIO, stats OSD disabled\n");
		return false;
	}
	if (!polly2_has_bands()) {
		printf("mister_support: bitstream REVISION %u has no border bands, "
		       "stats OSD disabled\n", polly2_revision());
		return false;
	}

	int fd = open("/dev/mem_wc", O_RDWR | O_SYNC);
	if (fd < 0) fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		printf("mister_support: cannot open /dev/mem(_wc), stats OSD disabled\n");
		return false;
	}
	void* m = mmap(0, STATS_FB_MAP, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
	               STATS_FB_PHYS);
	close(fd);
	if (m == MAP_FAILED) {
		printf("mister_support: band FB mmap failed, stats OSD disabled\n");
		return false;
	}
	stats_fb = (volatile u16*)m;

	for (u32 i = 0; i < POLLY2_BAND_W * POLLY2_BAND_H; i++) stats_fb[i] = 0;
	__asm__ volatile("dsb sy" ::: "memory");
	polly2_set_fb_top(STATS_FB_PHYS);   /* band on, black until first draw */

	state = 1;
	return true;
}

// ----------------------------------------------------------------- api ---

void ReportEmulatorStats(double speed_pct, double vblank_per_sec,
                         const char* display_mode, double renders_per_sec)
{
	stats.speed_pct = speed_pct;
	stats.vbs       = vblank_per_sec;
	stats.rps       = renders_per_sec;
	snprintf(stats.mode, sizeof(stats.mode), "%s",
	         display_mode ? display_mode : "");
}

void ReportRendererStats(double wait_ms, u32 cycles)
{
	stats.cycles_total += cycles;
	stats.frames++;

	if (wait_ms < stats.wait_min_ms) stats.wait_min_ms = wait_ms;
	if (wait_ms > stats.wait_max_ms) stats.wait_max_ms = wait_ms;
	if (cycles < stats.cycles_min)   stats.cycles_min  = cycles;
	if (cycles > stats.cycles_max)   stats.cycles_max  = cycles;
}

void UpdateStatsOSD()
{
	if (!osd_init())
		return;

	u32 hz = polly2_clock_hz();

	double render_max_ms = stats.cycles_max * 1e3 / hz;
	double rps_polly2 = stats.cycles_total ?
		(double)stats.frames * hz / (double)stats.cycles_total :
		0;

	char line[POLLY2_BAND_W / 8 + 1];
	snprintf(line, sizeof(line),
	         "S %5.1f%% V %4.1f R %4.1f %s WT %4.1f RT %4.1f PS %4.1f",
	         stats.speed_pct, stats.vbs, stats.rps, stats.mode,
	         stats.wait_max_ms, render_max_ms, rps_polly2);

	for (u32 i = 0; i < POLLY2_BAND_W * POLLY2_BAND_H; i++) stats_fb[i] = 0;
	draw_text(2, 11, line, 0xFFFF);
	__asm__ volatile("dsb sy" ::: "memory");

	reset_render_window();   // next OSD update covers exactly this gap
}
