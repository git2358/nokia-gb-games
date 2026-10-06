#!/usr/bin/env bash
# Regenerate the README banner: screenshots of the Game Boy ROM in both
# modes.
#
# Fully automated and headless. For each screenshot it builds the ROM with a
# scripted key sequence, runs it in SameBoy's core (tools/gb_run) and
# stitches the pictures into a labelled grid: the phone-sized mode and
# under it the full-screen mode; columns for the first screen, the Select
# game list, and each of the phone's five games being played. The
# full-screen row leaves the first screen out, which is the same for both
# modes. Only Snake II is here so far, so the other games' panels are left
# blank, and there is no GBA build yet, so no GBA rows.
#
#   ./scripts/make-banner.sh [output.png]     # default: docs/banner.png
#
# Needs everything `make check-gb` needs (the firmware dump,
# scripts/setup-sameboy.sh) and ImageMagick (`brew install imagemagick`).
# SAMEBOY names the emulator clone if it is not in tools/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/docs/banner.png}"
BUILD="$ROOT/build"
SAMEBOY="${SAMEBOY:-$ROOT/tools/SameBoy}"
BOOT="$SAMEBOY/build/bin/tester/dmg_boot.bin"
FONT="${BANNER_FONT:-/System/Library/Fonts/Menlo.ttc}"
GREEN='#9bbc0f' # Game Boy pea green, for the labels and the screens
DARK='#0f380f'

command -v magick >/dev/null || { echo "error: ImageMagick not found (brew install imagemagick)"; exit 1; }
[ -f "$BOOT" ] || { echo "error: no SameBoy boot ROM at $BOOT (scripts/setup-sameboy.sh)"; exit 1; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/nokia-banner.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

build() { make -C "$ROOT" SAMEBOY="$SAMEBOY" "$@" >/dev/null; }

# Key scripts (see menu_script in core/menu.c): s the Navi key, a the
# full-screen key, d down, z the power-on seed, w a second of the running
# game, t a tenth of one.
SELECT_PHONE="ss"
SELECT_FULL="as"
# Snake II from the power-on seed: a few seconds of the snake turning
# about the board, on the bigger board in full screen.
SNAKE_PLAY="wwutttttttdtttttttt"
SNAKE_PHONE="zsssss${SNAKE_PLAY}"
SNAKE_FULL="zassss${SNAKE_PLAY}"

# The ROM plays its keys before it shows anything, which takes the Game Boy
# a good ten seconds for a game under way, so each screenshot is taken a
# few frames after the screen first has something on it, after the boot
# ROM's logo, which is gone by frame GB_BOOT.
blank() { [ "$(magick "$1" -format '%k' info:)" -le 1 ]; }

# gb_shot KEYS OUT: a Game Boy screenshot at 3x, in the console's own green.
GB_BOOT=100
GB_STEP=5
gb_shot() {
  local steps=() n
  build build/nokia3410-keys.gb KEYS="$1"
  for n in $(seq 1 300); do steps+=("$GB_STEP" "shot:$TMP/gb-frame-$n.pgm"); done
  "$BUILD/gb_run" "$BUILD/nokia3410-keys.gb" "$BOOT" "$GB_BOOT" "${steps[@]}" >/dev/null 2>&1
  for n in $(seq 1 298); do
    blank "$TMP/gb-frame-$n.pgm" && continue
    magick "$TMP/gb-frame-$((n + 2)).pgm" -filter point -resize 300% +level-colors "$DARK","$GREEN" "$2"
    return
  done
  echo "error: the Game Boy ROM showed nothing after keys '$1'"; exit 1
}

build build/gb_run

# Panel 0 is the first screen, 1 the Select game list, 2 to 6 the games in
# the order of the phone's list.
gb_shot "" "$TMP/gb-0.png"
gb_shot "$SELECT_PHONE" "$TMP/gb-1.png"
gb_shot "$SNAKE_PHONE" "$TMP/gb-2.png"
gb_shot "$SELECT_FULL" "$TMP/gbfull-1.png"
gb_shot "$SNAKE_FULL" "$TMP/gbfull-2.png"

# --- layout: 480 px panels, a label column on the left, headers on top ------
PANEL=480
GUT=12
SIDE=56
HEAD=44
PANELS="0 1 2 3 4 5 6"

row() { # row PREFIX HEIGHT LABEL OUT
  local prefix="$1" h="$2" label="$3" out="$4" i files=()
  magick -size "${GUT}x${h}" xc:black "$TMP/gut.png"
  magick -size "${h}x${SIDE}" xc:black -font "$FONT" -pointsize 26 -fill "$GREEN" \
    -gravity center -annotate 0 "$label" -rotate -90 "$TMP/side.png"
  files+=("$TMP/side.png")
  for i in $PANELS; do
    # A panel with no screenshot is left blank.
    [ -f "$TMP/$prefix-$i.png" ] || magick -size "${PANEL}x${h}" xc:black "$TMP/$prefix-$i.png"
    [ "$i" = 0 ] || files+=("$TMP/gut.png")
    files+=("$TMP/$prefix-$i.png")
  done
  magick "${files[@]}" +append "$out"
}

header() { # header OUT
  local files=("$TMP/head.png") i=0 title
  magick -size "${SIDE}x${HEAD}" xc:black "$TMP/head.png"
  magick -size "${GUT}x${HEAD}" xc:black "$TMP/hg.png"
  for title in "First screen" "Select game" "Snake II" "Space Impact" "Bumper" "Bantumi" "Link5"; do
    magick -size "${PANEL}x${HEAD}" xc:black -font "$FONT" -pointsize 24 -fill "$GREEN" \
      -gravity center -annotate 0 "$title" "$TMP/h-$i.png"
    [ "$i" = 0 ] || files+=("$TMP/hg.png")
    files+=("$TMP/h-$i.png")
    i=$((i + 1))
  done
  magick "${files[@]}" +append "$1"
}

header "$TMP/header.png"
row gb 432 "Game Boy" "$TMP/row-gb.png"
row gbfull 432 "Full screen" "$TMP/row-gbfull.png"
W="$(magick identify -format '%w' "$TMP/row-gb.png")"
magick -size "${W}x${GUT}" xc:black "$TMP/hgut.png"
mkdir -p "$(dirname "$OUT")"
magick "$TMP/header.png" "$TMP/row-gb.png" "$TMP/hgut.png" "$TMP/row-gbfull.png" -append \
  -bordercolor black -border "${GUT}x${GUT}" -strip "$OUT"
echo "wrote $OUT ($(magick identify -format '%wx%h' "$OUT"))"
