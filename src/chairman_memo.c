/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* chairman_memo.c — Chairman memos and club business menu functions.
 *
 * Chunk 80 (partial): 5 exported functions ported from SWOS2.s lines 85420-85675.
 */

#include "chairman_memo.h"
#include "menu_system.h"
#include "input_text.h"

/* ---- ASM externs (globals) ---- */
extern MenuEntry menuEntries[];
extern uint16_t fireResetFlag;
extern uint16_t isNationalSelector;
extern volatile uint16_t currentMessage;
extern uint8_t *plNameForMessage;
extern volatile uint16_t currentGameTick;
extern uint16_t clubBusinessActive;
extern void _WaitForVSync(void);

/* ---- ASM externs (data labels) ---- */
extern uint8_t chairmanMemosMenu[];
extern uint8_t clubBusinessMenu[];
extern uint8_t *clubMessagesTable[];       /* array of string pointers, 2 per message */
extern uint8_t managerSexBuffer[];
extern uint8_t managerSurnameBuffer[];
extern uint8_t managerNameBuffer[];
extern uint8_t aDearAB_0[];               /* "DEAR %a %b" */
extern uint8_t aMemoFromThePre[];         /* "MEMO FROM THE PRESIDENT" */
extern uint8_t aThePresident[];           /* "THE PRESIDENT" */

/* ================================================================
 * ShowChairmanMessage  (SWOS2.s line 85422)
 *
 * Entry point for displaying chairman memos.
 * Loads the chairman memos menu and shows it.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ShowChairmanMessage(void)
{
    _ShowMenu(chairmanMemosMenu);
}

/* ================================================================
 * ChairmanMemosInit  (SWOS2.s line 85615)
 *
 * Menu init callback. Sets fire reset flag, changes title to
 * "MEMO FROM THE PRESIDENT" if national team, then falls through
 * to OnReturn to render the memo content.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ChairmanMemosInit(void)
{
    fireResetFlag = 1;

    if (isNationalSelector) {
        /* Change menu title to "MEMO FROM THE PRESIDENT" */
        menuEntries[0].fg = (uint32_t)aMemoFromThePre;
        /* Change sign-off to "THE PRESIDENT" (entry 5 = offset $118/56) */
        menuEntries[5].fg = (uint32_t)aThePresident;
    }

    /* Fall through to OnReturn */
    _ChairmanMemosOnReturn();
}

/* ================================================================
 * ChairmanMemosOnReturn  (SWOS2.s line 85626)
 *
 * Menu onReturn callback. Formats the "DEAR %a %b" greeting line
 * and the two message body lines from clubMessagesTable, draws the
 * menu, waits 100 VBlanks, then shows the "Continue" button.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ChairmanMemosOnReturn(void)
{
    /* Format "DEAR %a %b" into entry 1's text buffer */
    _PrintFormatted(aDearAB_0, (uint8_t *)menuEntries[1].fg,
                    0, 0, 0, 0, 0, 0, 0,
                    managerSexBuffer, managerSurnameBuffer, (void *)0);

    /* Look up message pair from clubMessagesTable (1-based, 2 pointers per message).
     * Guard against currentMessage being 0 or out of range (38 messages max).
     * Can happen if the chairman memo menu is triggered with stale state. */
    if (currentMessage == 0 || currentMessage > 38)
        return;

    int16_t msgIdx = (int16_t)(currentMessage - 1) * 2;

    /* Format message line 1 into entry 2's text buffer */
    _PrintFormatted((const uint8_t *)clubMessagesTable[msgIdx],
                    (uint8_t *)menuEntries[2].fg,
                    0, 0, 0, 0, 0, 0, 0,
                    managerNameBuffer, (const uint8_t *)plNameForMessage, (void *)0);

    /* Format message line 2 into entry 3's text buffer */
    _PrintFormatted((const uint8_t *)clubMessagesTable[msgIdx + 1],
                    (uint8_t *)menuEntries[3].fg,
                    0, 0, 0, 0, 0, 0, 0,
                    managerNameBuffer, (const uint8_t *)plNameForMessage, (void *)0);

    /* SDL build skips the original Amiga 100-VBlank (~2s) dwell before
     * the Continue button appears. With a series of chairman messages
     * during a season-end review, the cumulative wait was painful and
     * users have no way to short-circuit it on the original hardware
     * either — so we just show Continue immediately. */
    menuEntries[6].isInvisible = 0;
    _DrawMenu();
}

/* ================================================================
 * ChairmanMemosContinue  (SWOS2.s line 85666)
 *
 * "Continue" button callback. Clears the current message and
 * exits the menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ChairmanMemosContinue(void)
{
    currentMessage = 0;
    _SetExitMenuFlag();
}

/* ================================================================
 * ShowClubBussinesMenu  (SWOS2.s line 85672)
 *
 * Entry point for the club business/bank statement menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ShowClubBussinesMenu(void)
{
    clubBusinessActive = 0;
    _ShowMenu(clubBusinessMenu);
}
