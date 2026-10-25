/* md_main.c -- the Mega Drive build's entry point. */
#include "md_hw.h"
#include "md_game.h"

extern void md_frontend(void);

int main(void)
{
    md_vdp_init();
    md_pad_init();
    md_sound_init();
    md_font_upload();
    md_engine_boot();
    md_log("OpenSWOS Mega Drive: %s console", md_is_pal() ? "PAL" : "NTSC");
    md_irq_on();
    md_frontend();
    for (;;) {}
}
