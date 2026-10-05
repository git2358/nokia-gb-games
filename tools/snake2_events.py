#!/usr/bin/env python3
"""Turn a Snake II probe log from MAME into the replay's events.

Usage: snake2_events.py ERROR_LOG OUT_DIR

ERROR_LOG is the error.log of a run of the MAME fork's
mame_nokia_3410_snake2_probe.lua. Writes OUT_DIR/events.txt, the events the
core's handler is to be given, in hex (core/game.h), and OUT_DIR/setup.txt,
"SEED LEVEL MAZE": the generator's state at New game, in hex, and what the
context hands the game, 1 to 9 and 1 to 6.

On the 3410 the game's menu sends New game as the events 0x0a, 0x14, 0x0c,
which here is 2b; every later timer event 0 is a tick, 1 here; and each
direction the probe wrote into the game is the key for it.
"""
import re
import sys
from pathlib import Path

KEYS = {0: "d", 1: "b", 2: "f", 3: "11"}  # left 4, up 2, right 6, down 8


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    log = Path(sys.argv[1]).read_text(errors="replace").splitlines()
    out = Path(sys.argv[2])
    events, seed, level, maze, started = [], None, None, None, False
    for line in log:
        m = re.search(r"SEV (\w+) (\w+) \w+ c=\d+ seed=(\w+)", line)
        if m:
            event = int(m.group(1), 16)
            if event == 0x0c and not started:
                started = True
                seed = m.group(3)
                events.append("2b")
            elif started and event == 0:
                events.append("1")
            continue
        m = re.search(r"S3KEY (\d)", line)
        if m and started:
            events.append(KEYS[int(m.group(1))])
            continue
        m = re.search(r"S3 t=\S+ .* lvl=(\d+) mode=\d+ maze=(-?\d+)", line)
        if m and started and level is None:
            level, maze = int(m.group(1)), int(m.group(2)) + 1
    if not started:
        sys.exit("no New game in the log")
    out.mkdir(parents=True, exist_ok=True)
    (out / "events.txt").write_text(" ".join(events) + "\n")
    (out / "setup.txt").write_text(f"{seed} 0 {level} {maze}\n")
    print(f"{out}: {len(events)} events, seed {seed}, level {level}, maze {maze}")


if __name__ == "__main__":
    main()
