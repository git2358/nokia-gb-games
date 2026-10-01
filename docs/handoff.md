# Handoff: from firmware map to cartridge

Written 2026-10-01 at the end of the reverse-engineering phase. It records
what is decided, what the port can rely on, and what to do first.

## Decisions

1. **Re-implement in C; do not run the firmware's code.** The 3210's
   ARM7TDMI runs big-endian (the MAME driver instantiates `ARM7_BE`) and the
   game code depends on it: it copies four bytes into a word and extracts
   fields by shifting, reads 16-bit scores from byte records, and loads
   absolute RAM/ROM addresses from literal pools. The GBA is little-endian
   and the Game Boy is not ARM at all, so a shared C core is the only route
   that serves both targets.
2. **One portable core, two thin platform layers.** The firmware already has
   this shape: every game is a handler taking one event code. The core keeps
   that interface; each platform supplies input, a tick, a framebuffer blit,
   a beep and save storage.
3. **Assets come from the user's dump at build time.** Nothing derived from
   the firmware is committed.
4. **Verification is frame-exact against MAME.** The fork can run a scripted
   key sequence headlessly and save every LCD frame; the C core on the host
   must reproduce those frames before it is tried on hardware.

## What the core must reproduce

Event interface (one handler per game): `0x49` init, `0x53` resume, `0x54`
tick, `0x57` draw, key events as ASCII (`'1'..'9'`, `'*'`, `'#'`).

| Game | Keys | Notes |
|---|---|---|
| Snake | 2/4/6/8 | 4 px cells, ring of 2-bit directions, grow flag, food by `rand` |
| Memory | 2/4/6/8 move, 5 flip | 7x7 tiles, Fisher-Yates deal with `rand`, 1 or 2 players |
| Rotation | 2/4/6/8 move, rotate | number grid, 2x2 block below level 4, 3x3 ring above |
| Reaction (table entry 3) | 1..6 | six cells, typed targets, lives |
| Logic (undispatched) | 2/8 cycle, 4/5, `*` submit | Mastermind, 10 rows of 5 |

Services the games use, all small:

- `rand`/`srand`: ANSI LCG, `seed = seed * 0x41c64e6d + 0x3039`, result
  `(seed & 0x7fffffff) >> 16`.
- Tick: the handler re-arms itself. Snake's step is
  `floor(10 * speed[level] / 7.78125)` ticks of 7.78125 ms, speed table
  `66 48 38 30 23 18 14 11 9`: 653.6 ms at level 1, 85.6 ms at level 9.
  (MAME runs about 2.9 % faster; use the firmware's constant.)
- Drawing: `fill_rect`, `blit_bitmap` (column-major, bit `y & 7`), a 3x5
  digit renderer. LCD is 84x48, 1 bit.
- Settings: per game `{top score u16, level u8}`.
- A beep on events (food eaten, match found).

The menus (New game, Level, Top score, Instructions) are phone framework,
not game code. The port should draw its own simple menus rather than
reproduce the firmware's widget and menu layers.

## Screen and input mapping

- GBA 240x160: 84x48 at 2x is 168x96, centred. D-pad = 2/4/6/8, A = 5,
  B = back, Start = menu. The reaction game's six cells need a 3x2 mapping
  (for example D-pad left/up/right for the top row, B/down/A for the bottom).
- Game Boy 160x144: 84x48 at 1x leaves wide borders; 84 wide at 2x (168)
  does not fit. Options to decide: 1x with a phone-style bezel, or a 1.5x
  horizontal squeeze of the tile games only. Snake's 4 px cells map exactly
  onto half an 8x8 background tile, which makes a tile-based renderer
  attractive on the Game Boy.

## Reference material in the fork

Repository `~/git/nokia-dct3-re`, branch `games/re`:

- `docs/games_applications.md`: plugin table, events, settings records,
  timing, assets, per-function address tables, unnamed residue.
- `docs/data/games_function_notes.json`: evidence per function.
- `ghidra/symbols/3210.csv`: names.
- `make games-decomp`: decompiled C for every function in the games closure
  under the ignored `run_games/decomp/` (needs the local Ghidra project and
  your dump). This is the reading reference for the port.
- `make run-keys KEYS=... RUN_DIR=...`: headless MAME run with scripted keys;
  LCD frames land in the run directory as PGM. `tools/lcd_frame_sheet.py`
  makes contact sheets. `mame_nokia_dct3_force_game.lua` starts the reaction
  game; `mame_nokia_dct3_ram_probe.lua` taps or holds RAM bytes.
- Navigation: Menu (`enter`), `6` Games, `1` Rotation / `2` Snake /
  `3` Memory, `enter` on New game. Keys are accepted about 12 s after boot.

Asset addresses (v6.00 only): tiles `0x2d94bc` (7x7, 7 bytes each), card
back `0x2d96c4`, cursor `0x2d96cc`, digits `0x2d96ec` (3x5), speed table
`0x2d9738`, snake food `0x2d9744` (4x4), reaction sprites `0x2d995c`
(10x10) and scenes `0x2d9764` (84x48). These byte tables are in address
order in the raw `.fls`; the `_swap16.bin` image the static tools and Ghidra
use has each 16-bit pair swapped (see the fork's `roms/README.md`), so
`tools/extract_assets.py` swaps it back.

## First work items

1. `tools/extract_assets.py`: read the tables above from a dump (verify its
   hash first) and emit C arrays into an ignored `build/` directory.
2. Host build of the core with a PGM writer, starting with Snake: board,
   ring, check-move, tail, head, food, draw.
3. Golden frames: capture a deterministic Snake run in MAME (fixed keys, the
   seed read from RAM `0x11250c` at New game) and diff the host core's frames
   against it.
4. GBA layer (bare `arm-none-eabi-gcc`): mode 3 or 4 blit,
   input, timer-driven tick, SRAM settings.
5. Game Boy layer (SDCC `sm83`, no GBDK): tile renderer, input, timer tick,
   cartridge RAM settings.
6. Memory and Rotation, then the reaction game and Logic, each with the same
   golden-frame check.

## Open questions

- Game Boy layout for the 84-pixel-wide screen (see above).
- Whether to reproduce the phone's menu look or keep the port's menus plain.
- The reaction game's product name; `React` in the firmware's text block is
  the candidate but no code references it.
