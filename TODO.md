# Roadmap

Each version is a part of the game that you can play. The reference is the
Android release 4.32. The names in brackets are the classes and the screens
in the code of the original (`c_TMatch`, `TScreen_Casino`).

## 0.1 Import (done)

- [x] Read zip archives
- [x] Import the assets from an APK, an XAPK, or an IPA
- [x] Show an image from the import

## 0.2 Assets

- [ ] Decode JPG images (the backgrounds)
- [ ] Load the atlases of the interface (`NewGUI/Atlases`)
- [ ] Draw text with the fonts and the glyph tables of the game
      (`Fonts`, `FontRenderer`)
- [ ] Play the sounds and the music (OGG from an APK, MP3 from an IPA)
- [ ] Read the screen layouts (`.uis`, `.uih`) and draw a screen from them
- [ ] Read the season data (`MyData/v2014d`: clubs, nations, competitions,
      stadiums, promotion places)
- [ ] Read the text of each language (`MyData/Languages`)

## 0.3 Platforms

- [ ] Build for iOS
- [ ] Build for Android
- [ ] Touch input, and the mouse as touch on the desktop
- [ ] Save files in the data directory of each platform

## 0.4 A new career

- [ ] Main menu and save slots (`MainMenu`, `SaveSlot`)
- [ ] Make a player: name, nation, look (`ChooseNation`)
- [ ] Choose a league and a club (`ChooseLeague`, `ChooseClub`)
- [ ] Skills and the first contract (`Skills`, `SkillChoice`, `Contract`)
- [ ] The game menu between matches (`GameMenu`)

## 0.5 The match

- [ ] The ball physics (`c_TBall`, `c_TBall2`)
- [ ] The players on the pitch (`c_TMatchPlayer`)
- [ ] The match moments: pass, shoot, dribble, and the controls
      (`c_TMatch`, `TScreen_Match`)
- [ ] Before and after the match (`Prematch`, `MatchReview`, `MatchStats`,
      `Ratings`)
- [ ] Energy and boosts, with no Bux (`ChooseBoost`)
- [ ] A test harness that compares the rules with the original code

## 0.6 The season

- [ ] Fixtures and tables (`c_TFixture`, `c_TTable`, `LeagueFixtures`,
      `ClubFixtures`, `League`)
- [ ] Cups and continental competitions (`c_TCompetition`)
- [ ] The results of the other matches (`c_TFixData`)
- [ ] Promotion and relegation
- [ ] The end of the season (`SeasonReview`, `PlayerOfTheYear`,
      `Achievements`)
- [ ] International call-ups

## 0.7 The life of the player

- [ ] Relationships (`Relationships`)
- [ ] Dilemmas and interviews (`Dilemma`, `Interview`)
- [ ] Money, the lifestyle shop, boots, and match balls (`ShopLifestyle`,
      `BootShop`, `MatchBalls`, `BuyItem`)
- [ ] The agent, transfers, and contract talks (`AgentDetails`,
      `DesiredTransfer`, `Negotiate`)
- [ ] Retirement (`Retirement`)

## 0.8 Side games

- [ ] Casino: blackjack, roulette, slots, pairs (`Casino`, `BlackJack`,
      `Roulette`, `Slots`, `Pairs`)
- [ ] Horses: the stable, the market, and races (`MyStable`,
      `HorsesForSale`, `Racing`, `RacingResult`)

## 1.0 The full game

- [ ] All the content of 4.32
- [ ] All the languages of 4.32
- [ ] All the features in this file

## Not in the remake

- Advertisements (`FakeInterstitial`, `NSS5Ad`, `RewardedAdStaffSelector`,
  `GamesWeLike`, the ad SDKs)
- In-app purchases
- Bux. The things that Bux buy come from play or from money.
- The cloud saves and the analytics of New Star Games (`CloudSetup`,
  `CloudCohort`, Firebase, Flurry)

## A living world

The aim of the remake. See [docs/decisions.md](docs/decisions.md). The
living world starts in 0.6, with the season. Version 1.0 has all of it.

### The world around the player

- [ ] Other players with careers: age, skill, form, transfers, injuries,
      and retirement
- [ ] Clubs with squads, managers, finances, and histories
- [ ] Rival players: players of the same age who start with the player
      and follow their own careers. The game compares their stats with
      the stats of the player.
- [ ] Teammates who matter: a strike partner, a captain, a young player
      that the player mentors
- [ ] Managers with styles: a new manager can drop the player, change the
      role, or bring the player to a new club
- [ ] New young players each season, so the world stays full after the
      real players retire

### Stories and events

- [ ] A news feed: form, transfer rumours, crises at other clubs, records
- [ ] Press and fans: the opinion of the fans is a stat. The answers in
      interviews change the headlines and the rivals.
- [ ] Season stories: a fight against relegation, a cup run of a small
      club, a title on the last day
- [ ] Career moments: the debut, the first goal, the 100th appearance,
      the testimonial match
- [ ] Rivalries: derbies from the distance between stadiums, and rivalries
      that grow from cup finals, title races, and transfers
- [ ] Comebacks and form: a club in a bad run, a player who returns from
      an injury

### History

- [ ] The champions of each season, the records, and a hall of fame for
      each club, league, and nation
- [ ] A career timeline: the seasons, clubs, trophies, and big moments
- [ ] A legacy after retirement: how the world remembers the player. The
      world continues in a manager career or with a new player.

### The match

- [ ] Match context: a derby, a cup final, or a match that the club must
      win changes the crowd, the pressure, and the stakes
- [ ] Momentum: a goal changes the match, so a comeback from 2-0 is
      possible
- [ ] Weather and the pitch change how the ball moves
- [ ] Fair ratings and real scores. Players of the original complained
      about harsh ratings and very high scores.

### Off the pitch

- [ ] Rest and fitness: an injury has a recovery. The player can choose
      to play with pain, at a risk.
- [ ] Contract drama: release clauses, loans, a club that refuses to sell
- [ ] Business and charity, with effects on fame and on the world

### Replay value

- [ ] World seeds, so that friends can share a world
- [ ] Challenge starts: the lowest league at age 30, or one stat at its
      maximum
- [ ] Many careers in one world. The next player can play against the
      last one.

## The database

- [ ] A data format for the world: leagues, clubs, players, stadiums,
      kits, and logos. The format is open text, so that people can change
      it.
- [ ] An editor in the app for the leagues, the clubs, and the players
- [ ] Data packs: the player installs a pack from a file. A pack can
      change the names, the squads, the kits, and the logos.
- [ ] Art packs: a pack can replace any image of the game
- [ ] A 2026 data pack: the leagues, the clubs, and the squads of the
      2025-26 season. It is a separate project, not in this repository.
      See [docs/decisions.md](docs/decisions.md).
- [ ] Import the 2014 data of the original (`MyData/v2014d`) as the
      default world

## More features

- [ ] Choose the position of the player, and change it later
- [ ] More kinds of match moments: set pieces, defence, saves
- [ ] A manager career after retirement
- [ ] Replays of goals and matches
