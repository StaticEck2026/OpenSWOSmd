/*
 * dos_graphics.h — load DOS-version SWOS graphics into our internal indexed
 * (8bpp chunky) format. PC-only optional feature, gated on gDosGraphics
 * (dos_mode.h). The renderer is unchanged — these are loaders/converters only.
 * See docs/DOS_GRAPHICS.md for the full format spec.
 */
#ifndef SWOS_DOS_GRAPHICS_H
#define SWOS_DOS_GRAPHICS_H

#include <stdint.h>

/* DOS sprite indices are Amiga index + 114; store a decoded DOS sprite at
 * (dosIndex - DOS_TO_AMIGA_OFFSET) so our Amiga sim/renderer finds it. */
#define DOS_TO_AMIGA_OFFSET 114

/* DOS pitch tiles are 16x16, one byte/pixel (already chunky); 256 bytes each. */
#define DOS_TILE_BYTES 256

/* 6-bit VGA component (0..63) -> 8-bit. */
static inline uint8_t DosGfx_6to8(uint8_t v) { return (uint8_t)((unsigned)v * 255u / 63u); }

/* Decode one DOS planar sprite (4 bitplanes / 16 colours, MSB = leftmost px)
 * into 8bpp chunky. `planar` holds nlines*wquads*8 bytes; writes
 * nlines*(wquads*16) chunky bytes (0..15) to `chunkyOut`. Returns bytes written. */
int DosGfx_DecodePlanarSprite(const uint8_t *planar, int nlines, int wquads,
                              uint8_t *chunkyOut);

/* Build the active DOS palette: base game palette + pitch-condition overlay for
 * pitchType (0..6, clamped). Writes 256 RGB triples (8-bit) to rgb888Out[768]. */
void DosGfx_BuildPalette(int pitchType, uint8_t *rgb888Out);

/* Parse a PITCHn.DAT tile-index map (array of LE u32 offsets; tile = off/256).
 * Writes up to maxOut tile indices to tileIndexOut. Returns the count parsed. */
int DosGfx_ParsePitchMap(const uint8_t *datBuf, uint32_t datSize,
                         uint16_t *tileIndexOut, int maxOut);

#endif /* SWOS_DOS_GRAPHICS_H */
