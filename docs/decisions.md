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

## What the remake changes

- **The aim of the remake is a world that feels alive.** The owner chose
  this on 2026-10-26. The world has rivalries, events, comebacks, and
  histories. Other players exist and have careers of their own. Reason: in
  the original, the player of the user is the only player with a career.
  `c_TPlayer` has one instance (`g_player`), `c_StatHistory` records only
  that player, and a club is mostly a strength number from `Clubs.txt`.
- **The living world starts in 0.6, with the season.** The owner chose this
  on 2026-10-26. Reason: the fixtures, the tables, and the transfers use
  the squads and the histories. A later change to them is a rewrite.
- **Version 1.0 has all the features in `TODO.md`.** The owner chose this
  on 2026-10-26.

- **The remake has no Bux.** The owner chose this on 2026-10-26. The
  original sells Bux, and it also gives some after a match. The things that
  Bux buy in the original come from play or from money in the remake.
  Reason: the remake has no store, so a second currency has no purpose.

## The database and the packs

- **The app reads the world from data packs.** The owner asked for a 2026
  update and for real logos and real player names, as the fans want. The
  default world is the 2014 data of the original, from the import.
- **This repository holds no real logos and no real player names.** A pack
  holds them, and the player installs it from a file. Reason: club logos
  are trademarks, and the names of players are licensed (FIFPro licenses
  them for games). The model is the same as the option files of PES: the
  app is clean, and the community makes the packs.
- **The 2026 data pack is a separate project.** Reason: it changes each
  season and has its own legal risk. The app stays public and MIT.
- **A separate index repository lists the packs.** The owner chose this on
  2026-10-26. Each entry gives the name, the author, the version, the
  download URL, the size, and the SHA-256 of a pack. A person adds or
  changes an entry with a pull request.
- **The index holds no pack files.** Each author hosts the pack, for
  example as a release of their own repository. Reason: a takedown then
  goes to that pack, not to the account of the owner. The owner removes
  one entry from the index.
- **A check in CI tests each pull request.** It downloads the pack and
  checks the SHA-256, the size, and the format.
- **The app reads the index and shows a pack browser.** It downloads a
  pack, checks the SHA-256, and imports it with the same path checks as
  the import of the game.
- **A pack holds only data.** It holds no scripts and no code. Reason: a
  pack from a stranger cannot harm the device.
- **The builds for the App Store and Google Play have no pack browser.**
  They import a pack only from a file. Reason: a store can refuse an app
  that downloads trademarks and licensed names. The desktop builds and the
  sideloaded builds have the browser.

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
- **stb_image decodes the PNG and JPG images.** Reason: SDL3 decodes only
  PNG, and the game also has JPG images. stb_image is one file, and it is
  public domain.
- **The screen is portrait, 640 x 960 logical pixels.** Reason: the game is
  portrait, and its 2x images fill 640 x 960. The 1x images are 320 x 480.

## Releases

- Each part of the roadmap is one minor release, as in Balatro4S. A release
  gets a git tag `vX.Y.Z`, a changelog entry, and a GitHub release with the
  macOS app.
- **An agent may make a full release with no review.** The owner asked for
  this on 2026-10-26. When a part of the roadmap is complete and its tests
  pass, the agent runs `scripts/release.sh`.
- **Version 1.0.0 needs all of `TODO.md`.** A release before that is a 0.x
  release.
- **SDL3 comes from its source, version 3.4.18.** Reason: the app then
  holds SDL3, and the iOS and Android builds need SDL3 from source.

## Identity

- Display name `Star Soccer Rebooted`. Bundle ID
  `io.github.stivengjekaj.starsoccerrebooted`.
