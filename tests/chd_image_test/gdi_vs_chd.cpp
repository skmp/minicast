// Opens the same dump as .gdi and .chd through minicast's own image readers
// and checks that both describe the same disc and return the same sectors.
#include "imgread/imgread_common.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

Disc* chd_parse(const wchar* file);
Disc* gdi_parse(const wchar* file);

Disc* cdi_parse(const wchar*) { return 0; }
Disc* cue_parse(const wchar*) { return 0; }
void os_DebugBreak() { printf("os_DebugBreak\n"); fflush(stdout); abort(); }

int msgboxf(const wchar* text, unsigned int type, ...) { printf("msgbox: %s\n", text); return 0; }

int main(int argc, char** argv)
{
	if (argc < 3) return 2;
	Disc* g = gdi_parse(argv[1]);
	Disc* c = chd_parse(argv[2]);
	if (!g || !c) { printf("FAIL: open gdi=%p chd=%p\n", (void*)g, (void*)c); return 1; }

	int bad = 0;
	if (g->tracks.size() != c->tracks.size()) { printf("FAIL: track count %zu vs %zu\n", g->tracks.size(), c->tracks.size()); return 1; }
	for (size_t i = 0; i < g->tracks.size(); i++) {
		const Track& a = g->tracks[i]; const Track& b = c->tracks[i];
		printf("track %zu: gdi %u-%u ctrl %u | chd %u-%u ctrl %u\n", i + 1, a.StartFAD, a.EndFAD, a.CTRL, b.StartFAD, b.EndFAD, b.CTRL);
		if (a.StartFAD != b.StartFAD || a.CTRL != b.CTRL) { printf("FAIL: track %zu layout differs\n", i + 1); bad++; }
	}
	printf("type gdi %x chd %x, EndFAD gdi %u chd %u\n", g->type, c->type, g->EndFAD, c->EndFAD);
	if (g->type != c->type) { printf("FAIL: disc type\n"); bad++; }

	// every FAD that holds a pattern in the synthetic dump, plus neighbours
	static u8 bg[2352], bc[2352];
	u32 checked = 0, nonzero = 0;
	struct { u32 lba0, frames, fmt; } tr[] = { {0, 600, 2048}, {600, 1200, 2352}, {45000, 504150, 2048} };
	for (auto& t : tr) {
		for (u32 i = 0; i < t.frames; i++) {
			bool pick = i < 66 || i + 66 >= t.frames || i % 997 <= 1;
			if (!pick) continue;
			u32 fad = t.lba0 + i + 150;
			memset(bg, 0xAA, sizeof bg); memset(bc, 0x55, sizeof bc);
			g->ReadSectors(fad, 1, bg, t.fmt);
			c->ReadSectors(fad, 1, bc, t.fmt);
			checked++;
			for (u32 k = 0; k < t.fmt; k++) if (bg[k]) { nonzero++; break; }
			if (memcmp(bg, bc, t.fmt) != 0) {
				if (bad < 10) printf("FAIL: FAD %u (fmt %u) differs: gdi %02x%02x%02x%02x chd %02x%02x%02x%02x\n", fad, t.fmt, bg[0], bg[1], bg[2], bg[3], bc[0], bc[1], bc[2], bc[3]);
				bad++;
			}
		}
	}
	printf("%u sectors compared (%u with data), %d mismatches\n", checked, nonzero, bad);
	printf(bad ? "CHD TEST FAILED\n" : "CHD TEST PASSED\n");
	return bad ? 1 : 0;
}
