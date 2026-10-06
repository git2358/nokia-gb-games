# One ROM for the three phones

A plan for a single `.gb` and a single `.gba` that hold all three ports.
The first screen picks the phone with up and down; A starts that phone just
as its own ROM starts today. Nothing here is built yet.

## The first screen

```
+--------------------------------+
|                                |
|          ^                     |
|     +---------+                |
|     |  NOKIA  |   Nokia 3310   |
|     | [84x48] |                |
|     |  phone  |   Snake II      |
|     +---------+   Space Impact |
|          v        Bantumi      |
|                   Pairs II     |
|                                |
|   A: start       up/down: phone|
+--------------------------------+
```

- Up and down step through the phones whose firmware was found at build
  time, wrapping round. With only one, the picker is skipped.
- Each phone is shown by its own Games icon frame (already extracted for
  the banners and the boot animation) inside a drawing of the phone, its
  name, and its list of games.
- A (or Start) starts the phone. The picker remembers the last phone and
  opens on it next time.
- A+B+Start+Select inside a phone returns to the picker (a soft reset into
  the launcher). Holding Select at power-on opens the picker even when a
  last phone is remembered and "start straight into it" is on.

The picker draws with Game Boy tiles and the GBA's tile modes directly. It
does not use any phone's LCD buffer, because the screens differ in size
(84x48 for the 3210 and 3310, 96x65 for the 3410).

## What stands in the way

**Names.** The ports use the same names for different things: `main`,
`menu_*`, `games_*`, `snake2_*`, `sound_*`, `platform_*`, and `lcd_*`
compiled at two sizes. The 3310 and 3410 builds share 117 global symbols,
and all three share 61. One link of the three would collide.

**Screen size.** `common/core/lcd.c` is compiled against each port's
`lcd.h`. So a 3410 `lcd_fill_rect` is a different function from a 3310
one, with the same name.

**Game Boy bank 0.** Only bank 0 is always mapped. Today each port fills
it with its own resident code:

| Port | Bank 0 (code + home) | Switchable banks | WRAM (`_DATA`) | Cart RAM |
|---|---|---|---|---|
| 3210 | 28.6 KB, no banking | none | 4.6 KB | 8 KB |
| 3310 | 15.1 KB | 6 (bank 1 to 6) | 7.3 KB | 8 KB (save + Space Impact data + game states) |
| 3410 | 10.1 KB | 2 | 6.5 KB | 8 KB (save + Snake II state) |

Together that is about 54 KB that wants to be resident, in a 16 KB bank.
WRAM is 18 KB against the DMG's 8 KB.

**Cart RAM.** Each port uses `0xa000` to `0xbfff` as if it owned it.

**Rumble.** On a rumble MBC5, bit 3 of the RAM bank register (`0x4000`)
is the motor. The 3310 and 3410 write it as `0x08` or `0`, which today
also means RAM bank 0.

## Game Boy: link each phone separately, share one bank 0

The ports would not be linked together at all. Each phone would be linked
as its own program against the same bank 0, and the banks would be spliced
into one ROM:

```
bank 0        launcher: crt0, vectors, picker, joypad, bank switch helpers,
              interrupt trampolines, the phone table      (same in every link)
bank 1..2     3210: its resident code, then its games
bank 3..9     3310: resident code (today's bank 0), then today's banks 1..6
bank 10..12   3410: resident code, then today's banks 1..2
bank 13..     the picker's pictures and text
```

That is about 14 banks, a 256 KB MBC5 ROM (`-yo 16`), rumble type `0x1e`.

- **Names stop mattering.** Each phone's link only sees its own code, so
  `menu_draw` can exist three times. Only the launcher's symbols are shared,
  and every phone's link gets them at the same addresses. The build checks
  that bank 0 came out byte-identical in all three links, and fails if it
  did not.
- **Entering a phone.** The first bytes of a phone's first bank are a jump
  table (`init`, `vblank`, `timer`, `lcd_stat`). The launcher maps that
  bank and jumps to `init`. The phone's main loop then runs from its own
  resident bank and never returns, except through the soft reset.
- **Calls within a phone.** Today the resident code is in bank 0 and calls
  the game banks through `far.c`. Now the resident code is itself in a
  switchable bank, so these calls go through a bank 0 trampoline that
  restores the caller's bank on return. SDCC's `__banked` calls already do
  this. The 3210's 28 KB of resident code is split between two banks the
  same way.
- **Interrupts.** The vectors are in bank 0, so they jump to bank 0 stubs
  that save the mapped bank, map the phone's resident bank, call the
  handler from its jump table, and restore. The VBlank, timer and LCD STAT
  handlers move into the phones as they are.
- **WRAM.** Only one phone runs at a time, so each phone's `_DATA` starts
  at the same address, just after the launcher's few hundred bytes. The
  largest (3310, 7.3 KB) still fits in 8 KB. This works only because the
  links are separate.
- **Cart RAM.** The combined ROM declares 32 KB (`-ya 4`, four 8 KB banks):
  bank 0 for the launcher (last phone, picker options), banks 1 to 3 for
  the 3210, 3310 and 3410. The launcher selects the phone's bank before
  `init`. Each phone keeps using `0xa000` to `0xbfff` unchanged, so saves
  and Space Impact's data never meet.
- **Rumble.** `platform_rumble` writes `ram_bank | 0x08` instead of `0x08`.
  This is the one change the separate ROMs need too, harmlessly, since their
  RAM bank is 0.
- **VRAM.** Each phone sets up its own tiles and maps in `init`, as it does
  at power-on today. The picker reloads its own on the way back.

## GBA: one link, each phone's names hidden

The GBA has room for everything at once (the 3310 is 85 KB of ROM and
11.5 KB of RAM), so one link works if the names are kept apart:

1. Each phone's objects (with its own copy of `common/` built at its screen
   size) are linked into one relocatable object (`ld -r`).
2. `objcopy --keep-global-symbol=phone_3310 ...` makes every other symbol
   local, so nothing collides.
3. The launcher, picker, libc and crt0 are linked once with the three
   objects.

Each phone exports one `const struct phone` with its name, its games' names,
its picker picture, and `init` / `frame` / `vblank` / `timer` entries. The
per-phone GBA `main.c` becomes that struct plus its loop body. Save memory
is the 64 KB of SRAM, split into fixed regions per phone, with
`platform_*_load/save` given a base offset.

The same `ld -r` + `objcopy` trick also builds a host version of the
combined ROM for the frame and replay checks.

## Building it

- `make combined` at the top level builds `nokia-phones.gb` and
  `nokia-phones.gba` from whichever phones' assets exist. A missing dump
  leaves that phone out of the picker; it is not an error.
- The separate ROMs stay as they are, and stay the reference: the menu and
  golden checks keep running on them. The combined build adds one check,
  that each phone entered from the picker draws the same first frames as
  its own ROM.
- The 3410 has no GBA platform layer yet. On the GBA the picker lists it
  once that exists.

## The common library, in order

What would move into `common/` next, each step useful on its own:

1. **Done:** the identical files (LCD buffer, font, test card, GBA linker
   script and libc, PGM writer, frame/ROM tools, mGBA setup).
2. **Game Boy platform:** the MBC registers, `far.c`, save, rumble and
   joypad code, once their differences are made into options. The
   launcher's bank 0 is built from these.
3. **The full-screen menus as pre-built tiles.** Each menu screen's text
   and pictures are rendered once (at build time where they are fixed, at
   entry where they are not) into Game Boy tiles and GBA tiles. Moving the
   selection then only rewrites a few tile map entries, instead of drawing
   into the LCD buffer and presenting it. This fixes the lag on fast
   presses in all three ports, and the picker is built the same way.
4. **The sprite engine and presenter**, shared by the 3310 and 3410.
5. **The GBA layer**, so the 3410 gets a `.gba`.
6. **The launcher and the combined build** described above.
