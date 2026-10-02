/* kit_art.h -- the kit drawings of the menus: a shirt, shorts, socks, the four
 * shirt-type icons and a little player, in a team's colours. Two sets of art, chosen
 * by [graphics] kit_art (docs/UNIFORM.md):
 *
 *   amiga    the Amiga's own, from the MENUS bank (the EDIT TEAM screen's sprites
 *            190-192 and 199-202), recoloured exactly as that screen recolours them;
 *   jaguar   the Atari Jaguar's shaded ones, from grafs_jag/JAGKITS.BIN
 *            (scripts/make_jag_graphics.py), four shades a colour.
 *
 * SDL only. Everything is drawn straight into the menu's frame, in menu coordinates
 * (the same ones a MenuEntry's x and y are in). */
#ifndef KIT_ART_H
#define KIT_ART_H

#include "game_types.h"

/* A kit as a team file stores it: a shirt type 0..3 and four colours 0..9. */
typedef struct KitLook {
    uint8_t type;
    uint8_t stripes;        /* the shirt's own colour (prStripesColor) */
    uint8_t basic;          /* its second colour (prBasicColor); unused by type 0 */
    uint8_t shorts;
    uint8_t socks;
} KitLook;

enum {
    KIT_ART_SHIRT,
    KIT_ART_SHORTS,
    KIT_ART_SOCKS,
    KIT_ART_ICON,           /* a shirt-type icon: look->type says which */
    KIT_ART_PLAYER,         /* the little player of the stadium screen, wearing it all */
    KIT_ART_PARTS
};

void    KitArt_SetJaguar(int32_t on);
int32_t KitArt_Jaguar(void);

/* The size of a part in the art in use (its drop shadow included). */
void KitArt_Size(int32_t part, int32_t *w, int32_t *h);

/* Draw a part centred in the box (x, y, w, h). */
void KitArt_Draw(int32_t part, const KitLook *look, int32_t x, int32_t y, int32_t w, int32_t h);

/* The Jaguar's shades need palette entries the menus leave unused: lent while a
 * screen that draws kits is up, and given back when it closes. Both are safe to
 * call again, and do nothing for the Amiga's art. */
void KitArt_Lend(void);
void KitArt_Return(void);

/* A kit from a team file's primary (second = 0) or secondary kit, and back. */
void KitArt_FromTeam(const TeamFile *team, int32_t second, KitLook *out);
void KitArt_ToTeam(const KitLook *look, int32_t second, TeamFile *team);

#endif /* KIT_ART_H */
