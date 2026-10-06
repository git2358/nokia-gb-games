"""Space Impact's data, for extract_assets.py: written as si_data.c and
si_data.h.

The 3410's game keeps its levels ("chapters") in a file of the format the
phone also takes downloaded chapters in, at CHAPTER_FILE; the file is
parsed here into the tables the game's parser (0x35cf30) builds from it.
Its pictures are 24-byte descriptors {u16 width, u16 height, u16 0, u16 1,
u32 bitmap, ...} of bitmaps in the LCD's band layout, an animation's frames
24 bytes apart; they become struct sprite_image entries in one table, and
everything that names a picture names its index there. See the MAME fork's
docs/games_si_3410.md for all of it.
"""
import struct

FLASH_BASE = 0x200000

CHAPTER_FILE = (0x49F3E0, 0xC29)
TEMPLATES = 0x4ACE3C          # 20 bytes for each of the game's own types 0..19
PICTURE_TABLE = 0x405B88      # u32 descriptor address by type, 54 types
PHONE_TYPES = 0x4C1AE4        # {frames, type, side, boss} for types 20..53
TYPE_COUNT = 54
PATHS = (0x4AD094, 0x4AD37C)  # the y paths, signed bytes by x
# The game-over box's ends and digits: the start of what extract_assets.py
# takes as snake2_box, the game's own copy so that it needs nothing else.
BOX = (0x4B47D4, 84)
# Pictures named by the game besides those of its types.
NAMED = {
    "HEART": 0x490104,
    "DIGIT": (0x4901A4, 10),
    "ICON_MISSILE": 0x490134,
    "ICON_WALL": 0x49014C,
    "ICON_BEAM": 0x490164,
    "BAR_TOP": 0x4AC1D8,
    "BAR_BOTTOM": 0x4AC1A8,
    "STAR": 0x4917A8,
    "LOGO_TOP": 0x4ABD88,
    "LOGO_BOTTOM": 0x4ABDA0,
    "TITLE_SHIP": 0x4ABD70,
    "TITLE_ENEMY": 0x4ABD58,
    "SCORES_ENEMY": 0x490E74,
}


def parse_chapter_file(f):
    """The parts of a chapter file, as si_chapters_parse_35cf30 reads them."""
    p = 0
    n = f[p]
    p += 1 + 2 * n
    p += 4                  # u32
    p += 1                  # CF+0x48
    flags = f[p]
    p += 2
    if flags & 0x10:
        raise SystemExit("chapter file with pictures of its own: not supported")
    tile_sets = list(f[p + 1:p + 1 + f[p]])
    p += 1 + f[p]
    chapter_entries = list(f[p + 1:p + 1 + f[p]])
    p += 1 + f[p]
    ceiling, kills = struct.unpack_from("<HH", f, p)
    p += 4
    maps = [(f[p + 1 + 2 * i], f[p + 2 + 2 * i]) for i in range(f[p])]
    p += 1 + 2 * len(maps)
    count = len(chapter_entries)
    records = [f[p + 6 * i:p + 6 * i + 6] for i in range(count)]
    p += 6 * count
    tiles = f[p:p + 32 * sum(tile_sets)]
    p += len(tiles)
    map_data = f[p:p + sum(w * h for w, h in maps)]
    p += len(map_data)
    scripts = f[p:p + 9 * sum(chapter_entries)]
    p += len(scripts)
    settings = []
    if flags & 8:
        for i in range(count):
            s = bytearray(f[p + 9 * i:p + 9 * i + 9])
            s[3], s[4] = s[4], s[3]  # stored with the two swapped (0x35ce02)
            settings.append(bytes(s))
        p += 9 * count
    return dict(flags=flags, tile_sets=tile_sets, chapter_entries=chapter_entries, ceiling=ceiling,
                kills=kills, maps=maps, records=records, tiles=tiles, map_data=map_data, scripts=scripts,
                settings=settings)


def extract_si(at, c_bytes):
    """Returns (header lines, source text) for si_data.h and si_data.c."""
    def u32(addr):
        return struct.unpack(">I", at(addr, 4))[0]

    pictures = []   # (descriptor address, bitmap address, w, h)
    index = {}

    def picture(desc):
        if desc not in index:
            w, h = struct.unpack(">HH", at(desc, 4))
            index[desc] = len(pictures)
            pictures.append((desc, u32(desc + 8), w, h))
        return index[desc]

    def frames(desc, count):
        first = picture(desc)
        for i in range(1, count):
            assert picture(desc + 24 * i) == first + i
        return first

    templates = at(TEMPLATES, 20 * 20)
    phone_types = at(PHONE_TYPES, 4 * (TYPE_COUNT - 20))
    type_first, type_frames = [], []
    for t in range(TYPE_COUNT):
        desc = u32(PICTURE_TABLE + 4 * t)
        count = templates[20 * t] if t < 20 else phone_types[4 * (t - 20)]
        if not desc:
            type_first.append(0xFFFF)
            type_frames.append(0)
            continue
        type_first.append(frames(desc, count))
        type_frames.append(count)
    named = {}
    for name, where in NAMED.items():
        named[name] = frames(*where) if isinstance(where, tuple) else picture(where)

    bitmaps, places = bytearray(), {}
    for desc, bitmap, w, h in pictures:
        size = w * ((h + 7) // 8)
        if bitmap not in places:
            places[bitmap] = len(bitmaps)
            bitmaps += at(bitmap, size)

    cf = parse_chapter_file(at(*CHAPTER_FILE))
    count = len(cf["chapter_entries"])
    script_first, tile_first, map_first, total = [], [], [], 0
    for n in cf["chapter_entries"]:
        script_first.append(total)
        total += n
    total = 0
    for n in cf["tile_sets"]:
        tile_first.append(total)
        total += n
    total = 0
    for w, h in cf["maps"]:
        map_first.append(total)
        total += w * h

    header = [
        f"#define SI_TYPE_COUNT {TYPE_COUNT}",
        f"#define SI_CHAPTER_COUNT {count}",
        f"#define SI_CEILING_CHAPTERS 0x{cf['ceiling']:04x}",
        f"#define SI_KILLING_CHAPTERS 0x{cf['kills']:04x}",
        f"#define SI_PATHS_BASE 0x{PATHS[0]:06x}ul",
    ]
    for name, i in named.items():
        header.append(f"#define SI_PIC_{name} {i}")
    header += [
        f"extern const struct sprite_image si_pictures[{len(pictures)}];",
        f"extern const uint16_t si_type_picture[{TYPE_COUNT}];",
        f"extern const uint8_t si_type_frames[{TYPE_COUNT}];",
        "extern const uint8_t si_templates[400];",
        f"extern const uint8_t si_phone_types[{len(phone_types)}];",
        f"extern const uint8_t si_paths[{PATHS[1] - PATHS[0]}];",
        f"extern const uint8_t si_box[{BOX[1]}];",
        f"extern const uint8_t si_chapter_records[{6 * count}];",
        f"extern const uint8_t si_chapter_settings[{9 * count}];",
        f"extern const uint16_t si_script_first[{count}];",
        f"extern const uint8_t si_scripts[{len(cf['scripts'])}];",
        f"extern const uint8_t si_tiles[{len(cf['tiles'])}];",
        f"extern const uint16_t si_tile_first[{count}];",
        f"extern const uint8_t si_maps[{len(cf['map_data'])}];",
        f"extern const uint16_t si_map_first[{count}];",
        f"extern const uint8_t si_map_size[{2 * count}];",
    ]

    def words(name, values, ctype="uint16_t"):
        lines = [f"const {ctype} {name}[{len(values)}] = {{"]
        for i in range(0, len(values), 12):
            lines.append("    " + " ".join(f"{v}," for v in values[i:i + 12]))
        return "\n".join(lines + ["};"])

    source = [c_bytes("si_bitmaps", bitmaps).replace("const uint8_t", "static const uint8_t", 1)]
    source.append(f"const struct sprite_image si_pictures[{len(pictures)}] = {{")
    for desc, bitmap, w, h in pictures:
        source.append(f"    {{ si_bitmaps + {places[bitmap]}, {w}, {h} }}, /* 0x{desc:06x} */")
    source.append("};")
    source.append(words("si_type_picture", type_first))
    source.append(words("si_type_frames", type_frames, "uint8_t"))
    source.append(c_bytes("si_templates", templates))
    source.append(c_bytes("si_phone_types", phone_types))
    source.append(c_bytes("si_paths", at(PATHS[0], PATHS[1] - PATHS[0])))
    source.append(c_bytes("si_box", at(*BOX)))
    source.append(c_bytes("si_chapter_records", b"".join(cf["records"])))
    source.append(c_bytes("si_chapter_settings", b"".join(cf["settings"])))
    source.append(words("si_script_first", script_first))
    source.append(c_bytes("si_scripts", cf["scripts"]))
    source.append(c_bytes("si_tiles", cf["tiles"]))
    source.append(words("si_tile_first", tile_first[:count]))
    source.append(c_bytes("si_maps", cf["map_data"]))
    source.append(words("si_map_first", map_first[:count]))
    source.append(c_bytes("si_map_size", bytes(v for wh in cf["maps"][:count] for v in wh)))
    return header, "\n\n".join(source)
