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
- [ ] Energy and boosts (`ChooseBoost`)
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

## Not in the remake

- Advertisements (`FakeInterstitial`, `NSS5Ad`, `RewardedAdStaffSelector`,
  `GamesWeLike`, the ad SDKs)
- In-app purchases
- The cloud saves and the analytics of New Star Games (`CloudSetup`,
  `CloudCohort`, Firebase, Flurry)

## Open questions

- Bux: the original sells them, and it also gives some after a match
  (`m_matchReviewBuxAwarded`). The remake has no store. Does the player get
  more Bux from play, or does the remake remove Bux?
- New features after 1.0: which features that the original does not have?
