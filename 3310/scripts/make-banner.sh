#!/usr/bin/env bash
# Regenerate the README banner: screenshots of both consoles in both modes.
#
# Fully automated and headless. For each screenshot it builds the ROM with a
# scripted key sequence, runs it in the emulator core (tools/gb_run on
# SameBoy's for the Game Boy, tools/gba_shot on mGBA's for the GBA), and
# stitches the pictures into a labelled grid: two rows per console, the
# phone-sized mode and under it the full-screen mode; columns for the first
# screen, the list of games, and each of the phone's four games being
# played. The full-screen rows leave the first screen out, which is the same
# for both modes.
#
#   ./scripts/make-banner.sh [output.png]     # default: docs/banner.png
#
# Needs everything `make check-gb` and `make check-gba` need (the firmware
# dump, scripts/setup-sameboy.sh, scripts/setup-mgba.sh) and ImageMagick
# (`brew install imagemagick`). SAMEBOY and MGBA name the emulator clones if
# they are not in tools/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/docs/banner.png}"
BUILD="$ROOT/build"
SAMEBOY="${SAMEBOY:-$ROOT/tools/SameBoy}"
MGBA="${MGBA:-$ROOT/tools/mgba}"
BOOT="$SAMEBOY/build/bin/tester/dmg_boot.bin"
FONT="${BANNER_FONT:-/System/Library/Fonts/Menlo.ttc}"
GREEN='#9bbc0f' # Game Boy pea green, for the labels and the Game Boy screens
DARK='#0f380f'

command -v magick >/dev/null || { echo "error: ImageMagick not found (brew install imagemagick)"; exit 1; }
[ -f "$BOOT" ] || { echo "error: no SameBoy boot ROM at $BOOT (scripts/setup-sameboy.sh)"; exit 1; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/nokia-banner.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

build() { make -C "$ROOT" SAMEBOY="$SAMEBOY" MGBA="$MGBA" "$@" >/dev/null; }

# Key scripts (see menu_script in core/menu.c): s the Navi key, a the
# full-screen key, u/d the direction buttons, w a second of the running
# game, t a tenth of one.
GAMES_PHONE="s"
GAMES_FULL="a"
# Space Impact's first level: the ship shoots at the first wave, moves down
# a row, loses a life to the second and is shooting at the third.
SI_PLAY="wwsttsttsttwwwsttsttdsttwwwwwwsttsttusttstt"
SI_PHONE="sdsss${SI_PLAY}"
SI_FULL="adsss${SI_PLAY}"
# Snake II from the power-on seed (z): a few seconds of the snake turning
# about the board, on the bigger board in full screen.
SNAKE_PLAY="wwwwsttwwwsttwwww"
SNAKE_PHONE="zssss${SNAKE_PLAY}"
SNAKE_FULL="zasss${SNAKE_PLAY}"
# Bantumi: the hand carrying the third pit's beans over the board.
BANTUMI_PLAY="wwwwwrrsttttt"
BANTUMI_PHONE="sddsss${BANTUMI_PLAY}"
BANTUMI_FULL="addsss${BANTUMI_PLAY}"
# Pairs II's Time trial from the power-on seed: the cards dealt and two
# turned over.
PAIRS_PLAY="wwwwwwwwsrs"
PAIRS_PHONE="zsdddssss${PAIRS_PLAY}"
PAIRS_FULL="zadddssss${PAIRS_PLAY}"

# The ROM plays its keys before it shows anything, which takes the Game Boy
# a good ten seconds for a game under way, so each screenshot is taken a
# few frames after the screen first has something on it. On the Game Boy
# that is after the boot ROM's logo, which is gone by frame GB_BOOT.
blank() { [ "$(magick "$1" -format '%k' info:)" -le 1 ]; }

# gb_shot KEYS OUT: a Game Boy screenshot at 3x, in the console's own green.
GB_BOOT=300
GB_STEP=5
gb_shot() {
  local steps=() n
  build build/nokia3310-keys.gb KEYS="$1"
  for n in $(seq 1 300); do steps+=("$GB_STEP" "shot:$TMP/gb-frame-$n.pgm"); done
  "$BUILD/gb_run" "$BUILD/nokia3310-keys.gb" "$BOOT" "$GB_BOOT" "${steps[@]}" >/dev/null 2>&1
  for n in $(seq 1 298); do
    blank "$TMP/gb-frame-$n.pgm" && continue
    magick "$TMP/gb-frame-$((n + 2)).pgm" -filter point -resize 300% +level-colors "$DARK","$GREEN" "$2"
    return
  done
  echo "error: the Game Boy ROM showed nothing after keys '$1'"; exit 1
}

# gba_shot KEYS OUT: a GBA screenshot at 2x.
gba_shot() {
  local n
  build build/nokia3310-keys.gba KEYS="$1"
  for n in $(seq 5 5 600); do
    "$BUILD/gba_shot" "$BUILD/nokia3310-keys.gba" "$TMP/gba.bmp" "$n"
    python3 "$ROOT/tools/bmp_to_png.py" "$TMP/gba.bmp" "$TMP/gba.png" 1 >/dev/null
    blank "$TMP/gba.png" && continue
    "$BUILD/gba_shot" "$BUILD/nokia3310-keys.gba" "$TMP/gba.bmp" "$((n + 5))"
    python3 "$ROOT/tools/bmp_to_png.py" "$TMP/gba.bmp" "$2" 2 >/dev/null
    return
  done
  echo "error: the GBA ROM showed nothing after keys '$1'"; exit 1
}

build build/gb_run build/gba_shot

# Panel 0 is the first screen, 1 the list of games, 2 to 5 the games in the
# order of the phone's list.
gb_shot "" "$TMP/gb-0.png"
gb_shot "$GAMES_PHONE" "$TMP/gb-1.png"
gb_shot "$SNAKE_PHONE" "$TMP/gb-2.png"
gb_shot "$SI_PHONE" "$TMP/gb-3.png"
gb_shot "$BANTUMI_PHONE" "$TMP/gb-4.png"
gb_shot "$PAIRS_PHONE" "$TMP/gb-5.png"
gb_shot "$GAMES_FULL" "$TMP/gbfull-1.png"
gb_shot "$SNAKE_FULL" "$TMP/gbfull-2.png"
gb_shot "$SI_FULL" "$TMP/gbfull-3.png"
gb_shot "$BANTUMI_FULL" "$TMP/gbfull-4.png"
gb_shot "$PAIRS_FULL" "$TMP/gbfull-5.png"

gba_shot "" "$TMP/gba-0.png"
gba_shot "$GAMES_PHONE" "$TMP/gba-1.png"
gba_shot "$SNAKE_PHONE" "$TMP/gba-2.png"
gba_shot "$SI_PHONE" "$TMP/gba-3.png"
gba_shot "$BANTUMI_PHONE" "$TMP/gba-4.png"
gba_shot "$PAIRS_PHONE" "$TMP/gba-5.png"
gba_shot "$GAMES_FULL" "$TMP/gbafull-1.png"
gba_shot "$SNAKE_FULL" "$TMP/gbafull-2.png"
gba_shot "$SI_FULL" "$TMP/gbafull-3.png"
gba_shot "$BANTUMI_FULL" "$TMP/gbafull-4.png"
gba_shot "$PAIRS_FULL" "$TMP/gbafull-5.png"

# --- layout: 480 px panels, a label column on the left, headers on top ------
PANEL=480
GUT=12
SIDE=56
HEAD=44

row() { # row PREFIX HEIGHT LABEL OUT
  local prefix="$1" h="$2" label="$3" out="$4" i
  magick -size "${GUT}x${h}" xc:black "$TMP/gut.png"
  # A panel with no screenshot is left blank.
  for i in 0 1 2 3 4 5; do
    [ -f "$TMP/$prefix-$i.png" ] || magick -size "${PANEL}x${h}" xc:black "$TMP/$prefix-$i.png"
  done
  magick -size "${h}x${SIDE}" xc:black -font "$FONT" -pointsize 26 -fill "$GREEN" \
    -gravity center -annotate 0 "$label" -rotate -90 "$TMP/side.png"
  magick "$TMP/side.png" "$TMP/$prefix-0.png" "$TMP/gut.png" "$TMP/$prefix-1.png" "$TMP/gut.png" \
    "$TMP/$prefix-2.png" "$TMP/gut.png" "$TMP/$prefix-3.png" "$TMP/gut.png" "$TMP/$prefix-4.png" \
    "$TMP/gut.png" "$TMP/$prefix-5.png" +append "$out"
}

header() { # header OUT
  local i=0 title
  magick -size "${SIDE}x${HEAD}" xc:black "$TMP/head.png"
  for title in "First screen" "Games" "Snake II" "Space impact" "Bantumi" "Pairs II"; do
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
