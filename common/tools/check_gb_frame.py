#!/usr/bin/env python3
"""Compare an emulator screenshot with a host frame of the whole screen.

Usage: check_gb_frame.py SCREENSHOT.bmp FRAME.pgm

The screenshot is a 32-bit BMP from SameBoy's tester (160x144) or from
tools/gba_shot (240x160). The frame is the core's framebuffer as the host
drew it, the size of the screen, with any magnification applied. Every
pixel must match, and the screen must hold only the two shades the layers
draw with.
"""
import struct
import sys


def read_bmp(path):
    """Returns (width, height, rows of pixel values), top row first."""
    data = open(path, "rb").read()
    offset, = struct.unpack_from("<I", data, 10)
    width, height, _, bpp = struct.unpack_from("<iiHH", data, 18)
    if bpp != 32:
        sys.exit(f"{path}: expected a 32-bit bitmap, got {bpp}-bit")
    rows = [struct.unpack_from(f"<{width}I", data, offset + y * width * 4) for y in range(abs(height))]
    return width, abs(height), rows if height < 0 else rows[::-1]


def dark(pixel):
    return max(pixel & 0xFF, pixel >> 8 & 0xFF, pixel >> 16 & 0xFF) < 128


def read_pgm(path):
    """Returns (width, height, rows of booleans, True where the pixel is set)."""
    data = open(path, "rb").read()
    width, height = (int(v) for v in data.split(b"\n", 2)[1].split())
    pixels = data[-width * height:]
    return width, height, [[pixels[y * width + x] == 0 for x in range(width)] for y in range(height)]


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    width, height, screen = read_bmp(sys.argv[1])
    frame_w, frame_h, frame = read_pgm(sys.argv[2])
    scale = width // frame_w
    if (frame_w * scale, frame_h * scale) != (width, height):
        sys.exit(f"{sys.argv[2]}: {frame_w}x{frame_h} does not scale to the {width}x{height} screen")
    if len({pixel & 0xFFFFFF for row in screen for pixel in row}) > 2:
        sys.exit(f"FAIL: {sys.argv[1]} has more than two shades")
    wrong = sum(dark(screen[y][x]) != frame[y // scale][x // scale] for y in range(height) for x in range(width))
    if wrong:
        sys.exit(f"FAIL: {wrong} pixels differ from {sys.argv[2]}")
    print(f"ok: {sys.argv[1]} matches {sys.argv[2]}")


if __name__ == "__main__":
    main()
