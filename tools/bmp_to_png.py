#!/usr/bin/env python3
"""Convert a SameBoy tester screenshot (32-bit BMP) to a scaled PNG.

Usage: bmp_to_png.py IN.bmp OUT.png [SCALE]
"""
import struct
import sys
import zlib


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    scale = int(sys.argv[3]) if len(sys.argv) == 4 else 1
    data = open(sys.argv[1], "rb").read()
    offset, = struct.unpack_from("<I", data, 10)
    width, height, _, bpp = struct.unpack_from("<iiHH", data, 18)
    if bpp != 32:
        sys.exit(f"{sys.argv[1]}: expected a 32-bit bitmap, got {bpp}-bit")
    # Channel masks follow the 40-byte info header: red, green, blue.
    shifts = [(m & -m).bit_length() - 1 for m in struct.unpack_from("<III", data, 54)]
    rows = []
    for y in range(abs(height)):
        row = bytearray()
        for x in range(width):
            pixel, = struct.unpack_from("<I", data, offset + (y * width + x) * 4)
            row += bytes((pixel >> s) & 0xFF for s in shifts) * scale
        rows += [b"\0" + bytes(row)] * scale
    if height > 0:
        rows.reverse()
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width * scale, abs(height) * scale, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(b"".join(rows)))
    png += chunk(b"IEND", b"")
    open(sys.argv[2], "wb").write(png)
    print(f"wrote {sys.argv[2]}")


if __name__ == "__main__":
    main()
