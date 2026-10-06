#!/usr/bin/env python3
"""Turn a Space Impact autopilot log from MAME into the replay's events.

Usage: si_events.py [--title] ERROR_LOG OUT_DIR

ERROR_LOG is the error.log of a run of the MAME fork's
mame_nokia_3410_si_bot.lua. Writes OUT_DIR/events.txt, every call of the
game's handler from New game on, one "EVENT A B KEYS" in hex per line, in
the 3410's own numbering (see the fork's docs/games_si_3410.md: 0 the timer,
1 and 2 a key going down and up with A its code, 3 a pause, 0x0a 0x14 0x0c
New game; "ff 3 0 0" is the autopilot setting the lives back to 3; KEYS
has bit k set when the game would see key code k held
(0x3b29d0: (byte & 0xf) >> 1 nonzero) at that call, as read when the game
polls them in play), and OUT_DIR/setup.txt, the ANSI generator's state at New game in
hex.

With --title the events start at the title (0x0e) instead, and setup.txt
is the generator's state as the title found it: the title reseeds it from
the clock and draws 60 numbers for its stars before its first tick, whose
logged state the 60 draws are undone from.
"""
import re
import sys
from pathlib import Path


def undo(state, draws):
    """The ANSI generator's state `draws` numbers earlier."""
    inverse = pow(0x41C64E6D, -1, 1 << 32)
    for _ in range(draws):
        state = (state - 0x3039) * inverse & 0xFFFFFFFF
    return state


def main():
    args = sys.argv[1:]
    title = args[:1] == ["--title"]
    if title:
        args = args[1:]
    if len(args) != 2:
        sys.exit(__doc__)
    log = Path(args[0]).read_text(errors="replace").splitlines()
    out = Path(args[1])
    events, seed = [], None
    last_was_call = False
    for line in log:
        if seed is not None and "SIPOKE lives 3" in line:
            # The autopilot's write as the handler was entered: a
            # pseudo-event before that call, whichever of the two
            # breakpoints logged first.
            if last_was_call:
                events.insert(len(events) - 1, "ff 3 0 0")
            else:
                events.append("ff 3 0 0")
            continue
        last_was_call = "SIEV" in line
        k = re.search(r"SIKEYS keys=(\w+)", line)
        if k and seed is not None and events:
            # The poll's own reading replaces the one at the handler's entry.
            raw = bytes.fromhex(k.group(1).rjust(24, "0"))
            keys = sum(1 << n for n, v in enumerate(raw) if (v & 0xf) >> 1)
            parts = events[-1].split()
            events[-1] = " ".join(parts[:3] + [f"{keys:x}"])
            continue
        m = re.search(r"SIEV (\w+) (\w+) (\w+) c=\d+ seed=(\w+) keys=(\w+)", line)
        if not m:
            continue
        event, a, b = (int(v, 16) for v in m.group(1, 2, 3))
        raw = bytes.fromhex(m.group(5).rjust(24, "0"))
        keys = sum(1 << k for k, v in enumerate(raw) if (v & 0xf) >> 1)
        if seed is None:
            if title:
                if event != 0x0e:
                    continue
                seed = "title"
            else:
                # New game is 0x0a with a = 1, then 0x14 and 0x0c.
                if event != 0x0a or a != 1:
                    continue
                seed = m.group(4)
        elif seed == "title" and event == 0:
            seed = f"{undo(int(m.group(4), 16), 60):08x}"
        events.append(f"{event:x} {a:x} {b:x} {keys:x}")
    if seed is None:
        sys.exit(f"{sys.argv[1]}: no New game in the log")
    out.mkdir(parents=True, exist_ok=True)
    (out / "events.txt").write_text("\n".join(events) + "\n")
    (out / "setup.txt").write_text(seed + "\n")
    print(f"{out}: {len(events)} events, seed {seed}")


if __name__ == "__main__":
    main()
