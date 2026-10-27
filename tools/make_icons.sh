#!/bin/zsh
# Makes the icons of the app from assets/icon.png (a square image of at
# least 1024 pixels, with no alpha). Run it on macOS after a change of the
# image. It writes:
#   cmake/macos/AppIcon.icns                          the icon of macOS
#   cmake/ios/Assets.xcassets/AppIcon.appiconset      the icon of iOS
#   android/app/src/main/res/mipmap-*/ic_launcher.png  the icons of Android
#   cmake/windows/app.ico                             the icon of Windows
set -e
cd "$(dirname "$0")/.."
SRC=assets/icon.png
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# macOS: an icon set, then iconutil.
SET=$TMP/AppIcon.iconset
mkdir -p "$SET" cmake/macos
for s in 16 32 128 256 512; do
    sips -z $s $s "$SRC" --out "$SET/icon_${s}x${s}.png" >/dev/null
    sips -z $((s * 2)) $((s * 2)) "$SRC" --out "$SET/icon_${s}x${s}@2x.png" >/dev/null
done
iconutil -c icns "$SET" -o cmake/macos/AppIcon.icns

# iOS: one image of 1024 pixels in an asset catalog.
CAT=cmake/ios/Assets.xcassets
mkdir -p "$CAT/AppIcon.appiconset"
sips -z 1024 1024 "$SRC" --out "$CAT/AppIcon.appiconset/icon-1024.png" >/dev/null
printf '{\n  "info" : {\n    "author" : "xcode",\n    "version" : 1\n  }\n}\n' > "$CAT/Contents.json"
cat > "$CAT/AppIcon.appiconset/Contents.json" <<'JSON'
{
  "images" : [
    {
      "filename" : "icon-1024.png",
      "idiom" : "universal",
      "platform" : "ios",
      "size" : "1024x1024"
    }
  ],
  "info" : {
    "author" : "xcode",
    "version" : 1
  }
}
JSON

# Android: the launcher icon at each density.
for pair in mdpi:48 hdpi:72 xhdpi:96 xxhdpi:144 xxxhdpi:192; do
    sips -z ${pair#*:} ${pair#*:} "$SRC" --out "android/app/src/main/res/mipmap-${pair%%:*}/ic_launcher.png" >/dev/null
done

# Windows: an ICO file with PNG images of 16 to 256 pixels.
mkdir -p cmake/windows
for s in 16 24 32 48 64 128 256; do sips -z $s $s "$SRC" --out "$TMP/ico_$s.png" >/dev/null; done
python3 - "$TMP" cmake/windows/app.ico <<'PY'
import struct, sys
sizes = [16, 24, 32, 48, 64, 128, 256]
images = [open(f'{sys.argv[1]}/ico_{s}.png', 'rb').read() for s in sizes]
out = struct.pack('<HHH', 0, 1, len(sizes))
offset = 6 + 16 * len(sizes)
for s, data in zip(sizes, images):
    out += struct.pack('<BBBBHHII', s % 256, s % 256, 0, 0, 1, 32, len(data), offset)
    offset += len(data)
open(sys.argv[2], 'wb').write(out + b''.join(images))
PY
echo "The icons are made."
