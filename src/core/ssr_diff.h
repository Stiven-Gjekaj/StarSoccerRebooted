// ssr_diff: compares a screen of the remake with a screenshot of the
// original game, to measure how far the remake is from it.

#ifndef SSR_DIFF_H
#define SSR_DIFF_H

#include "ssr_image.h"

// The screen is cut into this number of bands from the top to the bottom.
#define SSR_DIFF_BANDS 20

// A pixel differs when one of its channels differs by more than this.
#define SSR_DIFF_THRESHOLD 40

typedef struct {
    // The mean difference of a channel, from 0 to 255.
    double mean;
    // The part of the pixels that differ, from 0 to 1, for all the screen
    // and for each band.
    double changed;
    double bands[SSR_DIFF_BANDS];
} SSRDiff;

// Compares image with reference. The reference is scaled to the size of
// image first, with the mean of the pixels that each pixel covers. The
// alpha is not compared. heat, when it is not NULL, gets an image of the
// size of image: the pixels that differ are red, the others are a dim copy
// of image. Returns 0 when an image is empty.
int ssr_diff_images(const SSRImage *reference, const SSRImage *image, SSRDiff *diff, SSRImage *heat);

#endif
