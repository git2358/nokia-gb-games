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

## Firmware policy

No Nokia firmware, no extracted code or data from it, and nothing derived
from a firmware image is committed here. Graphics and strings are read from
your own legally obtained dump at build time into an ignored directory.

## Layout

- `core/`: the portable game core (framebuffer and drawing primitives,
  `rand`, the event and platform interface in `core/game.h`).
- `platform/host/`: host layer used for verification; writes LCD frames as
  PGM in the layout of the fork's MAME frames.
- `tools/extract_assets.py`: reads the game graphics from your dump into C
  arrays under the ignored `build/assets/`.
- `tests/`: host checks that need no firmware.

The Game Boy and GBA layers will live in `platform/gb/` and `platform/gba/`.

## Building

```
make test                       # host checks, no firmware needed
make assets DUMP=/path/to/3210f600a.fls
make sheet                      # draw the extracted assets to build/sheet_*.pgm
```

`DUMP` defaults to `../nokia-dct3-re/roms/3210f600a.fls`. The dump must be
NSE-8/9 v6.00 (SHA-256 `7bf29b96…0d8a` raw, or the fork's `_swap16.bin`
form); the extractor refuses anything else.

## Status

Scaffolded: asset extraction and the core's drawing and `rand` services
build and are checked on the host. No game is implemented yet. See
[`docs/handoff.md`](docs/handoff.md) for the decisions taken, the reference
material and the work items.
