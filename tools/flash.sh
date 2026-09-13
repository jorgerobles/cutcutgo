#!/usr/bin/env bash
# flash.sh — flash a UF2 into the Cricut via the bootloader mass-storage drive.
#
# Usage: tools/flash.sh <file.uf2> [timeout_s]
#
# Handles the BL lifecycle race: waits for the "Cutcutgo" drive, mounts,
# copies fast, treats the device vanishing (BL auto-reboot) as SUCCESS, and
# waits for the CDC serial port to come back in app mode.
set -u

UF2="${1:?usage: flash.sh <file.uf2> [timeout_s]}"
TMO="${2:-60}"
[ -f "$UF2" ] || { echo "ERR: no such file: $UF2"; exit 2; }

dev=""
deadline=$(( $(date +%s) + TMO ))
while [ "$(date +%s)" -lt "$deadline" ]; do
    dev=$(lsblk -rno NAME,LABEL 2>/dev/null | awk '$2=="Cutcutgo"{print "/dev/"$1; exit}')
    [ -n "$dev" ] && break
    sleep 0.5
done
[ -n "$dev" ] || { echo "ERR: bootloader drive not found (BL mode?)"; exit 2; }
echo "BL drive: $dev"

mp=""
deadline=$(( $(date +%s) + 10 ))
while [ "$(date +%s)" -lt "$deadline" ]; do
    mp=$(udisksctl mount -b "$dev" 2>/dev/null | sed -n 's/.*Mounted .* at \(.*\)\./\1/p')
    if [ -z "$mp" ]; then
        mp=$(lsblk -rno MOUNTPOINT "$dev" 2>/dev/null | head -1)
    fi
    [ -n "$mp" ] && break
    sleep 0.3
done
[ -n "$mp" ] || { echo "ERR: mount failed"; exit 2; }
echo "mounted: $mp"

cp "$UF2" "$mp/" 2>/dev/null
rc=$?
sync 2>/dev/null
echo "copy rc=$rc (device vanishing now = flash OK)"
sleep 2

deadline=$(( $(date +%s) + TMO ))
while [ "$(date +%s)" -lt "$deadline" ]; do
    for d in /dev/ttyACM* /dev/ttyUSB*; do
        if [ -e "$d" ]; then
            echo "CDC back: $d"
            exit 0
        fi
    done
    sleep 0.5
done
echo "WARN: CDC did not return in ${TMO}s (machine off?)"
exit 0
