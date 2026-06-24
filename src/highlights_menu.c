/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* highlights_menu.c — Highlights menu functions.
 *
 * Chunk 83: HighlightsMenuInit, HighlightsMenuOnReturn,
 *           HighlightsMenuCommon, ExitHighlights
 * Chunk 89: LoadHighlightsMenu, SaveHighlightsMenu
 */

#include "game_types.h"
#include "menu_system.h"
#include "string_utils.h"
#include "file_io.h"
#include "menus.h"
#include "highlights_menu.h"
#include "endian_helpers.h"

/* ---- Menu entries (defined in asm) ---- */
extern MenuEntry menuEntries[];

/* ---- Globals (defined in asm) ---- */
extern uint16_t hilNumGoals;
extern uint16_t hilTeam1Goals;
extern uint16_t hilTeam2Goals;
extern uint16_t hilPitchNumber;
extern uint16_t hilMaxSubstitutes;
extern uintptr_t extension;
extern uint8_t selectedFilename[];
extern uint8_t hilFileBuffer[];
extern uint32_t kHilFileSig;
extern uint8_t hilFilename[];

/* ---- String literals (defined in asm) ---- */
extern uint8_t aLoadHighlights[];     /* "LOAD HIGHLIGHTS" */
extern uint8_t aSaveHighlights[];     /* "SAVE HIGHLIGHTS" */
extern uint8_t aNotAHighlights[];     /* "NOT A HIGHLIGHTS FILE" */
extern uint8_t aCannotPlayOldV[];     /* "CANNOT PLAY OLD VERSION..." */

/* ---- Color constants ---- */
#define PINK_TO_BROWN_9         9
#define YELLOW_GREEN_BLACK_14   14

/* ================================================================
 * HighlightsMenuCommon  (SWOS2.s line 91039)
 *
 * Enables or disables the highlights menu entries based on whether
 * there are any goals recorded (hilNumGoals). If no goals, entries
 * 1 and 3 are disabled and colored gray. If goals exist, they are
 * enabled and colored normally. Also sets the current entry to 2
 * when disabled (so cursor lands on "EXIT" instead of greyed item).
 * ================================================================ */
__attribute__((used, externally_visible))
void HighlightsMenuCommon(void)
{
    if (hilNumGoals == 0) {
        /* No goals — disable entries 1 and 3 */
        menuEntries[1].isDisabled = 1;
        uint16_t color1 = menuEntries[1].bgColor;
        color1 = (color1 & 0xFFF0) + PINK_TO_BROWN_9;
        menuEntries[1].bgColor = color1;

        menuEntries[3].isDisabled = 1;
        uint16_t color3 = menuEntries[3].bgColor;
        color3 = (color3 & 0xFFF0) + PINK_TO_BROWN_9;
        menuEntries[3].bgColor = color3;

        SetCurrentEntry(2);
    } else {
        /* Goals exist — enable entries 1 and 3 */
        menuEntries[1].isDisabled = 0;
        uint16_t color1 = menuEntries[1].bgColor;
        color1 = (color1 & 0xFFF0) + YELLOW_GREEN_BLACK_14;
        menuEntries[1].bgColor = color1;

        menuEntries[3].isDisabled = 0;
        uint16_t color3 = menuEntries[3].bgColor;
        color3 = (color3 & 0xFFF0) + YELLOW_GREEN_BLACK_14;
        menuEntries[3].bgColor = color3;
    }
}

/* ================================================================
 * HighlightsMenuInit  (SWOS2.s line 91031)
 *
 * Initializes the highlights menu. Falls through to OnReturn
 * (which calls HighlightsMenuCommon).
 * ================================================================ */
__attribute__((used, externally_visible))
void HighlightsMenuInit(void)
{
    HighlightsMenuCommon();
}

/* ================================================================
 * HighlightsMenuOnReturn  (SWOS2.s line 91033)
 *
 * Called when returning to the highlights menu. Calls Common
 * to update enable/disable state.
 * ================================================================ */
__attribute__((used, externally_visible))
void HighlightsMenuOnReturn(void)
{
    HighlightsMenuCommon();
}

/* ================================================================
 * ExitHighlights  (SWOS2.s line 91142)
 *
 * Exits the highlights menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void ExitHighlights(void)
{
    SetExitMenuFlag();
}

/* ================================================================
 * LoadHighlightsMenu  (SWOS2.s line 89894)
 *
 * Menu callback: prompts for save disk, lets user select a .HIL
 * file, loads it, verifies signature, and copies filename to
 * hilFilename. Shows error for wrong extension or old version.
 * Then calls HighlightsMenuCommon to update menu state.
 * ================================================================ */
__attribute__((used, externally_visible))
void LoadHighlightsMenu(void)
{
    if (Asset_PromptInsertSaveDiskDf0(aLoadHighlights) != 0)
        return;

    if (GetFilenameAndExtension(0, aLoadHighlights, aLoadHighlights) != 0)
        goto cleanup;

    if (extension != 0x2E48494Cu) { /* '.HIL' */
        ShowErrorMenu(aNotAHighlights);
        goto cleanup;
    }

    if (LoadHilFile(selectedFilename) != 0)
        goto cleanup;

    /* HIL file header is big-endian (Amiga m68k); BE_READ_U32 is a no-op
     * on the Amiga build and a byteswap on x86. */
    if (BE_READ_U32(hilFileBuffer) != kHilFileSig) {
        ShowErrorMenu(aCannotPlayOldV);
        hilNumGoals = 0;
        goto cleanup;
    }

    /* BE→host swap for the 5 u16 header fields now lives inside
     * LoadHilFile's legacy load branch so the JSON load path can
     * skip it. Both source formats converge on host-native here. */

    StringCopy(selectedFilename, hilFilename);

cleanup:
    ShowInsertDisk2Dialog(aLoadHighlights);
    HighlightsMenuCommon();
}

/* ================================================================
 * SaveHighlightsMenu  (SWOS2.s line 89930)
 *
 * Menu callback: prompts for save disk, lets user select filename
 * with .HIL extension, saves highlights, and copies filename to
 * hilFilename on success.
 * ================================================================ */
__attribute__((used, externally_visible))
void SaveHighlightsMenu(void)
{
    if (Asset_PromptInsertSaveDiskDf0(aSaveHighlights) != 0)
        return;

    if (SelectFileToSaveDialog(0x2E48494Cu /* '.HIL' */, hilFilename, aSaveHighlights) != 0)
        goto cleanup;

    if (DoSaveHighlights(selectedFilename) != 0)
        goto cleanup;

    StringCopy(selectedFilename, hilFilename);

cleanup:
    ShowInsertDisk2Dialog(aSaveHighlights);
}

/* ================================================================
 * Chunk 161: ShowHighlights, ShowMatchSequence,
 *            ViewHighlightsSaveCurrentMenu, JmpShowHighlights
 * ================================================================ */
extern uint16_t hilStarted;
extern uint16_t hilMaxSubstitutes;
extern uint16_t gameMaxSubstitutes;
extern uint16_t teamsLoaded;
extern uint16_t poolplyrLoaded;
extern uint16_t menuMusic;
extern uint16_t deadVarAlways0;
extern volatile uint16_t menuStatus;
extern volatile uint16_t exitMenu;
extern uintptr_t currentMenuPtr;
extern uint8_t  currentMenu[];

extern void CopyTeamsAndHeader(void);
extern void StopMusic(void);
extern void InitMenuMusic(void);
extern void TryPlayMusic(void);
extern void PrepareMenuForFadeIn(void);
extern void ViewMatchSequence(void);
extern void ViewHighlights(void);

/* These are menu structs defined in assembly as labels */
extern uint8_t stadiumMenu[];

/* ================================================================
 * viewAndRestoreMenu — helper for ShowHighlights/ShowMatchSequence
 *
 * Saves current menu state, calls a view function, then restores
 * the previous menu. Pattern used by both ShowHighlights and
 * ShowMatchSequence in the original asm.
 * ================================================================ */
static void viewAndRestoreMenu(void (*viewFunc)(void))
{
    Menu *menu = (Menu *)currentMenu;
    MenuEntry *savedSelectedEntry = menu->selectedEntry;
    uintptr_t savedMenuPtr = currentMenuPtr;

    viewFunc();

    menuStatus = 1;
    exitMenu = 0;
    RestorePreviousMenu(savedSelectedEntry, (uint8_t *)savedMenuPtr);
}

/* ================================================================
 * ShowHighlights  (SWOS2.s line 29873)
 *
 * Starts highlight replay: loads teams, shows stadium menu,
 * plays highlights, and restores menu music.
 * ================================================================ */
__attribute__((used, externally_visible))
void ShowHighlights(void)
{
    hilStarted = 1;
    gameMaxSubstitutes = hilMaxSubstitutes;
    teamsLoaded = 0;
    poolplyrLoaded = 0;
    CopyTeamsAndHeader();

    if (menuMusic != 0)
        StopMusic();

    ShowMenu(stadiumMenu);
    deadVarAlways0 = 0;

    viewAndRestoreMenu(ViewHighlights);

    InitMenuMusic();
    if (menuMusic != 0)
        TryPlayMusic();

    PrepareMenuForFadeIn();
}

/* ================================================================
 * ShowMatchSequence  (SWOS2.s line 29905)
 *
 * Saves current menu, plays match sequence, restores menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void ShowMatchSequence(void)
{
    viewAndRestoreMenu(ViewMatchSequence);
}

/* ================================================================
 * ViewHighlightsSaveCurrentMenu  (SWOS2.s line 29921)
 *
 * Saves current menu, views all highlights, restores menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void ViewHighlightsSaveCurrentMenu(void)
{
    viewAndRestoreMenu(ViewHighlights);
}

/* ================================================================
 * JmpShowHighlights  (SWOS2.s line 79492)
 *
 * Trivial forwarder to ShowHighlights.
 * ================================================================ */
__attribute__((used, externally_visible))
void JmpShowHighlights(void)
{
    ShowHighlights();
}
