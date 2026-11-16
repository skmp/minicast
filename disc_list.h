/*
	DreamSTer multi-disc support: the list of images a game spans.

	Built once from the path DreamSTer.sh launches:
	  * a .m3u playlist: one image per line (relative paths are resolved
	    against the playlist's directory; blank lines and #comments are
	    skipped). The first entry boots.
	  * any other image: sibling discs are detected from "Disc N" style
	    names in the file name and its folder - "Disc", "Disk", "CD" or
	    "GD", optionally followed by a space, '_' or '-', then the number
	    (Redump "(Disc 1)", TOSEC "(Disc 1 of 2)", "CD1", ...). Every
	    occurrence of the number is replaced by 1..9 and the variants that
	    exist on disk are kept, in disc order.
	A single image (no pattern, or no siblings found) is a 1-entry list.
*/
#pragma once

#include <string>
#include <vector>

struct DiscList {
	std::vector<std::string> discs;   // image paths, disc order
	int current = 0;                  // index of the booted image
	bool from_m3u = false;
};

/* exists: file existence check, injectable for tests (nullptr = stat). */
DiscList disc_list_build(const std::string& launch_path,
                         bool (*exists)(const std::string&) = nullptr);

bool disc_list_is_m3u(const std::string& path);
