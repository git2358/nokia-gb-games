#!/usr/bin/env python3
"""Say where a Game Boy run spent its time, by function.

Usage: gb_profile.py LINK.map PROFILE [TOP]

PROFILE is what tools/gb_run's prof step wrote. Addresses below 0x4000 are
looked up among the symbols of the first bank, the rest among those of the
bank that was mapped. Prints the TOP (default 30) functions by share of
the time; a function's time includes any labels inside it that the map
does not list.
"""
import bisect
import re
import sys


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    symbols = {}  # bank -> sorted [(address, name)]
    for line in open(sys.argv[1]):
        m = re.match(r"\s+([0-9A-F]{8})\s+(\S+)", line)
        if not m:
            continue
        address, name = int(m.group(1), 16), m.group(2)
        if name.startswith(("s_", "l_", ".", "e_")):
            continue
        bank, low = address >> 16, address & 0xFFFF
        if low < 0x8000:
            symbols.setdefault(bank if low >= 0x4000 else 0, []).append((low, name))
    for bank in symbols:
        symbols[bank] = sorted(set(symbols[bank]))
    spent, total = {}, 0
    for line in open(sys.argv[2]):
        bank, pc, cycles = line.split()
        bank, pc, cycles = int(bank), int(pc, 16), int(cycles)
        total += cycles
        table = symbols.get(bank if pc >= 0x4000 else 0, [])
        i = bisect.bisect_right(table, (pc, "\xff")) - 1
        name = table[i][1] if i >= 0 and pc < 0x8000 else f"{pc:04x}"
        spent[name] = spent.get(name, 0) + cycles
    top = int(sys.argv[3]) if len(sys.argv) > 3 else 30
    for name, cycles in sorted(spent.items(), key=lambda item: -item[1])[:top]:
        print(f"{100 * cycles / total:5.1f}%  {name}")


if __name__ == "__main__":
    main()
