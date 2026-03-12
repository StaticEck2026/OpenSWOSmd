/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* edit_tactics.c — Edit tactics menu functions.
 *
 * Chunk 82: ChooseTacticsInit, ShowStandardChooseTacticsMenu,
 *           EditTacticsInit, EditTacticsOnRestore,
 *           EnableOrDisablePlayerFacesItems, CopyCurrentTacticName,
 *           EditTactics_UpdateExtraPlayerMenuVisibility, SetBigCheckSprite,
 *           CopyCurrentTactics, ApplyCurrentEditedTactics,
 *           SaveCurrentTactics, RestoreTactics
 * Chunk 83: DrawPlayerNamesAndFaces, EditTacticsMenuSavePlayerPositions,
 *           EditTactics_CommitPlayerPositionsAndRecalcSkill,
 *           ImportTacticsInit, ImportTacticsSelectTactics, ExitImportTactics,
 *           MarkSelectedTeamsTacticsChanged, FillSkinColorConversionTable
 * Chunk 84: SelectCopyTactics, UndoTactics, AbortEditTactics, FlipOnOff,
 *           ImportTactics, SaveAndExitEditTactics,
 *           EditTacticsOnSelect28, EditTacticsAfterDrawPlayerFace,
 *           EditTactics28AfterDraw, EditTactics2AfterDraw
 * Chunk 85: GetPlayerQuadrant, SetPlayerQuadrant, GetBallQuadrant,
 *           GetBallXYPositionFromQuadrant, GetPlayerXYPositionFromQuadrant,
 *           EditTactics_GetDirectionDelta, GetEditTacticsItemXYPosition,
 *           EditTactics_TryMoveQuadrantCursor,
 *           EditTactics_SetAllPlayersQuadrantForBallLoc,
 *           EditTactics_FlipMaybe_MirrorPlayersInArea,
 *           EditTactics_FlipMaybe_MirrorBallArea,
 *           EditTactics_CommitDraggedPlayerPosition,
 *           SortEditTacticsDrawOrderByY, SortPlayerAreasByY,
 *           EditTacticsDrawCursor, DrawPlayerDirections
 * Chunk 86: InitLittlePlayerSprites, DrawLittlePlayersAndBall,
 *           DrawEditTacticsHighlightedPlayerRoute
 * Chunk 87: EditTacicsMovePlayersOrBall, CopyTactics,
 *           DrawEditTacticsPitchAndPlayers, EditTacticsReadControls,
 *           HandlePlayersOrBallInEditTactics, EditTacticsStatus,
 *           EditTactics_HandlePitchInput
 * Chunk 88: GetPlayerInfoBreakPosition, EditTactics_FindFreePairId,
 *           EditTactics_ClearPairId, EditTactics_SwapPlayersInSlots,
 *           EditTacticsCommon, EditTacticsPlayerNamesCommon
 * Chunk 89: LoadTactics
 */

#include "edit_tactics.h"
#include "menu_system.h"
#include "string_utils.h"
#include "file_io.h"
#include "teams.h"
#include "menus.h"
#include "math68k.h"
#include "panic.h"
#include "sprites.h"
#include "team_colors.h"
#include "input_text.h"
#include "career_teams.h"
#include "career_status.h"
#include "play_match_menu.h"
#include "player_sprites.h"
#include "match_sim.h"

/* ---- Menu entries (defined in asm) ---- */
extern MenuEntry menuEntries[];

/* ---- Tactics data (defined in asm) ---- */
extern uint8_t teamTacticsUSERA;    /* start of USER A tactics (name at offset 0) */
extern uint8_t teamTacticsUSERB;
extern uint8_t teamTacticsUSERC;
extern uint8_t teamTacticsUSERD;
extern uint8_t teamTacticsUSERE;
extern uint8_t teamTacticsUSERF;

extern uint32_t teamTacticsTable[];  /* pointer table indexed by chosenTactics */
extern TeamTactics editTacticsCurrentTactics;  /* 370-byte tactics buffer */
extern uint8_t savedCurrentTactics;        /* 370-byte saved tactics buffer */

/* ---- Globals (defined in asm) ---- */
extern uint16_t chosenTactics;
extern uint16_t flipOnOff;
extern uint32_t chooseTacticsTeamPtr;   /* stored as uint32_t, used as pointer */
extern uint16_t selTeamNumber;
extern uint16_t inEditTactics;
extern uint16_t careerNumPlayers;
extern uint16_t editTacticsNumPlayers;
extern uint16_t editTacticsSelectedPairSlotIndex;
extern uint32_t editTacticsSelectedPlayerMenuEntryPtr;
extern uint16_t editTacticsSelectedPlayerNameIndex;
extern uint16_t oldPlayerSkill;
extern uint16_t newPlayerSkill;

/* ---- Globals for Chunk 84 ---- */
extern uint16_t tacticsChangedFlag;
extern uint8_t importTacticsMenu[];
extern uint8_t selectedFilename[];
extern uint32_t extension;              /* file extension from GetFilenameAndExtension */
extern uint8_t importTacticsFilename;   /* filename buffer */

/* ---- String literals (defined in asm) ---- */
extern uint8_t aLoadEditedTact[];               /* "LOAD EDITED TACTICS" */
extern uint8_t aNotATacticsFil[];               /* "NOT A TACTICS FILE" */
extern uint8_t aEditTactics[];                  /* "EDIT TACTICS" */
extern uint8_t aEditedTacticsWillBeLost[];      /* "\x01EDITED TACTICS WILL BE LOST" */
extern uint8_t aContinue[];                     /* "CONTINUE" */
extern uint8_t aAbort[];                        /* "ABORT" */
extern uint8_t aSaveEditedTact[];               /* "SAVE EDITED TACTICS" */
extern uint8_t aTacticsHaveBeenChanged[];       /* "\x01TACTICS HAVE BEEN CHANGED" */
extern uint8_t aSave[];                         /* "SAVE" */
extern uint8_t aExit[];                         /* "EXIT" */
extern int16_t editTacticsRotateRedrawList[];   /* {10, 27, -1} */

/* ---- Globals for Chunk 85 ---- */
extern uint16_t editTacticsPitchStatus;
extern uint16_t markedPlayerNumber;
extern uint16_t ballRow;
extern uint16_t ballColumn;
extern uint16_t editTacticsBallLocation;
extern uint16_t editTacticsLastBallLocation;
extern uint16_t editTacticsPlayerX;
extern uint16_t editTacticsBallRow;
extern uint16_t editTacticsBallColumn;
extern uint16_t editTacticsDisableLittlePlayersAndBallDraw;
extern uint16_t editTacticsTransitionActive;
extern uint16_t editTacticsLastTick;
extern int16_t editTacticsPlayerPositions[];    /* 12 entries: -1,0,1,...,10 */
extern uint32_t editTacticsPlayerNumbers;       /* pointer stored as uint32_t */
extern int16_t playerAreas[];                   /* 35 entries: 0..34 */
extern uint16_t menuControlsDirection;
extern uint16_t menuControlsDirection2;
extern uint16_t fire;
extern uint16_t currentGameTick;
extern uint16_t panicReasonCode;

/* ---- Lookup tables (defined in asm) ---- */
extern int16_t ballXQuadrantCoordinates[];      /* 5 entries */
extern int16_t ballYQuadrantCoordinates[];      /* 7 entries */
extern int16_t xQuadrantToCoordinates[];        /* 15 entries */
extern int16_t yQuadrantToCoordinates[];        /* 16 entries */
extern int8_t arrowSpritesOffsets[];             /* 16 bytes (8 pairs of x,y) */
extern uint16_t tacticCursorFrames[];            /* 4 entries */
extern uint8_t editTacticsDirectionDeltaTable[]; /* 16 bytes (8 pairs) */

/* ---- Globals for Chunk 86 ---- */
extern uint16_t littlePlayersConvertTable[];    /* 16-word color conversion table */
extern uint8_t currentMatchPlayers[];           /* player number array for career team */
extern TeamFileEx careerTeam;                    /* start of career team data */

/* ---- Globals for Chunk 87 ---- */
extern int16_t fireCounter;
extern uint8_t up;
extern uint8_t down;
extern uint8_t left;
extern uint8_t right;
extern uint16_t editTacticsInputRepeatCounter;
extern uint16_t editTacticsNearestSelectPitchWidth;

/* ---- Globals for Chunk 88 ---- */
extern uint32_t positionsTable[];       /* pointer table indexed by formation */
extern uint16_t editTacticsSelectedPlayerSlotIndex;
extern uint32_t editTacticsSelectedPairMenuEntryPtr;
extern uint16_t gameType;
extern uint8_t playerPositions[];       /* player positions translation table (current team) */
extern uint16_t _cGetPlayerAtIndexFileIdx;
extern uint8_t *_GetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal);

/* ---- Menu definitions (defined in asm) ---- */
extern uint8_t chooseTacticsMenu[];
extern uint8_t editTacticsMenu[];

/* ---- C-callable bridges for asm functions (defined in swos2_unit.s) ---- */
extern void _SetBenchPlayersNumbers(uint8_t *teamPtr);
extern void _EditTactics_CommitPlayerPositionsAndRecalcSkill(void);
extern void _EditTacticsMenuSavePlayerPositions(void);
extern void _InitLittlePlayerSprites(void);
extern void _InitializeTacticsPositions(void);
extern void _DrawEditTacticsPitchAndPlayers(void);
/* _EditTactics_ApplyPlayerRowHighlighting declared in edit_tactics.h */
extern void _RotatePlayersLeft(uint8_t *teamPtr, uint16_t maxSubs);

/* ---- Sprite constants ---- */
#define SPR_CHECK_BIG       206
#define SPR_NOT_CHECK_BIG   207

/* ================================================================
 * ChooseTacticsInit  (SWOS2.s line 86669)
 *
 * Initializes the choose tactics menu by setting the fg string
 * pointer of menu entries 1-6 to the tactics name addresses
 * (USER A through USER F). Also calls CheckIsTeamNational.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ChooseTacticsInit(void)
{
    _CheckIsTeamNational();
    menuEntries[1].fg = (uint32_t)&teamTacticsUSERA;
    menuEntries[2].fg = (uint32_t)&teamTacticsUSERB;
    menuEntries[3].fg = (uint32_t)&teamTacticsUSERC;
    menuEntries[4].fg = (uint32_t)&teamTacticsUSERD;
    menuEntries[5].fg = (uint32_t)&teamTacticsUSERE;
    menuEntries[6].fg = (uint32_t)&teamTacticsUSERF;
}

/* ================================================================
 * ShowStandardChooseTacticsMenu  (SWOS2.s line 86450)
 *
 * Shows the choose tactics menu, then if a tactic was chosen
 * (chosenTactics != 0xFFFF), initializes little player sprites
 * and shows the edit tactics menu. Clears chooseTacticsTeamPtr
 * when done.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ShowStandardChooseTacticsMenu(uint8_t *teamPtr)
{
    chooseTacticsTeamPtr = (uint32_t)teamPtr;
    _ShowMenu(chooseTacticsMenu);

    if (chosenTactics != 0xFFFF) {
        _InitLittlePlayerSprites();
        _ShowMenu(editTacticsMenu);
    }

    chooseTacticsTeamPtr = 0;
}

/* ================================================================
 * EditTacticsInit  (SWOS2.s line 87481)
 *
 * Initializes the edit tactics menu. Sets bench player numbers,
 * determines editTacticsNumPlayers based on whether selected team
 * matches the career team, then clears selection state and
 * falls through to EditTacticsOnRestore.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTacticsInit(void)
{
    uint8_t *teamPtr = (uint8_t *)chooseTacticsTeamPtr;
    _SetBenchPlayersNumbers(teamPtr);

    uint16_t numPlayers;
    if (selTeamNumber == 0xFFFF || *(uint16_t *)teamPtr != selTeamNumber) {
        numPlayers = 16;
    } else {
        numPlayers = careerNumPlayers;
    }
    editTacticsNumPlayers = numPlayers;

    editTacticsSelectedPairSlotIndex = 0xFFFF;
    editTacticsSelectedPlayerMenuEntryPtr = 0;
    editTacticsSelectedPlayerNameIndex = 0xFFFF;

    _EditTactics_CommitPlayerPositionsAndRecalcSkill();
    _EditTacticsMenuSavePlayerPositions();

    /* Fallthrough to EditTacticsOnRestore */
    _EditTacticsOnRestore();
}

/* ================================================================
 * EditTacticsOnRestore  (SWOS2.s line 87506)
 *
 * Called when returning to the edit tactics menu. Enables/disables
 * player face items, copies tactic name, updates extra player
 * visibility, and sets the big check sprite.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTacticsOnRestore(void)
{
    _EnableOrDisablePlayerFacesItems();
    _CopyCurrentTacticName();
    _EditTactics_UpdateExtraPlayerMenuVisibility();
    _SetBigCheckSprite();
}

/* ================================================================
 * EnableOrDisablePlayerFacesItems  (SWOS2.s line 87869)
 *
 * Enables or disables 10 player face items in the edit tactics
 * menu based on the flipOnOff flag. Items start at menuEntries
 * index 30 (offset 0x658 / 56 = 29, but MENUENTRY_ALREADYDRAWN
 * offset $658 from menuEntries base = entry index
 * 0x658/56 = 28.89... Let me recalculate: 0x658 = 1624,
 * 1624/56 = 29). So entries 29..38 (10 entries).
 * ================================================================ */
__attribute__((used, externally_visible))
void _EnableOrDisablePlayerFacesItems(void)
{
    /* menuEntries + 0x658 = entry at byte offset 1624 = index 29
     * (0x658 / 56 = 29) */
    MenuEntry *entry = &menuEntries[29];
    for (int16_t i = 9; i >= 0; i--) {
        if (flipOnOff == 0) {
            entry->isDisabled = 1;
        } else {
            entry->isDisabled = 0;
        }
        entry++;
    }
}

/* ================================================================
 * CopyCurrentTacticName  (SWOS2.s line 87887)
 *
 * Builds the string "EDIT TACTICS (tacticname)" in the fg string
 * buffer of menu entry 1. Uses StringCopy for the parts.
 * ================================================================ */
__attribute__((used, externally_visible))
void _CopyCurrentTacticName(void)
{
    /* menuEntries[1].fg holds a pointer to the output string buffer */
    uint8_t *dst = (uint8_t *)menuEntries[1].fg;

    /* Copy "EDIT TACTICS" */
    dst = _StringCopy(aEditTactics, dst);

    /* Append " (" */
    *dst++ = ' ';
    *dst++ = '(';

    /* Copy current tactics name */
    dst = _StringCopy((uint8_t *)editTacticsCurrentTactics.name, dst);

    /* Append ")" and NUL */
    *dst++ = ')';
    *dst = 0;
}

/* ================================================================
 * EditTactics_UpdateExtraPlayerMenuVisibility  (SWOS2.s line 87992)
 *
 * Shows/hides extra player menu entries (17th and 18th players)
 * based on editTacticsNumPlayers. If 16, both hidden. If 17,
 * first shown. If 18, both shown.
 *
 * Entries at offsets 0x5E8 and 0x620:
 *   0x5E8 / 56 = 26.857... → but with MENUENTRY_ALREADYDRAWN=0,
 *   0x5E8 = 1512, 1512/56 = 27. Entry 27.
 *   0x620 = 1568, 1568/56 = 28. Entry 28.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTactics_UpdateExtraPlayerMenuVisibility(void)
{
    /* Start with both invisible */
    menuEntries[27].isInvisible = 1;
    menuEntries[28].isInvisible = 1;

    int16_t extra = (int16_t)(editTacticsNumPlayers - 16);
    if (extra == 0)
        return;

    /* At least 1 extra player - show entry 27 */
    menuEntries[27].isInvisible = 0;

    if (extra == 1)
        return;

    /* 2 extra players - show entry 28 */
    menuEntries[28].isInvisible = 0;
}

/* ================================================================
 * SetBigCheckSprite  (SWOS2.s line 88355)
 *
 * Shows/hides the big check/not-check sprite on menuEntries[0]
 * based on comparison of oldPlayerSkill vs newPlayerSkill.
 *   - If equal: hide entry, clear sprite bit
 *   - If new > old: show NOT_CHECK_BIG (skill decreased)
 *   - If new < old: show CHECK_BIG (skill increased)
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetBigCheckSprite(void)
{
    MenuEntry *entry = &menuEntries[0];

    /* Start invisible, clear bit 13 of stringFlags */
    entry->isInvisible = 1;
    entry->stringFlags &= ~(1u << 13);

    if (oldPlayerSkill == newPlayerSkill)
        return;

    /* Determine which sprite to show */
    if (oldPlayerSkill > newPlayerSkill) {
        /* Skill decreased - show red X */
        *(uint16_t *)&entry->fg = SPR_CHECK_BIG;
    } else {
        /* Skill increased - show green check */
        *(uint16_t *)&entry->fg = SPR_NOT_CHECK_BIG;
    }

    /* Set bit 13 of stringFlags and make visible */
    entry->stringFlags |= (1u << 13);
    entry->isInvisible = 0;
}

/* ================================================================
 * CopyCurrentTactics  (SWOS2.s line 89419)
 *
 * Copies 370 bytes from teamTacticsTable[chosenTactics] into
 * editTacticsCurrentTactics.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _CopyCurrentTactics(void)
{
    uint8_t *src = (uint8_t *)teamTacticsTable[chosenTactics];
    uint8_t *dst = (uint8_t *)&editTacticsCurrentTactics;

    for (int16_t i = 369; i >= 0; i--) {
        *dst++ = *src++;
    }
}

/* ================================================================
 * ApplyCurrentEditedTactics  (SWOS2.s line 89433)
 *
 * Copies 370 bytes from editTacticsCurrentTactics back into
 * teamTacticsTable[chosenTactics], then calls
 * InitializeTacticsPositions.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _ApplyCurrentEditedTactics(void)
{
    uint8_t *dst = (uint8_t *)teamTacticsTable[chosenTactics];
    uint8_t *src = (uint8_t *)&editTacticsCurrentTactics;

    for (int16_t i = 369; i >= 0; i--) {
        *dst++ = *src++;
    }

    _InitializeTacticsPositions();
}

/* ================================================================
 * SaveCurrentTactics  (SWOS2.s line 89485)
 *
 * Copies 370 bytes from editTacticsCurrentTactics into
 * savedCurrentTactics.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _SaveCurrentTactics(void)
{
    uint8_t *src = (uint8_t *)&editTacticsCurrentTactics;
    uint8_t *dst = &savedCurrentTactics;

    for (int16_t i = 369; i >= 0; i--) {
        *dst++ = *src++;
    }
}

/* ================================================================
 * RestoreTactics  (SWOS2.s line 89496)
 *
 * Copies 370 bytes from savedCurrentTactics back into
 * editTacticsCurrentTactics.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _RestoreTactics(void)
{
    uint8_t *src = &savedCurrentTactics;
    uint8_t *dst = (uint8_t *)&editTacticsCurrentTactics;

    for (int16_t i = 369; i >= 0; i--) {
        *dst++ = *src++;
    }
}

/* ================================================================
 * DrawPlayerNamesAndFaces  (SWOS2.s line 88239)
 *
 * Draws player name/face menu items in the edit tactics menu.
 * Draws entry at offset 0x268 (index 11), then 10 entries starting
 * at 0x658 (index 29), then entry 0 (the check sprite).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawPlayerNamesAndFaces(void)
{
    _CallDrawMenuItem(&menuEntries[11]);
    _CallDrawMultipleItems(10, &menuEntries[29]);
    _CallDrawMenuItem(&menuEntries[0]);
}

/* ================================================================
 * EditTacticsMenuSavePlayerPositions  (SWOS2.s line 88253)
 *
 * Copies playerPositionsArray (32 bytes) into oldPlayerPositionsArray,
 * sets newPlayerSkill = oldPlayerSkill, then calls SetBigCheckSprite.
 * Note: original asm does full d0-d7/a0-a6 save/restore internally.
 * ================================================================ */
extern uint8_t playerPositionsArray;   /* 32-byte array */
extern uint8_t oldPlayerPositionsArray; /* 32-byte array */

__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _EditTacticsMenuSavePlayerPositions(void)
{
    newPlayerSkill = oldPlayerSkill;
    uint8_t *src = &playerPositionsArray;
    uint8_t *dst = &oldPlayerPositionsArray;
    for (int16_t i = 31; i >= 0; i--) {
        *dst++ = *src++;
    }
    _SetBigCheckSprite();
}

/* ================================================================
 * EditTactics_CommitPlayerPositionsAndRecalcSkill  (SWOS2.s line 88268)
 *
 * Copies playerPositionsArray (32 bytes) to oldPlayerPositionsArray,
 * sets newPlayerSkill = oldPlayerSkill, sets team flag,
 * temporarily changes team's tactics byte to 0x12, calls
 * ComputeOldPlayerSkillForViewResult with d0=5, restores the
 * original tactics byte, then falls through to SetBigCheckSprite.
 * ================================================================ */
extern uint16_t _ComputeOldPlayerSkillForViewResult(uint8_t *team, uint16_t goalsCount);

__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _EditTactics_CommitPlayerPositionsAndRecalcSkill(void)
{
    newPlayerSkill = oldPlayerSkill;
    uint8_t *src = &playerPositionsArray;
    uint8_t *dst = &oldPlayerPositionsArray;
    for (int16_t i = 31; i >= 0; i--) {
        *dst++ = *src++;
    }

    TeamFileEx *teamPtr = (TeamFileEx *)chooseTacticsTeamPtr;
    teamPtr->team.someFlag = 1;

    /* Temporarily set tactics byte to 0x12, compute skill, restore */
    uint8_t savedTactics = teamPtr->team.tactics;
    teamPtr->team.tactics = 0x12;
    _ComputeOldPlayerSkillForViewResult((uint8_t *)teamPtr, 5);
    teamPtr->team.tactics = savedTactics;

    /* Fallthrough to SetBigCheckSprite */
    _SetBigCheckSprite();
}

/* ================================================================
 * ImportTacticsInit  (SWOS2.s line 89824)
 *
 * No-op initialization for the import tactics menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ImportTacticsInit(void)
{
    /* Empty — original was just rts */
}

/* ================================================================
 * ImportTacticsSelectTactics  (SWOS2.s line 89829)
 *
 * Menu callback when a tactics entry is selected in the import
 * tactics menu. Calculates the ordinal difference between the
 * selected entry and entry 1 (base), stores in importMenuSelectedTactics,
 * then exits the menu.
 * ================================================================ */
extern uint16_t importMenuSelectedTactics;

__attribute__((used, externally_visible))
void _ImportTacticsSelectTactics(MenuEntry *cur)
{
    importMenuSelectedTactics = cur->ordinal - menuEntries[1].ordinal;
    _SetExitMenuFlag();
}

/* ================================================================
 * ExitImportTactics  (SWOS2.s line 89838)
 *
 * Sets importMenuSelectedTactics to -1 (abort) and exits the menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ExitImportTactics(void)
{
    importMenuSelectedTactics = 0xFFFF;
    _SetExitMenuFlag();
}

/* ================================================================
 * MarkSelectedTeamsTacticsChanged  (SWOS2.s line 89983)
 *
 * Marks the career team and all selected teams whose tactics index
 * matches chosenTactics with the "tactics changed" flag.
 * ================================================================ */

extern uint16_t numSelectedTeams;
extern uint8_t selTeamsPtr;        /* start of selected teams array, stride = 0x2AC = 684 bytes */

#define SELECTED_TEAM_STRIDE   684

__attribute__((used, externally_visible))
void _MarkSelectedTeamsTacticsChanged(void)
{
    if (selTeamNumber != 0xFFFF) {
        careerTeam.team.someFlag = 1;
    }

    uint16_t count = numSelectedTeams;
    if (count == 0)
        return;

    TeamFileEx *team = (TeamFileEx *)&selTeamsPtr;
    for (int16_t i = (int16_t)(count - 1); i >= 0; i--) {
        if (team->team.tactics == (uint8_t)chosenTactics) {
            team->team.someFlag = 1;
        }
        team = (TeamFileEx *)((uint8_t *)team + SELECTED_TEAM_STRIDE);
    }
}

/* ================================================================
 * FillSkinColorConversionTable  (SWOS2.s line 90889)
 *
 * Fills a sprite color conversion table from the skin/hair color
 * conversion table. Reads entries from the table until a negative
 * terminator is encountered.
 * ================================================================ */
extern uint32_t skinAndHairColorConversionTable[];  /* array of pointers to conversion subtables */

__attribute__((used, externally_visible))
void _FillSkinColorConversionTable(uint16_t skinColor, uint16_t *convertTable)
{
    int16_t idx = (int16_t)(int8_t)(uint8_t)skinColor;  /* ext.w d0 */
    int16_t *subtable = (int16_t *)skinAndHairColorConversionTable[idx];

    for (;;) {
        int16_t entry = *subtable++;
        if (entry < 0)
            break;
        convertTable[entry] = (uint16_t)*subtable++;
    }
}

/* ================================================================
 * SelectCopyTactics  (SWOS2.s line 87689)
 *
 * Menu callback: moves cursor to entry 3 (copy tactics option).
 * ================================================================ */
__attribute__((used, externally_visible))
void _SelectCopyTactics(void)
{
    _SetCurrentEntry(3);
}

/* ================================================================
 * UndoTactics  (SWOS2.s line 87696)
 *
 * Menu callback: restores tactics to last saved state, redraws
 * the status entry (index 2), recalculates skill, and redraws
 * player names/faces.
 * ================================================================ */
__attribute__((used, externally_visible))
void _UndoTactics(void)
{
    _RestoreTactics();
    _CallDrawMenuItem(&menuEntries[2]);
    _EditTactics_CommitPlayerPositionsAndRecalcSkill();
    _DrawPlayerNamesAndFaces();
}

/* ================================================================
 * AbortEditTactics  (SWOS2.s line 87706)
 *
 * Menu callback: aborts edit tactics. If tactics were changed,
 * shows a confirmation dialog ("EDITED TACTICS WILL BE LOST").
 * If user picks "CONTINUE" (proceed with abort), exits the menu.
 * If user picks "ABORT" (cancel the abort), returns to editing.
 * ================================================================ */
__attribute__((used, externally_visible))
void _AbortEditTactics(void)
{
    _EditTacticsMenuSavePlayerPositions();
    if (tacticsChangedFlag != 0) {
        int16_t result = _DoContinueAbortMenu(
            aEditTactics,
            aEditedTacticsWillBeLost,
            aContinue,
            aAbort,
            1
        );
        if (result != 0)
            return;
    }
    _SetExitMenuFlag();
}

/* ================================================================
 * FlipOnOff  (SWOS2.s line 87820)
 *
 * Menu callback: toggles player face display on/off, then
 * enables/disables the corresponding menu entries and redraws.
 * ================================================================ */
__attribute__((used, externally_visible))
void _FlipOnOff(void)
{
    _EditTacticsMenuSavePlayerPositions();
    flipOnOff ^= 1;
    _EnableOrDisablePlayerFacesItems();
    _DrawMenu();
}

/* ================================================================
 * ImportTactics  (SWOS2.s line 87788)
 *
 * Menu callback: shows the import tactics submenu. If a standard
 * tactic is selected (not TACTIC_IMPORTED=1000), saves current
 * tactics, copies 370 position bytes from the selected tactic
 * into editTacticsCurrentTactics (preserving the name header),
 * then marks teams as changed and recalculates skill.
 * ================================================================ */
#define TACTIC_IMPORTED         1000

__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _ImportTactics(void)
{
    _EditTacticsMenuSavePlayerPositions();

    _ShowMenu(importTacticsMenu);

    int16_t selected = (int16_t)importMenuSelectedTactics;
    if (selected < 0)
        return;

    if (selected != TACTIC_IMPORTED) {
        _SaveCurrentTactics();
        tacticsChangedFlag = 1;

        /* Copy positions + unkTable + ballOutOfPlay from the selected tactic (361 bytes) */
        TeamTactics *srcTactics = (TeamTactics *)teamTacticsTable[selected];
        uint8_t *src = (uint8_t *)srcTactics->positions;
        uint8_t *dst = (uint8_t *)editTacticsCurrentTactics.positions;
        for (int16_t i = 369; i >= 0; i--) {
            *dst++ = *src++;
        }
    }

    /* Mark team as changed */
    TeamFileEx *teamEx = (TeamFileEx *)chooseTacticsTeamPtr;
    teamEx->team.someFlag = 1;
    _MarkSelectedTeamsTacticsChanged();
    _CopyCurrentTacticName();
    _EditTactics_CommitPlayerPositionsAndRecalcSkill();
}

/* ================================================================
 * SaveAndExitEditTactics  (SWOS2.s line 87724)
 *
 * Menu callback: saves edited tactics to disk. If tactics were
 * changed, applies them and shows a save/exit confirmation.
 * If user chooses "SAVE", prompts for disk, builds filename,
 * writes the .TAC file, updates the import filename, and exits.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SaveAndExitEditTactics(void)
{
    _EditTacticsMenuSavePlayerPositions();

    if (tacticsChangedFlag != 0) {
        _ApplyCurrentEditedTactics();
        _MarkSelectedTeamsTacticsChanged();

        int16_t result = _DoContinueAbortMenu(
            aSaveEditedTact,
            aTacticsHaveBeenChanged,
            aSave,
            aExit,
            0
        );

        if (result == 0) {
            /* User chose "SAVE" — proceed to file save dialog */
            if (_Asset_PromptInsertSaveDiskDf0(aSaveEditedTact) != 0)
                return;

            /* Build default filename: tactics name + ".TAC" */
            uint8_t *end = _StringCopy((uint8_t *)editTacticsCurrentTactics.name,
                                       &importTacticsFilename);
            *end++ = '.';
            *end++ = 'T';
            *end++ = 'A';
            *end++ = 'C';
            *end = '\0';

            /* Show file selection dialog */
            if (_SelectFileToSaveDialog(0x2E544143,
                                        &importTacticsFilename,
                                        aSaveEditedTact) != 0)
                goto cleanup;

            /* Copy selected filename (without extension) as new tactics name */
            _StringCopyUntilChar(selectedFilename,
                                 (uint8_t *)editTacticsCurrentTactics.name, 0x2E);

            /* Write the tactics file */
            if (_WriteTacticsFile(selectedFilename,
                                  (uint8_t *)&editTacticsCurrentTactics) != 0)
                goto cleanup;

            _CopyCurrentTacticName();
            _StringCopy(selectedFilename, &importTacticsFilename);
            _ApplyCurrentEditedTactics();
            _ShowInsertDisk2Dialog(aSaveEditedTact);
            _SetExitMenuFlag();
            return;

cleanup:
            _ShowInsertDisk2Dialog(aSaveEditedTact);
            return;
        }
    }
    _SetExitMenuFlag();
}

/* ================================================================
 * EditTacticsOnSelect28  (SWOS2.s line 87906)
 *
 * Menu callback: rotates players left for the current team
 * (maxSubs=5), then redraws entries 10, 27, and terminates.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTacticsOnSelect28(void)
{
    _RotatePlayersLeft((uint8_t *)chooseTacticsTeamPtr, 5);
    _DrawMenuItemsWaitForVSync(editTacticsRotateRedrawList);
}

/* ================================================================
 * EditTacticsAfterDrawPlayerFace  (SWOS2.s line 87919)
 *
 * After-draw callback for player face entries. Computes the
 * player index from the entry ordinal (faces start at ordinal 29),
 * applies row highlighting, then draws the info panel entry (39).
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTacticsAfterDrawPlayerFace(MenuEntry *cur)
{
    uint16_t playerIdx = cur->ordinal - 29 + 1;
    _EditTactics_ApplyPlayerRowHighlighting((int16_t)playerIdx, cur);
    _DrawMenuItem(&menuEntries[39]);
}

/* ================================================================
 * EditTactics28AfterDraw  (SWOS2.s line 87929)
 *
 * After-draw callback for entry 28 area. Draws menu entry at
 * index 27 (offset 0x5E8 / 56 = 27).
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTactics28AfterDraw(void)
{
    _DrawMenuItem(&menuEntries[27]);
}

/* ================================================================
 * EditTactics2AfterDraw  (SWOS2.s line 87901)
 *
 * After-draw callback for the edit tactics pitch area.
 * Draws the pitch background, player sprites, cursor, etc.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTactics2AfterDraw(void)
{
    _DrawEditTacticsPitchAndPlayers();
}

/* ================================================================
 * Chunk 85 — Edit Tactics Pitch Coordinate + Drawing Helpers
 * ================================================================ */

/* ================================================================
 * GetPlayerQuadrant  (SWOS2.s line 88977)
 *
 * Extracts X and Y quadrant from the current tactics for a given
 * player at a given ball location. The byte at
 * editTacticsCurrentTactics[9 + (player-1)*35 + ballLoc] encodes
 * the position: high nibble = X quadrant, low nibble = Y quadrant.
 * ================================================================ */
__attribute__((used, externally_visible))
void _GetPlayerQuadrant(uint16_t player, uint16_t ballLoc,
                        int16_t *outXQuad, int16_t *outYQuad)
{
    uint8_t *positions = (uint8_t *)editTacticsCurrentTactics.positions;
    uint16_t offset = (player - 1) * 35 + ballLoc;
    uint16_t val = positions[offset];
    *outXQuad = (int16_t)(val >> 4);
    *outYQuad = (int16_t)(val & 0x0F);
}

/* ================================================================
 * SetPlayerQuadrant  (SWOS2.s line 88992)
 *
 * Sets the position byte for a player at a given ball location.
 * Encodes xQuad in high nibble and yQuad in low nibble.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetPlayerQuadrant(uint16_t player, int16_t xQuad, int16_t yQuad,
                        uint16_t ballLoc)
{
    tacticsChangedFlag = 1;
    uint8_t *positions = (uint8_t *)editTacticsCurrentTactics.positions;
    uint16_t offset = (player - 1) * 35 + ballLoc;
    positions[offset] = (uint8_t)((xQuad << 4) | yQuad);
}

/* ================================================================
 * GetBallQuadrant  (SWOS2.s line 89034)
 *
 * Converts a ball location index (0..34) to column and row.
 * row = ballLoc / 5, col = ballLoc % 5.
 * ================================================================ */
__attribute__((used, externally_visible))
void _GetBallQuadrant(uint16_t ballLoc, int16_t *outCol, int16_t *outRow)
{
    uint32_t loc = (uint32_t)ballLoc;  /* ext.l d0 */
    *outRow = (int16_t)(loc / 5);
    *outCol = (int16_t)(loc % 5);
}

/* ================================================================
 * GetBallXYPositionFromQuadrant  (SWOS2.s line 89012)
 *
 * Converts ball row/column to screen X/Y coordinates on the pitch
 * sprite using lookup tables.
 * ================================================================ */
__attribute__((used, externally_visible))
void _GetBallXYPositionFromQuadrant(int16_t row, int16_t col,
                                     int16_t *outX, int16_t *outY)
{
    *outX = 129 - ballXQuadrantCoordinates[row] + 22;
    *outY = 199 - ballYQuadrantCoordinates[col] + 36;
}

/* ================================================================
 * GetPlayerXYPositionFromQuadrant  (SWOS2.s line 88941)
 *
 * Gets the screen X/Y position of a player on the pitch sprite.
 * If the player's quadrant is (15,15), returns the ball position
 * instead (with Y-3 offset).
 * ================================================================ */
__attribute__((used, externally_visible))
void _GetPlayerXYPositionFromQuadrant(uint16_t player, uint16_t ballLoc,
                                       int16_t *outX, int16_t *outY)
{
    int16_t xQuad, yQuad;
    _GetPlayerQuadrant(player, ballLoc, &xQuad, &yQuad);

    if (xQuad == 0x0F && yQuad == 0x0F) {
        /* Player is "on the ball" — return ball position */
        int16_t bCol, bRow;
        _GetBallQuadrant(ballLoc, &bCol, &bRow);
        _GetBallXYPositionFromQuadrant(bCol, bRow, outX, outY);
        *outY -= 3;
        return;
    }

    *outX = 0x81 - xQuadrantToCoordinates[xQuad] + 0x16;
    *outY = 0xC7 - yQuadrantToCoordinates[yQuad] + 0x24;
}

/* ================================================================
 * EditTactics_GetDirectionDelta  (SWOS2.s line 89175)
 *
 * Reads the current direction input (menuControlsDirection2) and
 * converts it to X/Y delta values using a lookup table.
 * Returns (0,0) if no direction is held.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTactics_GetDirectionDelta(int16_t *outDx, int16_t *outDy)
{
    *outDx = 0;
    *outDy = 0;
    int16_t dir = (int16_t)menuControlsDirection2;
    if (dir < 0)
        return;
    uint8_t *table = editTacticsDirectionDeltaTable;
    uint16_t idx = (uint16_t)dir * 2;
    *outDx = -((int8_t)table[idx]);
    *outDy = -((int8_t)table[idx + 1]);
}

/* ================================================================
 * GetEditTacticsItemXYPosition  (SWOS2.s line 88749)
 *
 * Dispatcher for getting the X/Y position of a draw-list item.
 * idx < 0: returns old ball position (editTacticsBallRow/Column)
 * idx == 0: returns current ball position (ballRow/Column)
 * idx > 0: returns player position for the current ball location
 *           (or editTacticsPlayerX during status 6)
 * ================================================================ */
__attribute__((used, externally_visible))
void _GetEditTacticsItemXYPosition(int16_t idx, int16_t *outX, int16_t *outY)
{
    if (idx < 0) {
        _GetBallXYPositionFromQuadrant((int16_t)editTacticsBallRow,
                                        (int16_t)editTacticsBallColumn,
                                        outX, outY);
        return;
    }
    if (idx == 0) {
        _GetBallXYPositionFromQuadrant((int16_t)ballRow,
                                        (int16_t)ballColumn,
                                        outX, outY);
        return;
    }
    /* idx > 0: player */
    uint16_t bl = editTacticsBallLocation;
    if (editTacticsPitchStatus == 6)
        bl = editTacticsPlayerX;
    _GetPlayerXYPositionFromQuadrant((uint16_t)idx, bl, outX, outY);
}

/* ================================================================
 * EditTactics_TryMoveQuadrantCursor  (SWOS2.s line 88557)
 *
 * Attempts to move the cursor by the current direction delta.
 * Clamps row to [0..4] and column to [0..6].
 * Returns 0 if cursor was moved, nonzero if it stayed.
 * Updates outRow, outCol, outBallLoc with the new position.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _EditTactics_TryMoveQuadrantCursor(int16_t row, int16_t col,
                                             int16_t *outRow, int16_t *outCol,
                                             int16_t *outBallLoc)
{
    int16_t dx, dy;
    _EditTactics_GetDirectionDelta(&dx, &dy);

    uint16_t noMove = 1;
    int16_t newRow = row + dx;
    if (newRow >= 0 && newRow < 5) {
        row = newRow;
        noMove = 0;
    }
    int16_t newCol = col + dy;
    if (newCol >= 0 && newCol < 7) {
        col = newCol;
        noMove = 0;
    }
    *outRow = row;
    *outCol = col;
    *outBallLoc = col * 5 + row;
    return noMove;
}

/* ================================================================
 * EditTactics_SetAllPlayersQuadrantForBallLoc  (SWOS2.s line 89834)
 *
 * For each of the 10 outfield players (1..10), copies their
 * quadrant position from ball location `ballLoc` to area `areaIdx`.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTactics_SetAllPlayersQuadrantForBallLoc(uint16_t ballLoc,
                                                   uint16_t areaIdx)
{
    for (uint16_t p = 1; p <= 10; p++) {
        int16_t xq, yq;
        _GetPlayerQuadrant(p, ballLoc, &xq, &yq);
        _SetPlayerQuadrant(p, xq, yq, areaIdx);
    }
}

/* ================================================================
 * EditTactics_FlipMaybe_MirrorPlayersInArea  (SWOS2.s line 89776)
 *
 * If flipOnOff is active and the area is not in the center column
 * (remainder != 2), mirrors all player X positions in the given
 * area (flips high nibble: 0xEF - highNibble), then swaps paired
 * players using the unkTable.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTactics_FlipMaybe_MirrorPlayersInArea(uint16_t areaIdx)
{
    if (flipOnOff == 0)
        return;

    /* Check if area is in center column (remainder == 2 from /5) */
    uint16_t rem = areaIdx % 5;
    if (rem == 2)
        return;

    uint8_t *positions = (uint8_t *)editTacticsCurrentTactics.positions;
    uint8_t *unkTable = editTacticsCurrentTactics.unkTable;

    /* Mirror X positions for 10 outfield players */
    uint8_t *p = positions;
    for (int16_t i = 9; i >= 0; i--) {
        uint8_t val = p[areaIdx];
        uint8_t mirrored = (uint8_t)(0xEF - val);
        mirrored &= 0xF0;
        mirrored |= (val & 0x0F);
        p[areaIdx] = mirrored;
        p += 0x23; /* next player: stride = 35 */
    }

    /* Swap paired players using unkTable */
    for (int16_t pairVal = 4; pairVal >= 0; pairVal--) {
        for (int16_t j = 9; j >= 0; j--) {
            if (unkTable[j] == (uint8_t)pairVal) {
                /* Found first of pair at index j — find second */
                int16_t k = j - 1;
                while (k >= 0) {
                    if (unkTable[k] == (uint8_t)pairVal)
                        break;
                    k--;
                }
                if (k < 0) {
                    panicReasonCode = 0x1387;
                    _PanicHaltLoop();
                }
                /* Swap positions between player j and k at this area */
                uint16_t offJ = (uint16_t)j * 0x23 + areaIdx;
                uint16_t offK = (uint16_t)k * 0x23 + areaIdx;
                uint8_t tmp = positions[offJ];
                positions[offJ] = positions[offK];
                positions[offK] = tmp;
                break; /* move to next pairVal */
            }
        }
    }
}

/* ================================================================
 * EditTactics_FlipMaybe_MirrorBallArea  (SWOS2.s line 89753)
 *
 * If flipOnOff is active and the ball location is not in the center
 * column, computes the mirrored ball location and copies all player
 * positions there, then mirrors them.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTactics_FlipMaybe_MirrorBallArea(uint16_t ballLoc)
{
    if (flipOnOff == 0)
        return;

    uint16_t loc = ballLoc;
    uint16_t row = loc / 5;
    uint16_t col = loc % 5;
    if (col == 2)
        return;

    /* Mirror: newArea = row*5 + (4 - col) */
    uint16_t mirroredArea = row * 5 + (4 - col);
    _EditTactics_SetAllPlayersQuadrantForBallLoc(ballLoc, mirroredArea);
    _EditTactics_FlipMaybe_MirrorPlayersInArea(mirroredArea);
}

/* ================================================================
 * EditTactics_CommitDraggedPlayerPosition  (SWOS2.s line 89213)
 *
 * Commits the player position after dragging. Copies all player
 * positions from editTacticsPlayerX to the ball location area,
 * then optionally mirrors if the player and ball are on opposite
 * sides of center.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTactics_CommitDraggedPlayerPosition(void)
{
    tacticsChangedFlag = 1;
    uint16_t playerX = editTacticsPlayerX;
    uint16_t ballLoc = editTacticsBallLocation;
    _EditTactics_SetAllPlayersQuadrantForBallLoc(playerX, ballLoc);

    /* Check if mirroring is needed based on column positions */
    uint16_t playerCol = playerX % 5;
    if (playerCol == 2)
        return; /* center column — no mirroring */

    uint16_t ballCol = ballLoc % 5;
    if (ballCol == 2)
        return; /* center column — no mirroring */

    /* Mirror if player and ball are on opposite sides */
    if (ballCol >= 3) {
        /* Ball on right side */
        if (playerCol >= 3)
            goto doMirrorBall; /* both right: mirror ball area */
        /* player left, ball right: mirror players in ball area first */
    } else {
        /* Ball on left side */
        if (playerCol < 3)
            goto doMirrorBall; /* both left: mirror ball area */
        /* player right, ball left: mirror players in ball area first */
    }
    /* Opposite sides: mirror players in ball area */
    _EditTactics_FlipMaybe_MirrorPlayersInArea(ballLoc);

doMirrorBall:
    _EditTactics_FlipMaybe_MirrorBallArea(ballLoc);
}

/* ================================================================
 * SortEditTacticsDrawOrderByY  (SWOS2.s line 88724)
 *
 * Bubble-sorts the editTacticsPlayerPositions array (12 entries)
 * by Y coordinate so items are drawn back-to-front.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SortEditTacticsDrawOrderByY(void)
{
    int16_t *arr = editTacticsPlayerPositions;
    int16_t swapped;
    do {
        swapped = 0;
        for (int16_t i = 10; i >= 0; i--) {
            int16_t x1, y1, x2, y2;
            _GetEditTacticsItemXYPosition(arr[0], &x1, &y1);
            _GetEditTacticsItemXYPosition(arr[1], &x2, &y2);
            if (y2 < y1) {
                int16_t tmp = arr[0];
                arr[0] = arr[1];
                arr[1] = tmp;
                swapped = 1;
            }
            arr++;
        }
        arr = editTacticsPlayerPositions;
    } while (swapped);
}

/* ================================================================
 * SortPlayerAreasByY  (SWOS2.s line 88822)
 *
 * Bubble-sorts the playerAreas array (35 entries) by Y coordinate
 * of the marked player at each area's ball location.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SortPlayerAreasByY(void)
{
    int16_t *arr = playerAreas;
    int16_t swapped;
    do {
        swapped = 0;
        for (int16_t i = 33; i >= 0; i--) {
            int16_t x1, y1, x2, y2;
            _GetPlayerXYPositionFromQuadrant(markedPlayerNumber, (uint16_t)arr[0],
                                              &x1, &y1);
            _GetPlayerXYPositionFromQuadrant(markedPlayerNumber, (uint16_t)arr[1],
                                              &x2, &y2);
            if (y2 < y1) {
                int16_t tmp = arr[0];
                arr[0] = arr[1];
                arr[1] = tmp;
                swapped = 1;
            }
            arr++;
        }
        arr = playerAreas;
    } while (swapped);
}

/* ================================================================
 * EditTacticsDrawCursor  (SWOS2.s line 88903)
 *
 * Draws the animated cursor on the pitch sprite. If a player is
 * selected (markedPlayerNumber != 0), cursor is at the player's
 * position; otherwise at the ball position.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTacticsDrawCursor(void)
{
    if (editTacticsPitchStatus != 2)
        return;

    int16_t x, y;
    if (markedPlayerNumber != 0) {
        _GetPlayerXYPositionFromQuadrant(markedPlayerNumber,
                                          editTacticsBallLocation,
                                          &x, &y);
        y += 1;
    } else {
        _GetBallXYPositionFromQuadrant((int16_t)ballRow, (int16_t)ballColumn,
                                        &x, &y);
        y -= 1;
    }

    /* Animate cursor: 4 frames, cycling every 4 ticks */
    uint16_t frame = (currentGameTick >> 2) & 3;
    uint16_t spriteId = tacticCursorFrames[frame];
    _DrawSpriteCentered(spriteId, x, y);
}

/* ================================================================
 * DrawPlayerDirections  (SWOS2.s line 88854)
 *
 * Draws direction arrows showing player movement between the
 * current and previous ball locations (only when status == 3,
 * i.e., fire is held and ball locations differ).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawPlayerDirections(void)
{
    if (editTacticsPitchStatus != 3)
        return;

    if (editTacticsBallLocation == editTacticsLastBallLocation)
        return;

    for (uint16_t p = 1; p <= 10; p++) {
        /* Get position at current ball location */
        int16_t curX, curY;
        _GetPlayerXYPositionFromQuadrant(p, editTacticsBallLocation,
                                          &curX, &curY);
        /* Get position at previous ball location */
        int16_t prevX, prevY;
        _GetPlayerXYPositionFromQuadrant(p, editTacticsLastBallLocation,
                                          &prevX, &prevY);

        /* Calculate direction from prev to cur */
        int32_t outDeltaX, outDeltaY;
        uint16_t dir = _CalculateDeltaXAndY(
            curX, curY, prevX, prevY, 0,
            &outDeltaX, &outDeltaY);
        if (dir == 0xFFFF)
            continue; /* same position, no arrow */

        /* Convert direction to arrow sprite index */
        uint8_t dirByte = (uint8_t)((dir + 16) & 0xFF);
        uint16_t arrowIdx = dirByte >> 5;

        /* Apply arrow offset */
        int16_t drawX = curX + (int8_t)arrowSpritesOffsets[arrowIdx * 2];
        int16_t drawY = curY + (int8_t)arrowSpritesOffsets[arrowIdx * 2 + 1];

        uint16_t spriteId = 173 + arrowIdx;
        _DrawSpriteCentered(spriteId, drawX, drawY);
    }
}

/* ================================================================
 * InitLittlePlayerSprites  (SWOS2.s line 88219)
 *
 * Initializes the edit tactics pitch display: clears state, copies
 * tactics, sets up player dress colors, and copies/colorizes the
 * little player and goalie sprites.
 * ================================================================ */
#define SHIRT_ORDINARY_VAL          0

__attribute__((used, externally_visible))
void _InitLittlePlayerSprites(void)
{
    editTacticsTransitionActive = 0;
    tacticsChangedFlag = 0;
    markedPlayerNumber = 0;
    ballRow = 2;
    ballColumn = 3;
    editTacticsBallLocation = 17;

    _CopyCurrentTactics();
    _SaveCurrentTactics();

    /* Set up player numbers pointer */
    TeamFileEx *teamEx = (TeamFileEx *)chooseTacticsTeamPtr;
    uint8_t *playerNums = teamEx->team.playerNumbers;
    if (teamEx == &careerTeam) {
        playerNums = currentMatchPlayers;
    }
    editTacticsPlayerNumbers = (uint32_t)playerNums;

    /* Convert dress colors into the little players color table */
    uint16_t stripesColor = _ConvertPlayerDressColor(
        (uint16_t)teamEx->team.prStripesColor);
    littlePlayersConvertTable[11] = stripesColor;   /* offset 0x16 */

    uint16_t basicColor = _ConvertPlayerDressColor(
        (uint16_t)teamEx->team.prBasicColor);
    if (teamEx->team.prShirtType == SHIRT_ORDINARY_VAL) {
        basicColor = stripesColor;
    }
    littlePlayersConvertTable[10] = basicColor;     /* offset 0x14 */

    uint16_t shortsColor = _ConvertPlayerDressColor(
        (uint16_t)teamEx->team.prShortsColor);
    littlePlayersConvertTable[14] = shortsColor;    /* offset 0x1C */

    uint16_t socksColor = _ConvertPlayerDressColor(
        (uint16_t)teamEx->team.prSocksColor);
    littlePlayersConvertTable[15] = socksColor;     /* offset 0x1E */

    /* Copy and colorize outfield player sprites (3 skin tones) */
    uint16_t shirtType = (uint16_t)(int8_t)teamEx->team.prShirtType;
    uint16_t srcBase = shirtType * 3 + 130;

    _CopyWholeSprite(srcBase, 124);
    _CopyWholeSprite(srcBase + 1, 125);
    _CopyWholeSprite(srcBase + 2, 126);

    _ConvertSpriteColors(124, littlePlayersConvertTable);
    _ConvertSpriteColors(125, littlePlayersConvertTable);
    _ConvertSpriteColors(126, littlePlayersConvertTable);

    /* Copy and colorize goalkeeper sprites (3 skin tones) */
    _CopyWholeSprite(142, 127);
    _CopyWholeSprite(143, 128);
    _CopyWholeSprite(144, 129);

    _ConvertSpriteColors(127, littlePlayersConvertTable);
    _ConvertSpriteColors(128, littlePlayersConvertTable);
    _ConvertSpriteColors(129, littlePlayersConvertTable);
}

/* ================================================================
 * DrawLittlePlayersAndBall  (SWOS2.s line 88613)
 *
 * Draws all little players and the ball on the edit tactics pitch.
 * Iterates through editTacticsPlayerPositions (12 entries: -1 for
 * ghost ball, 0 for real ball, 1-10 for players). Handles blinking
 * for selected items based on editTacticsPitchStatus.
 * ================================================================ */
#define SPR_BALL_SMALL              123
#define SPR_SMALL_PL_WHITE          124  /* SPR_EDIT_TACTICS_SMALL_PL_WHITE_GUY */
#define SPR_SMALL_GK_WHITE          127  /* goalie version */
#define SPR_SHIRT_NUMBER_BASE       161  /* SPR_NOT_PLAYING_WHOLE_SEASON */
__attribute__((used, externally_visible))
void _DrawLittlePlayersAndBall(void)
{
    if (editTacticsDisableLittlePlayersAndBallDraw != 0)
        return;

    _SortEditTacticsDrawOrderByY();

    TeamFileEx *teamEx = (TeamFileEx *)chooseTacticsTeamPtr;
    int16_t *posArr = editTacticsPlayerPositions;
    uint8_t *playerNums = (uint8_t *)editTacticsPlayerNumbers;

    for (int16_t iter = 11; iter >= 0; iter--) {
        int16_t idx = *posArr++;

        if (idx < 0) {
            /* Ghost ball (idx == -1): only in status 3 */
            if (editTacticsPitchStatus != 3)
                goto nextIter;
            if (editTacticsBallLocation == editTacticsBallRow)
                goto nextIter;
            /* Blink: skip if tick & 2 == 0 */
            if ((currentGameTick & 2) == 0)
                goto nextIter;
            int16_t gx, gy;
            _GetBallXYPositionFromQuadrant((int16_t)editTacticsBallRow,
                                           (int16_t)editTacticsBallColumn, &gx, &gy);
            _DrawSpriteCentered(SPR_BALL_SMALL, gx, gy);
            goto nextIter;
        }

        if (idx == 0) {
            /* Real ball */
            if (editTacticsPitchStatus == 6 || editTacticsPitchStatus == 4) {
                if (markedPlayerNumber == 0) {
                    /* Ball is selected -> blink */
                    if ((currentGameTick & 4) == 0)
                        goto nextIter;
                }
            }
            int16_t bx, by;
            _GetBallXYPositionFromQuadrant((int16_t)ballRow,
                                           (int16_t)ballColumn, &bx, &by);
            _DrawSpriteCentered(SPR_BALL_SMALL, bx, by);
            goto nextIter;
        }

        /* Player (idx = 1..10) */
        int8_t playerNum = (int8_t)playerNums[idx];

        /* In status 5, only draw the marked player */
        if (editTacticsPitchStatus == 5) {
            if ((uint16_t)idx != markedPlayerNumber)
                goto nextIter;
        }

        /* Get player position */
        uint16_t ballLoc = editTacticsBallLocation;
        if (editTacticsPitchStatus == 6)
            ballLoc = editTacticsPlayerX;

        int16_t px, py;
        _GetPlayerXYPositionFromQuadrant((uint16_t)idx, ballLoc, &px, &py);

        /* Determine skin color sprite */
        uint8_t posAndFace = teamEx->players[playerNum].positionAndFace;
        uint16_t skinIdx = (posAndFace & 0x18) >> 3;   /* 0..3 */
        uint16_t playerSprite = SPR_SMALL_PL_WHITE + skinIdx;
        _DrawSpriteCentered(playerSprite, px, py);

        /* Check if we should skip shirt number (blinking) */
        if (editTacticsPitchStatus == 4 && (uint16_t)idx == markedPlayerNumber) {
            if ((currentGameTick & 4) == 0)
                goto nextIter;
        }

        /* Draw shirt number above player */
        int8_t shirtNum = teamEx->players[playerNum].shirtNumber;
        uint16_t numSprite = SPR_SHIRT_NUMBER_BASE + (uint16_t)(int16_t)shirtNum;
        _DrawSpriteCentered(numSprite, px, py - 9);

nextIter:
        ;
    }

    /* Draw goalkeeper sprite at fixed position (bottom of pitch) */
    int8_t gkNum = (int8_t)playerNums[0];
    uint8_t gkPosAndFace = teamEx->players[gkNum].positionAndFace;
    uint16_t gkSkinIdx = (gkPosAndFace & 0x18) >> 3;
    uint16_t gkSprite = SPR_SMALL_GK_WHITE + gkSkinIdx;
    _DrawSpriteCentered(gkSprite, 87, 234);
}

/* ================================================================
 * DrawEditTacticsHighlightedPlayerRoute  (SWOS2.s line 88755)
 *
 * When a player is being dragged across ball areas (disable flag
 * is set), draws that player's sprite at every ball area position.
 * Shows the marked player's position for all 35 ball locations.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawEditTacticsHighlightedPlayerRoute(void)
{
    /* Only draw when little players are disabled (route view active) */
    if (editTacticsDisableLittlePlayersAndBallDraw == 0)
        return;
    if (markedPlayerNumber == 0)
        return;

    _SortPlayerAreasByY();

    int16_t *areas = playerAreas;
    uint8_t *playerNums = (uint8_t *)editTacticsPlayerNumbers;
    TeamFileEx *teamEx = (TeamFileEx *)chooseTacticsTeamPtr;

    /* Get marked player's info */
    uint16_t markedIdx = markedPlayerNumber;
    int8_t markedNum = (int8_t)playerNums[markedIdx];

    /* Determine skin color sprite for the player */
    uint8_t posAndFace = teamEx->players[markedNum].positionAndFace;
    uint16_t skinIdx = (posAndFace & 0x18) >> 3;
    uint16_t playerSprite = SPR_SMALL_PL_WHITE + skinIdx;

    /* Determine shirt number sprite */
    int8_t shirtNum = teamEx->players[markedNum].shirtNumber;
    uint16_t numSprite = SPR_SHIRT_NUMBER_BASE + (uint16_t)(int16_t)shirtNum;

    /* Loop over all 35 ball areas */
    for (int16_t i = 34; i >= 0; i--) {
        uint16_t areaIdx = (uint16_t)*areas++;

        /* Get player position for this area */
        int16_t px, py;
        _GetPlayerXYPositionFromQuadrant(markedIdx, areaIdx, &px, &py);

        /* Draw player sprite */
        _DrawSpriteCentered(playerSprite, px, py);

        /* Check if we should skip the number (blinking in status 4) */
        if (editTacticsPitchStatus == 4 &&
            areaIdx == editTacticsBallLocation) {
            if ((currentGameTick & 4) == 0)
                continue;
        }

        /* Draw shirt number above player */
        _DrawSpriteCentered(numSprite, px, py - 9);
    }
}

/* ================================================================
 * DrawEditTacticsPitchAndPlayers  (SWOS2.s line 88495)
 *
 * Draws the edit tactics pitch background, then overlays direction
 * arrows, cursor, little players/ball, and highlighted player route.
 * ================================================================ */
#define SPR_TACTICS_PITCH   226

__attribute__((used, externally_visible))
void _DrawEditTacticsPitchAndPlayers(void)
{
    /* Draw pitch background strips */
    _DrawMenuItemBackgroundMasked(21, 25, 134, 11);
    _DrawMenuItemBackgroundMasked(21, 25, 1, 211);
    _DrawMenuItemBackgroundMasked(152, 25, 3, 211);

    /* Draw pitch sprite */
    _DrawSprite16Pixels(SPR_TACTICS_PITCH, 22, 36);

    /* Draw overlays */
    _DrawPlayerDirections();
    _EditTacticsDrawCursor();
    _DrawLittlePlayersAndBall();
    _DrawEditTacticsHighlightedPlayerRoute();
}

/* ================================================================
 * HandlePlayersOrBallInEditTactics  (SWOS2.s line 88692)
 *
 * Finds the nearest player or ball in the current joystick direction
 * and selects it (sets markedPlayerNumber). Uses expanding cone
 * search: starts with width 384, expands by 64 up to 1280, then
 * by 192 up to 7680.
 * ================================================================ */
__attribute__((used, externally_visible))
void _HandlePlayersOrBallInEditTactics(void)
{
    if ((int16_t)menuControlsDirection < 0)
        return;

    /* Get current selection's position */
    int16_t selX, selY;
    if (markedPlayerNumber != 0) {
        _GetPlayerXYPositionFromQuadrant(markedPlayerNumber,
                                          editTacticsBallLocation, &selX, &selY);
    } else {
        _GetBallXYPositionFromQuadrant((int16_t)ballRow,
                                        (int16_t)ballColumn, &selX, &selY);
    }

    int16_t bestIdx = -1;
    int32_t bestDist = -1;  /* 0xFFFFFFFF unsigned, treated as max */
    editTacticsNearestSelectPitchWidth = 384;

outerLoop:
    for (uint16_t i = 0; i < 11; i++) {
        if (i == markedPlayerNumber)
            continue;

        int16_t px, py;
        if (i == 0) {
            _GetBallXYPositionFromQuadrant((int16_t)ballRow,
                                            (int16_t)ballColumn, &px, &py);
        } else {
            _GetPlayerXYPositionFromQuadrant(i, editTacticsBallLocation,
                                              &px, &py);
        }

        int16_t dx = px - selX;
        int16_t dy = py - selY;

        /* Direction-based cone filter */
        uint16_t dirBits = menuControlsDirection & 6;
        int16_t width = (int16_t)editTacticsNearestSelectPitchWidth;

        if (dirBits == 0) {
            /* Up */
            if (dy >= 0) continue;
            int16_t limit = (int16_t)((int32_t)dy * (int32_t)width >> 8);
            if (dx < limit) continue;
            if (dx > -limit) continue;
        } else if (dirBits == 2) {
            /* Right */
            if (dx < 0) continue;
            int16_t limit = (int16_t)((int32_t)dx * (int32_t)width >> 8);
            if (dy > limit) continue;
            if (dy < -limit) continue;
        } else if (dirBits == 4) {
            /* Down */
            if (dy < 0) continue;
            int16_t limit = (int16_t)((int32_t)dy * (int32_t)width >> 8);
            if (dx > limit) continue;
            if (dx < -limit) continue;
        } else {
            /* Left (dirBits == 6) */
            if (dx >= 0) continue;
            int16_t limit = (int16_t)((int32_t)dx * (int32_t)width >> 8);
            if (dy < limit) continue;
            if (dy > -limit) continue;
        }

        /* Calculate squared distance */
        int32_t dist = (int32_t)dx * (int32_t)dx + (int32_t)dy * (int32_t)dy;
        if ((uint32_t)dist >= (uint32_t)bestDist)
            continue;
        bestDist = dist;
        bestIdx = (int16_t)i;
    }

    if (bestIdx < 0) {
        /* Widen cone and retry */
        editTacticsNearestSelectPitchWidth += 64;
        if (editTacticsNearestSelectPitchWidth < 1280)
            goto outerLoop;
        editTacticsNearestSelectPitchWidth += 192;
        if (editTacticsNearestSelectPitchWidth < 7680)
            goto outerLoop;

        /* Still nothing found: check if direction is horizontal-ish */
        uint16_t dirBits = menuControlsDirection & 6;
        if (dirBits < 2 || dirBits > 4) {
            editTacticsPitchStatus = 0xFFFF;
            return;
        }
        return;
    }

    markedPlayerNumber = (uint16_t)bestIdx;
}

/* ================================================================
 * EditTacticsReadControls  (SWOS2.s line 88386)
 *
 * Handles joystick/button input for the edit tactics pitch when
 * no specific player is marked (ball/area cursor movement).
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTacticsReadControls(void)
{
    if (editTacticsPitchStatus == 6) {
        /* Status 6: dragging player - fire releases */
        if (fire != 0) {
            _SaveCurrentTactics();
            _EditTactics_CommitDraggedPlayerPosition();
            editTacticsPitchStatus = 0xFFFF;
            return;
        }
        goto moveBallCursor;
    }

    if (editTacticsPitchStatus == 3) {
        /* Status 3: browsing ball areas */
        if (fire != 0) goto tryMoveBallArea;
        /* Fire released: commit ball position */
        ballRow = editTacticsBallRow;
        ballColumn = editTacticsBallColumn;
        editTacticsBallLocation = editTacticsLastBallLocation;
        goto setStatus2;
    }

    if (editTacticsPitchStatus == 4) {
        /* Status 4: player/ball selected, fire handling */
        if (fire != 0) {
            if (fireCounter <= -7) goto startBallAreaBrowse;
            goto moveBallCursor;
        }
        if (fireCounter == 0) goto moveBallCursor;
        if (fireCounter <= 7) goto setStatus2;
        goto moveBallCursor;
    }

    /* Default status (2): idle, waiting for input */
    if (fire == 0) {
        if (fireCounter == 0) {
            _HandlePlayersOrBallInEditTactics();
            return;
        }
        if (fireCounter <= 7) goto setStatus4;
        return;
    }
    /* fire != 0 */
    if (fireCounter <= -7) goto startBallAreaBrowse;
    return;

setStatus4:
    editTacticsPitchStatus = 4;
    return;

startBallAreaBrowse:
    editTacticsBallRow = ballRow;
    editTacticsBallColumn = ballColumn;
    editTacticsLastBallLocation = editTacticsBallLocation;
    editTacticsPitchStatus = 3;
    return;

setStatus2:
    editTacticsPitchStatus = 2;
    return;

tryMoveBallArea:
    if ((int16_t)menuControlsDirection < 0)
        return;
    {
        int16_t outRow, outCol, outBallLoc;
        uint16_t noMove = _EditTactics_TryMoveQuadrantCursor(
            (int16_t)editTacticsBallRow, (int16_t)editTacticsBallColumn,
            &outRow, &outCol, &outBallLoc);
        if (noMove != 0) return;
        editTacticsBallRow = (uint16_t)outRow;
        editTacticsBallColumn = (uint16_t)outCol;
        editTacticsLastBallLocation = (uint16_t)outBallLoc;
    }
    return;

moveBallCursor:
    if ((int16_t)menuControlsDirection < 0)
        return;
    {
        int16_t outRow, outCol, outBallLoc;
        uint16_t noMove = _EditTactics_TryMoveQuadrantCursor(
            (int16_t)ballRow, (int16_t)ballColumn,
            &outRow, &outCol, &outBallLoc);
        if (noMove != 0) return;
        ballRow = (uint16_t)outRow;
        ballColumn = (uint16_t)outCol;
        editTacticsBallLocation = (uint16_t)outBallLoc;
    }
}

/* ================================================================
 * EditTactics_HandlePitchInput  (SWOS2.s line 88234)
 *
 * Handles joystick/button input when a player IS marked on the
 * edit tactics pitch. Manages player position movement, ball area
 * switching (status 5), and position commit.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTactics_HandlePitchInput(void)
{
    if (editTacticsPitchStatus == 4) goto status4;
    if (editTacticsPitchStatus == 5) goto status5;

    /* Default status (2): waiting for fire press/release */
    if (fire == 0) {
        if (fireCounter == 0) {
            _HandlePlayersOrBallInEditTactics();
            return;
        }
        if (fireCounter <= 7) goto saveAndSetStatus4;
        return;
    }
    /* fire != 0 */
    if (fireCounter <= -14) goto startStatus5;
    return;

status5:
    /* Ball area browsing with marked player */
    if (fire == 0 && fireCounter != 0) {
        if (fireCounter <= 7) goto backToStatus2;
    }

    /* Repeat counter for ball area scrolling */
    editTacticsInputRepeatCounter++;
    editTacticsInputRepeatCounter &= 7;
    if (editTacticsInputRepeatCounter != 0)
        return;

    /* Check direction keys for ball area navigation */
    {
        int16_t newLoc;
        if (up) {
            newLoc = (int16_t)editTacticsBallLocation + 5;
            if (newLoc >= 35) {
                newLoc -= 34;
                if (newLoc == 5) newLoc = 0;
            }
        } else if (down) {
            newLoc = (int16_t)editTacticsBallLocation - 5;
            if (newLoc < 0) {
                newLoc += 34;
                if (newLoc == 29) newLoc = 34;
            }
        } else if (left) {
            newLoc = (int16_t)editTacticsBallLocation + 1;
            if (newLoc == 35) newLoc = 0;
        } else if (right) {
            newLoc = (int16_t)editTacticsBallLocation - 1;
            if (newLoc < 0) newLoc = 34;
        } else {
            return;
        }

        editTacticsBallLocation = (uint16_t)newLoc;
        uint32_t divResult = (uint32_t)(uint16_t)newLoc;
        ballColumn = (uint16_t)(divResult / 5);
        ballRow = (uint16_t)(divResult % 5);
    }
    return;

status4:
    /* Player selected, moving quadrant position */
    if (fire == 0) {
        if (fireCounter == 0) goto movePlayerQuadrant;
        if (fireCounter <= 7) goto commitAndDone;
        goto movePlayerQuadrant;
    }
    if (fireCounter <= -14) goto startStatus5;

movePlayerQuadrant:
    if ((int16_t)menuControlsDirection < 0)
        return;
    {
        int16_t dx, dy;
        _EditTactics_GetDirectionDelta(&dx, &dy);

        int16_t xQuad, yQuad;
        _GetPlayerQuadrant(markedPlayerNumber, editTacticsBallLocation,
                           &xQuad, &yQuad);

        /* Try to move X */
        int16_t newX = xQuad + dx;
        if (newX >= 0 && newX < 15)
            xQuad = newX;

        /* Try to move Y */
        int16_t newY = yQuad + dy;
        if (newY >= 0 && newY < 16)
            yQuad = newY;

        /* Check if position conflicts with another player */
        int16_t targetX = xQuad;
        int16_t targetY = yQuad;
        for (uint16_t p = 1; p < 11; p++) {
            int16_t px, py;
            _GetPlayerQuadrant(p, editTacticsBallLocation, &px, &py);
            if (px == targetX && py == targetY)
                return;  /* Occupied, don't move */
        }

        /* Position is free: commit */
        _SetPlayerQuadrant(markedPlayerNumber, targetX, targetY,
                           editTacticsBallLocation);
        _EditTactics_FlipMaybe_MirrorBallArea(editTacticsBallLocation);
    }
    return;

saveAndSetStatus4:
    _SaveCurrentTactics();
    editTacticsPitchStatus = 4;
    _EditTacticsMenuSavePlayerPositions();
    _DrawPlayerNamesAndFaces();
    return;

commitAndDone:
    editTacticsPitchStatus = 7;
    _CallDrawMenuItem(&menuEntries[2]);
    _EditTactics_CommitPlayerPositionsAndRecalcSkill();
    _DrawPlayerNamesAndFaces();

backToStatus2:
    editTacticsPitchStatus = 2;
    return;

startStatus5:
    editTacticsInputRepeatCounter = 0;
    editTacticsPitchStatus = 5;
}

/* ================================================================
 * EditTacticsStatus  (SWOS2.s line 88167)
 *
 * Main state dispatcher for the edit tactics pitch. Handles
 * transition timing and delegates to HandlePitchInput (when
 * player marked) or ReadControls (when no player marked).
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTacticsStatus(uint16_t status)
{
    if (editTacticsTransitionActive == 0) {
        editTacticsPitchStatus = status;
        markedPlayerNumber = 0;
        editTacticsLastTick = currentGameTick;
        editTacticsTransitionActive = 1;
    }

    /* Advance tick tracking */
    editTacticsLastTick = currentGameTick;

    /* Status 0: wait for fire release */
    if (editTacticsPitchStatus == 0) {
        if (fire != 0)
            goto drawAndReturn;
        editTacticsPitchStatus = 2;
    }

    /* Status 1: wait for fire release (copy tactics mode) */
    if (editTacticsPitchStatus == 1) {
        if (fire != 0)
            goto drawAndReturn;
        editTacticsPitchStatus = 6;
    }

    /* Status -1 (0xFFFF): done */
    if (editTacticsPitchStatus == 0xFFFF) {
        editTacticsTransitionActive = 0;
        return;
    }

    /* Dispatch based on whether a player is marked */
    if (markedPlayerNumber != 0) {
        _EditTactics_HandlePitchInput();
    } else {
        _EditTacticsReadControls();
    }

drawAndReturn:
    _DrawEditTacticsPitchAndPlayers();
}

/* ================================================================
 * EditTacicsMovePlayersOrBall  (SWOS2.s line 87723)
 *
 * Menu action handler: initiates player/ball movement mode.
 * Saves positions, draws names, then enters EditTacticsStatus(0).
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTacicsMovePlayersOrBall(void)
{
    if (editTacticsTransitionActive == 0) {
        _EditTacticsMenuSavePlayerPositions();
        _DrawPlayerNamesAndFaces();
    }
    _EditTacticsStatus(0);
    if (editTacticsTransitionActive != 0)
        return;
    _CallDrawMenuItem(&menuEntries[2]);
    _SetCurrentEntry(4);
}

/* ================================================================
 * CopyTactics  (SWOS2.s line 87742)
 *
 * Menu action handler: initiates copy/drag mode for copying player
 * positions from one ball area to another. Changes menu entry
 * colors and enters EditTacticsStatus(1).
 * ================================================================ */
#define COLOR_PINK_TO_BROWN     9
#define COLOR_YELLOW_GREEN_BLK  14
#define COLOR_ORANGE_TO_BROWN   4

__attribute__((used, externally_visible))
void _CopyTactics(void)
{
    if (editTacticsTransitionActive == 0) {
        /* Set entry 4's bg color to pink */
        uint16_t *bgColor4 = (uint16_t *)&menuEntries[4].bg;
        *bgColor4 = (*bgColor4 & 0xFFF0) | COLOR_PINK_TO_BROWN;
        _CallDrawMenuItem(&menuEntries[4]);
        _EditTacticsMenuSavePlayerPositions();
        _DrawPlayerNamesAndFaces();
        editTacticsPlayerX = editTacticsBallLocation;
    }
    _EditTacticsStatus(1);
    if (editTacticsTransitionActive != 0)
        return;
    _CallDrawMenuItem(&menuEntries[2]);
    /* Restore entry 4's bg color to yellow-green */
    uint16_t *bgColor4 = (uint16_t *)&menuEntries[4].bg;
    *bgColor4 = (*bgColor4 & 0xFFF0) | COLOR_YELLOW_GREEN_BLK;
    _SetCurrentEntry(COLOR_ORANGE_TO_BROWN);
    _CallDrawMenuItem(&menuEntries[4]);
    _EditTactics_CommitPlayerPositionsAndRecalcSkill();
    _DrawPlayerNamesAndFaces();
}

/* ================================================================
 * GetPlayerInfoBreakPosition  (SWOS2.s line 79298)
 *
 * Given a team pointer and a requested slot, remaps the slot via
 * the current formation's position table (for slots 0-10), then
 * calls GetPlayerAtIndex to obtain the player info pointer.
 *
 * Asm returns: a0=player ptr, d0=remapped slot, d1=file index.
 * C version uses output pointers for the extra return values.
 * Asm body kept for asm callers (multi-register return).
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_GetPlayerInfoBreakPosition(uint8_t *teamPtr, uint16_t requestedSlot,
                                      uint16_t *outSlot, uint16_t *outPlayerIndex)
{
    uint16_t slot = requestedSlot;
    if (slot < 11) {
        int16_t tacticsIdx = (int8_t)((TeamFileEx *)teamPtr)->team.tactics;
        TeamTactics *tactics = (TeamTactics *)teamTacticsTable[tacticsIdx];
        int16_t formIdx = (int8_t)tactics->ballOutOfPlayTactics;
        uint8_t *positions = (uint8_t *)positionsTable[formIdx];
        slot = (uint8_t)(positions[requestedSlot] - 1);
    }
    uint16_t teamNumber = *(uint16_t *)teamPtr;
    uint8_t *player = _GetPlayerAtIndex(teamNumber, slot);
    *outSlot = slot;
    *outPlayerIndex = _cGetPlayerAtIndexFileIdx;
    return player;
}

/* ================================================================
 * EditTactics_FindFreePairId  (SWOS2.s line 87515)
 *
 * Scans the 10-byte unkTable in tactics data, builds a bitmask of
 * used pair IDs, and returns the first free pair ID (lowest bit
 * not set).
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _EditTactics_FindFreePairId(void)
{
    uint8_t *table = editTacticsCurrentTactics.unkTable;
    uint16_t used = 0;
    int16_t i;
    for (i = 9; i >= 0; i--) {
        int8_t val = (int8_t)table[i];
        if (val >= 0) {
            used |= (uint16_t)(1 << (int16_t)val);
        }
    }
    uint16_t id = 0;
    while (used & (1 << id))
        id++;
    return id;
}

/* ================================================================
 * EditTactics_ClearPairId  (SWOS2.s line 87540)
 *
 * Scans the 10-byte unkTable and sets all entries matching the
 * given pair ID value to 0xFF (unlinked).
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTactics_ClearPairId(uint16_t value)
{
    uint8_t *table = editTacticsCurrentTactics.unkTable;
    int16_t i;
    for (i = 9; i >= 0; i--) {
        if (table[i] == (uint8_t)value)
            table[i] = 0xFF;
    }
}

/* ================================================================
 * EditTactics_SwapPlayersInSlots  (SWOS2.s line 87610)
 *
 * Swaps two players identified by their slot indices. If one or both
 * slots cross the playing/bench boundary, uses GetPlayerAtIndex to
 * look up the players and swaps their position bytes and shirt
 * numbers. Also swaps entries in the appropriate player number array.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTactics_SwapPlayersInSlots(uint16_t slot1, uint16_t slot2)
{
    TeamFileEx *teamEx = (TeamFileEx *)(uint32_t)chooseTacticsTeamPtr;

    /* Check if we need to do the GetPlayerAtIndex-based swap.
     * Skip when both slots are in the same category:
     * - both < 11 (playing) → skip
     * - both >= 11 and both < 16 (bench) → skip
     * Otherwise do the full swap. */
    int doPlayerSwap = 0;
    if (slot1 >= 16 || slot2 >= 16) {
        doPlayerSwap = 1;
    } else if (slot1 < 11 && slot2 >= 11) {
        doPlayerSwap = 1;
    } else if (slot1 >= 11 && slot2 < 11) {
        doPlayerSwap = 1;
    }

    if (doPlayerSwap) {
        uint16_t teamNumber = *(uint16_t *)teamEx;
        uint8_t *player1 = _GetPlayerAtIndex(teamNumber, slot1);
        uint16_t fileIdx1 = _cGetPlayerAtIndexFileIdx;
        uint8_t *player2 = _GetPlayerAtIndex(teamNumber, slot2);
        uint16_t fileIdx2 = _cGetPlayerAtIndexFileIdx;

        /* Swap position bytes in playerPositionsArray */
        uint8_t tmp = (&playerPositionsArray)[fileIdx2];
        (&playerPositionsArray)[fileIdx2] = (&playerPositionsArray)[fileIdx1];
        (&playerPositionsArray)[fileIdx1] = tmp;

        /* Swap byte at offset 0x4E (78) in player records */
        uint8_t tmp2 = player2[0x4E];
        player2[0x4E] = player1[0x4E];
        player1[0x4E] = tmp2;
    }

    /* Swap entries in the player number mapping array */
    if (gameType == 4) {
        uint8_t tmp = currentMatchPlayers[slot2];
        currentMatchPlayers[slot2] = currentMatchPlayers[slot1];
        currentMatchPlayers[slot1] = tmp;
    } else {
        uint8_t *numbers = teamEx->team.playerNumbers;
        uint8_t tmp = numbers[slot2];
        numbers[slot2] = numbers[slot1];
        numbers[slot1] = tmp;
    }
}

/* ================================================================
 * EditTacticsCommon  (SWOS2.s line 87444)
 *
 * Called when a player pair entry is selected in the edit tactics
 * menu. Handles linking/unlinking player pairs via the unkTable.
 * First click selects a player; second click on a different player
 * links them as a pair; clicking the same player again cancels.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTacticsCommon(MenuEntry *cur)
{
    if ((int16_t)editTacticsSelectedPlayerNameIndex >= 0)
        return;

    /* Compute relative index: current entry vs menuEntries[29] */
    uint16_t relIdx = cur->ordinal - menuEntries[29].ordinal + 1;
    uint16_t savedRelIdx = relIdx;

    TeamFileEx *teamEx = (TeamFileEx *)(uint32_t)chooseTacticsTeamPtr;
    uint8_t savedTactics = teamEx->team.tactics;
    teamEx->team.tactics = editTacticsCurrentTactics.ballOutOfPlayTactics;

    uint16_t slot, playerIndex;
    _GetPlayerInfoBreakPosition((uint8_t *)teamEx, relIdx, &slot, &playerIndex);

    teamEx = (TeamFileEx *)(uint32_t)chooseTacticsTeamPtr;
    teamEx->team.tactics = savedTactics;
    slot -= 1;

    if ((int16_t)editTacticsSelectedPairSlotIndex < 0) {
        /* No previous selection - start new selection */
        editTacticsSelectedPairSlotIndex = slot;
        MenuEntry *entry = _CalcMenuEntryAddress(11 + savedRelIdx);
        editTacticsSelectedPairMenuEntryPtr = (uint32_t)(uint32_t)entry;
        _CallDrawMenuItem(entry);
        return;
    }

    if (editTacticsSelectedPairSlotIndex == slot) {
        /* Same slot clicked again - cancel selection */
        editTacticsSelectedPairSlotIndex = 0xFFFF;
        _CallDrawMenuItem((MenuEntry *)(uint32_t)editTacticsSelectedPairMenuEntryPtr);
        return;
    }

    /* Different slot - link/unlink pair */
    uint8_t *unkTable = editTacticsCurrentTactics.unkTable;
    uint8_t pairIdCurrent = unkTable[slot];
    uint8_t pairIdPrevious = unkTable[editTacticsSelectedPairSlotIndex];

    /* Clear both pair IDs from the table */
    _EditTactics_ClearPairId((uint16_t)unkTable[slot]);
    _EditTactics_ClearPairId((uint16_t)unkTable[editTacticsSelectedPairSlotIndex]);

    /* Assign new pair ID unless both already had the same valid ID */
    if ((int8_t)pairIdCurrent < 0 || (int8_t)pairIdPrevious < 0 ||
        pairIdCurrent != pairIdPrevious) {
        uint16_t newPairId = _EditTactics_FindFreePairId();
        unkTable[slot] = (uint8_t)newPairId;
        unkTable[editTacticsSelectedPairSlotIndex] = (uint8_t)newPairId;
    }

    editTacticsSelectedPairSlotIndex = 0xFFFF;

    /* Redraw the selected pair entry and all pair entries */
    _CallDrawMenuItem((MenuEntry *)(uint32_t)editTacticsSelectedPairMenuEntryPtr);
    _CallDrawMultipleItems(10, &menuEntries[29]);
}

/* ================================================================
 * EditTacticsPlayerNamesCommon  (SWOS2.s line 87555)
 *
 * Called when a player name is selected in the edit tactics menu.
 * First click selects a player name; second click on a different
 * player swaps them (positions and shirt numbers); clicking the
 * same player cancels the selection.
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditTacticsPlayerNamesCommon(MenuEntry *cur)
{
    if ((int16_t)editTacticsSelectedPairSlotIndex >= 0)
        return;

    /* Compute relative index: current entry vs menuEntries[11] */
    uint16_t relIdx = cur->ordinal - menuEntries[11].ordinal;

    TeamFileEx *teamEx = (TeamFileEx *)(uint32_t)chooseTacticsTeamPtr;
    uint8_t savedTactics = teamEx->team.tactics;
    teamEx->team.tactics = editTacticsCurrentTactics.ballOutOfPlayTactics;

    uint16_t slot, playerIndex;
    _GetPlayerInfoBreakPosition((uint8_t *)teamEx, relIdx, &slot, &playerIndex);

    teamEx = (TeamFileEx *)(uint32_t)chooseTacticsTeamPtr;
    teamEx->team.tactics = savedTactics;

    if ((int16_t)editTacticsSelectedPlayerNameIndex >= 0) {
        if (editTacticsSelectedPlayerNameIndex == playerIndex) {
            /* Same player - cancel by setting to 0xFFFF */
            playerIndex = 0xFFFF;
            /* Fall through to update selection state */
        } else {
            /* Different player - swap them */
            _EditTactics_SwapPlayersInSlots(
                editTacticsSelectedPlayerSlotIndex, slot);
            _SetBenchPlayersNumbers((uint8_t *)(uint32_t)chooseTacticsTeamPtr);
            teamEx = (TeamFileEx *)(uint32_t)chooseTacticsTeamPtr;
            teamEx->team.someFlag = 1;
            _EditTactics_CommitPlayerPositionsAndRecalcSkill();
            editTacticsSelectedPlayerNameIndex = 0xFFFF;
            _DrawMenu();
            return;
        }
    }

    /* Start or update selection */
    editTacticsSelectedPlayerNameIndex = playerIndex;
    editTacticsSelectedPlayerSlotIndex = slot;

    MenuEntry *prevEntry = (MenuEntry *)(uint32_t)editTacticsSelectedPlayerMenuEntryPtr;
    editTacticsSelectedPlayerMenuEntryPtr = (uint32_t)(uint32_t)cur;

    if (prevEntry != (MenuEntry *)0) {
        _CallDrawMenuItem(prevEntry);
    }
    _CallDrawMenuItem(cur);
    _EditTacticsMenuSavePlayerPositions();
    _DrawPlayerNamesAndFaces();
}

/* ================================================================
 * LoadTactics  (SWOS2.s line 88770)
 *
 * Menu callback: prompts for save disk, lets user select a .TAC
 * file, saves current tactics, loads selected tactics file into
 * editTacticsCurrentTactics, copies filename to importTacticsFilename,
 * sets importMenuSelectedTactics=1000 and tacticsChangedFlag=1.
 * Shows error if wrong extension or load fails.
 * ================================================================ */
__attribute__((used, externally_visible))
void _LoadTactics(void)
{
    importMenuSelectedTactics = 0xFFFF;

    if (_Asset_PromptInsertSaveDiskDf0(aLoadEditedTact) != 0)
        goto end;

    if (_GetFilenameAndExtension(0, aLoadEditedTact, aLoadEditedTact) != 0)
        goto cleanup;

    if (extension != 0x2E544143u) { /* '.TAC' */
        _ShowErrorMenu(aNotATacticsFil);
        goto cleanup;
    }

    _SaveCurrentTactics();

    if (_ReadTacticsFile(selectedFilename) != 0)
        goto cleanup;

    _StringCopy(selectedFilename, &importTacticsFilename);
    importMenuSelectedTactics = 1000;
    tacticsChangedFlag = 1;

cleanup:
    _ShowInsertDisk2Dialog(aLoadEditedTact);
end:
    _SetExitMenuFlag();
}

/* ================================================================
 * Chunk 117: EditTeamsFinish
 * ================================================================ */

/* ================================================================
 * Chunk 133: EditTeams functions
 * ================================================================ */

/* --- Externs for EditTeams --- */
extern uint8_t *editTeamsSelectedTeam;   /* pointer to source team data */
extern uint16_t saveCustomTeams;
extern uint16_t teamIsImported;
extern uint16_t controlMask;
extern uint16_t kTeamColorsTable[];      /* 10 word entries */
extern uint16_t editTeamsPrimaryShirtColTable[];   /* 16 word color table */
extern uint16_t editTeamsShortsColTable[];          /* 16 word color table + at +0x16 */
extern uint16_t editTeamsSocksColTable[];           /* 16 word color table + at +0x16 */
extern uint16_t editTeamsSecondaryShirtColTable[];  /* 16 word color table */
extern uint16_t editTeamsSecondaryShortsColTable[]; /* 16 word color table + at +0x16 */
extern uint16_t editTeamsSecondarySocksColTable[];  /* 16 word color table + at +0x16 */
extern uint16_t shirtTypesTable[];       /* 4 entries of 8 words each (4 shirt types) */
extern uint16_t shirtsIndexTable[];      /* 8 word entries (byte offsets into color table) */
extern uint32_t playerPositionsStringTable[];       /* 8 string pointers */

/* EditTeamsGetPlayerPointer — return pointer to player N in selTeamsPtr */
static uint8_t *EditTeamsGetPlayerPointerC(MenuEntry *entry)
{
    uint16_t idx = entry->ordinal - 23;
    return &selTeamsPtr + idx * 38;
}

/* PrimaryKitColorConversion — build primary shirt/shorts/socks color tables */
static void PrimaryKitColorConversionC(void)
{
    uint8_t *team = &selTeamsPtr;
    uint16_t *colTable = editTeamsPrimaryShirtColTable;
    uint16_t stripesCol = kTeamColorsTable[team[27]];  /* prStripesColor */
    uint16_t basicCol = kTeamColorsTable[team[28]];    /* prBasicColor */
    uint16_t *types = &shirtTypesTable[team[26] * 8];  /* prShirtType * 8 words */
    uint16_t *indices = shirtsIndexTable;

    for (int16_t i = 7; i >= 0; i--) {
        uint16_t idx = *indices++;
        if (*types++ == 2)
            colTable[idx / 2] = basicCol;
        else
            colTable[idx / 2] = stripesCol;
    }

    /* Shorts */
    uint16_t shortsCol = kTeamColorsTable[team[29]];  /* prShortsColor */
    ((uint16_t *)((uint8_t *)editTeamsShortsColTable + 0x16))[0] = shortsCol;

    /* Socks */
    uint16_t socksCol = kTeamColorsTable[team[30]];   /* prSocksColor */
    ((uint16_t *)((uint8_t *)editTeamsSocksColTable + 0x16))[0] = socksCol;
}

/* SecondaryKitColorConversion — build secondary shirt/shorts/socks color tables */
static void SecondaryKitColorConversionC(void)
{
    uint8_t *team = &selTeamsPtr;
    uint16_t *colTable = editTeamsSecondaryShirtColTable;
    uint16_t stripesCol = kTeamColorsTable[team[32]];  /* secStripesColor */
    uint16_t basicCol = kTeamColorsTable[team[33]];    /* secBasicColor */
    uint16_t *types = &shirtTypesTable[team[31] * 8];  /* secShirtType * 8 words */
    uint16_t *indices = shirtsIndexTable;

    for (int16_t i = 7; i >= 0; i--) {
        uint16_t idx = *indices++;
        if (*types++ == 2)
            colTable[idx / 2] = basicCol;
        else
            colTable[idx / 2] = stripesCol;
    }

    /* Shorts */
    uint16_t shortsCol = kTeamColorsTable[team[34]];  /* secShortsColor */
    ((uint16_t *)((uint8_t *)editTeamsSecondaryShortsColTable + 0x16))[0] = shortsCol;

    /* Socks */
    uint16_t socksCol = kTeamColorsTable[team[35]];   /* secSocksColor */
    ((uint16_t *)((uint8_t *)editTeamsSecondarySocksColTable + 0x16))[0] = socksCol;
}

/* MarkFirstKitShirt — highlight selected primary shirt type in menu */
static void MarkFirstKitShirtC(void)
{
    uint8_t *team = &selTeamsPtr;
    MenuEntry *e = &menuEntries[0x348 / 56]; /* menuEntries + $348 / 56 = entry 15 */
    uint8_t shirtType = team[26]; /* prShirtType */

    for (int16_t i = 3; i >= 0; i--) {
        *(uint16_t *)&e->bg = 9;  /* PINK_TO_BROWN_9 */
        if ((uint8_t)i == shirtType)
            *(uint16_t *)&e->bg = 10;  /* RED_10 */
        e = (MenuEntry *)((uint8_t *)e - 56);
    }
}

/* MarkSecondKitShirt — highlight selected secondary shirt type in menu */
static void MarkSecondKitShirtC(void)
{
    uint8_t *team = &selTeamsPtr;
    MenuEntry *e = &menuEntries[0x428 / 56]; /* entry 19 */
    uint8_t shirtType = team[31]; /* secShirtType */

    for (int16_t i = 3; i >= 0; i--) {
        *(uint16_t *)&e->bg = 9;  /* PINK_TO_BROWN_9 */
        if ((uint8_t)i == shirtType)
            *(uint16_t *)&e->bg = 10;  /* RED_10 */
        e = (MenuEntry *)((uint8_t *)e - 56);
    }
}

/* EditTeamsInit — copy team to edit buffer, clear import flag */
__attribute__((used, externally_visible))
void _EditTeamsInit(void)
{
    uint16_t *src = (uint16_t *)editTeamsSelectedTeam;
    uint16_t *dst = (uint16_t *)&selTeamsPtr;
    for (int16_t i = 341; i >= 0; i--)
        *dst++ = *src++;
    teamIsImported = 0;
    _EditTeamsFinish();
}

/* EditTeamsFinish — reinit edit teams menu after undo/load */
__attribute__((used, externally_visible))
void _EditTeamsFinish(void)
{
    _InitEditTeamsMenu();
    /* nullsub_16 was called here in asm — it's just rts, so omitted */
}

/* EditTeamsUndo — restore team from original, reinit menu */
__attribute__((used, externally_visible))
void _EditTeamsUndo(void)
{
    teamIsImported = 0;
    uint16_t *src = (uint16_t *)editTeamsSelectedTeam;
    uint16_t *dst = (uint16_t *)&selTeamsPtr;
    for (int16_t i = 341; i >= 0; i--)
        *dst++ = *src++;
    _InitEditTeamsMenu();
    _DrawMenu();
}

/* EditTeamsOK — save edited team if changed */
__attribute__((used, externally_visible))
void _EditTeamsOK(void)
{
    uint16_t *src = (uint16_t *)editTeamsSelectedTeam;
    uint16_t *cmp = (uint16_t *)&selTeamsPtr;

    for (int16_t i = 341; i >= 0; i--) {
        if (*src++ != *cmp++) {
            /* Teams differ — save changes */
            saveCustomTeams = 1;
            uint16_t *dst = (uint16_t *)editTeamsSelectedTeam;
            uint16_t *from = (uint16_t *)&selTeamsPtr;
            for (int16_t j = 341; j >= 0; j--)
                *dst++ = *from++;
            _SetExitMenuFlag();
            return;
        }
    }
    /* No changes */
    _SetExitMenuFlag();
}

/* EditTeamsChangeShirtsPrimary — change primary shirt color on joystick input */
__attribute__((used, externally_visible))
void _EditTeamsChangeShirtsPrimary(void)
{
    uint8_t *team = &selTeamsPtr;
    uint8_t d0;

    if (controlMask == 0x41) {          /* left+fire */
        d0 = team[27];  /* prStripesColor */
        d0 = (d0 == 0) ? 9 : d0 - 1;
        team[27] = d0;
    } else if (controlMask == 0x81) {   /* right+fire */
        d0 = team[27];
        d0 = (d0 + 1 == 10) ? 0 : d0 + 1;
        team[27] = d0;
    } else if (controlMask == 0x101) {  /* up+fire */
        if (team[26] == 0) {  /* SHIRT_ORDINARY */
            d0 = team[27];
            d0 = (d0 == 0) ? 9 : d0 - 1;
            team[27] = d0;
        } else {
            d0 = team[28];  /* prBasicColor */
            d0 = (d0 == 0) ? 9 : d0 - 1;
            team[28] = d0;
        }
    } else if (controlMask == 0x201) {  /* down+fire */
        if (team[26] == 0) {  /* SHIRT_ORDINARY */
            d0 = team[27];
            d0 = (d0 + 1 == 10) ? 0 : d0 + 1;
            team[27] = d0;
        } else {
            d0 = team[28];
            d0 = (d0 + 1 == 10) ? 0 : d0 + 1;
            team[28] = d0;
        }
    } else {
        return;
    }

    PrimaryKitColorConversionC();
    /* nullsub_16 */
    _CallDrawMultipleItems(1, &menuEntries[0x118 / 56]);
}

/* EditTeamsChangeShirtsSecondary — change secondary shirt color on joystick input */
__attribute__((used, externally_visible))
void _EditTeamsChangeShirtsSecondary(void)
{
    uint8_t *team = &selTeamsPtr;
    uint8_t d0;

    if (controlMask == 0x41) {          /* left+fire */
        d0 = team[32];  /* secStripesColor */
        d0 = (d0 == 0) ? 9 : d0 - 1;
        team[32] = d0;
    } else if (controlMask == 0x81) {   /* right+fire */
        d0 = team[32];
        d0 = (d0 + 1 == 10) ? 0 : d0 + 1;
        team[32] = d0;
    } else if (controlMask == 0x101) {  /* up+fire */
        if (team[31] == 0) {
            d0 = team[32];
            d0 = (d0 == 0) ? 9 : d0 - 1;
            team[32] = d0;
        } else {
            d0 = team[33];  /* secBasicColor */
            d0 = (d0 == 0) ? 9 : d0 - 1;
            team[33] = d0;
        }
    } else if (controlMask == 0x201) {  /* down+fire */
        if (team[31] == 0) {
            d0 = team[32];
            d0 = (d0 + 1 == 10) ? 0 : d0 + 1;
            team[32] = d0;
        } else {
            d0 = team[33];
            d0 = (d0 + 1 == 10) ? 0 : d0 + 1;
            team[33] = d0;
        }
    } else {
        return;
    }

    SecondaryKitColorConversionC();
    /* nullsub_16 */
    _CallDrawMultipleItems(1, &menuEntries[0x1F8 / 56]);
}

/* EditTeamsChangeShortsPrimary — change primary shorts color */
__attribute__((used, externally_visible))
void _EditTeamsChangeShortsPrimary(void)
{
    uint8_t *team = &selTeamsPtr;
    uint8_t d0 = team[29]; /* prShortsColor */

    if (controlMask == 0x41 || controlMask == 0x101) {
        d0 = (d0 == 0) ? 9 : d0 - 1;
    } else if (controlMask == 0x81 || controlMask == 0x201) {
        d0 = (d0 + 1 == 10) ? 0 : d0 + 1;
    } else {
        return;
    }

    team[29] = d0;
    PrimaryKitColorConversionC();
    /* nullsub_16 */
    _CallDrawMultipleItems(1, &menuEntries[0x150 / 56]);
}

/* EditTeamsChangeShortsSecondary — change secondary shorts color */
__attribute__((used, externally_visible))
void _EditTeamsChangeShortsSecondary(void)
{
    uint8_t *team = &selTeamsPtr;
    uint8_t d0 = team[34]; /* secShortsColor */

    if (controlMask == 0x41 || controlMask == 0x101) {
        d0 = (d0 == 0) ? 9 : d0 - 1;
    } else if (controlMask == 0x81 || controlMask == 0x201) {
        d0 = (d0 + 1 == 10) ? 0 : d0 + 1;
    } else {
        return;
    }

    team[34] = d0;
    SecondaryKitColorConversionC();
    /* nullsub_16 */
    _CallDrawMultipleItems(1, &menuEntries[0x230 / 56]);
}

/* EditTeamsChangeSocksPrimary — change primary socks color */
__attribute__((used, externally_visible))
void _EditTeamsChangeSocksPrimary(void)
{
    uint8_t *team = &selTeamsPtr;
    uint8_t d0 = team[30]; /* prSocksColor */

    if (controlMask == 0x41 || controlMask == 0x101) {
        d0 = (d0 == 0) ? 9 : d0 - 1;
    } else if (controlMask == 0x81 || controlMask == 0x201) {
        d0 = (d0 + 1 == 10) ? 0 : d0 + 1;
    } else {
        return;
    }

    team[30] = d0;
    PrimaryKitColorConversionC();
    _CallDrawMultipleItems(1, &menuEntries[0x188 / 56]);
}

/* EditTeamsChangeSocksSecondary — change secondary socks color */
__attribute__((used, externally_visible))
void _EditTeamsChangeSocksSecondary(void)
{
    uint8_t *team = &selTeamsPtr;
    uint8_t d0 = team[35]; /* secSocksColor */

    if (controlMask == 0x41 || controlMask == 0x101) {
        d0 = (d0 == 0) ? 9 : d0 - 1;
    } else if (controlMask == 0x81 || controlMask == 0x201) {
        d0 = (d0 + 1 == 10) ? 0 : d0 + 1;
    } else {
        return;
    }

    team[35] = d0;
    SecondaryKitColorConversionC();
    _CallDrawMultipleItems(1, &menuEntries[0x268 / 56]);
}

/* EditTeamsSelectPrimaryShirtType — set primary shirt type from menu ordinal */
__attribute__((used, externally_visible))
void _EditTeamsSelectPrimaryShirtType(MenuEntry *entry)
{
    uint8_t *team = &selTeamsPtr;
    team[26] = (uint8_t)(entry->ordinal - 12); /* prShirtType */
    MarkFirstKitShirtC();
    PrimaryKitColorConversionC();
    /* nullsub_16 */
    _CallDrawMultipleItems(4, &menuEntries[0x2A0 / 56]);
    _CallDrawMultipleItems(1, &menuEntries[0x118 / 56]);
}

/* EditTeamsSelectSecondaryShirtType — set secondary shirt type from menu ordinal */
__attribute__((used, externally_visible))
void _EditTeamsSelectSecondaryShirtType(MenuEntry *entry)
{
    uint8_t *team = &selTeamsPtr;
    team[31] = (uint8_t)(entry->ordinal - 16); /* secShirtType */
    MarkSecondKitShirtC();
    SecondaryKitColorConversionC();
    /* nullsub_16 */
    _CallDrawMultipleItems(4, &menuEntries[0x380 / 56]);
    _CallDrawMultipleItems(1, &menuEntries[0x1F8 / 56]);
}

/* EditTeamsChangePlayerName — edit player name via text input */
__attribute__((used, externally_visible))
void _EditTeamsChangePlayerName(MenuEntry *entry)
{
    _InputText(23, (uint8_t *)entry);
}

/* EditTeamsSelectTeamName — edit team name via text input */
__attribute__((used, externally_visible))
void _EditTeamsSelectTeamName(MenuEntry *entry)
{
    _InputText(17, (uint8_t *)entry);
}

/* EditTeamsSelectCoachName — edit coach name via text input */
__attribute__((used, externally_visible))
void _EditTeamsSelectCoachName(MenuEntry *entry)
{
    _InputText(23, (uint8_t *)entry);
}

/* EditTeamsChangePlayerFace — cycle through player face sprites */
__attribute__((used, externally_visible))
void _EditTeamsChangePlayerFace(MenuEntry *entry)
{
    uint8_t *player = EditTeamsGetPlayerPointerC(entry);
    uint8_t posAndFace = player[76 + 26]; /* PLAYERFILE_POSITIONANDFACE + 0x4C */
    uint16_t faceIdx = (posAndFace & 0x18) >> 3;

    faceIdx++;
    if (faceIdx == 3)
        faceIdx = 0;

    player[76 + 26] = (posAndFace & 0xE7) | (uint8_t)(faceIdx << 3);

    /* Update menu entry sprite to show new face */
    *(uint16_t *)&entry->fg = (uint16_t)(faceIdx + 186);  /* SPR_PL_FACE_WHITE_GUY */
    _CallDrawMenuItem(entry);
}

/* InitEditTeamsMenu — set player faces, names, kit colors in menu */
__attribute__((used, externally_visible))
void _InitEditTeamsMenu(void)
{
    /* Show/hide import entry based on teamIsImported */
    menuEntries[0x460 / 56].isInvisible = 0;
    if (teamIsImported != 0)
        menuEntries[0x460 / 56].isInvisible = 1;

    uint8_t *team = &selTeamsPtr;

    /* Team name → entry 0 */
    menuEntries[0].fg = (uint32_t)(uint8_t *)(team + 5);

    /* Coach name → entry at offset 0xA8 */
    menuEntries[0xA8 / 56].fg = (uint32_t)(uint8_t *)(team + 36);

    PrimaryKitColorConversionC();
    SecondaryKitColorConversionC();
    MarkFirstKitShirtC();
    MarkSecondKitShirtC();

    /* Set player faces and names */
    MenuEntry *faceEntry = &menuEntries[0x508 / 56];
    MenuEntry *nameEntry = &menuEntries[0x888 / 56];
    uint8_t *playerPtr = team;

    for (int16_t i = 15; i >= 0; i--) {
        uint8_t posAndFace = playerPtr[76 + 26];  /* PLAYERFILE_POSITIONANDFACE + 0x4C */
        uint16_t faceIdx = (posAndFace & 0x18) >> 3;
        *(uint16_t *)&faceEntry->fg = (uint16_t)(faceIdx + 186);  /* SPR_PL_FACE_WHITE_GUY */
        faceEntry = (MenuEntry *)((uint8_t *)faceEntry + 56);

        nameEntry->fg = (uint32_t)(uint8_t *)(playerPtr + 76 + 3);  /* PLAYERFILE_PLAYERNAME + 0x4C */
        nameEntry = (MenuEntry *)((uint8_t *)nameEntry + 56);

        playerPtr += 38;
    }
}

/* EditTeamsPlayersBeforeDraw — set up per-player overlay entries before draw */
__attribute__((used, externally_visible))
void _EditTeamsPlayersBeforeDraw(MenuEntry *entry)
{
    uint16_t playerIdx = entry->ordinal - 23;
    uint8_t *team = &selTeamsPtr;
    uint8_t *playerPtr = team + playerIdx * 38;

    uint8_t posAndFace = playerPtr[76 + 26];  /* PLAYERFILE_POSITIONANDFACE */
    int16_t y = entry->y;

    MenuEntry *e = &menuEntries[0xC08 / 56];

    /* Player number (shirt number byte at offset 78 relative to player start = team + playerIdx*38 + 78) */
    uint8_t shirtNum = playerPtr[78];
    *(uint16_t *)&e->fg = (uint16_t)shirtNum;
    e->y = y;
    e = (MenuEntry *)((uint8_t *)e + 56);

    /* Second entry — just set y */
    e->y = y;
    e = (MenuEntry *)((uint8_t *)e + 56);

    /* Position string */
    uint8_t posField = playerPtr[102];  /* offset 76+26 = 102, PLAYERFILE_POSITIONANDFACE */
    uint16_t posIdx = (posField & 0xE0) >> 3;  /* extract top 3 bits, shift to word index */
    e->fg = playerPositionsStringTable[posIdx / 4];
    e->y = y;
    e = (MenuEntry *)((uint8_t *)e + 56);

    /* Hidden entries */
    e->isInvisible = 1;
    e->y = y;
    e = (MenuEntry *)((uint8_t *)e + 56);

    e->isInvisible = 1;
    e->y = y;
    e = (MenuEntry *)((uint8_t *)e + 56);

    e->isInvisible = 1;
    e->isInvisible = 1;  /* written twice in asm */
    e->y = y;
    e = (MenuEntry *)((uint8_t *)e + 56);

    /* Draw all 6 entries */
    _DrawMultipleItems(6, &menuEntries[0xC08 / 56]);
}

/* ================================================================
 * Chunk 148: _EditTeamsCallback
 * ================================================================ */

extern uint8_t editTeamsMenu[];
extern void _ShowMenu(uint8_t *menu);

/* _EditTeamsCallback: called when a team is selected from team selection.
 * Stores team pointer, then shows the edit teams sub-menu.
 * a0 = selected team pointer. */
__attribute__((used, externally_visible))
void _EditTeamsCallback(uint8_t *team)
{
    editTeamsSelectedTeam = team;
    _ShowMenu(editTeamsMenu);
}


/* ==========================================================================
 * Chunk 203 — EditCustomTeam + ImportTeamsMenu
 * (SWOS2.s lines 44496-44537, 45967-46026)
 * ========================================================================== */

extern uint16_t numSelectedTeams;
extern uint8_t  lastLoadedExtNumber;
extern uint8_t  teamFileBuffer[];
extern uint8_t  editTeamsBuffer[];
extern uint8_t  lastSelectedTeam[];
extern uint8_t  editTeamsSaveVarsArea[];
extern uint8_t  selTeamImportField;
extern uint8_t  aEditCustomTeam[];
extern uint8_t  aSelectTeamToEd[];
extern uint8_t  aCustomTeamsChanged[];
extern uint8_t  aCustomsEdt[];
extern uint8_t  aSave[];
extern uint8_t  aAbort[];
extern uint8_t  aSelectTeamToIm[];

extern int16_t _CheckContinueAbortPrompt(uint8_t *header);
extern int16_t _DoContinueAbortMenu(uint8_t *header, uint8_t *changing,
                                     uint8_t *contText, uint8_t *abortTextStr,
                                     uint16_t defaultEntry);
extern int16_t _Asset_PromptInsertSaveDiskDf0(uint8_t *title);
extern int16_t _WriteFile(uint8_t *filename, uint8_t *src, uint32_t size);
extern void _ShowFsErrorByCode(uint16_t errCode);
extern void _ShowInsertDisk2Dialog(uint8_t *header);

/* EditCustomTeam — show "EDIT CUSTOM TEAMS" flow: choose team, edit, optionally save. */
__attribute__((used, externally_visible))
void _EditCustomTeam(void)
{
    if (_CheckContinueAbortPrompt(aEditCustomTeam) != 0)
        return;

    numSelectedTeams = 0;
    saveCustomTeams = 0;
    _ChooseTeamsDialog(0, 0, 1, aSelectTeamToEd, (void *)_EditTeamsCallback);

    if (saveCustomTeams == 0)
        goto done;

retry:
    if (_DoContinueAbortMenu(aEditCustomTeam, aCustomTeamsChanged,
                              aSave, aAbort, 0) != 0)
        goto done;

    /* Prompt for save disk */
    if (_Asset_PromptInsertSaveDiskDf0(aEditCustomTeam) != 0)
        goto retry;

    /* Write team file: size = teamCount * 684 + 2 (for the count word) */
    {
        uint16_t teamCount = *(uint16_t *)teamFileBuffer;
        uint32_t fileSize = (uint32_t)teamCount * 684 + 2;
        int16_t result = _WriteFile(aCustomsEdt, teamFileBuffer, fileSize);
        if (result != 0) {
            _ShowFsErrorByCode((uint16_t)result);
            goto retry;
        }
    }

done:
    lastLoadedExtNumber = 0xFF;
    _ShowInsertDisk2Dialog(aEditCustomTeam);
}

/* ImportTeamsMenu — import a team from club/national teams into the editor buffer. */
__attribute__((used, externally_visible))
void _ImportTeamsMenu(void)
{
    /* Save teamFileBuffer to editTeamsBuffer */
    {
        uint16_t *src = (uint16_t *)teamFileBuffer;
        uint16_t teamCount = *src;
        uint32_t wordCount = (uint32_t)teamCount * 684 + 2;
        wordCount >>= 1; /* convert bytes to words */
        volatile uint16_t *s = (volatile uint16_t *)src;
        volatile uint16_t *d = (volatile uint16_t *)editTeamsBuffer;
        while (wordCount > 0) {
            *d++ = *s++;
            wordCount--;
        }
    }

    /* Save lastSelectedTeam area (0xF6 words = 246 words = 492 bytes) */
    {
        volatile uint16_t *src = (volatile uint16_t *)lastSelectedTeam;
        volatile uint16_t *dst = (volatile uint16_t *)editTeamsSaveVarsArea;
        for (int16_t i = 0xF5; i >= 0; i--)
            *dst++ = *src++;
    }

    _ShowInsertDisk2Dialog(aSelectTeamToIm);

    /* Show team selection dialog (club + national, no custom) */
    _ChooseTeamsDialog(1, 1, 0, aSelectTeamToIm, (void *)0);

    extern uint16_t selectedTeamCountryNumber;
    extern uint32_t selectedTeamPtr;

    if ((int16_t)selectedTeamCountryNumber != -1) {
        /* Save selTeamsPtr team number and selTeamImportField, copy selected team data */
        uint16_t savedTeamNum = *(uint16_t *)&selTeamsPtr;
        uint8_t savedImportField = selTeamImportField;

        /* Copy 342 words (684 bytes) from selectedTeamPtr to selTeamsPtr */
        volatile uint16_t *src = (volatile uint16_t *)(uint32_t)selectedTeamPtr;
        volatile uint16_t *dst = (volatile uint16_t *)&selTeamsPtr;
        for (int16_t i = 341; i >= 0; i--)
            *dst++ = *src++;

        /* Restore saved values */
        selTeamImportField = savedImportField;
        *(uint16_t *)&selTeamsPtr = savedTeamNum;

        teamIsImported = 1;
        _SetCurrentEntry(21);
    }

    /* Restore lastSelectedTeam area from editTeamsSaveVarsArea */
    {
        volatile uint16_t *src = (volatile uint16_t *)editTeamsSaveVarsArea;
        volatile uint16_t *dst = (volatile uint16_t *)lastSelectedTeam;
        for (int16_t i = 245; i >= 0; i--)
            *dst++ = *src++;
    }

    lastLoadedExtNumber = 0xFF;

    /* Restore teamFileBuffer from editTeamsBuffer */
    {
        uint16_t *srcBuf = (uint16_t *)editTeamsBuffer;
        uint16_t teamCount = *srcBuf;
        uint32_t wordCount = (uint32_t)teamCount * 684 + 2;
        wordCount >>= 1;
        volatile uint16_t *s = (volatile uint16_t *)srcBuf;
        volatile uint16_t *d = (volatile uint16_t *)teamFileBuffer;
        while (wordCount > 0) {
            *d++ = *s++;
            wordCount--;
        }
    }

    _InitEditTeamsMenu();
}

/* Helper: set bg color index (high word of bg, low nibble) */
static void setBgColor(MenuEntry *e, uint16_t colorIdx) {
    uint16_t bgHigh = (uint16_t)(e->bg >> 16);
    bgHigh = (bgHigh & 0xFFF0) | colorIdx;
    e->bg = ((uint32_t)bgHigh << 16) | (e->bg & 0xFFFF);
}

/* ================================================================
 * ChooseTeamForEditTactics  (SWOS2.s line 52506)
 *
 * Display a list of teams to choose from for editing tactics.
 * Sorts teams alphabetically (bubble sort), shows menu, returns
 * selected team pointer (or NULL if cancelled).
 *
 * in:  a0 = menu title string
 *      a1 = pointer to team numbers array (uint16_t[])
 *      d0 = number of teams
 * out: a0 = team pointer (or NULL)
 * ================================================================ */
extern uint32_t chooseTeamMenuName;
extern uint16_t numTeamsCountryBuffer;
extern uint32_t dwArrayCountries[];
extern uint8_t indexCountries[];
extern uint8_t selectTeamsForTacticsMenu[];
extern uint16_t teamSelected;

__attribute__((used, externally_visible))
uint8_t *_ChooseTeamForEditTactics(uint8_t *menuTitle, uint16_t *teamNumbers,
                                    uint16_t numTeams)
{
    chooseTeamMenuName = (uint32_t)menuTitle;
    numTeamsCountryBuffer = numTeams;

    uint32_t *nameArray = dwArrayCountries;
    uint8_t *idxArray = indexCountries;
    uint16_t count = 0;

    uint16_t *teamNums = teamNumbers;
    uint16_t i;
    for (i = 0; i < numTeams; i++) {
        uint16_t teamNum = teamNums[i];
        uint8_t *teamPtr = _VerifyTeamLoaded(teamNum);
        idxArray[i] = (uint8_t)count;
        nameArray[count] = (uint32_t)(teamPtr + 5);
        count++;
    }

    /* Bubble sort by team name */
    if (numTeams >= 2) {
        int16_t limit = (int16_t)numTeams - 2;
        int16_t swapped;
        do {
            swapped = 0;
            for (int16_t j = 0; j <= limit; j++) {
                uint8_t idx1 = idxArray[j];
                uint8_t idx2 = idxArray[j + 1];
                uint8_t *name1 = (uint8_t *)nameArray[idx1];
                uint8_t *name2 = (uint8_t *)nameArray[idx2];
                /* Compare strings */
                uint8_t *p1 = name1;
                uint8_t *p2 = name2;
                int cmpResult = 0;
                while (1) {
                    uint8_t c1 = *p1++;
                    if (c1 == 0) break;
                    uint8_t c2 = *p2++;
                    if (c1 < c2) break;
                    if (c1 > c2) {
                        /* Swap */
                        uint8_t tmp = idxArray[j];
                        idxArray[j] = idxArray[j + 1];
                        idxArray[j + 1] = tmp;
                        swapped = 1;
                        cmpResult = 1;
                        break;
                    }
                }
                (void)cmpResult;
            }
        } while (swapped);
    }

    /* Show the selection menu */
    _ShowMenu(selectTeamsForTacticsMenu);

    int16_t selected = (int16_t)teamSelected;
    if (selected >= 0) {
        uint8_t finalIdx = indexCountries[selected];
        uint16_t teamNum = teamNumbers[finalIdx];
        return _VerifyTeamLoaded(teamNum);
    }

    return (uint8_t *)0;
}

/* ================================================================
 * EditTactics  (SWOS2.s line 68771)
 *
 * Entry point for Edit Tactics menu. Determines which team to edit:
 * - In career mode: uses careerTeam
 * - With multiple selected teams: shows team selection menu
 * - With one team: uses that team
 * - No teams: loads default team (Leicester City)
 * ================================================================ */
extern uint8_t aEditTacticsSel[];  /* "EDIT TACTICS - SELECT TEAM" */
extern uint8_t editTacticsCountryBuffer[];
extern uint8_t teamFileBuffer[];

__attribute__((used, externally_visible))
void _EditTactics(void)
{
    inEditTactics = 1;

    uint8_t *teamPtr = (uint8_t *)&careerTeam;

    if (gameType == GAME_TYPE_CAREER) {
        /* Career mode: use career team directly */
        goto show_menu;
    }

    if (numSelectedTeams == 0) {
        goto no_teams;
    }

    {
        uint8_t *selPtr = &selTeamsPtr;
        uint16_t *countryBuf = (uint16_t *)editTacticsCountryBuffer;
        uint16_t found = 0;
        uint8_t *lastNonComputer = (uint8_t *)0;

        for (uint16_t i = 0; i < numSelectedTeams; i++) {
            uint8_t *t = selPtr + (uint32_t)i * 684;
            if (t[4] != TEAM_COMPUTER) {  /* TEAMFILE_TEAMCONTROLS */
                *countryBuf++ = *(uint16_t *)t;
                lastNonComputer = t;
                found++;
            }
        }

        if (found == 0) {
            if (numSelectedTeams != 0) {
                goto check_sel_team;
            }
            goto no_teams;
        }

        teamPtr = lastNonComputer;

        if (found == 1) {
            goto show_menu;
        }

        /* Multiple non-computer teams: show selection */
        teamPtr = _ChooseTeamForEditTactics(aEditTacticsSel,
                    (uint16_t *)editTacticsCountryBuffer, found);
        if (teamPtr != (uint8_t *)0) {
            goto show_menu;
        }
        return;
    }

no_teams:
    /* Load English Premier Division and pick Leicester City (18th team) */
    _LoadTeamFile(8);
    _AddTeamToBuffer(teamFileBuffer + 0x2D6E);
    _InitTeams();
    goto show_menu_from_sel;

check_sel_team:
    teamPtr = (uint8_t *)&careerTeam;
    if (selTeamNumber != 0xFFFF) {
        goto show_menu;
    }

show_menu_from_sel:
    teamPtr = &selTeamsPtr;

show_menu:
    _ShowStandardChooseTacticsMenu(teamPtr);
}

/* ================================================================
 * EditTactics_ApplyPlayerRowHighlighting  (SWOS2.s line 70226)
 *
 * Sets the background color of menuEntries[39] based on the player
 * row index (position group: GK=0, defenders=1-10, subs=11+, slot16).
 * Also checks injury, selection, pair-swap, and flip-on states.
 *
 * in:  d7 = player ordinal (0-16)
 *      a5 = current menu entry
 * ================================================================ */
extern uint16_t editTacticsFlipPairColorTable[];

__attribute__((used, externally_visible))
void _EditTactics_ApplyPlayerRowHighlighting(int16_t playerOrdinal, MenuEntry *cur)
{
    MenuEntry *entry39 = &menuEntries[39];

    if (playerOrdinal == 16) {
        setBgColor(entry39, BLUE_TO_BLACK_8);
    } else if (playerOrdinal < 11) {
        if (playerOrdinal == 0) {
            setBgColor(entry39, YELLOW_GREEN_BLACK_14);
        } else {
            setBgColor(entry39, BRIGHT_BLUE_TO_BLUE_13);
        }
    } else if (playerOrdinal == 11) {
        setBgColor(entry39, YELLOW_GREEN_BLACK_14);
    } else {
        setBgColor(entry39, BLUE_TO_PURPLE_11);
    }

    /* Save/restore tactics byte, get player info */
    uint8_t *teamBase = (uint8_t *)(uint32_t)chooseTacticsTeamPtr;
    uint8_t savedTactics = teamBase[24]; /* TEAMFILE_TACTICS */
    teamBase[24] = editTacticsCurrentTactics.ballOutOfPlayTactics;

    uint16_t slot, playerIndex;
    uint8_t *playerPtr = _GetPlayerInfoBreakPosition(teamBase, (uint16_t)playerOrdinal, &slot, &playerIndex);

    teamBase = (uint8_t *)(uint32_t)chooseTacticsTeamPtr;
    teamBase[24] = savedTactics;

    /* Check position byte for injury - offset 0x67 from player base */
    uint8_t posByte = playerPtr[0x67];
    uint16_t posField = (uint16_t)posByte & 0xE0;
    if (posField != 0 && posField != 0x20) {
        setBgColor(entry39, RED_10);
    }

    /* Selected player highlight */
    if (playerIndex == editTacticsSelectedPlayerNameIndex) {
        setBgColor(entry39, ORANGE_TO_BROWN_6);
    }

    /* Pair swap highlight */
    if ((int16_t)editTacticsSelectedPairSlotIndex >= 0) {
        uint16_t pairSlot = editTacticsSelectedPairSlotIndex + 1;
        if (slot == pairSlot) {
            setBgColor(entry39, ORANGE_TO_BROWN_12);
        }
    }

    /* Flip-on pair coloring */
    if (flipOnOff != 0) {
        if (slot != 0 && slot < 11) {
            uint8_t *unkTable = (uint8_t *)&editTacticsCurrentTactics + 359; /* TEAMTACTICS_UNKTABLE */
            int8_t pairVal = (int8_t)unkTable[slot - 1];
            if (pairVal >= 0) {
                uint16_t colorIdx = editTacticsFlipPairColorTable[pairVal];
                setBgColor(entry39, colorIdx);
            } else {
                setBgColor(entry39, PINK_TO_BROWN_0);
            }
        } else {
            setBgColor(entry39, PINK_TO_BROWN_0);
        }
    }

    /* Copy Y position and set player face */
    entry39->y = cur->y;
    _SetMenuEntryPlayerFace(entry39, playerPtr);

    /* Check position changes (only for first 11 players) */
    if (slot < 11) {
        uint8_t *posArray = &playerPositionsArray;
        uint8_t *oldPosArray = &oldPlayerPositionsArray;
        uint8_t curPos = posArray[playerIndex];
        uint8_t oldPos = oldPosArray[playerIndex];
        if (curPos > oldPos) {
            entry39->fg = (entry39->fg & 0xFFFF0000u) | SPR_CHECK_SMALL;
        } else if (curPos < oldPos) {
            entry39->fg = (entry39->fg & 0xFFFF0000u) | SPR_NOT_CHECK_SMALL;
        }
    }
}

/* ================================================================
 * EditTacticsAfterDrawPlayerNames  (SWOS2.s line 70061)
 *
 * After-draw callback for edit tactics player name entries.
 * Sets up the info panel (entries 40-46) with player details:
 * shirt number, name, position, skills, value/status, face indicator.
 *
 * in:  a5 = current menu entry
 * ================================================================ */
extern uint8_t fundsBuffer[];

__attribute__((used, externally_visible))
void _EditTacticsAfterDrawPlayerNames(MenuEntry *cur)
{
    uint16_t d7 = cur->ordinal - 0xB;  /* entry ordinal - 11 */

    _EditTactics_ApplyPlayerRowHighlighting((int16_t)d7, cur);

    MenuEntry *entry41 = &menuEntries[41];
    uint16_t d5; /* flag: 1 if slot 16 (no player details) */

    if (d7 == 0x10) {
        d5 = 1;
        setBgColor(entry41, BLUE_TO_BLACK_8);
    } else if (d7 < 0xB) {
        d5 = 0;
        if (d7 == 0) {
            setBgColor(entry41, YELLOW_GREEN_BLACK_14);
        } else {
            setBgColor(entry41, BRIGHT_BLUE_TO_BLUE_13);
        }
    } else {
        d5 = 0;
        setBgColor(entry41, BLUE_TO_PURPLE_11);
    }

    /* Get player info */
    uint8_t *teamBase = (uint8_t *)(uint32_t)chooseTacticsTeamPtr;
    uint8_t savedTactics = teamBase[24]; /* TEAMFILE_TACTICS */
    teamBase[24] = editTacticsCurrentTactics.ballOutOfPlayTactics;

    uint16_t d7_slot, d1_fileIdx;
    uint8_t *playerPtr = _GetPlayerInfoBreakPosition(teamBase, d7, &d7_slot, &d1_fileIdx);

    teamBase = (uint8_t *)(uint32_t)chooseTacticsTeamPtr;
    teamBase[24] = savedTactics;

    /* Check if player is injured */
    if (_IsPlayerInjured(playerPtr, (int16_t)inEditTactics)) {
        setBgColor(entry41, RED_10);
    }

    /* Check if this player is selected */
    if (d1_fileIdx == editTacticsSelectedPlayerNameIndex) {
        setBgColor(entry41, ORANGE_TO_BROWN_6);
    }

    /* Check pair swap highlight */
    if ((int16_t)editTacticsSelectedPairSlotIndex >= 0) {
        uint16_t pairSlot = editTacticsSelectedPairSlotIndex + 1;
        if (d7_slot == pairSlot) {
            setBgColor(entry41, ORANGE_TO_BROWN_12);
        }
    }

    /* Set up detail entries (40-44) */
    MenuEntry *e = &menuEntries[40];
    uint16_t entryY = cur->y;

    /* Entry 40: shirt number */
    e->isInvisible = 1;
    if (!d5) {
        e->fg = (e->fg & 0xFFFF0000u) | (uint16_t)playerPtr[0x4E];
        e->isInvisible = 0;
    }
    e->y = entryY;
    e = (MenuEntry *)((uint8_t *)e + 56);  /* entry 41 */

    /* Entry 41: player name (surname) */
    uint8_t *surname = _ExtractSurname(playerPtr + 0x4F, 0x800D);
    e->fg = (uint32_t)surname;
    e->y = entryY;
    e = (MenuEntry *)((uint8_t *)e + 56);  /* entry 42 */

    /* Entry 42: position string */
    uint8_t posAndFace = playerPtr[0x66];
    uint16_t posIdx = ((uint16_t)posAndFace & 0xE0) >> 3;
    e->fg = playerPositionsStringTable[posIdx / 4];
    e->y = entryY;
    e = (MenuEntry *)((uint8_t *)e + 56);  /* entry 43 */

    /* Entry 43: top skills */
    _FormTopSkillsString((uint8_t *)e->fg, playerPtr);
    e->y = entryY;
    e = (MenuEntry *)((uint8_t *)e + 56);  /* entry 44 */

    /* Entry 44: value/status */
    e->isInvisible = 1;
    e->y = entryY;

    if (_IsTeamNational((uint8_t *)(uint32_t)chooseTacticsTeamPtr)) {
        _InitPlayerValueDisplayStruct(playerPtr, (uint8_t *)e);
    } else {
        _FormatPlayerValueOrStatusText(playerPtr, fundsBuffer);
        e->fg = (uint32_t)fundsBuffer;
    }
    e->isInvisible = 0;

    /* Determine color for entry 44 based on player status */
    uint16_t colorVal;
    if (playerPtr[0x71] == 0x9B) {
        colorVal = 0xD;
    } else if (playerPtr[0x71] == 0x9A) {
        colorVal = 1;
    } else if (playerPtr[0x66] & 0x02) {
        colorVal = 2;
    } else {
        colorVal = 0xF;
    }

    {
        uint16_t fgHigh = (uint16_t)(e->fg >> 16);
        fgHigh = (fgHigh & 0xFFF0) | colorVal;
        e->fg = ((uint32_t)fgHigh << 16) | (e->fg & 0xFFFF);
    }

    _DrawMultipleItems(6, &menuEntries[39]);
}
