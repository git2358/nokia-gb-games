#!/usr/bin/env python3
"""Check that Space Impact's banks hold its data at the same addresses.

Usage: gb_mirror_check.py LINK.map

The game's code names the first bank's copy of its data (si_templates and
so on); each other bank has a copy under names ending _mN. Every copy must
be at the same address within its bank as the first, so that the code
reads the same bytes whichever of the banks is mapped.
"""
import re
import sys


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    names = {}
    for line in open(sys.argv[1]):
        m = re.match(r"\s*[0-9A-F]+\s+[0-9A-F]+\s+[0-9A-F]+\s+([0-9A-F]{8})\s+(_si_\w+)", line) or \
            re.match(r"\s*([0-9A-F]{8})\s+(_si_\w+)", line)
        if m:
            names[m.group(2)] = int(m.group(1), 16)
    copies = {n: a for n, a in names.items() if re.search(r"_m\d+$", n)}
    if not copies:
        sys.exit("no copies of Space Impact's data in the map")
    wrong = []
    for name, address in sorted(copies.items()):
        first = names.get(re.sub(r"_m\d+$", "", name))
        if first is None or first & 0x3FFF != address & 0x3FFF:
            wrong.append(name)
    if wrong:
        sys.exit("Space Impact's data is not where the first bank has it: " + " ".join(wrong))
    print(f"Space Impact's data: {len(copies)} copies in place")


if __name__ == "__main__":
    main()
