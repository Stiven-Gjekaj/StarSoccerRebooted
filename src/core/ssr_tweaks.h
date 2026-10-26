// ssr_tweaks: the values that the game and its screens share, by group and
// name, for example ("Player", "Energy") or ("Menu", "TabPos"). A screen
// reads them in ShowIf(), twk(), and readouts. The game sets them. A value
// that nobody set is "" and counts as 0.

#ifndef SSR_TWEAKS_H
#define SSR_TWEAKS_H

typedef struct SSRTweaks SSRTweaks;

SSRTweaks *ssr_tweaks_create(void);

// Reads the start values of the game from Tweaks/TweakValues.txt: Float:
// and Var: (the value times the scale), Colour:, String:, and the first
// value of Range: in each Category:. Returns the number of values, or -1
// when the file is not there.
int ssr_tweaks_load(SSRTweaks *tweaks, const char *path);
void ssr_tweaks_free(SSRTweaks *tweaks);

void ssr_tweaks_set(SSRTweaks *tweaks, const char *group, const char *name, const char *value);
void ssr_tweaks_set_number(SSRTweaks *tweaks, const char *group, const char *name, double value);

// The text of the value, or "".
const char *ssr_tweaks_get(const SSRTweaks *tweaks, const char *group, const char *name);

// The value as a number. A value that is not a number counts as 0.
double ssr_tweaks_number(const SSRTweaks *tweaks, const char *group, const char *name);

// The value as a colour 0xRRGGBBAA. It reads "RRGGBBAA", "#RRGGBBAA", and
// "#RRGGBB". Another value gives fallback.
unsigned int ssr_tweaks_colour(const SSRTweaks *tweaks, const char *group, const char *name, unsigned int fallback);

// 1 when the value is the same as the text, or the same number. "1" and
// "1.0" are the same. An empty value is the same as "0".
int ssr_tweaks_equals(const SSRTweaks *tweaks, const char *group, const char *name, const char *text);

#endif
