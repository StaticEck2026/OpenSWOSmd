/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef FADE_H
#define FADE_H

typedef unsigned short uint16_t;

#define FADE_SENTINEL 0x03E7u
#define FADE_STEP     0x0020u   /* fadeStep: dc.w $0020 */
#define FADE_MAX      0x0080u

void FadeOutToBlack(void);
void FadeIn(void);
void FadeOutAndIn(void);
void ApplyFadeStep(uint16_t fadeLevel);
void FadeOutTick(void);
void FadeInTick(void);

#endif
