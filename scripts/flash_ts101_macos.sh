#!/bin/bash
# Flash a Miniware TS101 in DFU mode from macOS, retrying in the same DFU session until .RDY.
# Usage: sudo bash ~/Downloads/TS101-flash.sh <hex file> <TARGET.HEX> [tries]
set -u
SRC="$1"
NAME="$2"
TRIES="${3:-8}"
BASE="${NAME%.*}"
MNT=/private/tmp/ts101
VOL=/Volumes/TS101_DFU
export COPYFILE_DISABLE=1
mkdir -p "$MNT"

find_dev() { diskutil list external physical | awk '/TS101_DFU/{print $NF}'; }

wait_for_volume() {
    for _ in $(seq 1 40); do
        [ -d "$VOL" ] && ls "$VOL" >/dev/null 2>&1 && return 0
        sleep 1
    done
    return 1
}

result() { ls "$VOL" 2>/dev/null | grep -i -E "^$BASE\.(RDY|ERR|NOT)$"; }

wait_for_volume || { echo "TS101 im DFU-Modus nicht gefunden"; exit 1; }

for n in $(seq 1 "$TRIES"); do
    DEV=$(find_dev)
    echo "Versuch $n/$TRIES (/dev/$DEV) ..."
    diskutil unmount "$VOL" >/dev/null 2>&1
    if ! mount -t msdos -o nobrowse "/dev/$DEV" "$MNT"; then
        echo "  Mount fehlgeschlagen, neuer Versuch"
        sleep 2
        wait_for_volume
        continue
    fi
    cp -X "$SRC" "$MNT/$NAME"
    sync
    # The device drops off the bus after receiving the file and comes back with the result.
    for _ in $(seq 1 20); do
        mount | grep -q " on $MNT " || break
        sleep 1
    done
    umount -f "$MNT" >/dev/null 2>&1
    sleep 2
    wait_for_volume || { echo "  Gerät nicht wieder aufgetaucht"; exit 1; }
    sleep 1
    R=$(result)
    echo "  Ergebnis: ${R:-keins}"
    case "$R" in
        *.RDY|*.rdy) echo "ERFOLG – Kabel abziehen und Lötkolben normal einschalten."; exit 0 ;;
    esac
done
echo "Nach $TRIES Versuchen kein .RDY"
exit 1
