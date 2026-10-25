/* md_fade.c -- the engine's palette fades (src/fade.c), for the VDP.
 *
 * The state machine is the original's, line for line: the match reads _fadeState (the
 * replay key is refused mid-fade) and spins on it, and the VBlank ticks it
 * (md_isr.c). Only ApplyFadeStep differs: instead of writing the copper's colour
 * registers it records the level, and md_render.c scales the four palette lines by it
 * when it next uploads them.
 */
#include "md_hw.h"
#include "md_game.h"
#include "fade.h"

extern volatile uint16_t _fadeState;
extern volatile uint16_t _fadeCommand;
extern volatile uint16_t _fadeInitFlag;
extern uint16_t _quitToDos;

volatile uint16_t md_fadeLevel;          /* 0 black .. 0x80 full */
volatile uint8_t  md_paletteDirty;

void ApplyFadeStep(uint16_t fadeLevel)
{
    md_fadeLevel = fadeLevel > FADE_MAX ? FADE_MAX : fadeLevel;
    md_paletteDirty = 1;
}

/* FadeOutToBlack (fade.c, SWOS2.s line 6781) */
void FadeOutToBlack(void)
{
    if (_fadeInitFlag != FADE_SENTINEL) {
        while (_fadeState == 0) { if (_quitToDos) return; }
    }
    if (_fadeState == 0) return;
    if (_fadeCommand == FADE_SENTINEL) _fadeCommand = 0xFFFFu;
    while (_fadeState != 0) { if (_quitToDos) return; }
}

/* FadeIn (fade.c, SWOS2.s line 6760) */
void FadeIn(void)
{
    if (_fadeCommand != FADE_SENTINEL) {
        while (_fadeState != 0) { if (_quitToDos) return; }
        FadeOutToBlack();
    }
    if (_fadeState != 0) return;
    if (_fadeInitFlag == FADE_SENTINEL) _fadeInitFlag = 0xFFFFu;
    while (_fadeState == 0) { if (_quitToDos) return; }
}

void FadeOutAndIn(void)
{
    FadeOutToBlack();
    FadeIn();
}

/* FadeOutTick (fade.c, SWOS2.s line 6176), from the VBlank. */
void FadeOutTick(void)
{
    uint16_t level;
    if (_fadeCommand == FADE_SENTINEL) return;
    level = _fadeCommand;
    if ((int16_t)level < 0) { _fadeCommand = FADE_MAX; level = FADE_MAX; }
    ApplyFadeStep(level);
    _fadeCommand -= FADE_STEP;
    if ((int16_t)_fadeCommand < 0) { _fadeCommand = FADE_SENTINEL; _fadeState = 0; }
}

/* FadeInTick (fade.c, SWOS2.s line 6195), from the VBlank. */
void FadeInTick(void)
{
    uint16_t level;
    if (_fadeInitFlag == FADE_SENTINEL) return;
    level = _fadeInitFlag;
    if ((int16_t)level < 0) { _fadeInitFlag = 0; level = 0; }
    ApplyFadeStep(level);
    _fadeInitFlag += FADE_STEP;
    if (_fadeInitFlag > FADE_MAX) { _fadeInitFlag = FADE_SENTINEL; _fadeState = 1; }
}

/* platform_amiga_stubs.c's start-up state: no fade running, screen black. */
void md_fade_init(void)
{
    _fadeInitFlag = FADE_SENTINEL;
    _fadeCommand = FADE_SENTINEL;
    _fadeState = 0;
    md_fadeLevel = 0;
}
