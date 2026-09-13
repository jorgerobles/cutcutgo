#!/usr/bin/env bash
# flash.sh — flash a UF2 into the Cricut via the bootloader mass-storage drive.
#
# Usage: tools/flash.sh <file.uf2> [timeout_s]
#
# Handles the BL lifecycle race: waits for the "Cutcutgo" drive, mounts,
# copies fast, treats the device vanishing (BL auto-reboot) as SUCCESS, then
# waits for the SAME machine (matched by USB hardware id, not port) to
# re-enumerate its CDC serial port in app mode.
set -u

UF2="${1:?usage: flash.sh <file.uf2> [timeout_s]}"
TMO="${2:-60}"
[ -f "$UF2" ] || { echo "ERR: no such file: $UF2"; exit 2; }

byid_cdc() {
    ls /dev/serial/by-id/ 2>/dev/null | grep -iE 'microchip|cutcutgo|cdc' || true
}

# Baseline: CDC ids present right now (to detect the machine's re-enumeration,
# not some other device). Empty if the machine is off / in BL.
BASELINE=$(byid_cdc)

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

# Wait for the machine's CDC id to re-enumerate: an id NOT seen in the
# baseline, or the baseline id with the BL drive already gone (reboot done).
deadline=$(( $(date +%s) + TMO ))
while [ "$(date +%s)" -lt "$deadline" ]; do
    now=$(byid_cdc)
    if [ -n "$now" ]; then
        if [ -z "$BASELINE" ]; then
            echo "CDC back: $now"
            exit 0
        fi
        newid=$(comm -13 <(echo "$BASELINE" | sort) <(echo "$now" | sort) | head -1)
        if [ -n "$newid" ]; then
            echo "CDC back (new id): $newid"
            exit 0
        fi
        if ! lsblk -rno NAME,LABEL 2>/dev/null | grep -qi cutcutgo; then
            sameid=$(comm -12 <(echo "$BASELINE" | sort) <(echo "$now" | sort) | head -1)
            echo "CDC back (baseline id, BL gone): $sameid"
            exit 0
        fi
    fi
    sleep 0.5
done
echo "WARN: CDC (by-id) did not return in ${TMO}s (machine off?)"
exit 0
