#!/usr/bin/env python3
"""Extract the 3210 v6.00 game graphics from your own dump into C arrays.

Usage: extract_assets.py DUMP OUT_DIR

DUMP is either the raw flash file (3210f600a.fls) or the 16-bit byte-swapped
image the static tools use (3210f600a_swap16.bin); it is identified by hash.
Writes OUT_DIR/game_assets.c and OUT_DIR/game_assets.h. The output is derived
from the firmware and must stay in an ignored directory.
"""
import hashlib
import struct
import sys
from pathlib import Path

FLASH_BASE = 0x200000
SHA256_RAW = "7bf29b96e544b682c4d6d01c7a6eaef89909c4191a52d829115d37b31c0c0d8a"
SHA256_SWAP16 = "66d2ec57385099d6dca8d93b75d72fcde496f3f8a3246331351d8ebce6fac8c1"

# (name, address, size in bytes, description). Addresses are NSE-8/9 v6.00
# only. Bitmaps are column-major, bit (y & 7) of each byte is row y.
TABLES = [
    ("game_tile_bitmaps", 0x2D94BC, 74 * 7, "7x7 tiles, 7 bytes each: blank + 0x49 symbols"),
    ("game_tile_back", 0x2D96C4, 8, "card back, 7x7 plus a pad byte"),
    ("game_tile_cursor", 0x2D96CC, 8, "cursor frame, 7x7 plus a pad byte"),
    ("games_digit_glyphs", 0x2D96EC, 10 * 3, "3x5 digits 0..9, 3 bytes each"),
    ("game_speed_table", 0x2D9738, 9, "Snake speed per level, units of 10 ms"),
    ("snake_food_bitmap", 0x2D9744, 4, "4x4"),
    ("game3_background", 0x2D9764, 504, "84x48 scene, 6 bytes per column"),
    ("game3_sprites", 0x2D995C, 0x2D9A20 - 0x2D995C, "10x10 sprites, 20 bytes each, then smaller pieces"),
    ("menu_games_icon", 0x2C50D4, 128, "64x16 frame of the main menu's Games animation, 64 bytes per 8 rows"),
]

# The language pack ("PPM") holds the fonts and the text. Each chunk is
# {u32 checksum, u32 length, 4-byte name, ...}.
FONT_CHUNK = 0x2F041C - 8
TEXT_CHUNK = 0x2F2678 - 8
ENGLISH_BLOCK = 0x2E8  # offset of the ENGL block inside the TEXT chunk

FONT_NAMES = ["font_large_bold", "font_small_plain", "font_small_bold", "font_tiny_plain"]
FIRST_CHAR, LAST_CHAR = 0x20, 0x7E

# English text by index in the language pack's string table.
STRINGS = [
    ("text_select", 692),
    ("text_ok", 687),
    ("text_more", 685),
    ("text_game_over", 285),
    ("text_top_score_value", 286),
    ("text_level_title", 287),
    ("text_logic", 288),
    ("text_react", 289),
    ("text_memory", 290),
    ("text_games", 291),
    ("text_1_player", 292),
    ("text_2_players", 293),
    ("text_continue", 294),
    ("text_top_score", 295),
    ("text_instructions", 296),
    ("text_last_view", 297),
    ("text_level", 298),
    ("text_new_game", 299),
    ("text_rotation", 300),
    ("text_snake", 301),
    ("text_game_over_top_score", 302),
    ("text_game_over_lost", 303),
    ("text_game_over_score", 304),
    ("text_game_over_won", 305),
    ("text_help_logic", 976),
    ("text_help_react", 977),
    ("text_help_memory", 978),
    ("text_help_rotation", 979),
    ("text_help_snake", 980),
]


def load_image(path):
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest == SHA256_RAW:
        return data
    if digest == SHA256_SWAP16:
        # Byte tables are in address order in the raw file; undo the swap.
        out = bytearray(data)
        out[0::2], out[1::2] = data[1::2], data[0::2]
        return bytes(out)
    sys.exit(
        f"{path}: sha256 {digest} is not the NSE-8/9 v6.00 dump "
        f"(expected {SHA256_RAW} or its swap16 form {SHA256_SWAP16})"
    )


def chunk(image, addr, name):
    start = addr - FLASH_BASE
    length, tag = struct.unpack_from(">I4s", image, start + 4)
    if tag != name:
        sys.exit(f"no {name.decode()} chunk at 0x{addr:06x}")
    return image[start:start + 8 + length]


def extract_fonts(image):
    """Returns [(height, baseline, [columns per character])] for the fonts.

    A font record is 44 bytes: offsets (relative to the record) of the glyph
    pool table and of the character range table, and the range count. A range
    entry is {u16 first, u16 last, u32 packed}: row offset << 14, pool << 10,
    height << 5, baseline. A pool is one tall bitmap of a fixed width, stored
    as strips of 8 rows with one byte per column; a glyph is `height` rows of
    it starting at the row offset.
    """
    data = chunk(image, FONT_CHUNK, b"FONT")
    count, = struct.unpack_from(">I", data, 0x24)
    fonts = []
    for index in range(count):
        record = 0x28 + index * 44
        pools_at, _, ranges_at, ranges = struct.unpack_from(">IIII", data, record)
        glyphs, height, baseline = [], 0, 0
        for char in range(FIRST_CHAR, LAST_CHAR + 1):
            columns = []
            for r in range(ranges):
                first, last, packed = struct.unpack_from(">HHI", data, record + ranges_at + r * 8)
                if first <= char <= last:
                    height, baseline = packed >> 5 & 31, packed & 31
                    pool = record + pools_at + (packed >> 10 & 15) * 12
                    pool_data, _, width = struct.unpack_from(">IHH", data, pool)
                    row = (packed >> 14) + (char - first) * height
                    for x in range(width):
                        bits = 0
                        for y in range(height):
                            byte = data[pool + pool_data + ((row + y) >> 3) * width + x]
                            bits |= (byte >> ((row + y) & 7) & 1) << y
                        columns.append(bits)
                    break
            glyphs.append(columns)
        fonts.append((height, baseline, glyphs))
    return fonts


def extract_strings(image):
    data = chunk(image, TEXT_CHUNK, b"TEXT")
    size, tag = struct.unpack_from(">I4s", data, ENGLISH_BLOCK + 4)
    if tag != b"ENGL":
        sys.exit("no ENGL block in the TEXT chunk")
    lengths, end = ENGLISH_BLOCK + 16, ENGLISH_BLOCK + size
    count = total = 0
    while lengths + count + total < end:
        total += data[lengths + count]
        count += 1
    strings, at = [], lengths + count
    for i in range(count):
        strings.append(data[at:at + data[lengths + i]])
        at += data[lengths + i]
    if strings[0] != b"English\0":
        sys.exit("unexpected ENGL string table layout")
    return strings


def c_string(text):
    out = ""
    for byte in text:
        char = chr(byte)
        if char == "\n":
            out += "\\n"
        elif char in '"\\':
            out += "\\" + char
        elif 0x20 <= byte < 0x7F:
            out += char
        else:
            out += f'\\x{byte:02x}""'
    return f'"{out}"'


def c_array(name, data):
    lines = [f"const uint8_t {name}[{len(data)}] = {{"]
    for i in range(0, len(data), 12):
        lines.append("    " + " ".join(f"0x{b:02x}," for b in data[i:i + 12]))
    lines.append("};")
    return "\n".join(lines)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    image = load_image(Path(sys.argv[1]))
    out_dir = Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)

    banner = "/* Generated by tools/extract_assets.py from a firmware dump. Do not commit. */"
    header = [banner, "#ifndef GAME_ASSETS_H", "#define GAME_ASSETS_H", "", "#include <stdint.h>", ""]
    source = [banner, '#include "game_assets.h"', ""]
    for name, addr, size, desc in TABLES:
        data = image[addr - FLASH_BASE:addr - FLASH_BASE + size]
        header.append(f"/* 0x{addr:06x}: {desc} */")
        header.append(f"extern const uint8_t {name}[{size}];")
        source.append(c_array(name, data))
        source.append("")

    header += ["", '#include "font.h"', ""]
    for name, (height, baseline, glyphs) in zip(FONT_NAMES, extract_fonts(image)):
        columns = [c for glyph in glyphs for c in glyph]
        offsets, at = [], 0
        for glyph in glyphs:
            offsets.append(at)
            at += len(glyph)
        offsets.append(at)
        source.append(f"static const uint16_t {name}_columns[{len(columns)}] = {{")
        for i in range(0, len(columns), 10):
            source.append("    " + " ".join(f"0x{c:04x}," for c in columns[i:i + 10]))
        source.append("};")
        source.append(f"static const uint16_t {name}_offsets[{len(offsets)}] = {{")
        for i in range(0, len(offsets), 12):
            source.append("    " + " ".join(f"{o}," for o in offsets[i:i + 12]))
        source.append("};")
        source.append(f"const struct font {name} = {{ {height}, {baseline}, {name}_offsets, {name}_columns }};")
        source.append("")
        header.append(f"extern const struct font {name};")

    header.append("")
    strings = extract_strings(image)
    for name, index in STRINGS:
        source.append(f"const char {name}[] = {c_string(strings[index])};")
        header.append(f"extern const char {name}[];")
    source.append("")
    header += ["", "#endif", ""]

    (out_dir / "game_assets.h").write_text("\n".join(header))
    (out_dir / "game_assets.c").write_text("\n".join(source))
    print(f"wrote {out_dir}/game_assets.c and game_assets.h ({len(TABLES)} tables)")


if __name__ == "__main__":
    main()
