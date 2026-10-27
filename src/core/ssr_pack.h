// ssr_pack: the data packs of the player. A pack is a zip archive with the
// file "pack.txt" and data files. It holds no code. The app installs a
// pack into a directory of its own, and reads the world, the art, and the
// layouts of the pack over the files of the game.
//
// pack.txt has one name=value a line:
//
//     id=harbour-league
//     name=Harbour League
//     author=A. Person
//     version=1.0
//     format=1
//
// The other files of a pack:
//
//     world/Clubs.txt     the data set files of the game (Clubs.txt,
//                         Nations.txt, Competitions.txt, Continents.txt,
//                         PromotionPlaces.txt), with the same columns
//     world/Players.txt   the players of the world (ssr_people)
//     art/<name>.png      an image of the remake or of an atlas
//     layouts/<name>.uis  a screen

#ifndef SSR_PACK_H
#define SSR_PACK_H

#include <stddef.h>

// The format that the app reads.
#define SSR_PACK_FORMAT 1

typedef struct {
    char id[48];
    char name[96];
    char author[96];
    char version[32];
    int format;
} SSRPackInfo;

// Reads the text of pack.txt. Returns 1 when the text has a good id, a
// name, and a format that the app reads.
int ssr_pack_parse(const char *text, SSRPackInfo *info);

// Reads "<directory>/pack.txt". Returns 1 on success.
int ssr_pack_info(const char *directory, SSRPackInfo *info);

// Installs the pack in the archive into "<packs>/<id>". pack.txt can be at
// the top of the archive or in one folder. A name that can write outside
// the pack, or a file that is not data, stops the install before it
// writes a file. An installed pack with the same id goes first. info gets
// the info of the pack. Returns 1 on success.
int ssr_pack_install(const char *archive, const char *packs, SSRPackInfo *info);

// Makes the pack with the id in packs when it is not there: its directory,
// its folder "world", and a pack.txt with the name. Returns 1 when the
// pack is there.
int ssr_pack_make(const char *packs, const char *id, const char *name);

// Removes the pack with the id from packs. Returns 1 when it was there.
int ssr_pack_remove(const char *packs, const char *id);

// Puts the ids of the installed packs in ids, sorted, up to max. Returns
// the number of packs.
int ssr_pack_list(const char *packs, char ids[][48], int max);

#endif
