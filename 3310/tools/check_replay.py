#!/usr/bin/env python3
"""Compare replayed frames with the frames MAME captured from the firmware.

Usage: check_replay.py REPLAY_DIR GOLDEN_DIR

MAME writes a frame whenever the LCD changes, so its run holds every
picture the game showed, in order, among frames from before the game
started and frames caught part-way through a redraw. Every replayed frame
must therefore appear in the golden run, in order. The check reports the
first replayed frame that does not, against the golden frames around where
it should be.
"""
import sys
from pathlib import Path

W, H = 84, 48


def pixels(path):
    return Path(path).read_bytes()[-W * H:]


def distinct(paths):
    out, last = [], None
    for p in paths:
        px = pixels(p)
        if px != last:
            out.append((p.name, px))
        last = px
    return out


def show(ours, theirs):
    for y in range(H):
        row = ""
        for i in range(y * W, y * W + W):
            if ours[i] == theirs[i]:
                row += "#" if not ours[i] else "."
            else:
                row += "+" if not ours[i] else "-"
        print("  " + row)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    replay = distinct(sorted(Path(sys.argv[1]).glob("*.pgm")))
    golden = distinct(sorted(Path(sys.argv[2]).glob("*.pgm")))
    if not golden:
        print(f"no golden frames in {sys.argv[2]}; skipped")
        return
    at = 0
    for n, (name, px) in enumerate(replay):
        found = next((i for i in range(at, len(golden)) if golden[i][1] == px), None)
        if found is None and at == len(golden) and len(replay) - n <= 2:
            # The capture stopped before the picture of the last event or two reached the LCD.
            print(f"ok   {n} replayed frames all appear in order among {len(golden)} golden frames;"
                  f" the last {len(replay) - n} came after the capture ended")
            return
        if found is None:
            best = min(range(at, min(at + 6, len(golden))), key=lambda i: sum(a != b for a, b in zip(px, golden[i][1])),
                       default=None)
            print(f"FAIL replay frame {name} ({n + 1} of {len(replay)}) is not in the golden run after {golden[at - 1][0] if at else 'the start'}")
            if best is not None:
                wrong = sum(a != b for a, b in zip(px, golden[best][1]))
                print(f"nearest of the next golden frames: {golden[best][0]}, {wrong} pixels differ"
                      " ('+' only in ours, '-' only in theirs)")
                show(px, golden[best][1])
            sys.exit(1)
        at = found + 1
    print(f"ok   {len(replay)} replayed frames all appear in order among {len(golden)} golden frames"
          f" ({len(golden) - at} golden frames follow the last match)")


if __name__ == "__main__":
    main()
