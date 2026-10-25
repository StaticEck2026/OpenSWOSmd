/* md_frontend.c -- the cartridge's menus. */
#include "md_hw.h"
#include "md_game.h"

void md_frontend(void)
{
    for (;;) {
        md_play_friendly(0, 1, 0, 0, 0);
    }
}
