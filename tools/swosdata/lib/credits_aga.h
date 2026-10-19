/*
 * credits_aga.h -- the Amiga AGA CREDITS screen's backdrop (grafs_aga/CREDITS.RAW).
 *
 * A port of scripts/make_credits_aga.py: the AGA menu backdrop (352x272, indices
 * below 128) with four of the developer photos from bin/data/credits_icons.bmp
 * (a 280x40 strip of seven 40x40 photos drawn in indices 128-255) set into it,
 * each with a 1 px drop shadow in index 3, packed as an UNPACKED 8-plane RAW --
 * what SwosGfx's -bmp -output=raw -bitplanes=8 writes before RNC-packing.
 *
 * The geometry is the script's (and include/credits_aga_palette.h's, which the
 * Amiga menu code reads); change it there and here together. The palette header
 * the script also writes is not made here: the RAW holds indices only.
 */
#ifndef SWOSDATA_CREDITS_AGA_H
#define SWOSDATA_CREDITS_AGA_H

#include <stdint.h>
#include "sd_util.h"
#include "sd_image.h"

/* menubg: the AGA MENUBG picture (352x272); creditsIcons: the photo strip. Writes
 * the unpacked 8-plane RAW (malloc'd) to outRaw. 0 on success, else `err` says why. */
int32_t Sd_MakeCreditsAga(const SdImage *menubg, const SdImage *creditsIcons, SdBuf *outRaw, char *err);

#endif
