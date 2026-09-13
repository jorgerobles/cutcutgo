#!/usr/bin/env bash
# beep.sh — REAL audible warning before machine motion (hard safety directive).
#
# Usage: tools/beep.sh [pattern]
#   motion  (default) 5 urgent beeps — machine is about to MOVE
#   ok                 1 short beep — action done / awaiting input
#
# Tries paplay, then pw-play, then aplay, then ffplay. Generates the WAV with
# python3 stdlib (no assets needed).
set -eu

WAV="${TMPDIR:-/tmp}/cutcutgo-beep.wav"

gen() { # freq duration_ms
    python3 - "$1" "$2" "$WAV" <<'EOF'
import math, sys, wave, struct
freq, ms, path = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
rate = 44100
n = int(rate * ms / 1000)
w = wave.open(path, 'w')
w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
frames = bytearray()
for i in range(n):
    t = i / rate
    # 5 ms fade in/out to avoid clicks
    fade = min(1.0, t / 0.005, (ms / 1000 - t) / 0.005)
    v = int(28000 * fade * math.sin(2 * math.pi * freq * t))
    frames += struct.pack('<h', v)
w.writeframes(bytes(frames))
w.close()
EOF
}

pattern="${1:-motion}"
player() {
    command -v paplay >/dev/null && { paplay "$1"; return 0; }
    command -v pw-play >/dev/null && { pw-play "$1"; return 0; }
    command -v aplay   >/dev/null && { aplay -q "$1"; return 0; }
    command -v ffplay  >/dev/null && { ffplay -nodisp -autoexit -loglevel quiet "$1"; return 0; }
    echo "beep.sh: no audio player found" >&2; return 1
}

case "$pattern" in
    motion) gen 880 160; for i in 1 2 3 4 5; do player "$WAV"; sleep 0.12; done ;;
    ok)     gen 1320 220; player "$WAV" ;;
    *) echo "usage: beep.sh [motion|ok]" >&2; exit 2 ;;
esac
