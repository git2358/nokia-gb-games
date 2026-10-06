#!/usr/bin/env bash
# Clone mGBA at a pinned release into the repository's ignored tools/mgba
# and build only its core library, which tools/gba_shot.c links for
# headless GBA screenshots. Needs cmake and a C compiler.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${MGBA_DIR:-$ROOT/../tools/mgba}"
REMOTE="${MGBA_REMOTE:-https://github.com/mgba-emu/mgba.git}"
BASE_COMMIT="26b7884bc25a5933960f3cdcd98bac1ae14d42e2" # 0.10.5

if [[ -d "$DEST/.git" ]]; then
  echo "Updating existing clone at $DEST"
  git -C "$DEST" fetch origin
else
  echo "Cloning mGBA into $DEST"
  mkdir -p "$(dirname "$DEST")"
  git clone "$REMOTE" "$DEST"
fi
git -C "$DEST" checkout --force "$BASE_COMMIT"

cmake -S "$DEST" -B "$DEST/build" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DBUILD_SHARED=OFF -DBUILD_STATIC=ON -DBUILD_QT=OFF -DBUILD_SDL=OFF \
  -DBUILD_GL=OFF -DBUILD_GLES2=OFF -DBUILD_GLES3=OFF -DBUILD_LIBRETRO=OFF \
  -DBUILD_PERF=OFF -DBUILD_TEST=OFF -DBUILD_SUITE=OFF -DBUILD_CINEMA=OFF \
  -DBUILD_ROM_TEST=OFF -DBUILD_EXAMPLE=OFF -DBUILD_PYTHON=OFF \
  -DUSE_FFMPEG=OFF -DUSE_ZLIB=OFF -DUSE_MINIZIP=OFF -DUSE_PNG=OFF \
  -DUSE_LIBZIP=OFF -DUSE_SQLITE3=OFF -DUSE_ELF=OFF -DUSE_LUA=OFF \
  -DUSE_JSON_C=OFF -DUSE_LZMA=OFF -DUSE_DISCORD_RPC=OFF -DUSE_EPOXY=OFF \
  -DUSE_EDITLINE=OFF -DUSE_GDB_STUB=OFF -DUSE_DEBUGGERS=OFF \
  -DENABLE_SCRIPTING=OFF -DM_CORE_GB=OFF
cmake --build "$DEST/build" --target mgba -j 8
echo "mGBA library ready: $DEST/build/libmgba.a"
