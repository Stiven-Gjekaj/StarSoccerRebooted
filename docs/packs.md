# Data packs

A data pack changes the world of Star Soccer Rebooted: the clubs, the
nations, the leagues, the names of the players, the images, and the
screens. A pack holds only data. It holds no code.

## Install a pack

- Open Options, then Packs, and press Install. Choose the zip file of
  the pack.
- On the desktop, you can also drop the zip file of the pack on the
  window, or start the app with the path of the zip file.
- On iOS, put the zip file in the Documents folder of the app with
  Finder or the Files app. The app installs it at the next start.
- The app installs the pack into the folder `packs/<id>` of its data
  folder and chooses it. The world then reads the pack.
- A pack with the same id takes the place of the installed pack.
- The Packs screen chooses a pack, or the world of the game. Remove
  deletes the chosen pack.

## The files of a pack

A pack is a zip file. The file `pack.txt` is at the top of the zip file or
in one folder.

    pack.txt
    world/Clubs.txt
    world/Nations.txt
    world/Competitions.txt
    world/Continents.txt
    world/PromotionPlaces.txt
    world/Names.txt
    world/Players.txt
    crests/<club id>.png
    art/<name>.png
    art/<name>.patch.png
    layouts/<name>.uis
    layouts/<name>.uis.patch

All the files are optional, except `pack.txt`.

### pack.txt

One `name=value` on each line.

    id=harbour-league
    name=Harbour League
    author=A. Person
    version=1.0
    format=1

- `id` has 1 to 47 small letters, digits, `-`, and `_`.
- `name` is the name that the app shows.
- `format` is 1. The app refuses a pack with a newer format.

### world/

A file of the folder `world/` changes the file with the same name in the
data set of the game (`MyData/v2025b`). The names of the columns are the
same as in the files of the game.

- A row of the pack changes the record with the same `id`. A row with a
  new `id` adds a record. The other records of the game stay.
- A pack gives only the columns that it changes. A column that is not
  there, or an empty cell, keeps the value of the game.
- In `PromotionPlaces.txt`, the rows of a league (`parentid`) in the pack
  take the place of all the rows of that league.

These columns change what the player sees:

| File | Column | What it changes |
| --- | --- | --- |
| `Clubs.txt` | `shortname`, `tla` | The name of the club, and its short name |
| `Clubs.txt` | `homekitshirtcol1`, `homekitshirtcol2`, `homekitshortscol` | The home kit, as `#RRGGBB` |
| `Clubs.txt` | `awaykitshirtcol1`, `awaykitshortscol` | The away kit |
| `Clubs.txt` | `strength`, `leagueid` | The strength of the club, and its league |
| `Competitions.txt` | `tla` | The name of a league or a cup |
| `Nations.txt` | `shortname`, `tla` | The name of a nation |

This `Clubs.txt` renames one club and gives it a red and black kit:

    id	shortname	tla	homekitshirtcol1	homekitshirtcol2
    817	Bedford Eagles	EAGLES	#FF0000	#000000

This `Competitions.txt` renames one league:

    id	tla
    2320	National League

`world/Names.txt` gives the names of the made-up players. It has the same
format as `data/names.txt` of the remake:

    first <group> <names>
    last <group> <names>
    nation <nation id> <group>
    continent <continent id> <group>

### art/ and layouts/

A file `art/<name>.png` takes the place of the atlas image with the name.
A file `art/<name>.patch.png` changes only some pixels of the image, as
the patches of the remake do. A file in `layouts/` takes the place of a
screen. See [layouts/README.md](../layouts/README.md).

## The checks of the install

- A name of a file that can write outside the pack stops the install.
- Only the kinds `.txt`, `.png`, `.jpg`, `.uis`, `.uih`, and `.patch` can
  be in a pack. Another kind stops the install.
- A file of more than 16 MB stops the install.
- The checks come before the app writes a file.

### world/Players.txt

The real players and the managers of the clubs. The first row has the
names of the columns. The order of the columns is free.

| Column | Value |
| --- | --- |
| `club` | The `id` of the club in `Clubs.txt` |
| `first`, `last` | The names of the player |
| `position` | `GK`, `DEF`, `MID`, `ATT`, or `MGR` for the manager |
| `nation` | The `id` of the nation in `Nations.txt` |
| `age` | The age at the start of a new career |
| `skill` | The skill from 1 to 99 |
| `potential` | The skill that the player can reach, up to 100 |

- `club`, `last`, and `position` are necessary. The app makes up the
  other values when a cell is empty.
- A squad takes up to 40 real players. Made-up players fill each
  position to the shape of a squad: 3 keepers, 7 defenders, 7
  midfielders, and 5 attackers.

    club	first	last	position	nation	age	skill	potential
    817	Sam	Example	GK	62	28	60
    817	Alex	Sample	MGR	62	45

### crests/

A file `crests/<club id>.png` is the crest of the club. The crest takes
the place of the shield of the club on the screens of the match and on
the fixtures of the club.

- The crest goes on the round face of the badge, under its chrome ring.
  The face is turned a little to the side and bends like a dome, so the
  edge of the crest bends away.
- Give a square image with the crest in the middle, for example 256 by
  256 pixels. The app uses the circle in the middle of the image.
- A transparent part of the image shows white.

## The players of the world

The world has players. Each club that plays gets a squad of 22 made-up
players. The players play, score, get better and older, move to other
clubs, and retire. Young players come each season. Three rivals start at
the same age as the player of the user.

The players of a career are in `saves/peopleN.txt`, next to the career.
The seed of the world is in the career (`worldseed`). Two careers with the
same seed and the same club start with the same players.

## The editor

A tap on the name of the club on the squad screen changes the name. The
app keeps the edit in the pack `custom` (`packs/custom/world/Clubs.txt`)
and chooses that pack. The world reads the pack at each start. A zip file
of the folder `packs/custom` is a data pack that other players can
install.
