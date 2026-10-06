#!/usr/bin/env python3
"""Check a Game Boy ROM's screen against the host's frames, and its speed.

Usage: check_gb_run.py GB_RUN ROM.gb BOOT_ROM FRAME_TOOL KEYS FRAMES [FRAMES...]

Runs the ROM, which presses KEYS at power-on, in SameBoy's core and takes
its screen after each count of FRAMES. Each must equal a frame the host
tool makes for the same keys and some number of frames of time, "menu-KEYS+N":
the picture is then right to the pixel, and how N grows from one screenshot
to the next against how FRAMES did is how fast the ROM runs the game. N is
looked for from FRAMES downwards, since the ROM starts later than the host
(the boot ROM's logo comes first) and may run slower.

The ROM takes a frame or two to put a new picture on its screen, and a
screenshot taken meanwhile is part old and part new; so the screen is
taken on each of the TRIES frames from a count on, and one must match.
"""
import subprocess
import sys
import tempfile
from pathlib import Path


TRIES = 12


def main():
    if len(sys.argv) < 7:
        sys.exit(__doc__)
    gb_run, rom, boot, tool, keys = sys.argv[1:6]
    counts = [int(a) for a in sys.argv[6:]]
    with tempfile.TemporaryDirectory() as tmp:
        steps, last = [], 0
        for i, count in enumerate(counts):
            for k in range(TRIES):
                steps += [str(count + k - last), f"shot:{tmp}/rom{i}_{k}.pgm"]
                last = count + k
        subprocess.run([gb_run, rom, boot, *steps], check=True, stdin=subprocess.DEVNULL,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        found, low = [], 0
        for i, count in enumerate(counts):
            screens = {Path(f"{tmp}/rom{i}_{k}.pgm").read_bytes() for k in range(TRIES)}
            for n in range(count + TRIES, low - 1, -1):
                subprocess.run([tool, f"menu-{keys}+{n}", f"{tmp}/host.pgm"], check=True, stdout=subprocess.DEVNULL)
                if Path(f"{tmp}/host.pgm").read_bytes() in screens:
                    found.append(n)
                    break
            else:
                sys.exit(f"FAIL: the ROM's screen about {count} frames in matches no host frame for keys '{keys}'")
            low = found[-1]
    speeds = [100 * (found[i] - found[i - 1]) / (counts[i] - counts[i - 1]) for i in range(1, len(counts))]
    print(f"ok: keys '{keys}': the ROM's screen about {counts} frames in is the host's after {found}"
          + (f"; speed {' '.join(f'{s:.0f}%' for s in speeds)}" if speeds else ""))


if __name__ == "__main__":
    main()
