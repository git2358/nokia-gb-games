#!/usr/bin/env python3
"""Check that every bank of the Game Boy ROM fits its 16 KiB.

Usage: gb_bank_check.py LINK.map

Reads the linker's map: the areas outside _CODE_n make up the always-mapped
first bank, which must end by 0x4000; each _CODE_n must be at most 0x4000
long. Prints the room left in each.
"""
import re
import sys

BANK = 0x4000


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    areas = {}
    for line in open(sys.argv[1]):
        m = re.match(r"(_\w+)\s+([0-9A-F]{8})\s+([0-9A-F]{8}) =", line)
        if m:
            areas[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    used = {0: max(start + size for name, (start, size) in areas.items()
                   if not name.startswith("_CODE_") and start < 0x8000)}
    for name, (start, size) in areas.items():
        if name.startswith("_CODE_"):
            used[int(name[6:])] = size
    ram = max(start + size for start, size in areas.values() if 0xC000 <= start < 0xE000) - 0xC000
    print("  ".join(f"bank {n}: {BANK - size} free" for n, size in sorted(used.items()))
          + f"  work RAM: {0x2000 - ram} free below the stack's end")
    over = [n for n, size in used.items() if size > BANK]
    if over:
        sys.exit(f"bank {over} does not fit")


if __name__ == "__main__":
    main()
