# Plan

Where the 3410 port starts, from the static survey in the RE fork
(`docs/games_survey_3410.md`, reproduced by `tools/games_compare_3410.py`).
Nothing here has been run in MAME yet.

## Games

| Game | Kind | Starting point |
|---|---|---|
| Space Impact | native, adapted from the 3310 | 3310 port's code; new y-paths, level data and title art from the 3410 image; logic to be checked function by function |
| Snake | native, adapted from the 3310 (Snake II) | 3310 port's code; 23 x 13 board, maze table at `0x4973b8` in a 4-byte wall format |
| Bantumi | native, adapted from the 3310 | 3310 mapping; pit table at `0x4beccc`; title pictures unchanged |
| Link5 | native, new | needs a full map |
| Munkiki's Castles | Java (MIDP, Nokia UI API) | decompile the JAR, port by hand; no Java VM on the console |

Pairs II is not on the 3410.

## Screen

96 x 65. The Game Boy (160 x 144) shows it whole at 1x; at 2x 16 columns
do not fit. The GBA (240 x 160) shows it whole at 2x (192 x 130).

## Steps

1. Map the 3410 games' entry points in Ghidra (no 3410 symbols exist yet;
   the 3310 symbols and the shared table formats are the guide).
2. Confirm the survey's tables in MAME and measure tick timing, as was done
   for the 3310.
3. Copy the 3310 port's core and platform layers and make the screen size a
   parameter.
4. Port Snake and Bantumi first (closest to the 3310), then Space Impact,
   then Link5 and Munkiki's Castles.

## Known setup issue

The RE fork's local MAME tree is behind its patch list, so `make overlay`
and anything that rebuilds MAME fail. Runs work with `make run-prebuilt`
after copying `roms/noki3410/` into `mame/roms/noki3410/`.
