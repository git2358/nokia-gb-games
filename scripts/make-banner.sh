#!/usr/bin/env bash
# Regenerate the README banner: screenshots of both consoles in both modes.
#
# Fully automated and headless. For each screenshot it builds the ROM with a
# scripted key sequence, runs it in the emulator core (SameBoy's tester for
# the Game Boy, tools/gba_shot for the GBA), and stitches the pictures into a
# labelled grid: two rows per console, the phone-sized mode and under it the
# full-screen mode; columns for the first screen, the list of games, and
# Rotation, Snake and Memory being played. The full-screen rows leave the
# first screen out, which is the same for both modes; only Snake has a board
# of its own there, and Rotation and Memory are played on the phone's screen
# at 2x.
#
#   ./scripts/make-banner.sh [output.png]     # default: docs/banner.png
#
# Needs everything `make check-gb` and `make check-gba` need (the firmware
# dump, scripts/setup-sameboy.sh, scripts/setup-mgba.sh) and ImageMagick
# (`brew install imagemagick`). The default builds are restored at the end.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/docs/banner.png}"
BUILD="$ROOT/build"
TESTER="$ROOT/tools/SameBoy/build/bin/tester/sameboy_tester"
FONT="${BANNER_FONT:-/System/Library/Fonts/Menlo.ttc}"
GREEN='#9bbc0f' # Game Boy pea green, for the labels and the Game Boy screens
DARK='#0f380f'

command -v magick >/dev/null || { echo "error: ImageMagick not found (brew install imagemagick)"; exit 1; }
[ -x "$TESTER" ] || { echo "error: SameBoy tester not built (scripts/setup-sameboy.sh)"; exit 1; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/nokia-banner.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

moves() { printf 't%.0s' $(seq 1 "$1"); }

# Key scripts: s select, a the full-screen key, u/d/l/r the direction
# buttons, t one move or tick of the running game.
GAMES_PHONE="s"
GAMES_FULL="a"
# From a game's menu: its last level, reached with that many presses of up,
# then New game. The full-screen menus list the entries in another order.
level() { printf 'us%ssds' "$(printf 'u%.0s' $(seq 1 "$1"))"; }
level_full() { printf 'ds%ssus' "$(printf 'u%.0s' $(seq 1 "$1"))"; }
# Rotation on its biggest board, 6x6 with the 3x3 frame: the game's own ten
# opening turns, four ticks each, then the frame moved off the corner.
ROTATION_TURNS="$(moves 40)rd"
ROTATION="ss$(level 6)${ROTATION_TURNS}"
ROTATION_FULL="as$(level_full 6)${ROTATION_TURNS}"
# Each Snake eats the first food, in the middle of its board, then turns left.
SNAKE_PHONE="sdss$(moves 2)u$(moves 5)l$(moves 3)"
SNAKE_FULL_GB="adss$(moves 11)u$(moves 16)l$(moves 4)"
SNAKE_FULL_GBA="adss$(moves 6)u$(moves 8)l$(moves 3)"
# Memory on its biggest board, 10x6: three pairs found and one more card
# turned up. The scripted game starts from seed 0, so the cards are where
# these keys expect.
MEMORY_TRIES="srrrrrdddslluusluslsrrrddsrsddr"
MEMORY="sdds$(level 4)${MEMORY_TRIES}"
MEMORY_FULL="adds$(level_full 4)${MEMORY_TRIES}"

# gb_shot KEYS OUT: a Game Boy screenshot at 3x, in the console's own green.
gb_shot() {
  make -C "$ROOT" gb KEYS="$1" >/dev/null
  "$TESTER" --dmg --length 2 "$BUILD/nokia3210.gb" >/dev/null 2>&1
  python3 "$ROOT/tools/bmp_to_png.py" "$BUILD/nokia3210.bmp" "$TMP/raw.png" 3 >/dev/null
  magick "$TMP/raw.png" +level-colors "$DARK","$GREEN" "$2"
}

# gba_shot KEYS OUT: a GBA screenshot at 2x.
gba_shot() {
  make -C "$ROOT" gba KEYS="$1" >/dev/null
  "$BUILD/gba_shot" "$BUILD/nokia3210.gba" "$BUILD/nokia3210-gba.bmp" 14
  python3 "$ROOT/tools/bmp_to_png.py" "$BUILD/nokia3210-gba.bmp" "$2" 2 >/dev/null
}

make -C "$ROOT" build/gba_shot >/dev/null

gb_shot "" "$TMP/gb-0.png"
gb_shot "$GAMES_PHONE" "$TMP/gb-1.png"
gb_shot "$ROTATION" "$TMP/gb-2.png"
gb_shot "$SNAKE_PHONE" "$TMP/gb-3.png"
gb_shot "$MEMORY" "$TMP/gb-4.png"
gb_shot "$GAMES_FULL" "$TMP/gbfull-1.png"
gb_shot "$ROTATION_FULL" "$TMP/gbfull-2.png"
gb_shot "$SNAKE_FULL_GB" "$TMP/gbfull-3.png"
gb_shot "$MEMORY_FULL" "$TMP/gbfull-4.png"

gba_shot "" "$TMP/gba-0.png"
gba_shot "$GAMES_PHONE" "$TMP/gba-1.png"
gba_shot "$ROTATION" "$TMP/gba-2.png"
gba_shot "$SNAKE_PHONE" "$TMP/gba-3.png"
gba_shot "$MEMORY" "$TMP/gba-4.png"
gba_shot "$GAMES_FULL" "$TMP/gbafull-1.png"
gba_shot "$ROTATION_FULL" "$TMP/gbafull-2.png"
gba_shot "$SNAKE_FULL_GBA" "$TMP/gbafull-3.png"
gba_shot "$MEMORY_FULL" "$TMP/gbafull-4.png"

# Leave the default builds behind, not the last scripted ones.
make -C "$ROOT" gb gba KEYS= >/dev/null

# --- layout: 480 px panels, a label column on the left, headers on top ------
PANEL=480
GUT=12
SIDE=56
HEAD=44

row() { # row PREFIX HEIGHT LABEL OUT
  local prefix="$1" h="$2" label="$3" out="$4" i
  magick -size "${GUT}x${h}" xc:black "$TMP/gut.png"
  # A panel with no screenshot is left blank.
  for i in 0 1 2 3 4; do
    [ -f "$TMP/$prefix-$i.png" ] || magick -size "${PANEL}x${h}" xc:black "$TMP/$prefix-$i.png"
  done
  magick -size "${h}x${SIDE}" xc:black -font "$FONT" -pointsize 26 -fill "$GREEN" \
    -gravity center -annotate 0 "$label" -rotate -90 "$TMP/side.png"
  magick "$TMP/side.png" "$TMP/$prefix-0.png" "$TMP/gut.png" "$TMP/$prefix-1.png" "$TMP/gut.png" \
    "$TMP/$prefix-2.png" "$TMP/gut.png" "$TMP/$prefix-3.png" "$TMP/gut.png" "$TMP/$prefix-4.png" \
    +append "$out"
}

header() { # header OUT
  local i=0 title
  magick -size "${SIDE}x${HEAD}" xc:black "$TMP/head.png"
  for title in "First screen" "Games" "Rotation" "Snake" "Memory"; do
    magick -size "${PANEL}x${HEAD}" xc:black -font "$FONT" -pointsize 24 -fill "$GREEN" \
      -gravity center -annotate 0 "$title" "$TMP/h.png"
    if [ "$i" -gt 0 ]; then
      magick -size "${GUT}x${HEAD}" xc:black "$TMP/hg.png"
      magick "$TMP/head.png" "$TMP/hg.png" "$TMP/h.png" +append "$TMP/head.png"
    else
      magick "$TMP/head.png" "$TMP/h.png" +append "$TMP/head.png"
    fi
    i=$((i + 1))
  done
  cp "$TMP/head.png" "$1"
}

header "$TMP/header.png"
row gb 432 "Game Boy" "$TMP/row-gb.png"
row gbfull 432 "Game Boy, full screen" "$TMP/row-gbfull.png"
row gba 320 "Game Boy Advance" "$TMP/row-gba.png"
row gbafull 320 "GBA, full screen" "$TMP/row-gbafull.png"
W="$(magick identify -format '%w' "$TMP/row-gb.png")"
magick -size "${W}x${GUT}" xc:black "$TMP/hgut.png"
mkdir -p "$(dirname "$OUT")"
magick "$TMP/header.png" "$TMP/row-gb.png" "$TMP/hgut.png" "$TMP/row-gbfull.png" "$TMP/hgut.png" \
  "$TMP/row-gba.png" "$TMP/hgut.png" "$TMP/row-gbafull.png" -append \
  -bordercolor black -border "${GUT}x${GUT}" -strip "$OUT"
echo "wrote $OUT ($(magick identify -format '%wx%h' "$OUT"))"
