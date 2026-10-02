# Handoff: Nokia 3210 fonts for the EZ Flash Jr kernel (ezgb)

For an agent working in `~/git/ezgb`. Written 2026-10-01 from the
`nokia-3210-games` side. It says what font data exists, where it is, what
format it is in, and what is known about fitting it into the kernel's 8px
text. It does not decide how to do that; the open questions are at the end.

## The goal

Try the Nokia 3210's small font as the kernel's 8px font: the stock 8x8
text the kernel draws everywhere the 12px UI is not switched on. The 3210's
menu font is 8 rows tall, which is why it is a candidate.

## What exists

`~/git/nokia-3210-games` extracts the phone's four fonts from a firmware
dump. Run this there to (re)generate the sheets:

```
make fonts
```

It writes, under that repo's ignored `build/fonts/`:

| File | Rows | Glyphs | Advance widths | Used on the phone for |
|---|---:|---:|---|---|
| `nokia3210_small_bold.txt` | 8 | 95 | 2..9 | menu entries, softkey labels |
| `nokia3210_small_plain.txt` | 8 | 95 | 2..8 | instruction text |
| `nokia3210_large_bold.txt` | 13 | 95 | 2..11 | titles, notes |
| `nokia3210_tiny_plain.txt` | 6 | 54 | 3..6 | the menu path digits |

It needs the dump at `~/git/nokia-dct3-re/roms/3210f600a.fls` (already
there) and nothing else. The files may not exist until `make fonts` has been
run; `build/` is wiped by `make clean`.

## Sheet format

The same shape as `decomp/font12/font12.txt` in ezgb, with one addition:

```
glyph 0x67 'g' width 6
.........
.........
.####....
##.##....
##.##....
.####....
...##....
.###.....
```

- `#` is ink, `.` is paper. One line per pixel row.
- Codes are 0x20 to 0x7E. A code the font has no glyph for is left out.
- Every glyph is left-aligned in a cell as wide as the font's widest glyph
  (9 columns for small bold, 8 for small plain).
- `width` is the glyph's advance: its ink plus the gap to the next
  character, which is already part of the glyph (the trailing blank column).
  These are proportional fonts; there is no kerning data.
- Both 8-row fonts have their baseline after row 6: capitals and
  ascenders use rows 0..6, descenders reach row 7.

`font12-pack.py` will not read these files as they are: it expects 10x12
cells and no `width` field.

## What is known about the kernel's 8px text

From ezgb's own docs (`docs/dmg-ui-visibility.md`, `docs/font12.md`):

- `FontGlyphSheet` at `00:3206` is a 256-entry 1bpp sheet, 8 bytes per code
  (`$3206 + code*8`), CP437-shaped with Hebrew letters in part of it.
- `DrawGlyph` (`00:2701`) is a pure 8x8 blitter: one glyph is one tile, so
  the stock text is fixed-width, 8 pixels a character.

So a drop-in replacement means rewriting 8-byte entries of that sheet for
the codes the Nokia font covers, leaving the rest alone.

## Fitting problems to expect

- **Width.** The Nokia fonts are proportional. In a fixed 8-pixel cell most
  glyphs (4 to 7 pixels including their gap) will sit left-aligned with
  uneven spacing after them, unless they are centred in the cell. The widest
  advances are 9 (small bold) and 8 (small plain); those glyphs fill or
  overflow the cell once the gap is counted. Check `W`, `M`, `m`, `w`, `@`.
  Dropping the trailing gap column makes every small plain glyph fit in 8
  columns; small bold needs checking glyph by glyph.
- **Coverage.** Only 0x20..0x7E come across. The kernel's sheet has 256
  entries; box-drawing and other codes it uses must keep their stock glyphs.
- **Case and digits.** Check that digits and capitals line up on the same
  rows as the stock glyphs wherever the kernel mixes text with tile art.
- **Row 7.** Descenders use the bottom row of the cell, so lines of text on
  adjacent tile rows touch. The stock font may leave that row blank.

## Licence and provenance

The glyphs are Nokia's, copied from the 3210's firmware. `nokia-3210-games`
does not commit them: they are read from the user's own dump at build time,
and the sheets sit in an ignored directory. ezgb is a public repository.
Whether to commit Nokia's font there, keep it as a local build input, or
redraw a look-alike is the owner's decision; ask before committing any of
this data.

## Open questions for the ezgb side

1. Which of the two 8-row fonts: small bold (the phone's menu look) or
   small plain (lighter, narrower)?
2. Fixed cells with the glyph centred or left-aligned, or a proportional
   8px renderer in the style of `draw12.c`?
3. Opt-in through the `UI:` setting like the 12px font, or a replacement
   for the stock sheet?
4. How the data gets into the build without committing Nokia's glyphs, if
   that is the decision.

## Pointers

- Extraction code and the font format in the firmware:
  `~/git/nokia-3210-games/tools/extract_assets.py` (`extract_fonts`).
- Sheet writer: `~/git/nokia-3210-games/tools/export_fonts.py`.
- How the phone uses each font: `~/git/nokia-3210-games/core/menu.c`.
