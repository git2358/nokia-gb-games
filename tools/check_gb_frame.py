#!/usr/bin/env python3
"""Compare an emulator screenshot with a host LCD frame.

Usage: check_gb_frame.py SCREENSHOT.bmp FRAME.pgm [--zoom N]

The screenshot is a 32-bit BMP from SameBoy's tester (160x144) or from
tools/gba_shot (240x160). The Game Boy layer draws the 84x48 LCD 1:1 and the
GBA layer at 2x, both centred. Every LCD pixel must match the host frame and
everything around it must be one flat colour; a host frame the size of the
whole screen is compared pixel for pixel. With --zoom, the frame is the
whole screen but the phone's LCD in its middle is expected magnified N
times, and what the frame holds around the LCD is expected light on dark,
where the magnified LCD does not cover it.
"""
import struct
import sys

LCD_W, LCD_H = 84, 48
SCALES = {(160, 144): 1, (240, 160): 2}


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


def shades(screen):
    """Distinct pixel values on the screen."""
    return {pixel & 0xFFFFFF for row in screen for pixel in row}


def read_pgm(path):
    """Returns (width, height, rows of booleans, True where the pixel is set)."""
    data = open(path, "rb").read()
    width, height = (int(v) for v in data.split(b"\n", 2)[1].split())
    pixels = data[-width * height:]
    return width, height, [[pixels[y * width + x] == 0 for x in range(width)] for y in range(height)]


def check_zoomed(screen, frame, width, height, zoom):
    """Counts wrong pixels of a screen showing the frame's LCD magnified."""
    phone_x, phone_y = (width - LCD_W) // 2, (height - LCD_H) // 2
    zoom_x, zoom_y = (width - LCD_W * zoom) // 2, (height - LCD_H * zoom) // 2
    wrong = 0
    for y in range(height):
        for x in range(width):
            fx, fy = (x - zoom_x) // zoom, (y - zoom_y) // zoom
            if 0 <= fx < LCD_W and 0 <= fy < LCD_H:
                wrong += dark(screen[y][x]) != frame[phone_y + fy][phone_x + fx]
            else:
                wrong += dark(screen[y][x]) == frame[y][x]
    return wrong


def main():
    zoom = 0
    if len(sys.argv) == 5 and sys.argv[3] == "--zoom":
        zoom = int(sys.argv[4])
    elif len(sys.argv) != 3:
        sys.exit(__doc__)
    width, height, screen = read_bmp(sys.argv[1])
    frame_w, frame_h, frame = read_pgm(sys.argv[2])
    if zoom:
        if (frame_w, frame_h) != (width, height):
            sys.exit(f"{sys.argv[2]}: --zoom needs a frame the size of the screen")
        wrong = check_zoomed(screen, frame, width, height, zoom)
        if wrong:
            sys.exit(f"FAIL: {wrong} pixels differ from {sys.argv[2]}")
        print(f"ok: {sys.argv[1]} matches {sys.argv[2]}")
        return
    if (frame_w, frame_h) == (width, height):
        # A frame of the whole screen: compare every pixel. Anything but the
        # two shades the layer draws with is stray data.
        if len(shades(screen)) > 2:
            sys.exit(f"FAIL: {sys.argv[1]} has more than two shades")
        wrong = sum(dark(screen[y][x]) != frame[y][x] for y in range(height) for x in range(width))
        if wrong:
            sys.exit(f"FAIL: {wrong} pixels differ from {sys.argv[2]}")
        print(f"ok: {sys.argv[1]} matches {sys.argv[2]}")
        return
    if (width, height) not in SCALES:
        sys.exit(f"{sys.argv[1]}: unexpected size {width}x{height}")
    scale = SCALES[(width, height)]
    origin_x, origin_y = (width - LCD_W * scale) // 2, (height - LCD_H * scale) // 2
    border = screen[0][0]
    wrong = 0
    for y in range(height):
        for x in range(width):
            fx, fy = (x - origin_x) // scale, (y - origin_y) // scale
            if 0 <= fx < LCD_W and 0 <= fy < LCD_H:
                wrong += dark(screen[y][x]) != frame[fy][fx]
            else:
                wrong += screen[y][x] != border
    if wrong:
        sys.exit(f"FAIL: {wrong} pixels differ from {sys.argv[2]}")
    print(f"ok: {sys.argv[1]} matches {sys.argv[2]}")


if __name__ == "__main__":
    main()
