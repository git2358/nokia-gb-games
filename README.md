# Nokia 3410 games for Game Boy and Game Boy Advance

The Nokia 3410 (NHM-2 v5.46) follow-up to
<https://github.com/lukesau/nokia-3310-games>: the 3410's built-in games
re-implemented in C from a map of the firmware, with their data read from
your own dump at build time.

Nothing is ported yet. The 3410 has Space Impact, Snake and Bantumi, which
are the 3310's games adapted to the 3410's 96 x 65 screen (a bigger Snake
board with the mazes redrawn, a re-laid-out Bantumi board, taller Space
Impact paths), plus Link5 and one Java game, Munkiki's Castles. See
[docs/plan.md](docs/plan.md) and, in the RE fork,
`docs/games_survey_3410.md`.

## Firmware policy

No Nokia firmware, no extracted code or data from it, and nothing derived
from a firmware image is committed here. `roms/`, run output and build
output are ignored.

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
