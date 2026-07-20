#!/usr/bin/env python3
"""
Symbolize minicast jit frames in perf output.

perf cannot do this itself: the code cache lives inside SH4_TCB, a static array in
.text of minicast.elf, so perf resolves jit addresses against the binary and never
consults a perf-<pid>.map. This joins `perf script` text output against the map
written by perf_map.cpp instead, which avoids perf's symbolization entirely.

    perf script -i perf.data -F ip,sym,dso > samples.txt
    symbolize_perf.py samples.txt /tmp/jit.map minicast.elf

The map records *runtime* addresses; perf script reports offsets within the binary.
The two differ by a constant, which is recovered from the ELF rather than assumed:
SH4_TCB's vaddr is known from the symbol table, and the map's addresses all fall
inside SH4_TCB, so the lowest map address anchors the runtime base.
"""

import bisect
import collections
import re
import subprocess
import sys


def read_map(path):
    """Parse '<start-hex> <size-hex> <name>' lines into a sorted list."""
    blocks = []

    with open(path) as f:
        for line in f:
            parts = line.split(None, 2)
            if len(parts) != 3:
                continue
            try:
                start = int(parts[0], 16)
                size = int(parts[1], 16)
            except ValueError:
                continue
            blocks.append((start, size, parts[2].strip()))

    blocks.sort()
    return blocks


def sh4_tcb_range(elf):
    """(vaddr, size) of SH4_TCB from the ELF symbol table."""
    try:
        out = subprocess.run(["readelf", "-sW", elf], capture_output=True,
                             text=True, check=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        sys.exit("could not read symbols from %s: %s" % (elf, e))

    for line in out.splitlines():
        parts = line.split()
        # Num: Value Size Type Bind Vis Ndx Name
        if len(parts) >= 8 and parts[-1] == "SH4_TCB":
            value = int(parts[1], 16)
            size = int(parts[2], 16) if parts[2].startswith("0x") else int(parts[2])
            return value, size

    sys.exit("SH4_TCB not found in %s -- wrong binary?" % elf)


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)

    samples_path, map_path, elf_path = sys.argv[1:4]

    blocks = read_map(map_path)
    if not blocks:
        sys.exit("no usable entries in %s" % map_path)

    tcb_vaddr, tcb_size = sh4_tcb_range(elf_path)

    # The map holds runtime addresses; perf reports addresses relative to the elf.
    # For a non-PIE binary these are the same, and measured perf output does land
    # directly inside SH4_TCB's vaddr range -- so the shift is normally zero. It is
    # still computed rather than assumed, so a PIE/ASLR build stays correct: the
    # code cache starts at the page-aligned base of SH4_TCB, which anchors the
    # lowest recorded block to a known elf address.
    runtime_base = blocks[0][0] & ~0xFFF
    shift = runtime_base - ((tcb_vaddr + 0xFFF) & ~0xFFF)

    starts = [b[0] for b in blocks]

    def lookup(addr):
        """Map an elf-offset sample address to a block name, or None."""
        runtime = addr + shift

        i = bisect.bisect_right(starts, runtime) - 1
        if i < 0:
            return None

        start, size, name = blocks[i]
        if runtime < start + size:
            return name

        # inside the cache but not in any block: stale code from before a cache
        # clear, or padding between blocks
        if tcb_vaddr <= addr < tcb_vaddr + tcb_size:
            return "sh4_<unmapped>"
        return None

    counts = collections.Counter()
    total = 0
    jit = 0

    addr_re = re.compile(r"^\s*([0-9a-fA-F]+)\s")

    for line in open(samples_path):
        m = addr_re.match(line)
        if not m:
            continue

        total += 1
        addr = int(m.group(1), 16)

        name = lookup(addr)
        if name is None:
            continue

        jit += 1
        counts[name] += 1

    if not total:
        sys.exit("no sample addresses parsed from %s -- "
                 "was it produced with `perf script -F ip,sym,dso`?" % samples_path)

    print("# %d samples, %d in jit (%.1f%%)" % (total, jit, 100.0 * jit / total))
    print("# SH4_TCB vaddr %08x, runtime base %08x, shift %+#x"
          % (tcb_vaddr, runtime_base, shift))
    print()

    for name, n in counts.most_common():
        print("%8d  %5.2f%%  %s" % (n, 100.0 * n / total, name))


if __name__ == "__main__":
    main()
