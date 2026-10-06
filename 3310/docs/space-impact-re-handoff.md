# Handoff: find and understand Space Impact in the 3310 firmware

Written 2026-10-01 for a session working in `~/git/nokia-dct3-re` (fork
<https://github.com/lukesau/nokia-dct3-re>). It is the 3310 counterpart of
the 3210 games mapping on branch `games/re`.

## Goal

A function-level map of Space Impact in Nokia 3310 NHM-5 v6.39, good enough
to re-implement the game in C for `~/git/nokia-3310-games` (Game Boy and GBA
cartridges, same approach as `~/git/nokia-3210-games`) and to check that
re-implementation frame for frame against the firmware in MAME.

The deliverable is the 3310 equivalent of `docs/games_applications.md`,
restricted to Space Impact and the framework it needs:

- where the game is dispatched from and its event interface;
- state layout in RAM;
- the tick: period, how it is scheduled, what one tick does;
- player movement, shots, the special weapons and their counts, lives;
- level structure: scrolling, enemy spawn scripts, enemy movement patterns,
  bosses, the level-end condition, how many levels;
- collision rules and scoring;
- every graphic and table, with address, size and format;
- sounds;
- the settings record (top score) and where it is stored;
- any use of `rand`, with the seed's RAM address, for deterministic runs.

Snake II, Pairs II and Bantumi are out of scope except where they share code
with Space Impact. Record what is learned about the shared layer; do not map
the other games.

## Rules

- Nothing derived from the firmware is committed: no image, no decompiled
  text, no extracted graphics. Names, addresses, formats and your own notes
  are fine. Decompile output stays under the ignored `run_*/` directories.
- Addresses are for this image only. State the image hash in the document.
- Mark each conclusion as static (read from code), runtime (seen in MAME) or
  inferred. The 3210 document does this; follow it.
- Do not change the MAME driver for this work unless a game cannot run
  without it. If so, keep that change on its own branch as before
  (`fix/...`), separate from the mapping.

## The image

`~/git/nokia-3310-games/roms/noki3310/3310f639e.fls`, 2 MiB, flash base
`0x200000`:

| | |
|---|---|
| SHA-256 | `975ec791205f026d647254ee772d7fa32691fa50c72a68eecdaff7c8a5921442` |
| SHA-1 | `d5da65f417595200314eb0115bf46ca1fbf53128` |
| MCU | `0x200000..0x33ffff` |
| PPM E | `0x340000..0x3cffff` |
| PMM | `0x3d0000..0x3fffff` (from `v.2.pmm`) |

It is rebuilt with `make dump` in `nokia-3310-games` from the Wintesla files
in `roms/3310-nhm5-v639/`, and is the file the fork's `noki3310` driver
declares as BIOS `639`. A copy and its PMM are already installed in the
fork's `mame/roms/noki3310/`.

Byte order: this file is in the same form as the 3210's raw `.fls` (the
reset code at `0x200040` reads `e3 a0 16 02` in file order). The fork's
static tools and the 3210 Ghidra project use the pair-swapped form, so make
one first:

```
mkdir -p roms/noki3310
cp ~/git/nokia-3310-games/roms/noki3310/3310f639e.fls roms/noki3310/
make swap16 ROM=roms/noki3310/3310f639e.fls SWAP=roms/3310f639e_swap16.bin
```

## What is already known

Checked on 2026-10-01:

- The image boots in the fork's MAME to the idle screen, and the Menu key
  opens the first main-menu entry (Phone book):
  `make run-prebuilt PHONE=noki3310 BIOS=639 RUN_DIR=... SECONDS=13
  RUN_ENV='NOKIA_DCT3_POST_READY_KEYS=enter,wait1000,enter
  NOKIA_DCT3_POST_READY_KEY_DELAY_MS=6000
  NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200
  NOKIA_DCT3_POST_READY_KEY_GAP_MS=200
  NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1200'`.
  `nokia-3310-games` wraps this as `make phone PHONE_KEYS=...`.
- PPM E comes up in Russian. The English text is in the image too.
- The English games text block is ASCII at about `0x344b00..0x344cc0` in the
  PPM. It contains, in order, among others: `Maze 1`..`Maze 5`, `No maze`,
  `Bantumi`, `Time trial`, `Puzzle`, `Games`, `1 player`, `2 players`,
  `Continue`, `Top score`, `Instructions`, `Last view`, `Pairs`,
  `Club Nokia ID`, `Snake II` (`0x344c40`), `Space impact` (`0x344c48`),
  then several `Game over!` messages with `%N` score placeholders.
- UTF-16BE copies of the game names sit at `0x34b6da`, `0x3556f8` and
  `0x35fb98` (`Space`), presumably one per language table.
- `Snake` also appears as ASCII in the MCU at `0x2e1021`. Not looked at.

Not known yet, and not to be assumed from the 3210:

- whether the games use the same plugin table and event codes (`0x49` init,
  `0x53` resume, `0x54` tick, `0x57` draw, ASCII keys) as on the 3210;
- whether Space Impact runs in the emulator at all. The 3310 gates in the
  fork stop at idle, menu and Phone book navigation; nobody has opened
  Games on this image;
- how the PPM text is referenced. On the 3210 the game code does not hold
  string addresses; it uses text IDs, so the strings above will not have
  direct code references. Do not spend long looking for them.

## Suggested order of work

1. **Branch.** `games/re-3310` off `games/re`, so the 3210 games tooling is
   there.
2. **Reach the game in MAME.** Work out the key path from idle to Games and
   to Space Impact (the 3310's Games is main-menu entry 9; the menu wraps, so
   `up` from the first entry may be shorter than `down`). Use scripted keys
   and the LCD frames; `tools/lcd_frame_sheet.py` makes contact sheets.
   Switching the phone to English first makes the frames easier to read but
   costs keys on every run, since NVRAM is reset per run unless
   `PRESERVE_NVRAM=1`. If Games does not open or the game does not start,
   that is the first finding: say what happens and stop to decide, as the
   3210 needed an NV provisioning fix before its games ran.
3. **Find the code from the running game.** With the game on screen, use
   `mame_nokia_dct3_coverage.lua` and `tools/coverage_diff.py` to diff the
   code executed while playing against the code executed while idle in the
   menu. That gives the game's handler and its callees without needing
   string references. `mame_nokia_dct3_ram_probe.lua` then finds the state
   block by tapping RAM that changes with the ship's position.
4. **Find the dispatch.** Look for a table like the 3210's
   `game_table_2d9484` (12-byte records: Thumb handler, pointer, four
   parameter bytes). The 3310 has four offered games, so expect at least
   four records, possibly more, as the 3210 carried two games it never
   offered.
5. **Ghidra.** Import `roms/3310f639e_swap16.bin` as a new program
   (`GHIDRA_PROGRAM=3310f639e_swap16.bin`; a separate project
   `GHIDRA_PROJECT=nokia3310` keeps it apart from the 3210), ARM v4T
   big-endian at base `0x200000`, set up as the 3210 program was. Start a
   new `ghidra/symbols/3310.csv`.
6. **Parameterize the games tooling.** `games-entries`, `games-callgraph`,
   `games-decomp`, `games-worklist`, `games-packet`, `games-next` and the
   scripts behind them were written for the 3210: `SWAP`, `GAMES_INNER`
   (`0x240600-0x244000,0x2621c0-0x263500`), `ghidra/symbols/3210.csv`,
   `docs/data/games_function_notes.json` and `run-keys` (hardwired to
   `PHONE=noki3210` with the 3210's EEPROM guard) all need a product
   switch. Do this once the handler address from step 3 gives the inner
   range. Keep the 3210 behaviour unchanged and `make test-tools` passing.
7. **Map it.** Same loop as before: `make games-next`, name, note the
   evidence, repeat. Framework functions shared with the 3210 (LCD
   primitives, scheduler posting, `rand`, settings) can often be matched by
   shape against `ghidra/symbols/3210.csv`; note such matches as inferred
   until checked.
8. **Write `docs/games_applications_3310.md`** with the content listed
   under Goal, plus a navigation line and the timing of key acceptance
   after boot, as the 3210 handoff had.
9. **Golden frames.** A deterministic Space Impact run: fixed keys, the
   seed read from RAM if `rand` is used. Record the command; the frames go
   to `nokia-3310-games/golden/` (ignored), not into the fork.

## What the port will ask of this map

The questions that decided the 3210 port's design, to answer early:

- Is the game's logic tied to the 84x48 screen (scroll positions, spawn
  columns), and in what units does it scroll?
- Are levels data (tables of spawns and patterns) or code? Data can be read
  from the user's dump at build time; code has to be re-implemented.
- How large are the level and sprite tables in total? The Game Boy build
  has little RAM and banked ROM.
- Does anything depend on the big-endian packing the 3210 games used
  (bytes copied into a word and fields taken by shifting)? Describe the
  fields, not the packing.
- The exact tick length, and whether speed changes with level.

## References

- `docs/games_applications.md`, `docs/data/games_function_notes.json`,
  `ghidra/symbols/3210.csv`: the 3210 map and its conventions.
- `docs/tooling.md`, `docs/mmi_layer.md`, `docs/scheduler_delivery.md`: the
  shared layers the games sit on.
- `~/git/nokia-3210-games/docs/handoff.md`: what the previous map handed to
  the port, as a model for the end state.
- `~/git/nokia-3310-games/README.md`: the dump and the `make phone` targets.
