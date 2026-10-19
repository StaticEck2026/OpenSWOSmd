/*
 * dos_picture.h -- the PC game's full-screen pictures (.256) and their
 * enlargement to the Amiga's 352x272 screen (SWTITLE.256 -> MENUBG,
 * STAD.256 -> MENUBG2). A C port of SwosGfx's DosPicture / DosPictureScale.
 */
#ifndef SWOSDATA_DOS_PICTURE_H
#define SWOSDATA_DOS_PICTURE_H

#include <stdint.h>
#include "sd_image.h"

#define SD_DOS_PICTURE_W   320
#define SD_DOS_PICTURE_H   200
#define SD_AMIGA_SCREEN_W  352
#define SD_AMIGA_SCREEN_H  272

/* Load a .256 picture: 320x200 indices and its palette, the palette as SwosGfx
 * keeps it (6-bit VGA values x 4, and the least-used colour made white when no
 * colour is bright enough to write on -- see dos_picture.c). If textColor is
 * not NULL it gets that writing colour's index. `out` is initialised here;
 * SdImage_Free it. 0 on success; on failure `err` (256 bytes) says why. */
int32_t Sd_DosPictureLoadEx(const char *path, SdImage *out, int32_t *textColor, char *err);
int32_t Sd_DosPictureLoad(const char *path, SdImage *out, char *err);

/* Bilinear-enlarge (or shrink) `in` to w x h, each pixel put back on in's own
 * palette by nearest colour: SwosGfx's -picture -scale=WxH, to the bit. The
 * palette is copied to `out`, which is initialised here. 0 on success. */
int32_t Sd_DosPictureScale(const SdImage *in, int32_t w, int32_t h, SdImage *out, char *err);

/* The same to 352x272 (-scale with no size): MENUBG / MENUBG2. */
int32_t Sd_DosPictureScaleToAmiga(const SdImage *in, SdImage *out, char *err);

/* Write an 8bpp BMP exactly as SwosGfx's -picture -scale writes it
 * (DosPictureScale.SaveIndexedBmp). It differs from SdImage_WriteBmp in one
 * header field only: biClrUsed is 0 there, 256 in SwosGfx's other BMPs.
 * Pixels and palette are the same either way. 0 on success. */
int32_t Sd_DosPictureWriteBmp(const SdImage *img, const char *path, char *err);

#endif
