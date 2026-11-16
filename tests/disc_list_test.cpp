#include "../disc_list.h"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <unistd.h>

static int failures;

#define CHECK(cond) do { if (!(cond)) { \
	printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static std::set<std::string> fs;
static bool fake_exists(const std::string& p) { return fs.count(p) != 0; }

static void test_redump_folders()
{
	fs.clear();
	const std::string d1 = "/media/fat/games/Dreamcast/Grandia II (USA) (Disc 1)/Grandia II (USA) (Disc 1).gdi";
	const std::string d2 = "/media/fat/games/Dreamcast/Grandia II (USA) (Disc 2)/Grandia II (USA) (Disc 2).gdi";
	fs.insert(d1); fs.insert(d2);

	DiscList a = disc_list_build(d1, fake_exists);
	CHECK(a.discs.size() == 2);
	CHECK(a.current == 0);
	CHECK(a.discs[0] == d1 && a.discs[1] == d2);

	DiscList b = disc_list_build(d2, fake_exists);
	CHECK(b.discs.size() == 2);
	CHECK(b.current == 1);
	CHECK(b.discs[0] == d1);
}

static void test_tosec_of_n()
{
	fs.clear();
	const std::string d1 = "/g/Skies of Arcadia v1.002 (2000)(Sega)(US)(Disc 1 of 2)[!].cdi";
	const std::string d2 = "/g/Skies of Arcadia v1.002 (2000)(Sega)(US)(Disc 2 of 2)[!].cdi";
	fs.insert(d1); fs.insert(d2);
	DiscList a = disc_list_build(d1, fake_exists);
	CHECK(a.discs.size() == 2);
	CHECK(a.discs[1] == d2);
}

static void test_compact_and_padded()
{
	fs.clear();
	fs.insert("/g/game/game_cd2.cue");
	DiscList a = disc_list_build("/g/game/game_cd1.cue", fake_exists);
	CHECK(a.discs.size() == 2 && a.discs[1] == "/g/game/game_cd2.cue");

	fs.clear();
	fs.insert("/g/x/Game Disk-02.gdi");
	DiscList b = disc_list_build("/g/x/Game Disk-01.gdi", fake_exists);
	CHECK(b.discs.size() == 2 && b.discs[1] == "/g/x/Game Disk-02.gdi");

	fs.clear();
	fs.insert("/g/Shenmue II (Europe) (Disc 4).gdi");
	fs.insert("/g/Shenmue II (Europe) (Disc 2).gdi");
	DiscList c = disc_list_build("/g/Shenmue II (Europe) (Disc 1).gdi", fake_exists);
	CHECK(c.discs.size() == 3);
	CHECK(c.discs[2] == "/g/Shenmue II (Europe) (Disc 4).gdi");
}

static void test_no_false_positives()
{
	fs.clear();
	// "Discworld" / "Dreamcast" / "ABCD1" must not be read as disc numbers
	fs.insert("/g/Discworld 2.gdi");
	DiscList a = disc_list_build("/g/Discworld 1.gdi", fake_exists);
	CHECK(a.discs.size() == 1);

	DiscList b = disc_list_build("/media/fat/games/Dreamcast/Crazy Taxi/Crazy Taxi.gdi", fake_exists);
	CHECK(b.discs.size() == 1 && b.current == 0);

	fs.insert("/g/ABCD2.gdi");
	DiscList c = disc_list_build("/g/ABCD1.gdi", fake_exists);
	CHECK(c.discs.size() == 1);

	fs.insert("/media/CD 2/games/Game/Game.gdi");
	DiscList d = disc_list_build("/media/CD 1/games/Game/Game.gdi", fake_exists);
	CHECK(d.discs.size() == 1);

	fs.insert("/g/Game (Disc 2)/Game (Disc 1).gdi");
	DiscList e = disc_list_build("/g/Game (Disc 1)/Game (Disc 2).gdi", fake_exists);
	CHECK(e.discs.size() == 1);
}

static void test_m3u()
{
	char dir[] = "/tmp/disc_list_testXXXXXX";
	if (!mkdtemp(dir)) {
		printf("FAIL: mkdtemp\n");
		failures++;
		return;
	}
	std::string m3u = std::string(dir) + "/Grandia II.M3U";
	FILE* f = fopen(m3u.c_str(), "wb");
	if (!f) {
		printf("FAIL: fopen %s\n", m3u.c_str());
		failures++;
		return;
	}
	fputs("\xEF\xBB\xBF# Grandia II\r\n"
	      "Grandia II (Disc 1)/Grandia II (Disc 1).gdi\r\n"
	      "\r\n"
	      "  /abs/Grandia II (Disc 2).gdi  \r\n", f);
	fclose(f);

	CHECK(disc_list_is_m3u(m3u));
	DiscList a = disc_list_build(m3u);
	CHECK(a.from_m3u);
	CHECK(a.discs.size() == 2);
	CHECK(a.current == 0);
	CHECK(a.discs[0] == std::string(dir) + "/Grandia II (Disc 1)/Grandia II (Disc 1).gdi");
	CHECK(a.discs[1] == "/abs/Grandia II (Disc 2).gdi");

	f = fopen(m3u.c_str(), "wb");
	if (f) {
		fputs("Game (Disc 1)\\Game (Disc 1).gdi\rGame (Disc 2)\\Game (Disc 2).gdi", f);
		fclose(f);
	}
	DiscList b = disc_list_build(m3u);
	CHECK(b.discs.size() == 2);
	CHECK(b.discs.size() == 2 && b.discs[1] == std::string(dir) + "/Game (Disc 2)/Game (Disc 2).gdi");

	unlink(m3u.c_str());
	rmdir(dir);

	DiscList missing = disc_list_build("/nonexistent/x.m3u");
	CHECK(missing.discs.empty());
}

int main()
{
	test_redump_folders();
	test_tosec_of_n();
	test_compact_and_padded();
	test_no_false_positives();
	test_m3u();
	if (failures) {
		printf("disc_list: %d failure(s)\n", failures);
		return 1;
	}
	printf("disc_list: all tests passed\n");
	return 0;
}
