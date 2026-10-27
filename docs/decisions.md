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

- **Bux stay. Nobody can buy them with real money.** The owner chose this
  on 2026-10-26, and corrected it the same night. Star Bux are the cash of
  the game: the wage goes into them (`m_bank` in `c_TPlayer`), and they buy
  NRG drinks, boots, shop items, and bets in the casino. The original also
  sold Bux in its store. The remake keeps the currency and removes only the
  store. Reason: the remake has no store, and the economy needs its cash.
- **The casino and the races take their own random numbers.** The original
  seeds its one random stream from the clock when a casino game or the
  races open, and again when a career loads. The remake seeds a second
  stream from its clock for the casino and the races, and keeps the stream
  of the career as it is. Reason: a casino game cannot be the same twice in
  the original, and the tests of the career must stay the same.

## New screens

- **The remake has new screens, made from the assets of the game.** The
  owner chose this on 2026-10-27. A new screen is a layout file in the
  layout language of the game. It names the images of the atlases and the
  macros of the game, for example `M_StandardButton`.
- **The layouts of the remake are in `layouts/` of this repository.** The
  app looks there first, then in the import. Reason: a new screen then has
  the look of the game, and a pack can replace any screen.
- **The repository holds no text of the layout files of the game.** A new
  layout imports the macros of the game from the copy of the player when
  it runs.

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

## The screens

- **A screen runs as a tree of elements, as the game does (`c_Gel`).** The
  scene makes the tree once when the screen opens, then each frame runs
  the modifiers of each element and makes the list of draws. Reason: the
  converges, the movers, the delays, and the emitters keep state from one
  frame to the next, so a list that the app makes again each frame cannot
  move as the game does.
- **The time base is the one of the game:** frames of 60 a second, from 1
  to 5 frames a draw, and 16 to 80 ms a draw. Reason: the layouts count in
  frames, and a mover moves the same distance each draw.
- **The logic of the screens (`ssr_game`) sets tweaks and chooses the next
  screen.** It does not touch the elements. Reason: the modern layouts of
  the game read the state of the game from the tweaks (`ShowIf`, `twk()`,
  `ColReadout`), so the logic needs no link to the elements.
- **The remake keeps its settings in its own file**, `settings.txt` in the
  data directory, with one `name=value` a line. Reason: the app cannot
  read the saves of the original game on a phone, so the format of the
  game gives nothing.
- **The remake keeps a career in its own file**, `saves/careerN.txt` in
  the data directory, with one `name=value` a line and the names of the
  fields of the game. Reason: the save of the game is one long string of
  1136 fields in the order of the code, and nothing can read the save of
  the original on a phone.
- **The logic of a screen asks for changes of elements by ref**, for
  example the text of `NationText` or a new entry in `NationCombo`. Reason:
  the code of the game changes its gadgets by ref in the same way
  (`c_GGadget`, `c_ACombo`).
- **A reference capture of the original is good only from a fresh start.**
  The original stops the screen under a popup (the onboarding), and an
  emulator can leave the game stuck. Reason: a capture of a stuck game
  showed a ball and clouds that did not move.

- **Until the season of 0.6, the Play button of the game menu starts a
  match against the next club of the league.** The home and the away
  match change in turn. Reason: the fixtures come with the season.
- **The remake has no intercept game (chance type 6).** Version 4.32 sets
  `m_dodribbling` to 1 for each career, so the match gives a dribble
  (type 11) in its place, and type 6 never starts.
- **After a match, the date goes on by one day, and the rest days are 14.**
  The game moves the date to the next day with a fixture, and counts the
  rest days to the next fixture. Reason: the fixtures come with the season
  in 0.6. With no next fixture, the game also gives 14 rest days.
- **The work rate does not change the relations after a match.** The game
  reads `m_workratetally` for it, but 4.32 never adds to the tally, so
  `GetOverallWorkRate` is always 0.
- **The match stats skip the interview, the news, the records, the star
  man screen, and the star shower.** Reason: these screens and effects come
  in later parts. The rating, the relation changes, and the star rating are
  as in the game.
- **The type of the player is the type that its ratings give now.** The
  game shows the last type that a rating news named, and a news comes
  only once in 60 days for each type. Reason: the news come later.
- **A drill of the trials runs on the chance of a match.** The game runs
  the training engine with the same ball, tap ball, and keeper. The drill
  adds its objects and its rules of a hit and a lost life.
- **A cone that falls goes from the view.** The game plays the fall of the
  cone. Reason: the fall comes with the training of the game menu.
- **The cups move to 0.6, the interview to 0.7, and the harness to 1.0.**
  Reason: extra time and the shoot-out need the cups, the interview needs
  the dilemmas, and the test harness needs the full game.
- **The replay keeps a full copy of the chance for each tick.** The game
  keeps a stack of frames for each object, and the stacks do not always
  line up. One copy of the chance keeps all parts in step, and the view
  draws a copy as it draws the live chance.
- **The help of the first replay (`Player.ReplayActive`) is not saved.** It
  shows again after the app starts again.
- **`AnimateProgress` moves the value of a bar but draws no highlight, no
  overlay, and no burst.** Reason: these are gadgets of the game that the
  remake does not make yet. The bar has the same length at each frame.
- **A penalty plays on the middle pitch.** The game reads `Rand(3)` for the
  side of each chance, but the penalty pitch has its goal in the middle.
- **The scene draws a CloneTweak colour into each item.** The game draws
  each gel at once, with the tweak of that moment. The remake makes all the
  items first, so each item keeps its cel colours.
- **"Energy full!" shows in a message box.** The game shows it as a quick
  message over the screen (`splashmessageNRG`). The NRG button of a match
  drinks the next can at once, because the shop is not in the remake yet.
- **A `Sheet:` that renders into a surface draws its children each frame,
  in its place.** The game draws them one time into `$PITCH` and then
  draws the surface. The picture is the same, but sharper, and the remake
  needs no render to a texture.
- **A sheet with perspective draws each image as a grid of 8 x 8 cells.**
  Reason: the renderer maps a texture in a straight line between the
  points, and the game maps it with perspective. The grid keeps the error
  near 0.1 texel.
- **Each item keeps the box of its clip.** Reason: the layers sort the items
  after the clip, so a clip that starts and ends as items can lose its
  items.

## Releases

- Each part of the roadmap is one minor release, as in Balatro4S. A release
  gets a git tag `vX.Y.Z`, a changelog entry, and a GitHub release with the
  macOS app and, from 0.3.0, the Android APK. The APK has the debug key,
  so a phone can install it.
- **An agent may make a full release with no review.** The owner asked for
  this on 2026-10-26. When a part of the roadmap is complete and its tests
  pass, the agent runs `scripts/release.sh`.
- **Version 1.0.0 needs all of `TODO.md`.** A release before that is a 0.x
  release.
- **SDL3 comes from its source, version 3.4.18.** Reason: the app then
  holds SDL3, and the iOS and Android builds need SDL3 from source.

## Identity

- **The marks of New Star Games change, and nothing else of the look.**
  The owner chose this on 2026-10-27. The new logo keeps the style of the
  original: bold red type and a white star in a red box. The words are
  STAR SOCCER REBOOTED. The TV, the newspaper, the sponsor, and the
  stadium use the name SSR (SSR TV, SSR NEWS).
- **The text of the game uses the names of the remake.** "New Star
  Soccer" becomes "Star Soccer Rebooted", "NSS" becomes "SSR", and the
  boots and balls are "SSR-" in the place of "NS-". The credits of the
  original and the names of the other New Star games stay. Reason: the
  credits tell who made the original.
- **The remake does not keep the art or the layouts of the original.** A
  mark that is part of a larger image gets a patch of new pixels
  (`art/<name>.patch.png`). A layout line gets a patch that removes it
  (`layouts/<name>.patch`).

- Display name `Star Soccer Rebooted`. Bundle ID
  `io.github.stivengjekaj.starsoccerrebooted`.
