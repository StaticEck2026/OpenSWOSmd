/* md_render.h -- the frame the engine hands the renderer (md_display.c -> md_render.c). */
#ifndef MD_RENDER_H
#define MD_RENDER_H

#include <stdint.h>

struct Sprite;

#define MD_SPRLIST_MAX 80
typedef struct {
    struct Sprite *spr;   /* the engine's sprite (team, z for the HUD test) */
    uint16_t image;       /* imageIndex: the sprite-graphics slot */
    int16_t  x, y;        /* top-left, relative to the simulation camera (336x272 window) */
} MdSprCmd;

extern MdSprCmd md_sprList[MD_SPRLIST_MAX];
extern uint16_t md_sprCount;

#endif
