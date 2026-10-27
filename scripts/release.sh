#!/bin/sh
# Make a release of the version in CMakeLists.txt.
# Usage: scripts/release.sh
# It runs the tests, builds the macOS app for arm64 and x86_64 and the
# Android app, pushes the commits and the tag, and publishes the GitHub
# release with the notes of CHANGELOG.md, the zip of the macOS app, and the
# APK.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
REPO=Stiven-Gjekaj/StarSoccerRebooted
VERSION=$(sed -n 's/^project(StarSoccerRebooted VERSION \([0-9.]*\) .*/\1/p' "$ROOT/CMakeLists.txt")
TAG="v$VERSION"
APP="Star Soccer Rebooted.app"
ZIP="StarSoccerRebooted-$VERSION-macos.zip"
APK="StarSoccerRebooted-$VERSION-android.apk"
cd "$ROOT"

[ -n "$VERSION" ] || { echo "release: CMakeLists.txt has no version" >&2; exit 1; }
[ -z "$(git status --porcelain)" ] || { echo "release: commit the changes first" >&2; exit 1; }
grep -q "^## $VERSION " CHANGELOG.md || { echo "release: CHANGELOG.md has no entry for $VERSION" >&2; exit 1; }
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
    echo "release: the tag $TAG is already there" >&2
    exit 1
fi

mkdir -p build
scripts/test.sh >build/release-tests.log 2>&1 || { cat build/release-tests.log >&2; exit 1; }
grep "tests passed" build/release-tests.log

# The app for both kinds of Mac, in its own build directory.
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" >/dev/null
cmake --build build/release -j8 >build/release-build.log 2>&1 || { tail -30 build/release-build.log >&2; exit 1; }
lipo -archs "build/release/$APP/Contents/MacOS/Star Soccer Rebooted"
rm -f "build/$ZIP"
(cd build/release && ditto -c -k --keepParent "$APP" "../$ZIP")

# The Android app, signed with the debug key so that a phone can install it.
(cd android && ./gradlew -q assembleDebug) >build/release-android.log 2>&1 || { tail -30 build/release-android.log >&2; exit 1; }
cp android/app/build/outputs/apk/debug/app-debug.apk "build/$APK"

git push -q
git tag -a "$TAG" -m "Star Soccer Rebooted $VERSION"
git push -q origin "$TAG"
awk -v v="## $VERSION " 'index($0, v) == 1 {f = 1; next} /^## / {f = 0} f' CHANGELOG.md >build/notes.md
cat >>build/notes.md <<NOTES

Install on macOS: unzip the file and move the app to Applications. The app
is not signed. The first time, right-click the app and choose Open.

Start the app, then drop your copy of New Star Soccer (an APK, an XAPK, or
an IPA) on the window. The app holds no file of New Star Soccer.

Install on Android: install New Star Soccer from Google Play first, then
install the APK. The app imports the game from the phone.
NOTES
gh release create "$TAG" -R "$REPO" -t "Star Soccer Rebooted $VERSION" -F build/notes.md "build/$ZIP" "build/$APK"
echo "released $TAG"
