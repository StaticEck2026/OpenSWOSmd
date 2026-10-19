/*
 * font_extend.h -- draw the extended glyphs (full ASCII punctuation, accented
 * capitals, $, the euro, OE) into the free cells of a CHARSET bank.
 *
 * A port of SwosGfx's -font-extend (SwosFont.cs), without integration.txt.
 * The glyph shapes come from glyphs.txt (tools/swosdata/res/glyphs.txt: blocks
 * of "@<char> small|big" followed by 8 rows of '*' = ink, '-' = blank); the
 * 1-pixel down-right shadow is added here. Glyphs take the free 16x8 cells of
 * the sheet in a fixed order -- the small font in rows 0-13, the big font in
 * rows 14-31 -- which is the placement the game's font_ext_script.inc cuts
 * them from (slot 1220 on). Every cell of rows 0-27 left empty then gets the
 * stock "missing glyph" box, as the Amiga charset has.
 *
 * Works on any CHARSET bank (the PC-sourced one from Sd_DosToAmigaBanks, or the
 * Amiga's own); the palette is not touched: the font draws in index 2 (ink)
 * and 8 (shadow) in both.
 */
#ifndef SWOSDATA_FONT_EXTEND_H
#define SWOSDATA_FONT_EXTEND_H

#include <stdint.h>

#include "sd_image.h"

/* Extend `charset` in place from the glyphs file at `glyphsTxtPath`.
 * 0 on success; on failure `err` (256 bytes) says why. */
int32_t Sd_FontExtend(SdImage *charset, const char *glyphsTxtPath, char *err);

#endif
