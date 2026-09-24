#!/usr/bin/env python3
"""Map the A-axis reflectance signature from tty, using only existing commands
($DBGMOTOR=A,CW,<ms> to rotate, $ARQ to read refl). No firmware change.

Prints a table of (cumulative_rotation_ms, refl) so slots/chamfers can be seen.
"""
import argparse
import fcntl
import glob
import os
import re
import sys
import time

import serial


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
    raise SystemExit("no/multiple serial device")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dev")
    ap.add_argument("--step-ms", type=int, default=500)
    ap.add_argument("--steps", type=int, default=24)
    ap.add_argument("--timeout", type=float, default=60.0)
    args = ap.parse_args()

    dev = detect_port(args.dev)
    lock = open(os.path.join("/tmp", "cutcutgo-map-%s.lock" % os.path.basename(dev)), "w")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        raise SystemExit("serial busy")

    ser = serial.Serial(dev, 115200, timeout=0.1)
    buf = b""

    def read_until_idle(timeout):
        nonlocal buf
        deadline = time.time() + timeout
        out = []
        while time.time() < deadline:
            buf += ser.read(256)
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                out.append(line.decode("utf-8", "replace").rstrip("\r"))
            if any(l.startswith("ok") or l.startswith("error") for l in out):
                break
        return out

    def cmd(c, timeout=5.0):
        ser.reset_input_buffer()
        buf = b""
        ser.write(c.encode() + b"\r\n")
        ser.flush()
        return read_until_idle(timeout)

    refls = []
    total_ms = 0
    for i in range(args.steps):
        out = cmd("$ARQ")
        m = next((re.search(r"AREFL:(-?\d+)", l) for l in out if "AREFL" in l), None)
        refl = int(m.group(1)) if m else None
        refls.append(refl)
        print("rot_ms=%6d refl=%s" % (total_ms, refl), flush=True)
        cmd("$DBGMOTOR=A,CW,%d" % args.step_ms, timeout=args.step_ms / 1000 + 2)
        total_ms += args.step_ms

    ser.close()
    vals = [r for r in refls if r is not None]
    print("min=%s max=%s span=%s n=%d" % (
        min(vals) if vals else None, max(vals) if vals else None,
        (max(vals) - min(vals)) if vals else None, len(vals)))


if __name__ == "__main__":
    main()
