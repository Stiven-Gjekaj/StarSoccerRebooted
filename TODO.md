# Roadmap

Each version is a part of the game that you can play. The reference is the
Android release 4.32. The names in brackets are the classes and the screens
in the code of the original (`c_TMatch`, `TScreen_Casino`).

## 0.1 Import (done)

- [x] Read zip archives
- [x] Import the assets from an APK, an XAPK, or an IPA
- [x] Show an image from the import

## 0.2 Assets (done)

- [x] Decode JPG images (the backgrounds)
- [x] Load the atlases of the interface (`NewGUI/Atlases`)
- [x] Draw text with the TTF fonts of the game (`Fonts`)
- [x] Draw the star of `Fonts.txt` (`Inject:`): the bux icon in a text
- [x] The glyph tables of the other alphabets (`Fonts/Glyphs`)
- [x] Play the sounds (OGG from an APK, MP3 from an IPA). The game has no
      music.
- [x] Read the screen layouts (`.uis`, `.uih`) and draw a screen from them
- [x] Read the season data (`MyData/v2025b`: clubs, nations, competitions,
      promotion places)
- [x] Read the text of each language (`MyData/Languages`)

## The engine of the screens

- [x] Read the layouts of the remake from `layouts/` first, then from the
      import
- [x] Run a screen as a tree of elements, as the game does (`c_Gel`):
      animations, tweens, converges, delays, press feedback, movers,
      spinners, faders, lifetimes, image sequences, templates, instances,
      particle emitters, the crowd, layers, and the colour stack
- [x] The time base of the game: frames of 60 a second
- [x] Slanted text, the cels of an image, additive and gloss blends
- [x] Embedded draws (`Embed:`, `DrawEmbedded:`), readouts, selectors,
      `if` and `else`, the actions of a click, lists that scroll with a
      swipe, and lists that the code fills from a template
- [x] The expression language of the layouts: `set(...)`, `fmt()`,
      `oneof()`, `lerp()`, `if`, and `%` variables
- [x] The colours of the slots of a Spine gel (`gel.slotcol.rgba()`) and
      of the text shadow (`gel.text.shadcol`)
- [x] The news ticker shell (`ShowShell(Ticker)`, `ticker.uis`) with the
      welcome texts
- [x] The texts of the ticker that tell of the career (low energy, the
      next match, relations), and the speed-up when you touch the tape
- [ ] The other shells: the borders and the overlays
- [ ] The rest of the layout commands: font shadows (`FontShadow`), the
      fade of a screen (`Dissolve:`), global emitters, cached draws
      (`DrawCached:`), and the 3D models of the match
- [x] The rule of the game for a line that is wider than its box: it
      moves from side to side inside the box

## 0.3 Platforms (done)

- [x] Build for iOS
- [x] Build for Android. The app imports the game that is installed on
      the device.
- [x] Touch input, and the mouse as touch on the desktop
- [x] Save files in the data directory of each platform. They came with
      0.4.

## 0.4 A new career

- [x] Save files in the data directory of each platform
- [x] Main menu, options, and save slots (`MainMenu`, `Options`,
      `SaveSlot`): load, delete, and new career
- [x] Make a player: name, gender, nation (`ChooseClub`, `ChooseNation`).
      In the modern look the gender is the look.
- [x] The league country. The game chooses the club, as in the original
      (`GetProspectiveClubs`).
- [x] The first contract and the start skills. The original has no
      contract screen for a new career.
- [x] The five trials that set the first skills
- [x] The news of the first contract and the star transition (`WebPage`,
      `StarTransition`)
- [x] The game menu between matches (`GameMenu`)

## 0.5 The match

- [x] The ball physics (`c_TBall`, `c_TBall2`): the flight, the posts and
      the nets, and the tap ball with its time and its control boots
- [x] The players on the pitch (`c_TMatchPlayer`): the places of the keeper,
      the defenders, and the team-mates, their runs, the dives of the
      keeper, and offside
- [x] One chance of the player: aim, tap, flight, saves, passes, team-mate
      shots, and goals
- [x] The flow of a match (`c_TMatch`): the clock, the chances of the teams,
      the commentary, the half time, the full time, the energy, and the
      substitutions
- [x] The match screen (`match.uis`): the names, the scores, the time, the
      speed, the work rate, the focus, and the commentary
- [x] The view of a chance: the 3D pitch (`Sheet:`), the players in their
      kits, the ball, the aim arrow, the tap ball in front of the stadium,
      and the banner of the result
- [x] The render of a `Sheet:` into a surface (`Surf:`). The remake draws
      its children each frame in its place, which gives the same picture.
- [x] The dribble game (type 11). The intercept game (type 6) never starts
      in 4.32.
- [x] The replays of a goal (`WatchReplay`), the ball trail, and the
      make-run arrows of the team-mates
- [x] After the match: the match stats (`MatchStats`) with the rating, the
      relation changes, and the star rating, and the weekly update
      (`MatchReview`) with the energy and the pay
- [x] The skill ratings: the change after each chance, the type of the
      player, and the ratings screen (`Ratings`)
- [x] Energy drinks and boosts (`ChooseBoost`): the five kinds of can, the
      skill and the relation boosts, and the bolts
- [x] The five trials of a new career: the drills of technique, set
      pieces, power, dribbling, and vision, with their lives and hints

## 0.6 The season

- [x] Fixtures and tables (`c_TFixture`, `c_TTable`, `LeagueFixtures`,
      `ClubFixtures`, `League`)
- [x] Cups and continental competitions (`c_TCompetition`)
- [x] Extra time and the penalty shoot-out of a cup match
- [x] The results of the other matches (`c_TFixData`)
- [x] Promotion and relegation
- [x] The end of the season (`SeasonReview`, `PlayerOfTheYear`)
- [x] International call-ups: the nation picks the player by the relation
      with its boss, as the original does
- [x] The selection: the boss can make the player a substitute, or leave
      the player out
- [x] The news of a cup winner and of the result (`headlineresult`)
- [x] The trophy log and the stat history (`Achievements`)
- [x] The transfer at the end of the season (`DesiredTransfer`)

## 0.7 The life of the player (done)

- [x] Relationships (`Relationships`, `Pairs`)
- [x] The sounds of the relationships screen and of the pairs game
      (`CError`, `Card_Dealt_1`, `CSuccess`)
- [x] The NRG overlay when the energy is too low for an activity
      (`upsellnrg`, `NRGOverlay`)
- [x] Dilemmas and interviews (`Dilemma`, `Interview`), with the
      interview after a match
- [x] The news of the week before a match (`GSPlayerPlay`): the partner,
      the requests of the relations, the happiness, the style, the
      vehicles, the other clubs, the dilemma, and the tips
- [x] The news checks of the old pro boots, the transfer window, the
      captaincy, and the sponsors
- [x] The gambling news, with the casino of 0.8
- [x] The skill bar in the message of a dilemma accident
- [ ] Check the colour of the interview buttons against the original
- [x] Money, the lifestyle shop, and boots (`ShopLifestyle`, `BootShop`,
      `BuyItem`)
- [x] Match balls (`MatchBalls`): six free balls, one for each match
      type. The three balls of SpecialEffect are a purchase in 4.32, so
      they stay hidden
- [x] The warning of the trainer and the loss of skills with no training
- [x] The agent, transfers, and contract talks (`AgentDetails`,
      `DesiredTransfer`, `Negotiate`)
- [x] The digits of the cards of the negotiate game (`MechDigit`)
- [x] Retirement (`Retirement`), the score, and the hard mode unlock
- [x] The history of the seasons and the trophy log, with the medals and
      the seasons in the score of the retirement
- [x] The awards of the achievements screen (`CheckAchievement`, 108
      awards, the alert `bottommessageachievement`). The alerts wait in a
      queue, and a tap closes one.
- [x] The records of the career: the furthest goals and passes
      (`bottommessagerecord`), the hat tricks, and the form of the career
- The award 79 (a winner in the last minute) of a second leg does not
  check the away goals, as the game does in some cases.

## 0.8 Side games (done)

- [x] Casino: the stakes, the bets, blackjack, roulette, and slots
      (`Casino`, `BlackJack`, `Roulette`, `Slots`). The pairs game of the
      original is only in the relationships screen.
- [x] The gambling news, the trip with the team mates, and the catch of a
      player under 18
- [x] Horses: the stable, the market, training, sales, and races
      (`MyStable`, `Racing`, `RacingResult`)
- [x] The sounds of the casino and of the races. The gallop plays every
      890 ms on one channel; the original has two loops.
- [x] The alerts at the bottom of the screen: the energy of the bets
- [x] The coin shower and the ticker tape of a win (`Spawn`). Each win
      shows a new ticker tape. A tie of blackjack gives the bet back with
      no sound.
- [ ] The camera of the race moves all horses; check it against the
      original
- [ ] The size of the roulette wheel: the remake takes 545 for the width
      of the "Wheel" gadget. Check it against the original.
- The casino games take their numbers from the clock, as the original
  does. The remake keeps the numbers of the career apart, so a visit to
  the casino does not change them.

## 1.0 The full game

The content of 4.32 in 1.0 is the content of the new look. The classic
look is not in the remake.

- [ ] All the content of 4.32
- [x] All the languages of 4.32, with the star glyph and the glyph tables
- [ ] All the features in this file
- [ ] A test harness that compares the rules with the original code

## Gaps in the layout language

The app logs each modifier of a layout that the scene skips. The tool
`layoutscan` counts them over all the layouts of an import:

    build/layoutscan "<game directory>"

On 2026-10-27 it found these in 187 layouts. The number is the count of
layouts that use the modifier.

- [x] `Blend` (81): the blend factors of an image
- [x] `FontShadow` and `ShadowCol` on a text. One layout puts a
      `FontShadow` on an element that is not a text.
- [x] `DownloadImage` (56): an image from the web of the game. 4.32 has
      no definitions, so the image stays.
- [x] `ScrollIndicator` (29): the bar that shows the place in a list
- [x] `Message` (18) in the block of a click: the click sends the
      message
- [x] `SetMessage` (1): the save slots set it, and the delete button then
      sets the layout "message" again, so it has no effect
- [x] `Spawn` (5), `Slot` (1), `SortByRef` (1): copies made by a layout
- [x] `ReadoutConcat` (5): a readout of two values in one text
- [x] `FontStyle` (5), `LineSpacing` (4), `Padding` (3): text layout
- [x] `func` (2), `call` (2): functions of the expression language. Only
      the alerts of the achievements and the records use them
- [x] `RandomTweak` (1), `MechDigit` (1)
- `Touch` (1) is a stray word in a classic tutorial, and `ModifyTweak`
  (1) is on a debug screen of a cricket game. They are not for the
  remake.

## The look of the remake

The remake keeps the look of the original. Only the marks of New Star
Games change. The art of the remake is in `art/`. A file there takes the
place of the atlas image with the same name (`art/NewStarLogo.png`).

- [x] Replace an atlas image with a file of `art/`
- [x] The logo of Star Soccer Rebooted on the title (`NewStarLogo`)
- [x] The "TM" after the SOCCER letters of the title (`TitleMacros.uis`)
- [x] The TV of the match (`MapNSGTVLogo`) and the sponsor of the match
      (`NSGamesLogo`)
- [x] The newspaper (`NSNewsLogo`, `CLHeaderNewsLogo`)
- [x] The boards and the roof of the stadiums (`BannerTopS2`,
      `BannerFrontAbv`)
- [x] The shirt card of the pairs game (`PShirt`), the back of the cards
      (`PCardBackPattern1`), the strip behind the scroll windows
      (`LogoStrip`), and the banner of the classic match (`CLNewStar`)
- [x] The names in the text: "New Star Soccer", "NSS", and the "NS-" names
      of the boots and the balls. The credits keep the names of the
      original.
- [x] The scrolling boards of the pitch (`Banner01Text`, `Banner02Text`,
      and the hashtag, the plate, and the boot of `Banner04`). The NRG
      board stays: NRG is a brand of the game. The GOAL board is of
      Goal.com and stays.
- [x] A patch on a stripped image, as on a plain image
- [x] The marks of the casino: the title of the slots (`SlotsTitlePanel`),
      the table and the card backs of blackjack (`BlackJackLogo`,
      `CardBack`), the icon of blackjack (`IBlackJackColour`), the ace of
      the card banners (`CardAce1`), and the
      boards of the race tracks (`HorseBannerC`, `HorseBannerW`,
      `AdBoardColour`, `AdBoardWhite`)
- The microphones of the interview (`InterviewMics`) have no mark. They
  are black shapes.
- Of the images of `Images/`, only `Backgrounds/Sponsored_SPONSOR.png` has
  a mark. No screen of the new look and no code of the remake shows it.
- `BannerFrontLogo` stays: it is a ball with a star, with no name.

## Not in the remake

- Advertisements (`FakeInterstitial`, `NSS5Ad`, `RewardedAdStaffSelector`,
  `GamesWeLike`, the ad SDKs)
- In-app purchases
- Buying Bux with real money. Bux, the cash of the game, stay.
- The classic look of 4.32 (the `CL` layouts and images). The owner chose
  this on 2026-10-27.
- A manager career. The owner chose this on 2026-10-27.
- The cloud saves and the analytics of New Star Games (`CloudSetup`,
  `CloudCohort`, Firebase, Flurry)

## A living world

The aim of the remake. See [docs/decisions.md](docs/decisions.md). The
living world starts in 0.6, with the season. Version 1.0 has all of it.

### The world around the player

- [x] Other players with careers: age, skill, form, transfers, injuries,
      and retirement (`ssr_people`). Each club that plays gets a squad.
- [x] Clubs with squads, managers, finances, and histories: the squad
      screen shows the manager, the titles, and the budget of the club
- [x] Rival players: players of the same age who start with the player
      and follow their own careers. The squad screen and the ticker show
      them.
- [x] Compare the stats of the rivals with the stats of the player (the
      third list of the squad screen)
- [x] Teammates who matter: the strike partner, and a young player that
      the player mentors. A good team spirit helps the young player grow.
- [x] Managers with styles (balanced, youth, attacking). The managers of
      the bottom clubs can go at the end of a season. A new manager of the
      club of the player starts the boss relation again at 50.
- [x] A new manager who changes the role of the player or brings the
      player to a new club: an attacking boss plays the player as a
      striker, and a sacked boss can call the player to the new club
- [x] New young players each season, so the world stays full after the
      real players retire

### Stories and events

- [x] A news feed: form, transfer rumours, crises at other clubs, records.
      The ticker tells of the rivals, the top scorer, the transfers, a
      big club low in the table, a young player linked with a big club,
      and the chase of the scoring record of a season.
- [x] Press and fans: the opinion of the fans is a stat. The answers in
      interviews make the headline of the press, and good words before a
      derby or a rivalry match make the rivalry grow.
- [x] Season stories: the title race and the fight against relegation in
      the ticker
- [x] More season stories: a cup run of a small club in the ticker, and
      a title on the last day in the honours
- [x] Career moments: the debut, the first goal, the milestones of the
      matches and the goals, with an alert
- [x] The testimonial match ends the timeline of a career
- [x] Derbies from the distance between stadiums (under 30 km), in the
      ticker
- [x] Rivalries that grow from title races and transfers, in the ticker
- [x] Rivalries that grow from cup finals
- [x] Form: a club in a run of wins or defeats, in the ticker
- [x] Comebacks: the player returns from an injury, with an alert

### History

- [x] The champions and the top scorers of each league in each season
      (the Honours screen)
- [x] A hall of fame of the scorers of the world (Honours)
- [x] The records of each club, league, and nation (Honours, Records)
- [x] A career timeline: the clubs, the titles, the awards, and the
      moments (Honours, Career)
- [x] A legacy after retirement: the old player stays in the hall of fame
      of the world that a new player continues

### The match

- [x] Match context: the fans care more about a derby or a rivalry match
- [x] A cup final or a match that the club must win changes the crowd and
      the pressure: a full stadium, a stronger opponent, and two times the
      change of the fans and the boss in a final
- [x] Momentum: from the hour, the team that is behind pushes, so a
      comeback from 2-0 is possible
- [x] Weather: some matches are in the rain (more often far from the
      equator), and the ball rolls further on the wet pitch. In 4.32 a
      career match is always dry; only the training has weather.
- [x] Snow in the winter far from the equator, and the worn pitch of a
      small club in the winter
- [x] Fair ratings and real scores. Players of the original complained
      about harsh ratings and very high scores. The World screen chooses
      the real ones (a rating starts at 6, and 2.7 goals in a match) or
      the classic ones of 4.32 (3.2 goals in a match).

### Off the pitch

- [x] Rest and fitness: a match can injure the player, who misses the
      matches until the injury heals
- [x] The player can choose to play with pain, at a risk
- [x] Contract drama: release clauses, loans, a club that refuses to sell
- [x] Business and charity, with effects on fame and on the world

### Replay value

- [x] World seeds, so that friends can share a world. Options, World
      types the seed of the next career. The Honours screen shows the seed
      of the career.
- [x] A custom career: after the nation, the screen "Your start" chooses
      the age (16 to 32) and the least value of each skill (up to 70)
- [x] More choices for the custom career: the position, the first club,
      and the Bux
- [x] Many careers in one world: Options, World can make the next new
      career continue the world of the last career
- [x] The next player can play against the last one: a player of the
      last career who did not retire plays on in the continued world

## The database

- [x] A data format for the world: the files of the data set, the
      names of the players, images, and screens, in a zip file with
      `pack.txt` ([docs/packs.md](docs/packs.md))
- [x] Crests in a pack (`crests/<club id>.png`): the crest goes on the
      face of the badge of the match and of the fixtures
- [x] The real players and the managers of the clubs in a pack
      (`world/Players.txt`)
- [x] A pack changes only the rows and the columns that it gives
- [ ] Stadiums in a pack
- [x] The first community pack: the season 2025/26 for the four English
      leagues (`community/season-2025-26`)
- [x] An editor in the app, first step: a tap on the name of a club on the
      squad screen renames it. The edit goes into the pack "custom".
- [ ] An editor for the leagues, the kits, and the players
- [x] Data packs: the player installs a pack from a file (a drop on the
      window, or the path at the start)
- [x] A screen to choose or remove a pack (Options, Packs), and an
      install with a file dialog. On iOS the app installs each zip file of
      its Documents folder.
- [x] Art packs: a pack can replace any image of the game, and any
      screen
- [ ] The index repository of the packs, with a CI check for each pull
      request
- [ ] A pack browser in the app: read the index, download a pack, and
      check its SHA-256. Not in the store builds.
- [ ] A 2026 data pack: the leagues, the clubs, and the squads of the
      2025-26 season. It is a separate project, not in this repository.
      See [docs/decisions.md](docs/decisions.md).
- [x] Read the 2025 data of the original (`MyData/v2025b`) as the default
      world

## More features

- [x] Choose the position of the player, and change it later (the list
      of the rivals). The position gives the focus at the kick-off.
- [ ] More kinds of match moments: set pieces, defence, saves
- [ ] Replays of goals and matches

## After 1.0

- [ ] Challenge starts: for example the pace at its maximum, or no Bux and
      no NRG. The custom career takes their place for 1.0.
