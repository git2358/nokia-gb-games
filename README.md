# Nokia 3210 games for Game Boy and Game Boy Advance

Ports of the Nokia 3210 (NSE-8/9 v6.00) built-in games to two cartridges
built from one portable C core:

- a Game Boy ROM (`.gb`), and
- a Game Boy Advance ROM (`.gba`).

Targets: Snake, Memory and Rotation as shipped, plus the two games the
firmware carries but never offers: the six-cell reaction game in its plugin
table and the Mastermind-style Logic.

The games are re-implemented from a function-level map of the firmware, not
copied from it. The map, the tracing tools and the evidence live in a fork of
the upstream Nokia DCT3 MAME project:
<https://github.com/lukesau/nokia-dct3-re>, branch `games/re`, document
`docs/games_applications.md`.

## Why a re-implementation

The original plan was to run the firmware's own game code directly on the
GBA behind a thin wrapper, since the 3210 and the GBA share the same ARM7TDMI
core. Endianness foiled it: the 3210 runs its ARM7TDMI big-endian and the
game code depends on that (it packs bytes into words and pulls fields out by
shifting, and reads 16-bit values from byte records), while the GBA is
little-endian. Re-implementing the games in C was the way out, and it also
made a Game Boy build possible.

## Firmware policy

No Nokia firmware, no extracted code or data from it, and nothing derived
from a firmware image is committed here. Graphics and strings are read from
your own legally obtained dump at build time into an ignored directory.

## Layout

- `core/`: the portable game core (framebuffer and drawing primitives, the
  phone's fonts and menus, `rand`, the event and platform interface in
  `core/game.h`, and a test card).
- `platform/host/`: host layer used for verification; writes LCD frames as
  PGM in the layout of the fork's MAME frames.
- `platform/gb/`: Game Boy layer (84x48 drawn 1:1 as background tiles).
- `platform/gba/`: GBA layer (84x48 drawn at 2x in bitmap mode 3), with its
  own startup code and linker script.
- `tools/`: `extract_assets.py` reads the game graphics from your dump into
  C arrays under the ignored `build/assets/`; `gbafix.py` finishes the GBA
  header; `check_gb_frame.py` compares an emulator screenshot with a host
  frame.
- `scripts/setup-sameboy.sh`: clones upstream SameBoy at a pinned commit
  into the ignored `tools/SameBoy/` and builds its headless tester.
- `tests/`: host checks that need no firmware.

## Toolchains

On macOS with Homebrew:

```
brew install sdcc rgbds arm-none-eabi-gcc
scripts/setup-sameboy.sh
```

- Game Boy: SDCC's `sm83` port with its stock startup code, and `makebin`
  for the cartridge header. No GBDK.
- GBA: bare `arm-none-eabi-gcc` with no C library; `platform/gba/` supplies
  the startup code, the linker script and `memset`/`memcpy`.
- Emulators: SameBoy's tester for headless Game Boy frames; `make run-gb`
  and `make run-gba` open the ROMs in SameBoy.app and mGBA.app.

## Building

```
make test                       # host checks, no firmware needed
make gb gba                     # build/nokia3210.gb and build/nokia3210.gba
make check-gb                   # run the .gb headlessly, compare with the host frame
make shot-gb KEYS=sd            # screenshot after menu keys (u, d, s select, b back)
make check-golden               # compare host frames with MAME frames in golden/
make assets DUMP=/path/to/3210f600a.fls
make sheet                      # draw the extracted assets to build/sheet_*.pgm
```

`DUMP` defaults to `../nokia-dct3-re/roms/3210f600a.fls`. The dump must be
NSE-8/9 v6.00 (SHA-256 `7bf29b96…0d8a` raw, or the fork's `_swap16.bin`
form); the extractor refuses anything else.

The GBA ROM is built without the boot logo, which a real console's BIOS
checks. To run on hardware, pass a GBA ROM you own to copy it from:
`make gba GBA_LOGO_FROM=/path/to/some.gba`.

## Status

In progress: the phone's menus. Both cartridges build; the Game Boy ROM
opens on the main menu's Games entry and walks the Games list and each
game's menu, drawn with the phone's own fonts and text and matching frames
captured from the firmware in MAME. The Level, Top score and Instructions
pages and the games themselves are not implemented yet (Snake's starting
position is). See
[`docs/handoff.md`](docs/handoff.md) for the decisions taken, the reference
material and the work items.
