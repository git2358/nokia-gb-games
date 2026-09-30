# nokia-snake-gba

A Game Boy Advance cartridge wrapper for the built-in games of the Nokia 3210
(NSE-8/9 v6.00 firmware): Rotation, Snake and Memory.

The reverse engineering that locates and maps the games lives in a fork of
the upstream Nokia DCT3 MAME project, under its `games/` workspace:
<https://github.com/lukesau/nokia-dct3-re>. That fork owns the firmware
analysis, the MAME-based tracing harness and the symbol names. This
repository consumes the exported maps and builds the cartridge.

## Firmware policy

No Nokia firmware, no extracted code or data from it, and nothing derived
from a firmware image is committed here. Bring your own legally obtained
dump; the build reads it from an ignored location.

## Status

Scaffold only. See the fork's `games/README.md` for the current analysis
state, including the located Snake core, the per-game settings records and
the shared games framework.
