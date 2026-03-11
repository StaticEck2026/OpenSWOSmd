/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "panic.h"
#include "text_render.h"

typedef signed short int16_t;

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint16_t panicReasonCode;
extern uint32_t panicRegDumpBlock[];    /* 15 longs (d0-d7/a0-a6), written by asm stub */
extern uint16_t paletteIndex;
extern uint8_t bigCharsTable[];         /* CharTable font data */
extern volatile uint8_t pl1Fire;
extern volatile int16_t pl1Direction;
extern volatile uint8_t pl2Fire;
extern volatile int16_t pl2Direction;

/* ---- External C functions ---- */
extern void _FadeOutToBlack(void);
extern void _FadeIn(void);
extern void _WaitForVSync(void);
extern void _Player1StatusProc(void);
extern void _Player2StatusProc(void);


/* ================================================================
 * panicLookupMessagePtr  (SWOS2.s line 41114)
 *
 * Converts a 16-bit panic code to a 4-digit uppercase hex ASCII string.
 * Returns pointer to the static buffer.
 * ================================================================ */
static uint8_t hexWordStringBuf[6];

static uint8_t *panicLookupMessagePtr(uint16_t code)
{
    uint8_t *p = hexWordStringBuf;
    int16_t i;

    for (i = 3; i >= 0; i--) {
        /* Rotate left 4 bits (get next nibble from high end) */
        code = (uint16_t)((code << 4) | (code >> 12));
        uint8_t nibble = (uint8_t)(code & 0x0F);
        nibble += 0x30;  /* '0' */
        if (nibble > 0x39)  /* '9' */
            nibble += 7;   /* skip to 'A' */
        *p++ = nibble;
    }
    *p = 0;

    return hexWordStringBuf;
}

/* ================================================================
 * _PanicHaltLoop  (SWOS2.s line 5500)
 *
 * Crash handler: displays a 4-digit hex panic code and hangs forever.
 *
 * The asm stub in SWOS2.s saves d0-d7/a0-a6 to panicRegDumpBlock
 * before jumping here, preserving register state for debugging.
 *
 * Flow:
 *   1. Convert panicReasonCode to hex string
 *   2. Fade screen to black
 *   3. Wait for both joysticks to press fire with no direction
 *   4. Draw the hex code on screen centered
 *   5. Fade in and hang forever
 * ================================================================ */
__attribute__((used, externally_visible, noreturn))
void _PanicHaltLoop(void)
{
    /* Convert panic code to hex string → panicOutBuf equivalent */
    uint8_t *hexStr = panicLookupMessagePtr(panicReasonCode);

    /* The asm version uses PrintFormatted with format "%a" and a2 = hexStr.
       "%a" just copies the a2 string, so we can copy directly. */
    uint8_t panicOutBuf[96];
    uint8_t *src = hexStr;
    uint8_t *dst = panicOutBuf;
    while (*src)
        *dst++ = *src++;
    *dst = 0;

    /* Fade screen to black */
    _FadeOutToBlack();

    /* Wait for both joystick fire buttons pressed with no directional input */
    for (;;) {
        _Player1StatusProc();
        if (pl1Fire == 0)
            continue;
        if (pl1Direction != 0)
            continue;
        _Player2StatusProc();
        if (pl2Fire == 0)
            continue;
        if (pl2Direction != 0)
            continue;
        break;
    }

    /* Draw the panic hex code centered on screen */
    /* d1=0x98 (x), d2=0xE1 (y), d3=0x0F (color=15) */
    _DrawMenuTextCentered(0x98, 0xE1, 0x0F, panicOutBuf, bigCharsTable);

    /* Wait for vsync and fade in */
    _WaitForVSync();
    paletteIndex = 4;
    _FadeIn();

    /* Infinite loop — system is halted */
    for (;;)
        ;
}
