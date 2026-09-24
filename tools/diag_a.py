#!/usr/bin/env python3
"""Non-interactive A-axis capture + steps/rev estimation.

Sends $AT (spike 'cap' port: continuous A spin sampling encoder steps + GREEN
reflectance into a ring buffer, CSV dump). Optionally positions Z2 first
($ZL / $ZU=400) so the chamfer sits in the sensor light path.

Prints one JSON object on stdout. The two slots are 180 deg apart, so
steps_per_rev = 2 * median(slot-to-slot gap).

Usage:
  tools/diag_a.py [--dev /dev/...] [--prep-z2] [--slot-th 30] [--peak-th 70]
                  [--timeout 60]
"""
import argparse
import fcntl
import glob
import json
import os
import re
import sys
import time

import serial

LOCK_DIR = "/tmp"


def detect_port(requested):
    if requested:
        return requested
    for pat in ("*cutcutgo*", "*pic32*", "*cdc*"):
        for p in sorted(glob.glob("/dev/serial/by-id/" + pat)):
            if not re.search(r"if\d+$", p):
                return p
    cands = sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*"))
    if len(cands) == 1:
        return cands[0]
    if not cands:
        raise SystemExit("no serial device found (machine in bootloader mode has none)")
    raise SystemExit("multiple candidates, pick one with --dev: " + ", ".join(cands))


def median(xs):
    if not xs:
        return None
    s = sorted(xs)
    n = len(s)
    mid = n // 2
    return s[mid] if n % 2 else (s[mid - 1] + s[mid]) // 2


def find_runs(vals, pred):
    """Runs of consecutive indices where pred(value) is true; return (lo,hi)."""
    runs = []
    start = None
    prev = None
    for i, v in enumerate(vals):
        if pred(v):
            if start is None:
                start = i
            prev = i
        else:
            if start is not None:
                runs.append((start, prev))
                start = prev = None
    if start is not None:
        runs.append((start, prev))
    return runs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dev")
    ap.add_argument("--prep-z2", action="store_true",
                    help="lower Z2 ($ZL) then retract 400 ($ZU=400) before capture")
    ap.add_argument("--slot-th", type=int, default=30)
    ap.add_argument("--peak-th", type=int, default=70)
    ap.add_argument("--timeout", type=float, default=60.0)
    args = ap.parse_args()

    dev = detect_port(args.dev)
    lock = open(os.path.join(LOCK_DIR, "cutcutgo-diag-%s.lock" % os.path.basename(dev)), "w")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        raise SystemExit("serial busy (lock %s held)" % lock)

    try:
        ser = serial.Serial(dev, 115200, timeout=0.1)
    except serial.SerialException as e:
        raise SystemExit("cannot open %s: %s" % (dev, e))

    buf = b""

    def read_lines(deadline):
        nonlocal buf
        buf += ser.read(256)
        out = []
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            out.append(line.decode("utf-8", "replace").rstrip("\r"))
        return out

    def drain():
        nonlocal buf
        ser.reset_input_buffer()
        buf = b""

    def send(cmd):
        ser.write(cmd.encode() + b"\r\n")
        ser.flush()

    if args.prep_z2:
        drain()
        send("$ZL")
        t = time.time() + 10
        while time.time() < t:
            for l in read_lines(t):
                if "Z2:DOWN" in l or "LOWER_FAIL" in l:
                    break
            else:
                continue
            break
        time.sleep(1.5)  # spring rebound settle (matches $HA precondition)
        send("$ZU=400")
        time.sleep(2.0)

    drain()
    send("$AT")

    steps = []
    z2s = []
    refl = []
    done = False
    header = {}
    deadline = time.time() + args.timeout
    while time.time() < deadline:
        for line in read_lines(deadline):
            line = line.strip()
            if not line:
                continue
            if line.startswith("[ATDONE"):
                done = True
                break
            m = re.match(r"^\[AT\] total=(\d+) dump=(\d+)$", line)
            if m:
                header = {"total": int(m.group(1)), "dump": int(m.group(2))}
                continue
            m = re.match(r"^(-?\d+),(-?\d+),(-?\d+)$", line)
            if m:
                steps.append(int(m.group(1)))
                z2s.append(int(m.group(2)))
                refl.append(int(m.group(3)))
                continue
        if done:
            break

    ser.close()

    if not steps:
        raise SystemExit("no capture data (header=%r)" % header)

    total_steps = steps[-1] - steps[0] if len(steps) > 1 else 0
    z2_drift = (max(z2s) - min(z2s)) if z2s else 0
    rmin = min(refl)
    rmax = max(refl)

    slot_runs = find_runs(refl, lambda r: r < args.slot_th)
    peak_runs = find_runs(refl, lambda r: r > args.peak_th)
    slot_centers = [(steps[a] + steps[b]) // 2 for a, b in slot_runs]
    gaps = [slot_centers[i + 1] - slot_centers[i]
            for i in range(len(slot_centers) - 1)]
    half = median(gaps)
    full = None
    if len(slot_centers) >= 3:
        full = slot_centers[-1] - slot_centers[0]

    print(json.dumps({
        "ok": done,
        "header": header,
        "samples": len(steps),
        "total_steps": total_steps,
        "z2_drift": z2_drift,
        "refl": {"min": rmin, "max": rmax, "span": rmax - rmin},
        "slot_th": args.slot_th,
        "peak_th": args.peak_th,
        "slot_centers": slot_centers,
        "slot_count": len(slot_runs),
        "slot_gaps": gaps,
        "half_rev_median": half,
        "steps_per_rev": (2 * half) if half else None,
        "full_rev": full,
        "peak_runs": [[steps[a], steps[b]] for a, b in peak_runs],
        "peak_widths": [steps[b] - steps[a] for a, b in peak_runs],
    }, indent=2))


if __name__ == "__main__":
    main()
