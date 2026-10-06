#!/usr/bin/env python3
"""Finish a GBA ROM header: boot logo, fixed byte and header checksum.

Usage: gbafix.py ROM.gba [--logo-from OTHER.gba]

A real console's BIOS refuses a cartridge whose boot logo (header bytes
0x04..0x9f) is wrong; emulators do not mind. The logo is not in this
repository: it is copied from a GBA ROM you have. --logo-from names one;
without it the first of these that holds the logo is used:

  roms/gba-logo/*.gba         this repository's ROMs as last built with it
                              (roms/ is never committed)
  tools/mgba/cinema/**/*.gba  mGBA's test ROMs, which scripts/setup-mgba.sh
                              clones
  /Volumes/OMEGADE/*.gba      the ROMs on the Omega flash cart's card

Every ROM finished with the logo is kept in roms/gba-logo/ for next time,
so the cart's card is needed at most once. When none is found the ROM is
left without the logo, with a warning: make cards refuses it.
"""
import hashlib
import shutil
import sys
from pathlib import Path

LOGO = slice(0x04, 0xA0)
LOGO_SHA1 = "17daa0fec02fc33c0f6abb549a8b80b6613b48ee"
REPO = Path(__file__).resolve().parents[2]
KEPT = REPO / "roms" / "gba-logo"


def has_logo(path):
    try:
        with open(path, "rb") as f:
            header = f.read(0xA0)
    except OSError:
        return False
    return len(header) == 0xA0 and hashlib.sha1(header[LOGO]).hexdigest() == LOGO_SHA1


def find_logo_source(exclude):
    candidates = sorted(KEPT.glob("*.gba"))
    candidates += sorted((REPO / "tools" / "mgba" / "cinema").glob("**/*.gba"))
    candidates += sorted(Path("/Volumes/OMEGADE").glob("*.gba"))
    for path in candidates:
        if path.resolve() != exclude and has_logo(path):
            return path
    return None


def main():
    args = sys.argv[1:]
    logo_from = None
    if "--logo-from" in args:
        i = args.index("--logo-from")
        logo_from = Path(args[i + 1])
        del args[i:i + 2]
    if len(args) != 1:
        sys.exit(__doc__)
    path = Path(args[0])
    rom = bytearray(path.read_bytes())
    if len(rom) < 0xC0:
        sys.exit(f"{path}: too short to hold a GBA header")

    if logo_from:
        if not has_logo(logo_from):
            sys.exit(f"{logo_from}: its header does not hold the standard boot logo; use another ROM")
    else:
        logo_from = find_logo_source(path.resolve())
    if logo_from:
        rom[LOGO] = logo_from.read_bytes()[LOGO]
    rom[0xB2] = 0x96
    rom[0xBD] = (-(sum(rom[0xA0:0xBD]) + 0x19)) & 0xFF
    path.write_bytes(rom)
    if not logo_from:
        print(f"{path}: header checksum 0x{rom[0xBD]:02x}, no logo (found no ROM to copy it from; "
              f"see gbafix.py): a console will refuse it, and make cards too")
        return
    KEPT.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(path, KEPT / path.name)
    print(f"{path}: header checksum 0x{rom[0xBD]:02x}, logo copied from {logo_from}; kept in {KEPT / path.name}")


if __name__ == "__main__":
    main()
