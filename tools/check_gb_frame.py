#!/usr/bin/env python3
"""Compare a SameBoy tester screenshot with a host LCD frame.

Usage: check_gb_frame.py SCREENSHOT.bmp FRAME.pgm

The Game Boy layer draws the 84x48 LCD 1:1, centred on the 160x144 screen.
Every LCD pixel must match the host frame and everything around it must be
blank.
"""
import struct
import sys

GB_W, GB_H = 160, 144
LCD_W, LCD_H = 84, 48
ORIGIN_X, ORIGIN_Y = (GB_W - LCD_W) // 2, (GB_H - LCD_H) // 2


def read_bmp(path):
    """Returns rows of booleans, True where the pixel is dark."""
    data = open(path, "rb").read()
    offset, = struct.unpack_from("<I", data, 10)
    width, height, _, bpp = struct.unpack_from("<iiHH", data, 18)
    if (width, abs(height), bpp) != (GB_W, GB_H, 32):
        sys.exit(f"{path}: expected a {GB_W}x{GB_H} 32-bit bitmap, got {width}x{abs(height)} {bpp}-bit")
    rows = []
    for y in range(GB_H):
        row = data[offset + y * GB_W * 4:offset + (y + 1) * GB_W * 4]
        # The three colour channels are equal on a DMG; skip a zero pad byte.
        rows.append([max(row[x * 4:x * 4 + 4]) < 128 for x in range(GB_W)])
    return rows if height < 0 else rows[::-1]


def read_pgm(path):
    data = open(path, "rb").read()
    pixels = data[-LCD_W * LCD_H:]
    return [[pixels[y * LCD_W + x] == 0 for x in range(LCD_W)] for y in range(LCD_H)]


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    screen, frame = read_bmp(sys.argv[1]), read_pgm(sys.argv[2])
    wrong = 0
    for y in range(GB_H):
        for x in range(GB_W):
            fx, fy = x - ORIGIN_X, y - ORIGIN_Y
            expected = 0 <= fx < LCD_W and 0 <= fy < LCD_H and frame[fy][fx]
            wrong += screen[y][x] != expected
    if wrong:
        sys.exit(f"FAIL: {wrong} pixels differ from {sys.argv[2]}")
    print(f"ok: {sys.argv[1]} matches {sys.argv[2]}")


if __name__ == "__main__":
    main()
