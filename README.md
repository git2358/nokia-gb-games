# Nokia 3310 games for Game Boy and Game Boy Advance

The Nokia 3310 (NHM-5 v6.39) follow-up to
<https://github.com/lukesau/nokia-3210-games>: Space Impact re-implemented
in C from a map of the firmware, with the levels, sprites and object
tables read from your own dump at build time. It runs on the host and as a
GBA cartridge that shows the phone's 84x48 screen at 2x.

What is there so far:

- the sprite and scrolling-terrain layer the 3310's games draw with;
- the player, shots, the three special weapons, the level scripts, the
  movement patterns, collisions, scoring, lives and continues;
- the bosses of all eight levels, so the game can be played to its end.

What is not: there is no sound, no title, menu, game-over page or top
score (when a game ends the screen stops until Start begins another); and
there is no Game Boy build.

## Firmware policy

No Nokia firmware, no extracted code or data from it, and nothing derived
from a firmware image is committed here. `roms/`, run output and build
output are ignored.

That being said I got my firmware at firmware center
<https://firmware.center/firmware/Nokia/3310%20(NHM-5)/Flash%20Files/NHM-5%20v.06.39%203310.rar>

## Dump

Put the Wintesla flash files of NHM-5 v6.39 in `roms/3310-nhm5-v639/`:

| file | SHA-256 |
|---|---|
| `NHM5NY06.390` (MCU) | `c3710f27f68472a70cca0b2af1f8f5048fa435d40d99a36b0f69ea3ab598ea5f` |
| `NHM5NY06.39E` (PPM E) | `3cbdb4e75289016cc4e4ccf7013bba9f7e591d5a27b381f7ad98d90bcbdaa5d9` |
| `v.2.pmm` | `b825400f99a9bd3eba161fc97915084cef8286215796f8792f0523d06f36945f` |

`make dump` strips their record headers and lays the three regions out at
their flash addresses (MCU `0x200000`, PPM `0x340000`, PMM `0x3d0000`):

| output in `roms/noki3310/` | size | SHA-256 |
|---|---:|---|
| `3310f639e.fls` | `0x200000` | `975ec791205f026d647254ee772d7fa32691fa50c72a68eecdaff7c8a5921442` |
| `3310 v2 pmm.bin` | `0x30000` | `dcb2212579f2a2a7059ed85ef81174d337003566ce2f83f284f20bc70aef8bf4` |

These are the files the `noki3310` driver of the MAME fork
<https://github.com/lukesau/nokia-dct3-re> declares as BIOS `639`. The
extractor and the emulator are the fork's; it is expected next to this
directory (`DCT3_RE=../nokia-dct3-re`) with its MAME already built.

## Running the phone

```
make phone                                   # headless, 15 emulated seconds
make phone PHONE_SECONDS=13 PHONE_KEYS=enter,wait1000,enter
make phone-window                            # a MAME window
```

`make phone` leaves every LCD frame as PGM in the ignored `run_phone/` and
the last one as `run_phone/latest.png`. PPM E starts in Russian.

## Building

```
make test          # core checks, no firmware needed
make assets        # extract the game data from the dump into build/assets/
make sheet         # the extracted sprites and tiles as build/sheet_*.pgm
make gba           # build/nokia3310.gba (needs arm-none-eabi-gcc)
make run-gba       # open it in mGBA
```

On the GBA the D-pad moves the ship, A fires, B uses the special weapon and
Start begins a new game. The phone moves the ship with 8, 0, * and #, fires
with 1 or 3 and uses the special with 4 or 6, one key at a time; the pad
is mapped onto those keys, so only the button pressed last counts.

`make check-gba` and `make shot-gba` run the ROM headlessly in mGBA's core
(`scripts/setup-mgba.sh` builds it; `MGBA=` names an existing build).

## Golden run

The core is checked against the firmware itself. `make golden` plays a
scripted game in MAME (`GOLDEN_KEYS` in the `Makefile`), keeps every LCD
frame and logs every event the firmware hands its Space Impact
(`tools/mame_event_log.lua`). `make check-golden` feeds the same events to
the core and requires every picture it draws to appear, in order, among
MAME's.

The recorded run covers the first 20 seconds of level 1: the shielded
start, the first waves, shots, a missile, kills and score. It reaches no
terrain, enemy fire, boss or level change, so those parts of the core
follow the firmware's code but have not been compared with it running.
The bosses were only checked by a bot that plays each level through on the
host.

The frames and the event log are derived from the firmware and stay in the
ignored `golden/`.

## Layout

- `core/` is portable C: `lcd` (framebuffer), `sprite` (sprite list and
  tile layer), `si` (the game), `games` (keys and timers to game events),
  `rand`.
- `platform/host/` has the tools above; `platform/gba/` is the cartridge.
- `tools/extract_assets.py` copies the game's data region out of the dump;
  the core reads it by firmware address.
- The firmware map the core follows is `docs/games_applications_3310.md`
  in the MAME fork.

`tools/export_fonts.py`, `tools/check_golden.py` and the Game Boy tools are
unchanged copies from the 3210 project and are not used yet.
