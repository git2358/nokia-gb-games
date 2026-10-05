# Nokia 3410 games for Game Boy

The Nokia 3410 (NHM-2 v5.46) follow-up to
<https://github.com/lukesau/nokia-3310-games>: the 3410's built-in games
re-implemented in C from a map of the firmware, with their data read from
your own dump at build time. One portable core runs on the host and as a
Game Boy ROM (`.gb`); a GBA build is to come.

What is there so far:

- the phone's menus: the main menu's Games entry, Games (Select game and
  Settings; not the phone's Download game and More games), the Select game
  list with all five games, and Snake II's own menu with New game, High
  scores, Options (Game options: the six mazes and the nine levels) and
  Instructions, drawn with the phone's fonts and text. The Select game
  list, Snake II's menu, Game options, the mazes and the Level page equal
  the phone's to the pixel;
- Snake II: the 3410's game, the 3310's on a 23 by 13 board with the mazes
  redrawn, its creatures, scoring, speeds and the dead snake's blinking,
  frame for frame as the phone plays it;
- the games' Settings (Game sounds, Game lights, Shakes, Club Nokia ID),
  the vibrator as a rumble motor, and the 3310's eat and death sounds,
  which the 3410 holds too but which have not been checked against it yet;
- the top score and the chosen level and maze, kept in battery-backed
  cartridge RAM.

Not yet: the other four games, the title animations, the High scores
page's animation (a plain page for now), the picture the phone shows at
game over (a plain page with the score for now), the main menu's Games
icon and the lists' sliding highlight.

The game is shown at the phone's size, in the middle of the Game Boy's
screen. Start on the first screen picks the full-screen variant, whose
menus fill the screen; the game stays at the phone's size.

| | Menus | Snake II |
|---|---|---|
| D-pad | up and down | steer (2, 4, 6, 8) |
| A | select | turn clockwise (#) |
| B | back | turn anticlockwise (*) |
| Start, Select | select | pause |

See [docs/plan.md](docs/plan.md) and, in the RE fork,
`docs/games_applications_3410.md` and `docs/games_snake2_3410.md`.

## Building

`make dump` makes the flash image from the Wintesla files below (it needs
the RE fork, in whose `ports/` directory this repository is kept, or
`DCT3_RE=` pointing at it), `make gb` the ROM, `make run-gb` opens it in SameBoy. `make test` runs the host
checks; `make check-gb` runs the ROM headlessly against the host's frames.
`make golden-snake` records Snake II games in the fork's MAME and `make
check-golden` replays them through the core; `make check-menus` compares
the menu pages with the phone's in `golden/menus/`.

## Firmware policy

No Nokia firmware, no extracted code or data from it, and nothing derived
from a firmware image is committed here. `roms/`, run output and build
output are ignored.

That being said I got my firmware at firmware center
<https://firmware.center/firmware/Nokia/3410%20%28NHM-2%29/Flash%20Files/NHM-2%20v.05.46%203410.rar>

## Dump

Put the Wintesla flash files of NHM-2 v5.46 in `roms/3410-nhm2-v546/`:

| file | SHA-256 |
|---|---|
| `NHM2NX05.460` (MCU) | `713c6de148eabd907b3eefd4dd43637279b621eb2c94bed09385e9dc4094693f` |
| `NHM2NX05.46E` (PPM E) | `779b3c13726a5878a117eed9e91d021597bc4d74dedeb1dba26e9335df1ee633` |
| `3410 virgin eeprom.pmm` | `35cf261f9aacae40ad2b95f6646be2b07220e92c8b6c119f958c6bc5e929a632` |

For now the RE fork's `make normalize-3410` lays them out for MAME:

| output in `roms/noki3410/` | size | SHA-256 |
|---|---:|---|
| `3410f546e.fls` | `0x370000` | `4b0e815a07dc18b3b5f1cff1134de55aa514eaa7ce5a0eecc44a270867562b04` |
| `3410 virgin eeprom 005f0000.fls` | `0x90000` | `f00f6974a6c03846ed1109e55d76e336e55767c2954be4ea2ae36687260a5863` |

Munkiki's Castles is a JAR inside the PMM (40888 bytes, SHA-256
`e94f9c942968acfadd13e19c62fc0aa8e48f0758a2ec00f2a9ba42269433caf8`),
kept locally as `roms/munkikis_castles.jar`.

## License

GNU Affero General Public License v3.0, see [LICENSE](LICENSE).
