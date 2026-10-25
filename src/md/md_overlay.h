/* md_overlay.h -- the engine's in-frame drawing as plane A cells (md_overlay.c). */
#ifndef MD_OVERLAY_H
#define MD_OVERLAY_H

#include <stdint.h>
#include "md_render.h"

#define MD_OV_ROWS    30
#define MD_OV_COLS    40
/* a cell: character (ASCII, 0 none) | background << 8 */
#define MD_OV_DARK    1         /* DarkenRectangle */
#define MD_OV_SOLID   2         /* + colour: FillRectangle */
#define MD_OV_SPR_MAX 24

extern MdSprCmd md_ovSpr[MD_OV_SPR_MAX];   /* drawn over everything, this frame */
extern uint16_t md_ovSprCount;

void md_ov_reset(int16_t offY, int16_t screenH);
void md_ov_put(int16_t col, int16_t row, const char *s, int16_t max);
void md_ov_commit(int hudChanged);
int  md_ov_drawn(void);
int  md_ov_static(void);   /* the engine's drawing is the last frames': only hashed */

#endif
