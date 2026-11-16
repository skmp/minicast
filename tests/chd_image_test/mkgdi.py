# Synthetic full-size GD-ROM dump (sparse): 3 tracks, recognizable sectors.
import sys, os, struct, hashlib
out = sys.argv[1]
T1, T2_LBA, T2, T3_LBA = 600, 600, 1200, 45000
T3 = 549150 - T3_LBA
def sector(lba, audio):
    seed = hashlib.sha256(b"%d" % lba).digest()
    body = (seed * 74)[:2352]
    if audio:
        return body
    # raw MODE1 layout: sync(12) header(4) data(2048) edc/ecc(288); the
    # header/ecc are filler here, only the 2048 data bytes get compared
    return b"\x00" + b"\xff" * 10 + b"\x00" + bytes([0, 2, 0, 1]) + body[:2048] + b"\x00" * 288
def write(name, lba0, frames, audio):
    with open(os.path.join(out, name), "wb") as f:
        f.truncate(frames * 2352)
        picks = set(range(0, min(frames, 64))) | set(range(max(0, frames - 64), frames)) | set(range(0, frames, 997))
        for i in sorted(picks):
            f.seek(i * 2352); f.write(sector(lba0 + i, audio))
write("track01.bin", 0, T1, False)
write("track02.raw", T2_LBA, T2, True)
write("track03.bin", T3_LBA, T3, False)
open(os.path.join(out, "game.gdi"), "w").write(
    "3\n1 0 4 2352 track01.bin 0\n2 %d 0 2352 track02.raw 0\n3 %d 4 2352 track03.bin 0\n" % (T2_LBA, T3_LBA))
