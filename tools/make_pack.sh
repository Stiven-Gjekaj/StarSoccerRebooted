#!/bin/sh
# Make the zip file of a data pack.
# Usage: tools/make_pack.sh <pack folder> [zip file]
# Example: tools/make_pack.sh community/season-2025-26
# The default zip file is build/<id>.zip, with the id of pack.txt.
# The zip holds only the kinds of files that the app installs: .txt, .png,
# .jpg, .uis, .uih, and .patch. Other files, for example README.md, stay
# out of the zip. pack.txt is at the top of the zip.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PACK=$1
[ -n "$PACK" ] || { echo "usage: tools/make_pack.sh <pack folder> [zip file]" >&2; exit 1; }
[ -f "$PACK/pack.txt" ] || { echo "make_pack: $PACK has no pack.txt" >&2; exit 1; }
PACK=$(cd "$PACK" && pwd)

ID=$(sed -n 's/^id=//p' "$PACK/pack.txt" | tr -d '\r' | head -n 1)
echo "$ID" | grep -Eq '^[a-z0-9_-]{1,47}$' || { echo "make_pack: the id \"$ID\" is not correct" >&2; exit 1; }

ZIP=${2:-$ROOT/build/$ID.zip}
mkdir -p "$(dirname "$ZIP")"
ZIP=$(cd "$(dirname "$ZIP")" && pwd)/$(basename "$ZIP")
rm -f "$ZIP"

# A file of more than 16 MB stops the install.
BIG=$(cd "$PACK" && find . -type f -size +16M)
[ -z "$BIG" ] || { echo "make_pack: these files are more than 16 MB: $BIG" >&2; exit 1; }

cd "$PACK"
find . -type f \( -name '*.txt' -o -name '*.png' -o -name '*.jpg' -o -name '*.uis' -o -name '*.uih' \
    -o -name '*.patch' \) ! -name '.*' | sed 's|^\./||' | LC_ALL=C sort | zip -X -q "$ZIP" -@
COUNT=$(unzip -Z1 "$ZIP" | wc -l | tr -d ' ')
echo "$ZIP: $COUNT files"
