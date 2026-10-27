#!/bin/sh
# Compares one screen of the remake with a screenshot of the original game.
#
#     scripts/screen-diff.sh screen "messages" reference.png out-directory
#
# The screen is the name of a layout, for example "gamemenu". The messages
# go to SSR_MESSAGES, so that the app goes to the screen. Take the
# reference on a phone or an emulator of 1080x2400, for example with
# "adb exec-out screencap -p > reference.png". The script writes
# remake.png and heat.png into the out directory and prints the bands
# that differ.
set -eu
screen=$1
messages=$2
reference=$3
out=$4
root=$(cd "$(dirname "$0")/.." && pwd)
app="$root/build/Star Soccer Rebooted.app/Contents/MacOS/Star Soccer Rebooted"
mkdir -p "$out"
rm -f "$out/remake.png"
SSR_WINDOW=360x800 SSR_MESSAGES="$messages" SSR_SHOT_SCREEN="$screen" SSR_SHOT_MS="${SSR_SHOT_MS:-3000}" \
    SSR_SHOT="$out/remake.png" "$app" > "$out/remake.log" 2>&1 &
pid=$!
i=0
while [ ! -f "$out/remake.png" ] && [ $i -lt 40 ]; do
    sleep 1
    i=$((i + 1))
done
kill $pid 2>/dev/null || true
if [ ! -f "$out/remake.png" ]; then
    echo "The app did not get to the screen $screen. See $out/remake.log." >&2
    exit 1
fi
"$root/build/screendiff" "$reference" "$out/remake.png" "$out/heat.png"
