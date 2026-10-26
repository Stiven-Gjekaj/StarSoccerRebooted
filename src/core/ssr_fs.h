// ssr_fs: opens the files of the import when the case of a name is not the
// same as on the disk. The game names "MainMenu.uis", but the file is
// "mainmenu.uis". macOS and Windows ignore the case. iOS, Android, and
// Linux do not.

#ifndef SSR_FS_H
#define SSR_FS_H

#include <stddef.h>
#include <stdio.h>

// Finds the file with each part of path in any case. out gets the path as
// it is on the disk. Returns 1 when the file is there.
int ssr_fs_resolve(const char *path, char *out, size_t capacity);

// Opens the file as fopen() does. When the name is not there, it opens the
// file that has the same name in another case.
FILE *ssr_fs_open(const char *path, const char *mode);

#endif
