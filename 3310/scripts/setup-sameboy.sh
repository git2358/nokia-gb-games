#!/usr/bin/env bash
# Clone upstream SameBoy at a pinned commit into the repository's ignored
# tools/SameBoy and build its boot ROMs and its core as a library, which
# tools/gb_run and with it `make check-gb` are built on. Needs rgbds (for
# the boot ROMs) and a C compiler.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${SAMEBOY_DIR:-$ROOT/../tools/SameBoy}"
REMOTE="${SAMEBOY_REMOTE:-https://github.com/LIJI32/SameBoy.git}"
BASE_COMMIT="213a12ce93d66b105a113debd9396306066a7cfc"

if [[ -d "$DEST/.git" ]]; then
  echo "Updating existing clone at $DEST"
  git -C "$DEST" fetch origin
else
  echo "Cloning SameBoy into $DEST"
  mkdir -p "$(dirname "$DEST")"
  git clone "$REMOTE" "$DEST"
fi
git -C "$DEST" checkout --force "$BASE_COMMIT"

make -C "$DEST" tester lib
echo "SameBoy ready: $DEST/build/lib/libsameboy.a and $DEST/build/bin/tester/dmg_boot.bin"
