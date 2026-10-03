# Nokia 3310 games for Game Boy and Game Boy Advance

![The first screen, the list of games and Space Impact being played, on the Game Boy and the Game Boy Advance, each in the phone-sized mode and in full screen, with blank panels for the games not here yet: Snake II, Bantumi and Pairs II](docs/banner.png)

The Nokia 3310 (NHM-5 v6.39) follow-up to
<https://github.com/lukesau/nokia-3210-games>: Space Impact re-implemented
in C from a map of the firmware, behind the phone's own Games menus, with
the levels, sprites, fonts and text read from your own dump at build time.
One portable core runs on the host, as a Game Boy ROM (`.gb`) and as a
Game Boy Advance ROM (`.gba`).

What is there so far:

- the phone's menus: the main menu's Games entry, the list of games,
  Space Impact's menu with Continue while a game is paused, its Top score
  page with the animation, its Instructions and the Game over page, drawn
  with the phone's fonts and text;
- the sprite and scrolling-terrain layer the 3310's games draw with;
- the player, shots, the three special weapons, the level scripts, the
  movement patterns, collisions, scoring, lives and continues;
- the bosses of all eight levels, so the game can be played to its end;
- the top score, kept in battery-backed cartridge RAM. It starts at 0;
- the game's five buzzer sounds: the shot, the missile or wall, the beam,
  a bonus collected and the ship destroyed;
- the phone's vibrator, as a rumble motor: half a second when the ship is
  hit and when a boss explodes;
- the Games menu's Settings: the phone's four pages, Sounds, Lights, Shakes
  and Club Nokia ID, with their Off and On lists and the Done note. Sounds
  and Shakes do what they say; Lights is kept but does nothing, and the
  score ID has no ID. All are kept in cartridge RAM with the top score.

Start on the first screen picks a full-screen mode instead, as in the 3210
project: the port's own menus laid out for the console's whole screen in
the phone's large font. In that mode the game is shown at 2x on the Game
Boy and at 3x on the GBA, where the display hardware does the scaling.
Either way the phone's 84 columns are four too many for the screen's
width, so the last four are left off: nothing of the score, which ends at
column 75, but enemies come on four columns late and the ship can fly its
nose out of sight.

| | Menus | Space Impact |
|---|---|---|
| D-pad | up and down | move the ship |
| A | select | fire |
| B | back | special weapon |
| Start, Select | select | pause |

What is not: Snake II, Bantumi and Pairs II are in the list but do
nothing, and Space Impact's title animation is not shown. A paused game
continues exactly where it stopped, where the phone gives the ship a
second and a half of shield.

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

## Toolchains

On macOS with Homebrew:

```
brew install sdcc arm-none-eabi-gcc imagemagick
```

- Game Boy: SDCC's `sm83` port and `makebin`, with this project's own
  startup code. No GBDK.
- GBA: bare `arm-none-eabi-gcc` with no C library.
- Emulators, for the headless checks: SameBoy's core as a library
  (`scripts/setup-sameboy.sh`, needs rgbds) and mGBA's
  (`scripts/setup-mgba.sh`, needs cmake). `SAMEBOY=` and `MGBA=` name
  existing builds, such as the 3210 project's.

## Version

The first screen shows the port's version in its bottom right corner, and
the full-screen variant's list of games ends with an About entry that
shows it with this repository's address. Both come from `GAME_VERSION` in
`core/version.h`, set by hand; change the string there when a release
deserves a new number.

## Building

```
make test          # core checks, no firmware needed
make assets        # extract the game data, fonts and text from the dump into build/assets/
make sheet         # the extracted sprites and tiles as build/sheet_*.pgm
make gb gba        # build/nokia3310.gb and build/nokia3310.gba
make run-gb        # open the .gb in SameBoy
make run-gba       # open the .gba in mGBA
```

The GBA ROM is built without the boot logo, which a real console's BIOS
checks. To run on hardware, pass a GBA ROM you own to copy it from:
`make gba GBA_LOGO_FROM=/path/to/some.gba`.

`make cards` (`scripts/copy-to-cards.sh`) copies the ROMs to the root of
the flash carts' SD cards: the `.gb` to `/Volumes/EZGB_FW4` and
`/Volumes/EZGB_FW5`, the `.gba` to `/Volumes/OMEGADE`. It skips a card
that is not mounted, refuses a `.gba` without the boot logo, checks each
copy and ejects nothing.

The phone moves the ship with 8, 0, * and #, fires with 1 or 3 and uses
the special with 4 or 6, one key at a time; the pad is mapped onto those
keys, so only the button pressed last counts.

## Checks

```
make check-menus                 # the menu pages against the phone's own, to the pixel
make check-golden                # the game against a recorded run of the firmware (below)
make check-gb                    # the .gb in SameBoy against the host's frames, and its speed
make check-gb KEYS=a3sdss        # the same in the full-screen mode's 2x, starting at the fourth level
make check-gba                   # the .gba in mGBA against the host's frames
make shot-gb KEYS=sds            # a screenshot after scripted keys
```

`KEYS` are pressed by a test ROM at power-on (`menu_script` in
`core/menu.c` lists them); the ROMs `make gb` and `make gba` build press
none. `check-gb` also reports how fast the ROM ran the game, 100% being
the phone's pace.

`golden/menus/` holds frames of the phone's menus captured in MAME with
the phone switched to English (Menu, 6, 2, 1, up, Select), each named
after the host frame it must equal. The Top score page's two frames show
the 4075 the PMM dump holds and differ from the port's 0 by design.

The rumble is checked the same way: `build/gb_run ... rumble 1400` prints
each frame the motor was on for, and `build/gba_shot` prints every switch
of the motor. In a scripted game left to be hit, both show it on for
about 29 frames at a time.

## Game Boy

The Game Boy has a slow processor for this game, and the things the game
does every tick that the compiler makes too slow are the Game Boy's own,
in assembly (`platform/gb/draw.s`): putting a sprite into the picture,
finding what a shot has hit, and turning the picture into background
tiles, of which only those that changed are made again.

At 2x the terrain, which moves a column every tick from the second level
on, is not made into tiles at all (`platform/gb/strip.c`). Its rows of
the tile map hold tiles made once per level and the LCD scrolls just
those rows, between two cuts across the screen. Where anything else is
over the terrain, a ship or a shot, that cell gets a tile of its own made
from the picture, so what is shown is still the phone's picture to the
pixel; `make check-gb` holds it to that.

When a tick still takes longer to show than the 93 ms between ticks, the
game keeps the phone's pace and shows fewer pictures: every tick is
played, not every one is drawn.

The ROM is four 16 KiB banks on an MBC5 with 8 KiB of battery-backed RAM
(`platform/gb/far.h` says what is where); MBC5 for its rumble pin, see
below. Space Impact's 7 KiB of data is
copied from the ROM to cartridge RAM at power-on, because both the game
and the sprite code, which are in different banks, read it. The save file
therefore holds a copy of that data next to the top score.

## Sound

The sounds are read from the dump with the rest: each is a short run of
notes for the phone's buzzer, a pitch and a length in the units of the
phone's timers, 7.8 ms. The notes are too short to be timed by screen
frames, two units for most, so both consoles keep a timer that interrupts
once a unit, and play the notes as a square wave on their second pulse
channel. A sound that starts while another plays takes its place, as on
the phone.

The pitches are the phone's, 440 Hz to 4186 Hz, as near as the pulse
channel's frequency register comes: within 3 Hz up to 1 kHz and within
1.2% above, 4228 Hz for the highest. The notes were compared with a trace
of the firmware's writes to the buzzer in MAME (the fork's
`docs/games_applications_3310.md`, Sounds) by recording the ROMs:

```
build/gb_run build/nokia3310-keys.gb BOOT_ROM 400 audio:shot.raw +a 4 -a 40
GBA_SHOT_AUDIO=shot.raw build/gba_shot build/nokia3310-keys.gba shot.bmp 360 0x1 300 304
```

Both write the left channel as raw signed 16-bit samples at 32768 Hz.
The phone plays no sound until Sounds in the games' settings and Warning
and game tones in the profile are both on; here Sounds starts on and is
enough.

## Rumble

Space Impact pulses the phone's vibrator when the ship is hit and when a
boss explodes, for 62 of the phone's timer units, half a second, when
Shakes in the games' settings is on; a hit during a pulse starts the half
second over. The port does the same with whatever motor the cartridge has:

- on the GBA, through the cartridge's general-purpose port as the rumble
  cartridges (Drill Dozer, WarioWare: Twisted!) use it: pin 3 of the
  registers at `0x80000c4`, left clear of code in the ROM header's tail.
  mGBA gives a ROM that port when its game code is a known rumble
  cartridge's, or through an override, `[override.N33E]` with
  `hardware=2` in its `config.ini`; `build/gba_shot` sets it up itself. The EZ-Flash Omega Definitive Edition has a motor and a Mode B
  setting of Rumble in its menu for cartridges that drive it this way;
  that is how the ROM is meant to rumble there, and has not been tried
  on the cart yet;
- on the Game Boy, through the MBC5's rumble pin, bit 3 of the RAM bank
  register, which is why the header now says MBC5. SameBoy's cartridge
  rumble follows it; the EZ-Flash Junior has no motor.

Shakes starts on, as on the phone.

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

- `core/` is portable C: `lcd` (framebuffer), `font`, `menu` (the phone's
  menus, its Settings pages and the full-screen ones), `sprite` (sprite
  list and tile layer), `si`, `si_setup` and `si_base` (the game), `games`
  (keys and timers to game events, the settings the games follow and the
  vibrator's timer), `sound` (the notes of the sound in progress), `rand`.
- `platform/host/` has the tools above; `platform/gb/` and `platform/gba/`
  are the cartridges.
- `tools/extract_assets.py` copies the game's data region, its sounds, the
  fonts, the English text and the menus' pictures, the Top score page's
  and the Done note's animations among them, out of the dump; the game
  reads its data by firmware address.
- `tools/gb_run.c` runs a Game Boy ROM headlessly with scripted buttons,
  screenshots, sound capture, memory peeks and a profiler
  (`tools/gb_profile.py`).
- The firmware map the core follows is `docs/games_applications_3310.md`
  in the MAME fork.

`tools/export_fonts.py` and `tools/gb_audio.c` are unchanged copies from
the 3210 project and are not used yet.
