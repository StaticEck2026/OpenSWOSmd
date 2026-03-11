/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "input.h"

/* ---- Hardware register definitions ---- */
#define HW_JOY0DAT  (*(volatile uint16_t *)0x00DFF00A)
#define HW_JOY1DAT  (*(volatile uint16_t *)0x00DFF00C)
#define HW_CIAA_PRA (*(volatile uint8_t  *)0x00BFE001)

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint8_t pl1Right;
extern uint8_t pl1Left;
extern uint8_t pl1Down;
extern uint8_t pl1Up;
extern uint8_t pl1Fire;
extern uint8_t pl1UpRight;
extern uint8_t pl1DownRight;
extern uint8_t pl1DownLeft;
extern uint8_t pl1UpLeft;
extern uint8_t pl1LastFired;
extern uint8_t pl1ShortFire;
extern uint16_t pl1FireCounter;
extern uint8_t pl1LeftPress;
extern uint8_t pl1WasLeft;
extern uint8_t pl1RightPress;
extern uint8_t pl1WasRight;
extern uint8_t pl1UpPress;
extern uint8_t pl1WasUp;
extern uint8_t pl1DownPress;
extern uint8_t pl1WasDown;
extern uint16_t pl1Direction;

/* ================================================================
 * Player1StatusProc  (SWOS2.s line 5562)
 *
 * Reads Amiga joystick port 1 hardware registers (JOY1DAT, CIAA_PRA)
 * and decodes them into direction flags, diagonal combinations,
 * press/release tracking state, and a compass direction value.
 *
 * JOY1DAT decoding (active-low inverted by sne/seq):
 *   bit 1  -> right
 *   bit 9  -> left
 *   bit 0 (after XOR with bit>>1) -> down
 *   bit 8 (after XOR with bit>>1) -> up
 *
 * CIAA_PRA bit 7 -> fire button (active low, seq inverts)
 *
 * Direction compass values:
 *   0=Up, 1=UpRight, 2=Right, 3=DownRight,
 *   4=Down, 5=DownLeft, 6=Left, 7=UpLeft,
 *   0xFFFF=no direction
 * ================================================================ */
__attribute__((used, externally_visible))
void _Player1StatusProc(void)
{
    uint16_t joy, decoded;

    /* ---- Read JOY1DAT and decode directions ---- */
    joy = HW_JOY1DAT;

    /* sne: set 0xFF if bit set, 0x00 otherwise */
    pl1Right = (joy & (1 << 1)) ? 0xFF : 0x00;
    pl1Left  = (joy & (1 << 9)) ? 0xFF : 0x00;

    /* XOR trick for vertical: d1 = d0 >> 1; d0 ^= d1 */
    decoded = joy ^ (joy >> 1);
    pl1Down = (decoded & (1 << 0)) ? 0xFF : 0x00;
    pl1Up   = (decoded & (1 << 8)) ? 0xFF : 0x00;

    /* Fire button: CIAA_PRA bit 7, active low (seq = set if equal/zero) */
    pl1Fire = (HW_CIAA_PRA & (1 << 7)) ? 0x00 : 0xFF;

    /* ---- Compute diagonal combinations ---- */
    pl1UpRight  = pl1Up   & pl1Right;
    pl1DownRight = pl1Down & pl1Right;
    pl1DownLeft  = pl1Down & pl1Left;
    pl1UpLeft    = pl1Up   & pl1Left;

    /* ---- Fire button press/release tracking ---- */
    if (pl1LastFired != 0)
    {
        /* Was previously pressed */
        pl1ShortFire = 0;
        if (pl1Fire == 0)
        {
            /* Released now */
            pl1LastFired = 0;
            pl1FireCounter = (uint16_t)(-(int16_t)pl1FireCounter);
        }
        else
        {
            /* Still held */
            if (pl1FireCounter != 0)
            {
                pl1FireCounter--;
            }
        }
    }
    else
    {
        /* Was not previously pressed */
        pl1ShortFire = pl1Fire;
        pl1LastFired = pl1Fire;
        if (pl1Fire != 0)
        {
            pl1FireCounter = 0xFFFF;
        }
    }

    /* ---- Left press/release tracking ---- */
    if (pl1WasLeft != 0)
    {
        pl1LeftPress = 0;
        if (pl1Left == 0)
            pl1WasLeft = 0;
    }
    if (pl1WasLeft == 0)
    {
        pl1LeftPress = pl1Left;
        pl1WasLeft = pl1Left;
    }

    /* ---- Right press/release tracking ---- */
    if (pl1WasRight != 0)
    {
        pl1RightPress = 0;
        if (pl1Right == 0)
            pl1WasRight = 0;
    }
    if (pl1WasRight == 0)
    {
        pl1RightPress = pl1Right;
        pl1WasRight = pl1Right;
    }

    /* ---- Up press/release tracking ---- */
    if (pl1WasUp != 0)
    {
        pl1UpPress = 0;
        if (pl1Up == 0)
            pl1WasUp = 0;
    }
    if (pl1WasUp == 0)
    {
        pl1UpPress = pl1Up;
        pl1WasUp = pl1Up;
    }

    /* ---- Down press/release tracking ---- */
    if (pl1WasDown != 0)
    {
        pl1DownPress = 0;
        if (pl1Down == 0)
            pl1WasDown = 0;
    }
    if (pl1WasDown == 0)
    {
        pl1DownPress = pl1Down;
        pl1WasDown = pl1Down;
    }

    /* ---- Compute compass direction ---- */
    {
        uint16_t dir;

        if (pl1UpRight)
            dir = 1;
        else if (pl1DownRight)
            dir = 3;
        else if (pl1DownLeft)
            dir = 5;
        else if (pl1UpLeft)
            dir = 7;
        else if (pl1Up)
            dir = 0;
        else if (pl1Right)
            dir = 2;
        else if (pl1Down)
            dir = 4;
        else if (pl1Left)
            dir = 6;
        else
            dir = 0xFFFF;

        pl1Direction = dir;
    }
}

/* ---- Player 2 globals (all live in SWOS2.s) ---- */
extern uint8_t pl2Right;
extern uint8_t pl2Left;
extern uint8_t pl2Down;
extern uint8_t pl2Up;
extern uint8_t pl2Fire;
extern uint8_t pl2UpRight;
extern uint8_t pl2DownRight;
extern uint8_t pl2DownLeft;
extern uint8_t pl2UpLeft;
extern uint8_t pl2LastFired;
extern uint8_t pl2ShortFire;
extern uint16_t pl2FireCounter;
extern uint8_t pl2LeftPress;
extern uint8_t pl2WasLeft;
extern uint8_t pl2RightPress;
extern uint8_t pl2WasRight;
extern uint8_t pl2UpPress;
extern uint8_t pl2WasUp;
extern uint8_t pl2DownPress;
extern uint8_t pl2WasDown;
extern uint16_t pl2Direction;

/* ================================================================
 * Player2StatusProc  (SWOS2.s line 5580)
 *
 * Reads Amiga joystick port 0 hardware registers (JOY0DAT, CIAA_PRA)
 * and decodes them into direction flags, diagonal combinations,
 * press/release tracking state, and a compass direction value.
 *
 * Identical logic to Player1StatusProc but reads JOY0DAT (port 0)
 * and CIAA_PRA bit 6 (second fire button), writing to pl2* globals.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Player2StatusProc(void)
{
    uint16_t joy, decoded;

    /* ---- Read JOY0DAT and decode directions ---- */
    joy = HW_JOY0DAT;

    pl2Right = (joy & (1 << 1)) ? 0xFF : 0x00;
    pl2Left  = (joy & (1 << 9)) ? 0xFF : 0x00;

    decoded = joy ^ (joy >> 1);
    pl2Down = (decoded & (1 << 0)) ? 0xFF : 0x00;
    pl2Up   = (decoded & (1 << 8)) ? 0xFF : 0x00;

    /* Fire button: CIAA_PRA bit 6, active low */
    pl2Fire = (HW_CIAA_PRA & (1 << 6)) ? 0x00 : 0xFF;

    /* ---- Compute diagonal combinations ---- */
    pl2UpRight   = pl2Up   & pl2Right;
    pl2DownRight = pl2Down & pl2Right;
    pl2DownLeft  = pl2Down & pl2Left;
    pl2UpLeft    = pl2Up   & pl2Left;

    /* ---- Fire button press/release tracking ---- */
    if (pl2LastFired != 0)
    {
        pl2ShortFire = 0;
        if (pl2Fire == 0)
        {
            pl2LastFired = 0;
            pl2FireCounter = (uint16_t)(-(int16_t)pl2FireCounter);
        }
        else
        {
            if (pl2FireCounter != 0)
            {
                pl2FireCounter--;
            }
        }
    }
    else
    {
        pl2ShortFire = pl2Fire;
        pl2LastFired = pl2Fire;
        if (pl2Fire != 0)
        {
            pl2FireCounter = 0xFFFF;
        }
    }

    /* ---- Left press/release tracking ---- */
    if (pl2WasLeft != 0)
    {
        pl2LeftPress = 0;
        if (pl2Left == 0)
            pl2WasLeft = 0;
    }
    if (pl2WasLeft == 0)
    {
        pl2LeftPress = pl2Left;
        pl2WasLeft = pl2Left;
    }

    /* ---- Right press/release tracking ---- */
    if (pl2WasRight != 0)
    {
        pl2RightPress = 0;
        if (pl2Right == 0)
            pl2WasRight = 0;
    }
    if (pl2WasRight == 0)
    {
        pl2RightPress = pl2Right;
        pl2WasRight = pl2Right;
    }

    /* ---- Up press/release tracking ---- */
    if (pl2WasUp != 0)
    {
        pl2UpPress = 0;
        if (pl2Up == 0)
            pl2WasUp = 0;
    }
    if (pl2WasUp == 0)
    {
        pl2UpPress = pl2Up;
        pl2WasUp = pl2Up;
    }

    /* ---- Down press/release tracking ---- */
    if (pl2WasDown != 0)
    {
        pl2DownPress = 0;
        if (pl2Down == 0)
            pl2WasDown = 0;
    }
    if (pl2WasDown == 0)
    {
        pl2DownPress = pl2Down;
        pl2WasDown = pl2Down;
    }

    /* ---- Compute compass direction ---- */
    {
        uint16_t dir;

        if (pl2UpRight)
            dir = 1;
        else if (pl2DownRight)
            dir = 3;
        else if (pl2DownLeft)
            dir = 5;
        else if (pl2UpLeft)
            dir = 7;
        else if (pl2Up)
            dir = 0;
        else if (pl2Right)
            dir = 2;
        else if (pl2Down)
            dir = 4;
        else if (pl2Left)
            dir = 6;
        else
            dir = 0xFFFF;

        pl2Direction = dir;
    }
}
