# Star Soccer Rebooted

![Star Soccer Rebooted](docs/banner.jpg)

A remake of New Star Soccer for iOS, Android, macOS, Windows, and Linux.
It uses C99 and SDL3.

> [!IMPORTANT]
> This is a fan project. It is not from New Star Games. The repository
> holds no file of New Star Soccer. You need your own copy.

## Status

The app runs on macOS, iOS, and Android. It imports the assets from your
copy and runs the screens of the game from the layout files of your copy,
with their animations: the main menu, the options, and the about screen
work. The career and the match are not there yet.

## You need a copy of New Star Soccer

The art, the sounds, the fonts, and the text of New Star Soccer belong to
New Star Games. The app reads them from your copy. New Star Soccer is free
on Google Play and on the App Store.

The app accepts these files:

- an APK, for example from `adb pull` on your Android phone. On Android
  the app imports the game that is installed on the phone.
- an XAPK, which holds the APK
- an IPA that you decrypted from your own install

The reference version is the Android release 4.32.

## Build

You need CMake. The build gets SDL3 3.4.18 and builds it from its source.
On macOS:

    brew install cmake
    cmake -S . -B build
    cmake --build build

For Android, open `android/` in Android Studio, or run
`./gradlew assembleDebug` in it. For iOS, make an Xcode project with
`cmake -S . -B build/ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS
-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0`. Add `-DCMAKE_OSX_SYSROOT=iphonesimulator` for
the simulator.

## Run

Give the app the path of your copy:

    "./build/Star Soccer Rebooted.app/Contents/MacOS/Star Soccer Rebooted" path/to/NewStarSoccer.xapk

You can also drop the file on the window. The app copies the assets into
its data directory. On macOS this is
`~/Library/Application Support/StivenGjekaj/StarSoccerRebooted/game`.
After the first import, start the app with no path.

To save the first frame as a PNG and quit:

    SSR_SHOT=frame.png "./build/Star Soccer Rebooted.app/Contents/MacOS/Star Soccer Rebooted"

Add `SSR_SHOT_MS=3000` to save the frame 3 seconds later, when the screen
has moved in.

To show another screen of the game, give its layout file:

    SSR_SCREEN=options.uis "./build/Star Soccer Rebooted.app/Contents/MacOS/Star Soccer Rebooted"

To press a point of the screen 1.5 seconds after the first frame:

    SSR_PRESS=500,540 "./build/Star Soccer Rebooted.app/Contents/MacOS/Star Soccer Rebooted"

## Test

    ctest --test-dir build --output-on-failure

The tests build the archives that they need. They do not read a copy of
the game.

## Decisions

[docs/decisions.md](docs/decisions.md) holds each decision and its reason.

## Licence

The code is under the [MIT licence](LICENSE).
