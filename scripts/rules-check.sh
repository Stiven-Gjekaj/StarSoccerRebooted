#!/bin/sh
# Compares rules of the remake with the rules of the original game.
# Usage: scripts/rules-check.sh
#
# The original 4.32 must be on an Android device or emulator that adb sees.
# The script builds tools/rules/RulesProbe.java into a dex file, and runs it
# on the device with dalvikvm and the APK of the game. It runs rulecheck on
# this computer, and compares the two outputs line by line. It needs javac,
# and d8 of the build tools of the Android SDK.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
SDK=${ANDROID_HOME:-$HOME/Library/Android/sdk}
ADB=$SDK/platform-tools/adb
D8=$(ls -d "$SDK"/build-tools/*/d8 2>/dev/null | sort -V | tail -1)
PACKAGE=com.newstargames.newstarsoccer
OUT=$ROOT/build/rules
cd "$ROOT"

[ -x "$ADB" ] || { echo "rules-check: adb is not in $SDK" >&2; exit 1; }
[ -n "$D8" ] || { echo "rules-check: d8 is not in $SDK/build-tools" >&2; exit 1; }
APK=$("$ADB" shell pm path "$PACKAGE" | head -1 | sed 's/^package://' | tr -d '\r')
[ -n "$APK" ] || { echo "rules-check: the device has no $PACKAGE" >&2; exit 1; }

cmake --build build -j8 --target rulecheck >/dev/null
rm -rf "$OUT"
mkdir -p "$OUT/classes"
javac --release 11 -d "$OUT/classes" tools/rules/RulesProbe.java
"$D8" --output "$OUT" "$OUT/classes/RulesProbe.class"
"$ADB" push "$OUT/classes.dex" /data/local/tmp/rulesprobe.dex >/dev/null 2>&1
"$ADB" shell "dalvikvm64 -cp $APK:/data/local/tmp/rulesprobe.dex RulesProbe" | tr -d '\r' >"$OUT/original.txt"
build/rulecheck >"$OUT/remake.txt"

cases=$(wc -l <"$OUT/original.txt" | tr -d ' ')
if diff "$OUT/original.txt" "$OUT/remake.txt" >"$OUT/diff.txt"; then
    echo "rules-check: $cases cases agree"
else
    echo "rules-check: the rules differ (original <, remake >):" >&2
    head -40 "$OUT/diff.txt" >&2
    exit 1
fi
