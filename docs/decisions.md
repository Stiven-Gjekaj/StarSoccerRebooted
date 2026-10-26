# Decisions

This file records each choice of the owner and each technical decision.
Each entry gives the reason. The owner made these choices on 2026-10-26.

## What the app is

- **The name is Star Soccer Rebooted.** The owner chose it on 2026-10-26.
  The repository folder keeps the name `NewStarSoccerRebooted`.
- **Star Soccer Rebooted is a remake of New Star Soccer** for modern
  platforms: iOS, Android, macOS, Windows, and Linux. Reason: New Star Games
  seldom updates the game now.
- **The game logic is a native rewrite.** The app does not run the code of
  the original.
- **The original code is the reference for the rules.** The original is
  Monkey X code. On Android it is Java in the `classes*.dex` files. `jadx`
  turns these files into Java that a person can read.
- **The reference version is the Android release 4.32** (version code 138).

## The assets of New Star Soccer

- **The repository holds no file of New Star Soccer.** The licence of the
  new code is MIT. The art, the sounds, the fonts, and the text of New Star
  Soccer belong to New Star Games.
- **The app reads the assets from the copy of the player.** At the first
  start, the player gives the app a file. The app copies the assets into its
  data directory. Reason: the same model as Balatro4S. The app can then go
  to GitHub and to the stores with no file of the game.
- **The app accepts these files:**
  - an APK. The assets are under `assets/monkey/`.
  - an XAPK. This is a zip that holds the APK.
  - an IPA. The assets are under `Payload/<name>.app/data/`.
- **The tests do not read a copy of the game.** A test builds the archive
  that it needs.
- **Do not use a cracked copy as a source.** A cracked copy can hold
  injected libraries. Reason: the model above depends on a real copy of the
  player. The game is free on Google Play and on the App Store.

## Toolchain

- **C99 and SDL3.** Reason: one code base reaches all five platforms. The
  owner wrote Balatro4S in C, so the same patterns apply.
- **CMake builds the app and the tests.** Reason: SDL3 uses CMake for iOS
  and Android.
- **The zip reader comes from Balatro4S** (`bo_zip`, MIT, the same owner).
  It uses zlib.
- **The tests are plain C programs** that CTest runs. Reason: XCTest runs
  only on Apple platforms.
- The prefix of the C names is `ssr`.

## Identity

- Display name `Star Soccer Rebooted`. Bundle ID
  `io.github.stivengjekaj.starsoccerrebooted`.
