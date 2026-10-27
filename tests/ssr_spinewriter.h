// ssr_spinewriter: writes a small .spinary file for the tests.

#ifndef SSR_SPINEWRITER_H
#define SSR_SPINEWRITER_H

#include <stddef.h>

// Writes the file into out (4096 bytes) and returns its size. The file
// "Test" has two bones: a root at (100, 0) and a child at (10, 0). Two
// slots are on the child: "Leaf" with the image Title_Grass01, and
// "Unused" that the animation does not use. The one animation "Sway" of 2
// seconds turns the child by 90 degrees from frame 2 on, and makes the
// slot "Leaf" red at 0.5 seconds.
size_t ssr_spine_writer_build(unsigned char *out);

#endif
