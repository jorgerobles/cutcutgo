#!/usr/bin/env python3
"""cutcutgo serial console: send/monitor/waitfor against the machine's USB CDC.

Exit codes: 0 ok, 2 device/lock unavailable, 3 timeout.
"""
import argparse
import fcntl
import glob
import os
import re
import serial
import sys
import time

LOCK_DIR = "/tmp"


def detect_port(requested):
    if requested:
        return requested
    for pat in ("*cutcutgo*", "*pic32*", "*cdc*"):
        for p in sorted(glob.glob("/dev/serial/by-id/" + pat)):
            return p
    cands = sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*"))
    if len(cands) == 1:
        return cands[0]
    if not cands:
        err("no serial device found (machine in bootloader mode has none)")
    err("multiple candidates, pick one with --dev: " + ", ".join(cands))


def err(msg):
    print(msg, file=sys.stderr)
    sys.exit(2)


class Console:
    def __init__(self, dev):
        name = os.path.basename(dev)
        self.lock_path = os.path.join(LOCK_DIR, "cutcutgo-tty-%s.lock" % name)
        self.lock_fd = open(self.lock_path, "w")
        try:
            fcntl.flock(self.lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            err("console busy: lock %s held (operator terminal or other tty.py?)" % self.lock_path)
        try:
            self.ser = serial.Serial(dev, 115200, timeout=0.1)
        except serial.SerialException as e:
            err("cannot open %s: %s" % (dev, e))
        self.buf = b""

    def read_lines(self):
        self.buf += self.ser.read(256)
        out = []
        while b"\n" in self.buf:
            line, self.buf = self.buf.split(b"\n", 1)
            out.append(line.decode("utf-8", "replace").rstrip("\r"))
        return out

    def drain(self):
        self.ser.reset_input_buffer()
        self.buf = b""

    def send_line(self, cmd):
        self.ser.write(cmd.encode() + b"\r\n")
        self.ser.flush()


def cmd_send(args):
    con = Console(detect_port(args.dev))
    deadline = time.time() + args.timeout
    con.drain()
    con.send_line(args.command)
    if args.echo:
        print("> " + args.command)
    while time.time() < deadline:
        for line in con.read_lines():
            print(line)
            if line == "ok" or line.startswith("error"):
                return 0
    print("TIMEOUT after %ss" % args.timeout, file=sys.stderr)
    return 3


def cmd_monitor(args):
    con = Console(detect_port(args.dev))
    deadline = time.time() + args.secs
    while time.time() < deadline:
        for line in con.read_lines():
            print(line, flush=True)
    return 0


def cmd_waitfor(args):
    con = Console(detect_port(args.dev))
    rx = re.compile(args.pattern)
    deadline = time.time() + args.timeout
    while time.time() < deadline:
        for line in con.read_lines():
            print(line, flush=True)
            if rx.search(line):
                return 0
    print("TIMEOUT after %ss waiting for %r" % (args.timeout, args.pattern), file=sys.stderr)
    return 3


def main():
    ap = argparse.ArgumentParser(description="cutcutgo serial console")
    ap.add_argument("--dev", help="force serial device (default: autodetect)")
    sub = ap.add_subparsers(dest="cmdname", required=True)

    p_send = sub.add_parser("send", help="send a command, capture until ok/error/timeout")
    p_send.add_argument("command")
    p_send.add_argument("--timeout", type=float, default=10.0)
    p_send.add_argument("--echo", action="store_true", help="print the sent command")
    p_send.set_defaults(fn=cmd_send)

    p_mon = sub.add_parser("monitor", help="raw stream for N seconds")
    p_mon.add_argument("--secs", type=float, default=10.0)
    p_mon.set_defaults(fn=cmd_monitor)

    p_wait = sub.add_parser("waitfor", help="print lines until regex matches")
    p_wait.add_argument("pattern")
    p_wait.add_argument("--timeout", type=float, default=30.0)
    p_wait.set_defaults(fn=cmd_waitfor)

    args = ap.parse_args()
    sys.exit(args.fn(args))


if __name__ == "__main__":
    main()
