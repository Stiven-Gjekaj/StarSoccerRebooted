# Season 2025/26

This data pack puts the English football of the 2025/26 season into Star
Soccer Rebooted. The pack format is in [docs/packs.md](../../docs/packs.md).

## The contents

| File | Contents |
| --- | --- |
| `pack.txt` | The id `season-2025-26`, the name, the author, the version, and the format. |
| `world/Clubs.txt` | 92 clubs: the 20 clubs of the Premier League, and the 24 clubs of each of the Championship, League One, and League Two. Each row gives the short name, the tla, the strength, the home kit, the away kit, and the league. |
| `world/Competitions.txt` | 46 names of the English leagues, the FA Cup, the EFL Cup, the FA Trophy, and the play-offs. |
| `world/Players.txt` | 1242 rows: the first-team squads and the managers of the 20 Premier League clubs and the 24 Championship clubs. |
| `crests/<club id>.png` | 92 crests, 256 by 256 pixels. |

All the clubs use the ids of the data set of the game. The pack adds no new
club.

### The leagues

The leagues are the leagues of the 2025/26 season, after the promotions and
the relegations of 2024/25. The pack sets `leagueid` for each club.

- Premier League (2270): Burnley, Leeds United, and Sunderland come up.
  Leicester, Ipswich, and Southampton go down.
- Championship (2280): Birmingham, Wrexham, and Charlton come up. Luton,
  Plymouth, and Cardiff go down.
- League One (2290): Doncaster, Port Vale, Bradford City, and AFC Wimbledon
  come up. Bristol Rovers, Crawley, Cambridge United, and Shrewsbury go
  down.
- League Two (2300): Barnet and Oldham come up. Carlisle and Morecambe go
  down.

### The players

- The squads are the squads at the close of the summer transfer window, on
  1 September 2025. A player on loan to another club for the full season is
  not in the squad.
- The manager is the manager at the start of the season, in August 2025.
- The age is the age on 15 August 2025.
- The nation is the nation that the player plays for. For a player with no
  senior cap, the nation is the nation of his youth teams or of his birth.

### The skill and the potential

The skill and the potential are not real ratings. They do not come from
Football Manager, EA FC, or a different game. The pack makes them from a
formula:

1. Start at the strength of the club, minus 4.
2. Add the part of the player in the squad: key player +3, regular starter
   0, rotation player -5, backup or young player -11.
3. Add a small fixed value from the name of the player, from -2 to +2.
4. Below the age of 22, subtract 2 for each year below 22. Above the age of
   31, subtract 1 for each year above 31.
5. The potential is the skill plus 2 for each year below the age of 27, to
   a maximum of 100.

The part of each player in the squad is a judgement of the community. You
can change it.

### The crests

The crests are not the real crests of the clubs. The real crests are
trademarks. The script [tools/art/crests.py](../../tools/art/crests.py)
draws each crest from the colours of the club, a simple pattern, and the
initials of the club. The app cuts the crest to a circle.

## The sources of the facts

- The squads, the transfers, the loans, and the managers: the Wikipedia
  pages "2025–26 <club> season", the Premier League squad lists of
  September 2025 (premierleague.com), footballsquads.co.uk, and the
  websites of the clubs.
- The dates of birth and the nations: the Wikipedia page of each player.
  Some come from Transfermarkt, ESPN, Soccerway, and FotMob.
- The kits: the kit launches of the clubs and the kit makers, Wikipedia,
  footyheadlines.com, and footballkitarchive.com.
- The strength of each club is a judgement of the community, from the
  results of 2024/25 and the level of the league.

## What is not sure

- About 25 Championship players have only a year of birth. Their age can
  be wrong by one year.
- Some dates of birth come from one source only.
- The colours are near the real colours. They are not exact. Some colours
  of the shorts have no source, for example the home shorts of Tottenham and
  the away shorts of Coventry, Leicester, and Doncaster.
- A loan that started near 1 September 2025 can be in the wrong squad.
- League One and League Two have no real players. The app makes up their
  squads.

## Make the zip file

Run this command from the root of the repository:

    tools/make_pack.sh community/season-2025-26

The command writes `build/season-2025-26.zip`. The zip file holds only the
files that the app installs. This README stays out of the zip file.

To draw the crests again after a change of the colours, run this command:

    python3 tools/art/crests.py community/season-2025-26

The script needs `fonttools`, `rsvg-convert`, and the font Impact of macOS.
