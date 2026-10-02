#!/usr/bin/env python3
"""Finish a GBA ROM header: fixed byte and header checksum.

Usage: gbafix.py ROM.gba [--logo-from OTHER.gba]

The boot logo (header bytes 0x04..0x9f) is left as built unless --logo-from
names a GBA ROM you own to copy it from. Emulators run without it; a real
console's BIOS refuses a cartridge whose logo is wrong.
"""
import hashlib
import sys
from pathlib import Path

LOGO = slice(0x04, 0xA0)
LOGO_SHA1 = "17daa0fec02fc33c0f6abb549a8b80b6613b48ee"


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
        logo = logo_from.read_bytes()[LOGO]
        if hashlib.sha1(logo).hexdigest() != LOGO_SHA1:
            sys.exit(f"{logo_from}: its header does not hold the standard boot logo; use another ROM")
        rom[LOGO] = logo
    rom[0xB2] = 0x96
    rom[0xBD] = (-(sum(rom[0xA0:0xBD]) + 0x19)) & 0xFF
    path.write_bytes(rom)
    print(f"{path}: header checksum 0x{rom[0xBD]:02x}" + (", logo copied" if logo_from else ", no logo"))


if __name__ == "__main__":
    main()
