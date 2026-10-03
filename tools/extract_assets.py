#!/usr/bin/env python3
"""Extract the 3310 v6.39 Space Impact data, fonts and text from your own dump into C arrays.

Usage: extract_assets.py DUMP OUT_DIR

DUMP is the 2 MiB flash image `make dump` writes (3310f639e.fls), or the
16-bit byte-swapped form the static tools use; it is identified by hash.
Writes OUT_DIR/game_assets.h and five sources: si_data.c (Space Impact's
data region), si_tables.c (its small tables and the games' sounds),
snake2_data.c (Snake II's data region), title_data.c (the games' title
animations) and game_assets.c (the menus' fonts, text and pictures), apart
so that a platform can place them apart.
The output is derived from the firmware and must stay in an ignored
directory.

The game's data is one contiguous region of the image that refers to itself
by absolute address (sprite descriptors hold bitmap pointers, level headers
hold spawn list pointers). It is copied whole and the core reads it by
firmware address; see core/si_data.h and the map in the MAME fork's
docs/games_applications_3310.md.
"""
import hashlib
import struct
import sys
from pathlib import Path

FLASH_BASE = 0x200000
SHA256_RAW = "975ec791205f026d647254ee772d7fa32691fa50c72a68eecdaff7c8a5921442"

# Tilemaps, spawn lists, level headers, y paths, tiles, sprite bitmaps and
# descriptors, HUD icons and the object templates.
DATA_START, DATA_END = 0x311740, 0x313384
# Snake II's data: its bitmaps and their descriptors, the maze walls, the
# speeds and the maze records, referring to each other by absolute address
# as Space Impact's do; see core/snake2_data.h and the MAME fork's
# docs/games_snake2_3310.md.
SNAKE2_START, SNAKE2_END = 0x327D4C, 0x328168
# The four games' title animations: bitmaps, then the 12-byte descriptors
# that point at them (see core/title.c).
TITLE_START, TITLE_END = 0x317CA8, 0x318F8C
# Initialised-data images of tables the firmware keeps in RAM.
DIGIT_GLYPHS = (0x2F2320, 40)  # ten 4x5 digits, 4 bytes each
LEVEL_TABLE = (0x2F30D4, 8)  # pointers to the level headers
TYPE_FRAMES = (0x2F3108, 37)  # per object type, pointer to its sprite descriptors

# The phone's sounds, by id: 8-byte records that start with the address of
# a tone script. The games' are a zero byte, the command 9, pairs of note
# and length, and the command 11; a note of 0x40 is a rest, and the
# commands 5 n ... 6 play what they enclose n times.
SOUND_TABLE = 0x321E6C
SOUND_IDS = [0x17, 0x18, 0x19, 0x1A, 0x1F, 0x20, 0x22]  # the games' SOUND_ codes
NOTE_FIRST, NOTE_LAST = 0x7C, 0xAB  # 440 Hz and 47 semitones above it
NOTE_REST = 0x40
SCRIPT_END, SCRIPT_REPEAT, SCRIPT_AGAIN = 0x0B, 0x05, 0x06
SOUND_REST = 0xFE  # core/sound.h

# Pictures the phone's menus use, as strips of 8 rows with a byte per column.
PICTURES = [
    ("menu_games_icon", 0x2F6C68, 128, "64x16 frame of the main menu's Games animation, 64 bytes per 8 rows"),
    ("top_score_sparkle", 0x2FAA40, 11 * 84, "11 pictures of the Top score page's animation, 21x32, 21 bytes per 8 rows"),
    ("done_tick", 0x2F9E50, 3 * 88, "3 pictures of the Done page's animation, 22x32, 22 bytes per 8 rows"),
]

# The language pack ("PPM") holds the fonts and the text. Each chunk is
# {u32 checksum, u32 length, 4-byte name, ...}.
FONT_CHUNK = 0x34041C - 8
TEXT_CHUNK = 0x342984 - 8
ENGLISH_BLOCK = 0x2B4  # offset of the ENGL block inside the TEXT chunk

FONT_NAMES = ["font_large_bold", "font_small_plain", "font_small_bold", "font_tiny_plain"]
FIRST_CHAR, LAST_CHAR = 0x20, 0x7E

# English text by index in the language pack's string table.
STRINGS = [
    ("text_select", 360),
    ("text_more", 1002),
    ("text_top_score_value", 537),
    ("text_bantumi", 545),
    ("text_games", 549),
    ("text_continue", 552),
    ("text_top_score", 553),
    ("text_instructions", 554),
    ("text_new_game", 560),
    ("text_settings", 562),
    ("text_sounds", 563),
    ("text_lights", 557),
    ("text_shakes", 782),
    ("text_off", 564),
    ("text_on", 565),
    ("text_ok", 1004),
    ("text_club_nokia_id", 572),
    ("text_no_id", 569),
    ("text_done", 495),
    ("text_pairs", 566),
    ("text_snake", 574),
    ("text_space_impact", 575),
    ("text_game_over_top_score", 576),
    ("text_game_over_score", 578),
    ("text_help_space_impact", 1332),
    ("text_help_snake", 1331),
    ("text_level", 556),
    ("text_level_value", 538),
    ("text_mazes", 533),
    ("text_no_maze", 544),
    ("text_maze_1", 539),
    ("text_maze_2", 540),
    ("text_maze_3", 541),
    ("text_maze_4", 542),
    ("text_maze_5", 543),
    ("text_maze_selected", 546),
]


def swap16(data):
    out = bytearray(data)
    out[0::2], out[1::2] = data[1::2], data[0::2]
    return bytes(out)


def load_image(path):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() == SHA256_RAW:
        return data
    if hashlib.sha256(swap16(data)).hexdigest() == SHA256_RAW:
        return swap16(data)
    sys.exit(f"{path} is not the NHM-5 v6.39 PPM E image (SHA-256 {SHA256_RAW})")


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


def c_bytes(name, data):
    lines = [f"const uint8_t {name}[{len(data)}] = {{"]
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 16]) + ",")
    lines.append("};")
    return "\n".join(lines)


def c_refs(name, pointers):
    """Pointers into the copied region as 16-bit places in it; null is 0xffff."""
    refs = [p - DATA_START if p else 0xFFFF for p in pointers]
    lines = [f"const uint16_t {name}[{len(refs)}] = {{"]
    for i in range(0, len(refs), 8):
        lines.append("    " + ", ".join(f"0x{r:04x}" for r in refs[i:i + 8]) + ",")
    lines.append("};")
    return "\n".join(lines)


def extract_sounds(image):
    """Returns the games' sounds as one run of bytes and, by id less the
    first id, each sound's place in it (0xff for an id that is not the
    games'). A sound is pairs of note, in semitones above 440 Hz or
    SOUND_REST, and length in timer units, ended by 0xff. Repeats are
    written out, and notes of one pitch in a row are made one, as the
    phone's buzzer sounds them."""
    scripts, places = bytearray(), [0xFF] * (SOUND_IDS[-1] - SOUND_IDS[0] + 1)
    for sound in SOUND_IDS:
        record = SOUND_TABLE + sound * 8 - FLASH_BASE
        at = int.from_bytes(image[record:record + 4], "big") - FLASH_BASE
        if image[at:at + 2] != b"\x00\x09":
            sys.exit(f"sound {sound:#x} is not a plain run of notes")
        at += 2
        steps, repeat = [], None
        while image[at] != SCRIPT_END:
            if image[at] == SCRIPT_REPEAT:
                repeat = (len(steps), image[at + 1])
                at += 2
                continue
            if image[at] == SCRIPT_AGAIN and repeat:
                first, times = repeat
                steps += steps[first:] * (times - 1)
                repeat = None
                at += 1
                continue
            note, length = image[at], image[at + 1]
            if note == NOTE_REST and length:
                steps.append((SOUND_REST, length))
            elif NOTE_FIRST <= note <= NOTE_LAST and length:
                steps.append((note - NOTE_FIRST, length))
            else:
                sys.exit(f"sound {sound:#x} has something other than a note at 0x{at + FLASH_BASE:06x}")
            at += 2
        notes = []
        for note, length in steps:
            if notes and notes[-1][0] == note and notes[-1][1] + length < 256:
                notes[-1][1] += length
            else:
                notes.append([note, length])
        places[sound - SOUND_IDS[0]] = len(scripts)
        scripts += bytes(b for note in notes for b in note) + b"\xff"
    return bytes(scripts), bytes(places)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    image = load_image(Path(sys.argv[1]))
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)

    def at(addr, size):
        return image[addr - FLASH_BASE:addr - FLASH_BASE + size]

    def words(addr, count):
        return [int.from_bytes(at(addr + 4 * i, 4), "big") for i in range(count)]

    data = at(DATA_START, DATA_END - DATA_START)
    snake2 = at(SNAKE2_START, SNAKE2_END - SNAKE2_START)
    titles = at(TITLE_START, TITLE_END - TITLE_START)
    levels = words(*LEVEL_TABLE)
    frames = words(*TYPE_FRAMES)
    sounds, sound_places = extract_sounds(image)
    for pointer in levels + [f for f in frames if f]:
        if not DATA_START <= pointer < DATA_END:
            sys.exit(f"pointer {pointer:#x} is outside the copied region")

    header = f"""/* Generated by tools/extract_assets.py from the firmware dump. Do not commit. */
#ifndef GAME_ASSETS_H
#define GAME_ASSETS_H

#include <stdint.h>

#define TITLE_DATA_BASE 0x{TITLE_START:06x}ul
#define TITLE_DATA_SIZE {len(titles)}
extern const uint8_t title_data[TITLE_DATA_SIZE];

#define SNAKE2_DATA_BASE 0x{SNAKE2_START:06x}ul
#define SNAKE2_DATA_SIZE {len(snake2)}
extern const uint8_t snake2_data[SNAKE2_DATA_SIZE];

#define SI_DATA_BASE 0x{DATA_START:06x}ul
#define SI_DATA_SIZE {len(data)}
#define SI_TYPE_COUNT {len(frames)}
#define SI_LEVEL_COUNT {len(levels)}

extern const uint8_t si_data[SI_DATA_SIZE];
extern const uint8_t si_digit_glyphs[{DIGIT_GLYPHS[1]}];
extern const uint16_t si_level_table[SI_LEVEL_COUNT];
extern const uint16_t si_type_frames[SI_TYPE_COUNT];
/* The game's sounds: pairs of note, in semitones above 440 Hz, and length
   in the phone's timer units, each sound ended by 0xff; and by sound code
   less SI_SOUND_FIRST, where each starts. */
#define SI_SOUND_FIRST 0x{SOUND_IDS[0]:02x}
extern const uint8_t si_sounds[{len(sounds)}];
extern const uint8_t si_sound_places[{len(sound_places)}];
"""
    banner = '/* Generated by tools/extract_assets.py from the firmware dump. Do not commit. */\n#include "game_assets.h"'
    (out / "si_data.c").write_text("\n\n".join([banner, c_bytes("si_data", data)]) + "\n")
    (out / "title_data.c").write_text("\n\n".join([banner, c_bytes("title_data", titles)]) + "\n")
    (out / "snake2_data.c").write_text("\n\n".join([banner, c_bytes("snake2_data", snake2)]) + "\n")
    (out / "si_tables.c").write_text("\n\n".join([
        banner,
        c_bytes("si_digit_glyphs", at(*DIGIT_GLYPHS)),
        c_refs("si_level_table", levels),
        c_refs("si_type_frames", frames),
        c_bytes("si_sounds", sounds),
        c_bytes("si_sound_places", sound_places),
    ]) + "\n")
    source = [banner, ""]
    header = [header]
    for name, addr, size, desc in PICTURES:
        header.append(f"/* 0x{addr:06x}: {desc} */")
        header.append(f"extern const uint8_t {name}[{size}];")
        source.append(c_bytes(name, at(addr, size)))
        source.append("")

    header += ["", '#include "font.h"', ""]
    for name, (height, baseline, glyphs) in zip(FONT_NAMES, extract_fonts(image)):
        columns = [c for glyph in glyphs for c in glyph]
        offsets, place = [], 0
        for glyph in glyphs:
            offsets.append(place)
            place += len(glyph)
        offsets.append(place)
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

    (out / "game_assets.h").write_text("\n".join(header))
    (out / "game_assets.c").write_text("\n".join(source))
    print(f"wrote {out}/game_assets.c ({len(data)} bytes of game data)")


if __name__ == "__main__":
    main()
