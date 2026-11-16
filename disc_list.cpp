#include "disc_list.h"

#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <fstream>
#include <iterator>

static bool file_exists(const std::string& path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

static std::string lower(const std::string& s)
{
	std::string r(s);
	for (size_t i = 0; i < r.size(); i++)
		r[i] = (char)tolower((unsigned char)r[i]);
	return r;
}

bool disc_list_is_m3u(const std::string& path)
{
	std::string l = lower(path);
	return l.size() >= 4 && l.compare(l.size() - 4, 4, ".m3u") == 0;
}

static std::string trim(const std::string& s)
{
	size_t b = 0, e = s.size();
	while (b < e && isspace((unsigned char)s[b])) b++;
	while (e > b && isspace((unsigned char)s[e - 1])) e--;
	return s.substr(b, e - b);
}

static DiscList parse_m3u(const std::string& m3u)
{
	DiscList dl;
	dl.from_m3u = true;

	std::string dir;
	size_t slash = m3u.find_last_of('/');
	if (slash != std::string::npos)
		dir = m3u.substr(0, slash + 1);

	// playlists made on other systems: CR / CRLF, UTF-8 BOM, '\\' separators
	std::ifstream f(m3u.c_str(), std::ios::binary);
	std::string text((std::istreambuf_iterator<char>(f)),
	                 std::istreambuf_iterator<char>());
	if (text.size() >= 3 && (unsigned char)text[0] == 0xEF &&
	    (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
		text = text.substr(3);

	size_t pos = 0;
	while (pos < text.size()) {
		size_t end = text.find_first_of("\r\n", pos);
		if (end == std::string::npos)
			end = text.size();
		std::string line = trim(text.substr(pos, end - pos));
		pos = end + 1;
		if (line.empty() || line[0] == '#')
			continue;
		for (size_t i = 0; i < line.size(); i++)
			if (line[i] == '\\')
				line[i] = '/';
		dl.discs.push_back(line[0] == '/' ? line : dir + line);
	}
	return dl;
}

namespace {
struct NumRun { size_t pos, len; int value; bool zero_pad; };
}

/* "Disc N"-style numbers in the last two path components (folder + file) */
static std::vector<NumRun> find_disc_numbers(const std::string& path)
{
	static const char* tokens[] = { "disc", "disk", "cd", "gd" };
	std::vector<NumRun> runs;
	std::string l = lower(path);

	size_t start = 0;
	size_t last = l.find_last_of('/');
	if (last != std::string::npos && last > 0) {
		size_t prev = l.find_last_of('/', last - 1);
		start = prev == std::string::npos ? 0 : prev + 1;
	}

	for (size_t i = start; i < l.size(); i++) {
		if (i > start && isalnum((unsigned char)l[i - 1]))
			continue;
		for (const char* tok : tokens) {
			size_t tl = strlen(tok);
			if (l.compare(i, tl, tok) != 0)
				continue;
			size_t j = i + tl;
			if (j < l.size() && (l[j] == ' ' || l[j] == '_' || l[j] == '-'))
				j++;
			size_t k = j;
			while (k < l.size() && k - j < 2 && isdigit((unsigned char)l[k]))
				k++;
			if (k == j || (k < l.size() && isalnum((unsigned char)l[k])))
				continue;
			runs.push_back({ j, k - j, atoi(l.substr(j, k - j).c_str()),
			                 k - j == 2 && l[j] == '0' });
			i = k - 1;
			break;
		}
	}
	return runs;
}

DiscList disc_list_build(const std::string& launch_path,
                         bool (*exists)(const std::string&))
{
	if (!exists)
		exists = file_exists;

	if (disc_list_is_m3u(launch_path))
		return parse_m3u(launch_path);

	DiscList dl;
	std::vector<NumRun> runs = find_disc_numbers(launch_path);

	// every occurrence must name the same disc, else don't guess
	bool consistent = !runs.empty();
	for (size_t r = 1; r < runs.size(); r++)
		if (runs[r].value != runs[0].value)
			consistent = false;

	if (!consistent || runs[0].value < 1) {
		dl.discs.push_back(launch_path);
		return dl;
	}

	int cur = runs[0].value;
	int max_n = cur > 9 ? cur : 9;
	for (int n = 1; n <= max_n; n++) {
		std::string cand = launch_path;
		for (size_t r = runs.size(); r-- > 0; ) {
			std::string num = std::to_string(n);
			if (runs[r].zero_pad && num.size() < 2)
				num = "0" + num;
			cand.replace(runs[r].pos, runs[r].len, num);
		}
		if (n == cur) {
			dl.current = (int)dl.discs.size();
			dl.discs.push_back(launch_path);
		} else if (exists(cand)) {
			dl.discs.push_back(cand);
		}
	}
	return dl;
}
