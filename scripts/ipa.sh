#!/bin/sh
# Builds an unsigned build/StarSoccerRebooted.ipa for iOS.
# Usage: scripts/ipa.sh
# SideStore or AltStore signs the IPA when it installs it.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/build/ios
APP="Star Soccer Rebooted.app"
cd "$ROOT"

cmake -S . -B "$OUT" -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -U "ZLIB_*" >"$ROOT/build/ipa-config.log" 2>&1 ||
    { tail -30 "$ROOT/build/ipa-config.log" >&2; exit 1; }
cmake --build "$OUT" --config Release --target ssr -- -quiet -sdk iphoneos CODE_SIGNING_ALLOWED=NO \
    >"$ROOT/build/ipa-build.log" 2>&1 || { tail -30 "$ROOT/build/ipa-build.log" >&2; exit 1; }
rm -rf "$ROOT/build/ipa"
mkdir -p "$ROOT/build/ipa/Payload"
cp -R "$OUT/Release-iphoneos/$APP" "$ROOT/build/ipa/Payload/"
rm -f "$ROOT/build/StarSoccerRebooted.ipa"
(cd "$ROOT/build/ipa" && zip -qry ../StarSoccerRebooted.ipa Payload)
echo "$ROOT/build/StarSoccerRebooted.ipa"
