#!/usr/bin/env python3
"""Time the benchmark ROM (make bench-gb) in SameBoy and report what each
tick of the recorded game costs the Game Boy.

Usage: bench_report.py GB_RUN ROM.gb BOOT_ROM LINK.map EVENTS HOST_LAST.pgm [--top N]

EVENTS is the recorded game's events.txt, which the ROM was built from,
and HOST_LAST.pgm the last frame the host's replay of it wrote
(platform/host/si_replay_main.c): the Game Boy's screen at the end must be
the same, the phone's area of it, or the Game Boy played another game.

The ROM plays a recorded game's events as fast as it can and marks each
step (bench_step), and each picture's drawing (bench_render), putting on
the screen (bench_present) and end (bench_drawn); gb_run's times: step
gives the cycle of each mark. A tick on the phone is 100 ms, 419,430
Game Boy cycles: in it the Game Boy runs the tick and the key events
since the last one, and draws one picture. A tick whose share comes to
more than that is one the Game Boy shows late or not at all.
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

TICK = 4194304 // 10
MARKS = ("bench_step", "bench_render", "bench_present", "bench_drawn", "bench_end")


def symbols(map_path):
    found = {}
    for line in open(map_path):
        m = re.match(r"\s+([0-9A-F]{8})\s+_(\w+)", line)
        if m and m.group(2) in MARKS:
            found[m.group(2)] = int(m.group(1), 16) & 0xFFFF
    missing = [m for m in MARKS if m not in found]
    if missing:
        sys.exit(f"{map_path}: no {' '.join(missing)}: not a benchmark ROM's map")
    return found


def pgm(path):
    data = path.read_bytes()
    parts = data.split(maxsplit=4)
    w, h = int(parts[1]), int(parts[2])
    return w, h, data[len(data) - w * h:]


def compare_end(gb_shot, host_frame):
    """The pixels of the phone's area (96 by 65 at 32, 40) that differ."""
    gw, _, g = pgm(gb_shot)
    hw, hh, h = pgm(host_frame)
    if (hw, hh) != (96, 65):
        sys.exit(f"{host_frame}: not a frame of the phone's LCD")
    return sum((g[(40 + y) * gw + 32 + x] < 128) != (h[y * hw + x] < 128) for y in range(65) for x in range(96))


def percentile(values, p):
    values = sorted(values)
    return values[min(len(values) - 1, int(len(values) * p))]


def main():
    args = sys.argv[1:]
    top = 10
    if "--top" in args:
        i = args.index("--top")
        top = int(args[i + 1])
        del args[i:i + 2]
    if len(args) != 6:
        sys.exit(__doc__)
    gb_run, rom, boot, map_path, events_path, host_last = args
    kinds = [int(line.split()[0], 16) for line in open(events_path) if line.strip()]
    kinds = [k for k in kinds if k != 0xFF]
    addr = symbols(map_path)
    name = {v: k for k, v in addr.items()}
    marks = ",".join(f"{addr[m]:04x}" for m in MARKS[:-1]) + f",!{addr['bench_end']:04x}"
    shot = Path(tempfile.mkdtemp()) / "end.pgm"
    run = subprocess.run([gb_run, rom, boot, f"times:2000000:{marks}", "30", f"shot:{shot}"], capture_output=True,
                         text=True, stdin=subprocess.DEVNULL, check=True)
    events = []
    for line in run.stdout.splitlines():
        m = re.match(r"([0-9a-f]{4}) at (\d+)", line)
        if m:
            mark, at = name[int(m.group(1), 16)], int(m.group(2))
            # A mark reached again at once: an interrupt was taken there.
            if events and events[-1][0] == mark and at - events[-1][1] < 100:
                continue
            events.append((mark, at))
    if not events or events[-1][0] != "bench_end":
        sys.exit("the benchmark did not reach its end")
    # The game starts with New game, drawn at once: marks before its step
    # are a boot ROM's code at the same addresses (a Game Boy Color's
    # covers 0x200 to 0x8ff), not the benchmark's.
    first = next(i for i, (mark, _) in enumerate(events) if mark == "bench_render")
    first = max(i for i in range(first) if events[i][0] == "bench_step")
    events = events[first:]
    differ = compare_end(shot, Path(host_last))
    print(f"the end: {'the host' + chr(39) + 's last frame' if not differ else f'{differ} pixels differ from the host' + chr(39) + 's last frame'}")

    # Steps: the game's part of each event, and the picture drawn after it.
    steps = []
    for i, (kind, at) in enumerate(events):
        if kind != "bench_step":
            continue
        step = {"at": at, "logic": None, "render": 0, "present": 0, "drawn": False}
        j = i + 1
        nxt = events[j]
        step["logic"] = nxt[1] - at
        if nxt[0] == "bench_render":
            render_at, present_at, drawn_at = events[j][1], events[j + 1][1], events[j + 2][1]
            step.update(render=present_at - render_at, present=drawn_at - present_at, drawn=True)
        steps.append(step)
    print(f"{len(steps)} steps, {sum(s['drawn'] for s in steps)} pictures, "
          f"{events[-1][1] / 4194304:.0f} s of Game Boy time")
    for part in ("logic", "render", "present"):
        values = [s[part] for s in steps if s["drawn"] or part == "logic"]
        print(f"  {part:8} mean {sum(values) // len(values):7d}  p50 {percentile(values, 0.5):7d}  "
              f"p90 {percentile(values, 0.9):7d}  p99 {percentile(values, 0.99):7d}  max {max(values):7d}")

    if len(kinds) != len(steps):
        sys.exit(f"{len(steps)} steps timed but {len(kinds)} events recorded: not this ROM's game")
    # Ticks: on the phone a tick, every 100 ms, comes with the key events
    # since the last one; the Game Boy in play runs them all and draws one
    # picture. So a tick costs its key steps' game, its own, and its
    # picture (the benchmark's after the tick step).
    ticks, logic, first = [], 0, 0
    for index, (s, kind) in enumerate(zip(steps, kinds)):
        logic += s["logic"]
        if kind != 0:
            continue
        ticks.append({"first": first, "logic": logic, "render": s["render"], "present": s["present"],
                      "total": logic + s["render"] + s["present"]})
        logic, first = 0, index + 1
    totals = [t["total"] for t in ticks]
    over = [t for t in ticks if t["total"] > TICK]
    print(f"{len(ticks)} ticks, with their key events and a picture: mean {sum(totals) // len(totals)}, "
          f"p90 {percentile(totals, 0.9)}, p99 {percentile(totals, 0.99)}, max {max(totals)} "
          f"of {TICK} a tick; {len(over)} ({100 * len(over) / len(ticks):.1f}%) over")
    print(f"the {top} costliest:")
    for t in sorted(ticks, key=lambda t: -t["total"])[:top]:
        print(f"  events from {t['first']:6d}: {t['total']:7d} (game {t['logic']}, drawing {t['render']}, "
              f"screen {t['present']})")


if __name__ == "__main__":
    main()
