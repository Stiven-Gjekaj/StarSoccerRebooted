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

A file of the folder `world/` takes the place of the file with the same
name in the data set of the game (`MyData/v2025b`). The columns are the
same as in the files of the game. The other files come from the game.

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

## The players of the world

The world has players. Each club that plays gets a squad of 22 made-up
players. The players play, score, get better and older, move to other
clubs, and retire. Young players come each season. Three rivals start at
the same age as the player of the user.

The players of a career are in `saves/peopleN.txt`, next to the career.
The seed of the world is in the career (`worldseed`). Two careers with the
same seed and the same club start with the same players.
