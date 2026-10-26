// ssr_text: the text of the game in one language, from
// "MyData/Languages/<code>.txt" of the import. Each row has a tag, the
// text, and the text for a female player.

#ifndef SSR_TEXT_H
#define SSR_TEXT_H

#include <stddef.h>

typedef struct SSRText SSRText;

// Reads the language file at path. Returns NULL when the file is not there
// or has no Tag column.
SSRText *ssr_text_load(const char *path);
void ssr_text_free(SSRText *text);

// The number of tags.
int ssr_text_count(const SSRText *text);

// The text of the tag. When female is 1 and the female text is not empty,
// it gives the female text. When the tag is not there, it gives the tag.
const char *ssr_text_get(const SSRText *text, const char *tag, int female);

// Copies template into out and replaces each "$name" with the value of the
// name. names and values have count items. A longer name wins, so "$teamname"
// is not "$team" with "name" after it. Returns the length of the full
// result, as snprintf does.
size_t ssr_text_format(char *out, size_t capacity, const char *template, const char *const *names,
                       const char *const *values, int count);

#endif
