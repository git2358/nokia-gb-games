#!/usr/bin/env bash
# Regenerate the README banner: screenshots of both consoles in both modes.
#
# Fully automated and headless. For each screenshot it builds the ROM with a
# scripted key sequence, runs it in the emulator core (SameBoy's tester for
# the Game Boy, tools/gba_shot for the GBA), and stitches the pictures into a
# labelled grid: one row per console; columns for the first screen, Snake's
# menu in the phone-sized and full-screen modes, and Snake being played in
# each.
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

# Key scripts: s select, a the full-screen key, d down, u/l turn, t one move.
MENU_PHONE="sds"
MENU_FULL="ads"
# Each game eats the first food, in the middle of its board, then turns left.
PLAY_PHONE="sdss$(moves 2)u$(moves 5)l$(moves 3)"
PLAY_FULL_GB="adss$(moves 11)u$(moves 16)l$(moves 4)"
PLAY_FULL_GBA="adss$(moves 6)u$(moves 8)l$(moves 3)"

# gb_shot KEYS OUT: a Game Boy screenshot at 3x, in the console's own green.
gb_shot() {
  make -C "$ROOT" gb KEYS="$1" >/dev/null
  "$TESTER" --dmg --length 2 "$BUILD/nokia3310.gb" >/dev/null 2>&1
  python3 "$ROOT/tools/bmp_to_png.py" "$BUILD/nokia3310.bmp" "$TMP/raw.png" 3 >/dev/null
  magick "$TMP/raw.png" +level-colors "$DARK","$GREEN" "$2"
}

# gba_shot KEYS OUT: a GBA screenshot at 2x.
gba_shot() {
  make -C "$ROOT" gba KEYS="$1" >/dev/null
  "$BUILD/gba_shot" "$BUILD/nokia3310.gba" "$BUILD/nokia3310-gba.bmp" 14
  python3 "$ROOT/tools/bmp_to_png.py" "$BUILD/nokia3310-gba.bmp" "$2" 2 >/dev/null
}

make -C "$ROOT" build/gba_shot >/dev/null

gb_shot "" "$TMP/gb-0.png"
gb_shot "$MENU_PHONE" "$TMP/gb-1.png"
gb_shot "$MENU_FULL" "$TMP/gb-2.png"
gb_shot "$PLAY_PHONE" "$TMP/gb-3.png"
gb_shot "$PLAY_FULL_GB" "$TMP/gb-4.png"

gba_shot "" "$TMP/gba-0.png"
gba_shot "$MENU_PHONE" "$TMP/gba-1.png"
gba_shot "$MENU_FULL" "$TMP/gba-2.png"
gba_shot "$PLAY_PHONE" "$TMP/gba-3.png"
gba_shot "$PLAY_FULL_GBA" "$TMP/gba-4.png"

# Leave the default builds behind, not the last scripted ones.
make -C "$ROOT" gb gba KEYS= >/dev/null

# --- layout: 480 px panels, a label column on the left, headers on top ------
PANEL=480
GUT=12
SIDE=56
HEAD=44

row() { # row PREFIX HEIGHT LABEL OUT
  local prefix="$1" h="$2" label="$3" out="$4"
  magick -size "${GUT}x${h}" xc:black "$TMP/gut.png"
  magick -size "${h}x${SIDE}" xc:black -font "$FONT" -pointsize 26 -fill "$GREEN" \
    -gravity center -annotate 0 "$label" -rotate -90 "$TMP/side.png"
  magick "$TMP/side.png" "$TMP/$prefix-0.png" "$TMP/gut.png" "$TMP/$prefix-1.png" "$TMP/gut.png" \
    "$TMP/$prefix-2.png" "$TMP/gut.png" "$TMP/$prefix-3.png" "$TMP/gut.png" "$TMP/$prefix-4.png" \
    +append "$out"
}

header() { # header OUT
  local i=0 title
  magick -size "${SIDE}x${HEAD}" xc:black "$TMP/head.png"
  for title in "First screen" "Menu, phone-sized" "Menu, full screen" "Snake, phone-sized" "Snake, full screen"; do
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
row gba 320 "Game Boy Advance" "$TMP/row-gba.png"
W="$(magick identify -format '%w' "$TMP/row-gb.png")"
magick -size "${W}x${GUT}" xc:black "$TMP/hgut.png"
mkdir -p "$(dirname "$OUT")"
magick "$TMP/header.png" "$TMP/row-gb.png" "$TMP/hgut.png" "$TMP/row-gba.png" -append \
  -bordercolor black -border "${GUT}x${GUT}" -strip "$OUT"
echo "wrote $OUT ($(magick identify -format '%wx%h' "$OUT"))"
