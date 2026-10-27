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

// Puts the names of the remake in the place of the names of New Star
// Games: "New Star Soccer" becomes "Star Soccer Rebooted", "NS-" at the
// start of a word becomes "SSR-", and the word "NSS" becomes "SSR". The credits of the original and the
// other games of New Star Games keep their names. Returns the number of
// texts that change.
int ssr_text_rebrand(SSRText *text);

// Copies template into out and replaces each "$name" with the value of the
// name. names and values have count items. A longer name wins, so "$teamname"
// is not "$team" with "name" after it. Returns the length of the full
// result, as snprintf does.
size_t ssr_text_format(char *out, size_t capacity, const char *template, const char *const *names,
                       const char *const *values, int count);

// The code of a language (c_TLocale.GetNationCode): 0 "en", 1 "tr", 2 "br",
// 3 "it", 4 "es", 5 "pl", 6 "de", 7 "fr", 8 "pt", 9 "ru", 10 "ja",
// 11 "ko", 12 "ch". Another number gives "en".
const char *ssr_text_language_code(int language);

// The language of the device from the table of
// "MyData/Languages/DeviceLanguageCodes.txt" (GSSetLanguageFromDevice).
// A code with a country, for example "pt_BR", needs the same language and
// country. The first row that agrees wins. metres gets the "Distance"
// column. Returns -1 when no row agrees.
int ssr_text_device_language(const char *table, const char *language, const char *country, int *metres);

#endif
