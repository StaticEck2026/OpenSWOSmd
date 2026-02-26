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

/* ---- Menu entries (defined in asm) ---- */
extern MenuEntry menuEntries[];

/* ---- Globals (defined in asm) ---- */
extern uint16_t hilNumGoals;
extern uint32_t extension;
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
void _HighlightsMenuCommon(void)
{
    if (hilNumGoals == 0) {
        /* No goals — disable entries 1 and 3 */
        menuEntries[1].isDisabled = 1;
        uint16_t color1 = *(uint16_t *)&menuEntries[1].bg;
        color1 = (color1 & 0xFFF0) + PINK_TO_BROWN_9;
        *(uint16_t *)&menuEntries[1].bg = color1;

        menuEntries[3].isDisabled = 1;
        uint16_t color3 = *(uint16_t *)&menuEntries[3].bg;
        color3 = (color3 & 0xFFF0) + PINK_TO_BROWN_9;
        *(uint16_t *)&menuEntries[3].bg = color3;

        _SetCurrentEntry(2);
    } else {
        /* Goals exist — enable entries 1 and 3 */
        menuEntries[1].isDisabled = 0;
        uint16_t color1 = *(uint16_t *)&menuEntries[1].bg;
        color1 = (color1 & 0xFFF0) + YELLOW_GREEN_BLACK_14;
        *(uint16_t *)&menuEntries[1].bg = color1;

        menuEntries[3].isDisabled = 0;
        uint16_t color3 = *(uint16_t *)&menuEntries[3].bg;
        color3 = (color3 & 0xFFF0) + YELLOW_GREEN_BLACK_14;
        *(uint16_t *)&menuEntries[3].bg = color3;
    }
}

/* ================================================================
 * HighlightsMenuInit  (SWOS2.s line 91031)
 *
 * Initializes the highlights menu. Falls through to OnReturn
 * (which calls HighlightsMenuCommon).
 * ================================================================ */
__attribute__((used, externally_visible))
void _HighlightsMenuInit(void)
{
    _HighlightsMenuCommon();
}

/* ================================================================
 * HighlightsMenuOnReturn  (SWOS2.s line 91033)
 *
 * Called when returning to the highlights menu. Calls Common
 * to update enable/disable state.
 * ================================================================ */
__attribute__((used, externally_visible))
void _HighlightsMenuOnReturn(void)
{
    _HighlightsMenuCommon();
}

/* ================================================================
 * ExitHighlights  (SWOS2.s line 91142)
 *
 * Exits the highlights menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ExitHighlights(void)
{
    _SetExitMenuFlag();
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
void _LoadHighlightsMenu(void)
{
    if (_Asset_PromptInsertSaveDiskDf0(aLoadHighlights) != 0)
        return;

    if (_GetFilenameAndExtension(0, aLoadHighlights, aLoadHighlights) != 0)
        goto cleanup;

    if (extension != 0x2E48494Cu) { /* '.HIL' */
        _ShowErrorMenu(aNotAHighlights);
        goto cleanup;
    }

    if (_LoadHilFile(selectedFilename) != 0)
        goto cleanup;

    if (*(uint32_t *)hilFileBuffer != kHilFileSig) {
        _ShowErrorMenu(aCannotPlayOldV);
        hilNumGoals = 0;
        goto cleanup;
    }

    _StringCopy(selectedFilename, hilFilename);

cleanup:
    _ShowInsertDisk2Dialog(aLoadHighlights);
    _HighlightsMenuCommon();
}

/* ================================================================
 * SaveHighlightsMenu  (SWOS2.s line 89930)
 *
 * Menu callback: prompts for save disk, lets user select filename
 * with .HIL extension, saves highlights, and copies filename to
 * hilFilename on success.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SaveHighlightsMenu(void)
{
    if (_Asset_PromptInsertSaveDiskDf0(aSaveHighlights) != 0)
        return;

    if (_SelectFileToSaveDialog(0x2E48494Cu /* '.HIL' */, hilFilename, aSaveHighlights) != 0)
        goto cleanup;

    if (_DoSaveHighlights(selectedFilename) != 0)
        goto cleanup;

    _StringCopy(selectedFilename, hilFilename);

cleanup:
    _ShowInsertDisk2Dialog(aSaveHighlights);
}
