# Star Soccer Rebooted

A remake of New Star Soccer for iOS, Android, macOS, Windows, and Linux.
It uses C99 and SDL3.

> [!IMPORTANT]
> This is a fan project. It is not from New Star Games. The repository
> holds no file of New Star Soccer. You need your own copy.

## Status

The app imports the assets from your copy and shows the logo of the game.
The game itself is not there yet.

## You need a copy of New Star Soccer

The art, the sounds, the fonts, and the text of New Star Soccer belong to
New Star Games. The app reads them from your copy. New Star Soccer is free
on Google Play and on the App Store.

The app accepts these files:

- an APK, for example from `adb pull` on your Android phone
- an XAPK, which holds the APK
- an IPA that you decrypted from your own install

The reference version is the Android release 4.32.

## Build

You need CMake and SDL3. On macOS:

    brew install cmake sdl3
    cmake -S . -B build
    cmake --build build

## Run

Give the app the path of your copy:

    ./build/ssr path/to/NewStarSoccer.xapk

You can also drop the file on the window. The app copies the assets into
its data directory. On macOS this is
`~/Library/Application Support/StivenGjekaj/StarSoccerRebooted/game`.
After the first import, start the app with no path.

To save the first frame as a PNG and quit:

    SSR_SHOT=frame.png ./build/ssr

## Test

    ctest --test-dir build --output-on-failure

The tests build the archives that they need. They do not read a copy of
the game.

## Decisions

[docs/decisions.md](docs/decisions.md) holds each decision and its reason.

## Licence

The code is under the [MIT licence](LICENSE).
