#include "menus.h"
#include "game_utils.h"
#include "game_types.h"
#include "frontend.h"
#include "menu_system.h"
#include "string_utils.h"
#include "text_render.h"
#include "edit_tactics.h"
#include "teams.h"
#include "audio.h"

/* ---- Constants ---- */
#define GAME_TYPE_NO_GAME 0
#define CN_ENGLAND        8
#define SEASONAL          0xFFFE
#define PLAYERPOSITIONS_SIZE         35

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint32_t menuImageStreamBaseEnd;
extern uint16_t menuBGImageCmdStream[];
extern uint8_t aGrafsMenubgRaw[];
extern uint16_t twoPlayers;
extern uint16_t player1ClearFlag;
extern uint16_t player2ClearFlag;
extern uint16_t deadVar01;
extern uint16_t deadVar02;
extern uint16_t deadVar03;
extern uint16_t flipOnOff;
extern uint16_t inFriendlyMenu;
extern uint16_t isNationalTeam;
extern uint16_t menuStatus;
extern uint16_t menuFade;
extern uint16_t exitMenu;
extern uint16_t fireResetFlag;
extern uint16_t seasonUpdateInProgress;
extern uint16_t internationalAutoAdvanceFlag;
extern uint16_t highlightedLeagueTeamId;
extern uint16_t writeOnlyVar01;
extern uint16_t coachOrPlayer;
extern uint8_t plNationality;
extern uint16_t competitionOver;
extern uint16_t numSelectedTeams;
extern uint16_t teamsLoaded;
extern uint16_t poolplyrLoaded;
extern uint8_t importTacticsFilename;
extern uint32_t chooseTacticsTeamPtr;
extern uint8_t swosMainMenu[];

/* ZeroOutStars globals */
extern uint8_t aResigned[];
extern void InitSkinAndDefaultPointers(void); /* asm stub — used as end-of-strings marker */

/* SetDefaultOptions globals */
extern uint16_t gameLength;
extern uint16_t autoReplays;
extern uint16_t menuMusic;
extern uint16_t autoSaveHighlights;
extern uint16_t pitchType;
extern uint16_t allPlayerTeamsEqual;

/* SetDefaultNameAndSurname globals */
extern uint16_t mrOrMs;
extern volatile uint16_t currentGameTick;
extern uint32_t defaultNamesTable[6];
extern uint32_t defaultSurnamesTable[6];
extern uint8_t managerNameBuffer[9];
extern uint8_t managerSurnameBuffer[13];

/* InitCareerVariables globals */
extern volatile uint16_t currentTick;
extern uint16_t randomSeed;
extern uint8_t newRandomSeedCopy;
extern uint16_t gameType;
extern uint16_t contestStarted;
extern uint16_t careerAutoAdvanceInProgress;
extern uint16_t careerLoadedFlag;
extern uint16_t selTeamNumber;
extern uint16_t currentTeamNumber;
extern uint16_t currentSeasonShowing;
extern uint8_t seasonPlaying;
extern uint8_t competitionFilename[14];
extern uint8_t hilFilename[14];
extern uint16_t careerFileBuffer;

/* InitUserTactics globals */
extern TeamTactics teamTactics532;
extern TeamTactics teamTactics442;
extern TeamTactics teamTacticsSweep;
extern TeamTactics teamTacticsUSERA;
extern TeamTactics teamTacticsUSERB;
extern TeamTactics teamTacticsUSERC;
extern TeamTactics teamTacticsUSERD;
extern TeamTactics teamTacticsUSERE;
extern TeamTactics teamTacticsUSERF;
extern uint8_t aUSER_A[];
extern uint8_t aUSER_B[];
extern uint8_t aUSER_C[];
extern uint8_t aUSER_D[];
extern uint8_t aUSER_E[];
extern uint8_t aUSER_F[];

/* InitializeTacticsPositions globals */
extern uint32_t playersPositionsByTactics[18];
extern uint32_t teamTacticsTable[19];
extern uint16_t tacticsPositionDefensiveness[11];

/* SaveOptions / RestoreOptions globals */
extern uint8_t saveGameLength[];

/* ShowErrorMenu globals */
extern uint16_t errorPromptsEnabled;
extern uint32_t errorText;
extern uint8_t continueMenu[];

/* DoContinueAbortMenu globals */
extern uint32_t promptHeader;
extern uint32_t whatIsChanging;
extern uint32_t continueText;
extern uint32_t abortText;
extern uint16_t contAbortDefault;
extern uint16_t contAbortResult;
extern uint8_t continueAbortMenu[];
extern uint8_t aAbort[];
extern MenuEntry menuEntries[];

/* ---- External asm functions (C-callable bridges in swos2_unit.s) ---- */
extern void _ProcessImageCommandStream(const uint16_t *cmdBuf, uint8_t *workBuf);
extern void _LoadOverlayToFrontbuffer(const uint8_t *path);

/* ---- Local helpers ---- */

/* ================================================================
 * ZeroOutStars  (SWOS2.s)
 *
 * Scans through strings from aResigned up to InitSkinAndDefaultPointers,
 * replacing the first '*' character in each string with a null terminator.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ZeroOutStars(void)
{
    uint8_t *p = aResigned;
    uint8_t *end = (uint8_t *)InitSkinAndDefaultPointers;

    for (;;) {
        /* Scan string: look for '*' or end of string */
        while (*p != 0) {
            if (*p == '*') {
                *p = 0;    /* replace '*' with null */
                p++;
                break;     /* skip to end-of-string scan */
            }
            p++;
        }
        /* Skip rest of string past null terminator */
        while (*p != 0)
            p++;
        p++;
        if (p == end)
            return;
    }
}

/* ================================================================
 * SetDefaultOptions  (SWOS2.s)
 *
 * Initializes game options to default values.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetDefaultOptions(void)
{
    gameLength = 0;
    autoReplays = 1;
    menuMusic = 1;
    autoSaveHighlights = 1;
    pitchType = SEASONAL;
    allPlayerTeamsEqual = 0;
}

/* ================================================================
 * SetDefaultNameAndSurname  (SWOS2.s)
 *
 * Selects a pseudo-random default name/surname pair from lookup
 * tables using currentGameTick as seed. Clears mrOrMs.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetDefaultNameAndSurname(void)
{
    uint16_t idx;

    mrOrMs = 0;

    /* Original: tick & 0xFF, * 6, >> 8, << 2 = index into pointer table */
    idx = currentGameTick & 0xFF;
    idx = (uint16_t)((idx * 6) >> 8);
    idx = idx << 2;  /* multiply by 4 for longword pointer table offset */

    /* idx is now a byte offset into the pointer tables (0, 4, 8, 12, 16, 20) */
    {
        uint32_t *nameTable = (uint32_t *)((uint8_t *)defaultNamesTable + idx);
        uint8_t *name = (uint8_t *)(*nameTable);
        _StringCopy(name, managerNameBuffer);
    }
    {
        uint32_t *surnameTable = (uint32_t *)((uint8_t *)defaultSurnamesTable + idx);
        uint8_t *surname = (uint8_t *)(*surnameTable);
        _StringCopy(surname, managerSurnameBuffer);
    }
}

/* ================================================================
 * InitializeTacticsPositions  (SWOS2.s)
 *
 * For each of 18 tactics formations, computes a defensiveness score
 * for each of the 10 outfield players, then sorts players by score
 * (descending — most defensive first) using bubble sort.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitializeTacticsPositions(void)
{
    int16_t formation;

    for (formation = 0; formation < 18; formation++) {
        TeamTactics *tactics = (TeamTactics *)teamTacticsTable[formation];
        uint8_t *posArr = (uint8_t *)playersPositionsByTactics[formation];

        int16_t player;

        /* Initialize: byte 0 = 0, bytes 1-10 = player numbers */
        posArr[0] = 0;

        for (player = 1; player <= 10; player++) {
            int16_t pos;
            posArr[player] = (uint8_t)player;
            tacticsPositionDefensiveness[player] = 0;

            /* Accumulate defensiveness over positions 15-34 (20 half-field positions) */
            for (pos = 34; pos >= 15; pos--) {
                int16_t offset = (player - 1) * PLAYERPOSITIONS_SIZE + pos;
                uint8_t posData = ((uint8_t *)tactics->positions)[offset];
                int16_t xCoord = (posData >> 4) & 0xF;
                int16_t yCoord = posData & 0xF;
                int16_t xDist = xCoord - 7;
                int16_t yDist;
                int16_t contrib;

                if (xDist < 0) xDist = -xDist;  /* |x - 7| */
                yDist = 15 - yCoord;

                /* contribution = max(xDist, yDist) */
                contrib = xDist;
                if ((uint16_t)yDist > (uint16_t)contrib)
                    contrib = yDist;

                tacticsPositionDefensiveness[player] += (uint16_t)contrib;
            }
        }

        /* Bubble sort players 1-10 by defensiveness (descending) */
        {
            int16_t swapped;
            do {
                int16_t j;
                swapped = 0;
                for (j = 0; j < 9; j++) {
                    if (tacticsPositionDefensiveness[j + 1] < tacticsPositionDefensiveness[j + 2]) {
                        /* Swap scores */
                        uint16_t tmpScore = tacticsPositionDefensiveness[j + 1];
                        tacticsPositionDefensiveness[j + 1] = tacticsPositionDefensiveness[j + 2];
                        tacticsPositionDefensiveness[j + 2] = tmpScore;
                        /* Swap player numbers in position array */
                        {
                            uint8_t tmpPlayer = posArr[j + 1];
                            posArr[j + 1] = posArr[j + 2];
                            posArr[j + 2] = tmpPlayer;
                        }
                        swapped = 1;
                    }
                }
            } while (swapped);
        }
    }

}

/* ================================================================
 * InitUserTactics  (SWOS2.s)
 *
 * Initializes 6 user-defined tactics by:
 * 1) Copying 70 bytes from 532 formation positions to 442 and Sweep
 * 2) Copying full 442 tactics (370 bytes) to each user slot (A-F)
 * 3) Overwriting user slot names with "USER A"-"USER F"
 * 4) Calling InitializeTacticsPositions to sort player positions
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _InitUserTactics(void)
{
    int16_t i;

    /* Copy positions[8..9] (70 bytes) from 532 formation to both 442 and Sweep */
    {
        uint8_t *src = (uint8_t *)&teamTactics532.positions[8];
        uint8_t *dst442 = (uint8_t *)&teamTactics442.positions[8];
        uint8_t *dstSweep = (uint8_t *)&teamTacticsSweep.positions[8];
        for (i = 0; i < 70; i++) {
            dst442[i] = src[i];
            dstSweep[i] = src[i];
        }
    }

    /* Copy full 370-byte 442 tactics to each user slot */
    {
        TeamTactics *userSlots[6];
        int16_t slot;
        userSlots[0] = &teamTacticsUSERA;
        userSlots[1] = &teamTacticsUSERB;
        userSlots[2] = &teamTacticsUSERC;
        userSlots[3] = &teamTacticsUSERD;
        userSlots[4] = &teamTacticsUSERE;
        userSlots[5] = &teamTacticsUSERF;

        for (slot = 0; slot < 6; slot++) {
            uint8_t *dst = (uint8_t *)userSlots[slot];
            uint8_t *src = (uint8_t *)&teamTactics442;
            for (i = 0; i < (int16_t)sizeof(TeamTactics); i++) {
                dst[i] = src[i];
            }
        }
    }

    /* Overwrite each user slot's name field */
    _StringCopy(aUSER_A, (uint8_t *)teamTacticsUSERA.name);
    _StringCopy(aUSER_B, (uint8_t *)teamTacticsUSERB.name);
    _StringCopy(aUSER_C, (uint8_t *)teamTacticsUSERC.name);
    _StringCopy(aUSER_D, (uint8_t *)teamTacticsUSERD.name);
    _StringCopy(aUSER_E, (uint8_t *)teamTacticsUSERE.name);
    _StringCopy(aUSER_F, (uint8_t *)teamTacticsUSERF.name);

    _InitializeTacticsPositions();
}

/* ================================================================
 * InitCareerVariables  (SWOS2.s)
 *
 * Initializes career mode variables. Seeds random generator from
 * currentTick, sets game type, clears flags, then calls Randomize2
 * and InitUserTactics.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitCareerVariables(uint16_t gameTypeParam)
{
    randomSeed = currentTick;
    newRandomSeedCopy = (uint8_t)(currentTick & 0xFF);
    gameType = gameTypeParam;
    contestStarted = 0;
    teamsLoaded = 0;
    careerAutoAdvanceInProgress = 0;
    careerLoadedFlag = 0;
    selTeamNumber = 0xFFFF;
    currentTeamNumber = 0xFFFF;
    currentSeasonShowing = 0;
    seasonPlaying = 96;
    competitionFilename[0] = 0;
    hilFilename[0] = 0;
    careerFileBuffer = 0;

    _Randomize2(0);
    _InitUserTactics();
}

/* ================================================================
 * PrepareMenuBackground  (SWOS2.s line 42468)
 *
 * Processes the menu background image command stream, then loads
 * the menu background raw image into the frontbuffer overlay.
 * ================================================================ */
__attribute__((used, externally_visible))
void _PrepareMenuBackground(void)
{
    _ProcessImageCommandStream(menuBGImageCmdStream, (uint8_t *)menuImageStreamBaseEnd);
    _LoadOverlayToFrontbuffer(aGrafsMenubgRaw);
}

/* ================================================================
 * InitMainMenuStuff  (SWOS2.s line 42280)
 *
 * Initializes the main menu state: clears all game/menu flags,
 * sets defaults, initializes user tactics, then activates the
 * main SWOS menu.
 * ================================================================ */
__attribute__((used, externally_visible, noinline))
void _InitMainMenuStuff(void)
{
    _ZeroOutStars();

    twoPlayers = 0;
    player1ClearFlag = 0;
    player2ClearFlag = 0;
    deadVar01 = 0;
    deadVar02 = 0;

    _SetDefaultOptions();

    flipOnOff = 1;
    inFriendlyMenu = 0;
    isNationalTeam = 0;

    _InitCareerVariables(GAME_TYPE_NO_GAME);

    menuStatus = 0;
    menuFade = 0;
    deadVar03 = 0;
    exitMenu = 0;
    fireResetFlag = 0;
    seasonUpdateInProgress = 0;
    internationalAutoAdvanceFlag = 0;
    highlightedLeagueTeamId = 0xFFFF;
    writeOnlyVar01 = 0;
    coachOrPlayer = 1;

    _SetDefaultNameAndSurname();

    plNationality = CN_ENGLAND;

    /* Original asm clears competitionOver 3 times (likely a bug or
     * intended to clear 3 consecutive words, but label is the same) */
    competitionOver = 0;
    competitionOver = 0;
    competitionOver = 0;

    numSelectedTeams = 0;
    teamsLoaded = 0;
    poolplyrLoaded = 0;
    importTacticsFilename = 0;
    chooseTacticsTeamPtr = 0;

    _InitUserTactics();

    /* Original: lea (swosMainMenu),a6; bra.w ActivateMenu */
    _ActivateMenu(swosMainMenu);
}

/* ================================================================
 * SaveOptions  (SWOS2.s line 69187)
 *
 * Copies 6 consecutive uint16_t values starting at gameLength
 * into a save area (saveGameLength) for later restore.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _SaveOptions(void)
{
    uint16_t *src = &gameLength;
    uint16_t *dst = (uint16_t *)saveGameLength;
    int16_t i;
    for (i = 5; i >= 0; i--) {
        *dst++ = *src++;
    }
}

/* ================================================================
 * RestoreOptions  (SWOS2.s line 69198)
 *
 * Restores 6 consecutive uint16_t values from saveGameLength
 * back into gameLength area.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _RestoreOptions(void)
{
    uint16_t *src = (uint16_t *)saveGameLength;
    uint16_t *dst = &gameLength;
    int16_t i;
    for (i = 5; i >= 0; i--) {
        *dst++ = *src++;
    }
}

/* ================================================================
 * ShowErrorMenu  (SWOS2.s line 90693)
 *
 * Sets error text and shows the continue menu for user acknowledgement.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ShowErrorMenu(uint8_t *errorTextStr)
{
    errorPromptsEnabled = 1;
    errorText = (uint32_t)errorTextStr;
    _ShowMenu(continueMenu);
}

/* ================================================================
 * DoContinueAbortMenu  (SWOS2.s line 90685)
 *
 * Shows a continue/abort prompt with customizable text.
 *
 * Asm register convention:
 *   in:  a0 = prompt header, a1 = "what is changing" text,
 *        a2 = continue text, a3 = abort text, d0 = default entry
 *   out: d0 = 0 (continue) or 1 (abort), Z flag reflects d0
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _DoContinueAbortMenu(uint8_t *header, uint8_t *changing,
                              uint8_t *contText, uint8_t *abortTextStr,
                              uint16_t defaultEntry)
{
    promptHeader = (uint32_t)header;
    whatIsChanging = (uint32_t)changing;
    continueText = (uint32_t)contText;
    abortText = (uint32_t)abortTextStr;
    contAbortDefault = defaultEntry;

    _ShowMenu(continueAbortMenu);

    return contAbortResult;
}

/* ================================================================
 * ContinueInit  (SWOS2.s line 90741)
 *
 * Menu init callback for the Continue (error) menu.
 * Sets the error text string, and if prompts are disabled,
 * draws immediately and exits the menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ContinueInit(void)
{
    menuEntries[0].fg = errorText;

    if (errorPromptsEnabled == 0) {
        /* Gray out and auto-dismiss */
        /* move.w #7 writes to upper 16 bits of bg (big-endian) */
        menuEntries[0].bg = (7u << 16) | (menuEntries[0].bg & 0xFFFFu);
        menuEntries[1].isInvisible = 1;  /* hide continue button */
        _DrawMenu();
        _SetExitMenuFlag();
    }
}

/* ================================================================
 * ContinueMenuContinueSelected  (SWOS2.s line 90757)
 *
 * Menu callback for the Continue button in the continue menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ContinueMenuContinueSelected(void)
{
    _SetExitMenuFlag();
}

/* ================================================================
 * ContinueAbortInit  (SWOS2.s line 90853)
 *
 * Menu init callback for the Continue/Abort menu.
 * Sets up the four entries: header, what's changing, continue, abort.
 * Adjusts header width if needed, colors abort button red, and
 * optionally sets default entry.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ContinueAbortInit(void)
{
    extern uint8_t smallCharsTable[];

    if (promptHeader == 0) {
        /* No header text: hide entry 0 */
        menuEntries[0].isInvisible = 1;
    } else {
        /* Compute text width and adjust entry 0 width */
        uint16_t width = _GetTextWidth((uint8_t *)promptHeader, smallCharsTable);
        width += 20;
        if (width <= 120)
            width = 120;

        menuEntries[0].dwWidth = width;
        menuEntries[0].x -= width >> 1;
        menuEntries[0].fg = promptHeader;
    }

    /* Set text pointers for entries 1-3 */
    menuEntries[1].fg = whatIsChanging;
    menuEntries[2].fg = continueText;
    menuEntries[3].fg = abortText;

    if (abortText == 0) {
        /* No abort text: hide abort button */
        menuEntries[3].isInvisible = 1;
        return;
    }

    /* If abort text is the standard "ABORT" string, color it red */
    if (abortText == (uint32_t)aAbort) {
        /* move.w reads/writes upper 16 bits of bg (big-endian) */
        uint16_t bgColor = (uint16_t)(menuEntries[3].bg >> 16);
        bgColor = (bgColor & 0xFFF0) + 10; /* RED_10 */
        menuEntries[3].bg = ((uint32_t)bgColor << 16) | (menuEntries[3].bg & 0xFFFFu);
    }

    /* Set default entry if requested */
    if (contAbortDefault != 0) {
        _SetCurrentEntry(3);
    }
}

/* ================================================================
 * ContinueSelected  (SWOS2.s line 90900)
 *
 * Menu callback: continue was selected.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ContinueSelected(void)
{
    contAbortResult = 0;
    _SetExitMenuFlag();
}

/* ================================================================
 * AbortSelected  (SWOS2.s line 90906)
 *
 * Menu callback: abort was selected.
 * ================================================================ */
__attribute__((used, externally_visible))
void _AbortSelected(void)
{
    contAbortResult = 1;
    _SetExitMenuFlag();
}

/* ---- Stadium Menu externs (Chunk 89) ---- */
extern TeamGame topTeamInGame;
extern TeamGame bottomTeamInGame;
extern uint8_t gameName[];
extern uint8_t gameRound[];
extern uint16_t hilStarted;
extern uint16_t topTeamCoachNo;
extern uint16_t bottomTeamCoachNo;
extern uint16_t topTeamPlayerNo;
extern uint16_t bottomTeamPlayerNo;
extern uint16_t trainingGame;
extern uint16_t gameMaxSubstitutes;
extern uint8_t aGrafsMenubg2Ra[];     /* "grafs/menubg2.raw" */
extern uint16_t stadiumMenuLittlePlayer;
extern uint16_t littlePlayersColorTable[];
extern uint16_t littlePlayersSpriteTable[];

/* Color constants */
#define BRIGHT_BLUE_TO_BLUE_13  13
#define BLUE_TO_PURPLE_11       11
#define RED_10                  10

#define SPR_LITTLE_PLAYER_GOALIE  213
#define SHIRT_VERTICAL_STRIPES    2

/* ================================================================
 * ShowStadiumInit  (SWOS2.s line 89572)
 *
 * Initializes the stadium menu: loads background, sets team names
 * with appropriate colors, fills player name pointers for both
 * teams, hides unused bench slots, then draws and exits.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ShowStadiumInit(void)
{
    _LoadOverlayToFrontbuffer(aGrafsMenubg2Ra);

    /* Entry 0: game name */
    menuEntries[0].fg = (uint32_t)gameName;

    if (gameRound[0] == 0) {
        /* No round info: hide entry 1, nudge entry 0 down */
        menuEntries[1].isInvisible = 1;
        menuEntries[0].y += 5;
    } else {
        /* Show round info in entry 1 */
        menuEntries[1].fg = (uint32_t)gameRound;
    }

    /* Entry 2: top team name + trailing space */
    {
        uint8_t *dst = (uint8_t *)menuEntries[2].fg;
        uint8_t *end = _StringCopy((uint8_t *)topTeamInGame.teamName, dst);
        *end++ = ' ';
        *end = 0;
    }

    /* Color top team name if not in highlights mode */
    if (hilStarted == 0) {
        uint16_t color = BRIGHT_BLUE_TO_BLUE_13;
        if (topTeamCoachNo != 0) {
            /* coached */
        } else if (topTeamPlayerNo != 0) {
            color = BLUE_TO_PURPLE_11;
        } else if (trainingGame != 0) {
            color = BRIGHT_BLUE_TO_BLUE_13;
        } else {
            color = RED_10;
        }
        *(uint16_t *)&menuEntries[2].bg = color;
    }

    /* Entry 4: leading space + bottom team name */
    {
        uint8_t *dst = (uint8_t *)menuEntries[4].fg;
        *dst++ = ' ';
        _StringCopy((uint8_t *)bottomTeamInGame.teamName, dst);
    }

    /* Color bottom team name if not in highlights mode */
    if (hilStarted == 0) {
        uint16_t color = BRIGHT_BLUE_TO_BLUE_13;
        if (bottomTeamCoachNo != 0) {
            /* coached */
        } else if (bottomTeamPlayerNo != 0) {
            color = BLUE_TO_PURPLE_11;
        } else if (trainingGame != 0) {
            color = BRIGHT_BLUE_TO_BLUE_13;
        } else {
            color = RED_10;
        }
        *(uint16_t *)&menuEntries[4].bg = color;
    }

    /* Entries 16-31: top team player names */
    {
        int16_t i;
        for (i = 0; i < 16; i++) {
            menuEntries[16 + i].fg = (uint32_t)topTeamInGame.players[i].fullName;
        }
    }

    /* Entries 43-58: bottom team player names */
    {
        int16_t i;
        for (i = 0; i < 16; i++) {
            menuEntries[43 + i].fg = (uint32_t)bottomTeamInGame.players[i].fullName;
        }
    }

    /* Hide excess bench entries based on gameMaxSubstitutes */
    {
        int16_t entriesToHide = 5 - (int16_t)gameMaxSubstitutes;
        if (entriesToHide > 0) {
            int16_t j;
            for (j = 0; j < entriesToHide; j++) {
                menuEntries[31 - j].isInvisible = 1;
                menuEntries[58 - j].isInvisible = 1;
            }
        }
    }

    _DrawMenu();
    _SetExitMenuFlag();
}

/* ================================================================
 * StadiumPlayerSpritesBeforeDraw  (SWOS2.s line 89693)
 *
 * Sets up the little player sprite and color table for a given
 * player in the stadium menu. Handles goalie vs outfield differences
 * in sprite selection and shirt color assignment. Fills skin color.
 *
 * in: team = TeamGame pointer, playerIndex = 0-based player index
 * ================================================================ */
__attribute__((used, externally_visible))
void _StadiumPlayerSpritesBeforeDraw(TeamGame *team, uint16_t playerIndex)
{
    if (playerIndex == 0) {
        /* Goalie */
        stadiumMenuLittlePlayer = SPR_LITTLE_PLAYER_GOALIE;
        littlePlayersColorTable[11] = 11;
        littlePlayersColorTable[10] = 10;
        littlePlayersColorTable[14] = (uint16_t)team->prShortsCol;
        littlePlayersColorTable[15] = (uint16_t)team->prSocksCol;
    } else {
        /* Outfield player */
        stadiumMenuLittlePlayer = littlePlayersSpriteTable[team->prShirtType];
        uint16_t col1 = (uint16_t)team->prShirtCol;
        uint16_t col2 = (uint16_t)team->prStripesCol;
        if (team->prShirtType == SHIRT_VERTICAL_STRIPES) {
            uint16_t tmp = col1;
            col1 = col2;
            col2 = tmp;
        }
        littlePlayersColorTable[11] = col1;
        littlePlayersColorTable[10] = col2;
        littlePlayersColorTable[14] = (uint16_t)team->prShortsCol;
        littlePlayersColorTable[15] = (uint16_t)team->prSocksCol;
    }

    /* Set skin color from player's face2 field */
    int16_t skinColor = (int8_t)team->players[playerIndex].face2;
    _FillSkinColorConversionTable((uint16_t)skinColor, littlePlayersColorTable);
}

/* ================================================================
 * StadiumBeforeDrawLeftTeam  (SWOS2.s line 89674)
 *
 * Menu beforeDraw callback for left (top) team player entries.
 * Extracts player index from entry ordinal and delegates to
 * StadiumPlayerSpritesBeforeDraw with topTeamInGame.
 *
 * in: entry = MenuEntry* (a5 in asm)
 * ================================================================ */
__attribute__((used, externally_visible))
void _StadiumBeforeDrawLeftTeam(void *entryPtr)
{
    MenuEntry *entry = (MenuEntry *)entryPtr;
    uint16_t playerIndex = entry->ordinal - 5;
    _StadiumPlayerSpritesBeforeDraw(&topTeamInGame, playerIndex);
}

/* ================================================================
 * StadiumBeforeDrawRightTeam  (SWOS2.s line 89682)
 *
 * Menu beforeDraw callback for right (bottom) team player entries.
 * Extracts player index from entry ordinal and delegates to
 * StadiumPlayerSpritesBeforeDraw with bottomTeamInGame.
 *
 * in: entry = MenuEntry* (a5 in asm)
 * ================================================================ */
__attribute__((used, externally_visible))
void _StadiumBeforeDrawRightTeam(void *entryPtr)
{
    MenuEntry *entry = (MenuEntry *)entryPtr;
    uint16_t playerIndex = entry->ordinal - 32;
    _StadiumPlayerSpritesBeforeDraw(&bottomTeamInGame, playerIndex);
}

/* ================================================================
 * Squad menu callbacks (Chunk 111, SWOS2.s lines 56661-56711)
 * ================================================================ */

/* ---- External asm functions (not yet ported) ---- */
extern void PlayTrainingGame(void);
extern void SquadMenuSavePlayerPositions(void);
extern void SquadUpdateMenuState(void);

/* ---- External asm data ---- */
extern uint8_t squadMenu[];
extern uint16_t squadPrevSelectedPlayerIndex;
extern uint16_t isSquadViewMode;
extern uint32_t squadTeamFilePtr;

/* ================================================================
 * SelectTraining  (SWOS2.s line 56661)
 *
 * Starts training match, then re-activates squad menu at entry 9.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SelectTraining(void)
{
    PlayTrainingGame();
    _ActivateMenu(squadMenu);
    _SetCurrentEntry(9);
}

/* ================================================================
 * SelectGoals  (SWOS2.s line 56671)
 *
 * Switches squad view to goals/clubs mode. Saves positions,
 * sets view mode to 1, updates menu state, selects entry 11.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SelectGoals(void)
{
    squadPrevSelectedPlayerIndex = 0xFFFF;
    SquadMenuSavePlayerPositions();
    isSquadViewMode = 1;
    SquadUpdateMenuState();
    _SetCurrentEntry(11);
    _DrawMenu();
}

/* ================================================================
 * SquadOnSelect11  (SWOS2.s line 56682)
 *
 * Returns from goals/clubs view to normal squad view.
 * Clears view mode, updates menu state, selects entry 8.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SquadOnSelect11(void)
{
    isSquadViewMode = 0;
    SquadUpdateMenuState();
    _SetCurrentEntry(8);
    _DrawMenu();
}

/* ================================================================
 * SquadOnSelect45  (SWOS2.s line 56696)
 *
 * Rotates players left by 5 positions and redraws items 23, 44.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SquadOnSelect45(void)
{
    static const int16_t squad45Items[] = { 23, 44, -1 };
    _RotatePlayersLeft((uint8_t *)squadTeamFilePtr, 5);
    _DrawMenuItemsWaitForVSync(squad45Items);
}

/* ================================================================
 * Squad45AfterDraw  (SWOS2.s line 56709)
 *
 * Draws menu entry 44 (0x9A0 / 56 = 44).
 * ================================================================ */
__attribute__((used, externally_visible))
void _Squad45AfterDraw(void)
{
    _DrawMenuItem(&menuEntries[44]);
}

/* ================================================================
 * Squad operations (Chunk 112, SWOS2.s lines 57494-57562)
 * ================================================================ */

#define SPR_CHECK_BIG       206
#define SPR_NOT_CHECK_BIG   207

extern uint16_t oldPlayerSkill;
extern uint16_t newPlayerSkill;
extern uint8_t playerPositionsArray[];
extern uint8_t oldPlayerPositionsArray[];
extern uint16_t teamSkillDiffAfterChange;

/* ================================================================
 * UpdateBigCheckMark  (SWOS2.s line 57537)
 *
 * Updates the big check mark sprite on menu entry 1 (offset 0x38)
 * based on skill difference after formation change.
 * - If teamSkillDiffAfterChange >= 0: hide the check mark
 * - If old > new skill: show green check (SPR_CHECK_BIG)
 * - If old < new skill: show red X (SPR_NOT_CHECK_BIG)
 * - If equal: return (no change)
 * ================================================================ */
__attribute__((used, externally_visible))
void _UpdateBigCheckMark(void)
{
    MenuEntry *entry = &menuEntries[1]; /* menuEntries + 0x38 */
    uint8_t *base = (uint8_t *)entry;

    entry->isInvisible = 1;

    if ((int16_t)teamSkillDiffAfterChange >= 0)
        return;

    uint16_t flags = entry->stringFlags;
    flags &= ~(1 << 13); /* bclr #13 */
    entry->stringFlags = flags;

    if (oldPlayerSkill > newPlayerSkill) {
        /* Skill decreased: show check mark */
        *(uint16_t *)(base + 38) = SPR_CHECK_BIG;
    } else if (oldPlayerSkill < newPlayerSkill) {
        /* Skill increased: show not-check mark */
        *(uint16_t *)(base + 38) = SPR_NOT_CHECK_BIG;
    } else {
        /* Equal: no mark shown */
        return;
    }

    flags = entry->stringFlags;
    flags |= (1 << 13); /* bset #13 */
    entry->stringFlags = flags;
    entry->isInvisible = 0;
}

/* ================================================================
 * SquadRedrawPlayerListAndCheckMark  (SWOS2.s line 57496)
 *
 * Redraws entries 28..38 (0x620/56=28, count=11) and entry 1
 * (0x38/56=1).
 * ================================================================ */
__attribute__((used, externally_visible))
void _SquadRedrawPlayerListAndCheckMark(void)
{
    _CallDrawMultipleItems(11, &menuEntries[28]);
    _CallDrawMenuItem(&menuEntries[1]);
}

/* ================================================================
 * SquadMenuSavePlayerPositions  (SWOS2.s line 57506)
 *
 * Copies newPlayerSkill → oldPlayerSkill,
 * copies 32 bytes from playerPositionsArray → oldPlayerPositionsArray,
 * then calls UpdateBigCheckMark.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SquadMenuSavePlayerPositions(void)
{
    volatile uint8_t *dst = (volatile uint8_t *)oldPlayerPositionsArray;
    volatile uint8_t *src = (volatile uint8_t *)playerPositionsArray;
    int16_t i;
    newPlayerSkill = oldPlayerSkill;
    for (i = 0; i < 32; i++)
        dst[i] = src[i];
    _UpdateBigCheckMark();
}

/* ================================================================
 * SquadCommitPositionsAndUpdateSkill  (SWOS2.s line 57521)
 *
 * Same copy as SquadMenuSavePlayerPositions, then marks team file
 * byte 0x3B as 1, calls ComputeOldPlayerSkillForViewResult(team, 5),
 * and falls through to UpdateBigCheckMark.
 * ================================================================ */
extern uint16_t _ComputeOldPlayerSkillForViewResult(uint8_t *team, uint16_t goalsCount);

__attribute__((used, externally_visible))
void _SquadCommitPositionsAndUpdateSkill(void)
{
    volatile uint8_t *dst = (volatile uint8_t *)oldPlayerPositionsArray;
    volatile uint8_t *src = (volatile uint8_t *)playerPositionsArray;
    int16_t i;
    newPlayerSkill = oldPlayerSkill;
    for (i = 0; i < 32; i++)
        dst[i] = src[i];

    uint8_t *teamFile = (uint8_t *)squadTeamFilePtr;
    teamFile[0x3B] = 1;

    _ComputeOldPlayerSkillForViewResult(teamFile, 5);
    _UpdateBigCheckMark();
}

/* ================================================================
 * Options menu callbacks (Chunk 113, SWOS2.s lines 47471-47518)
 * ================================================================ */

extern uint16_t showPrereleaseCounter;

/* OptionsMenuBeforeDraw: clears prerelease counter on menu entry */
__attribute__((used, externally_visible))
void _OptionsMenuBeforeDraw(void)
{
    showPrereleaseCounter = 0;
}

/* ChangePitchType: cycle pitchType 0→1→2→3→4→5→6→0xFFFE→0→... */
__attribute__((used, externally_visible))
void _ChangePitchType(MenuEntry *entry)
{
    pitchType++;
    if (pitchType == 7)
        pitchType = 0xFFFE; /* RANDOM */
    _CallDrawMenuItem(entry);
}

/* ChangeGameLength: cycle gameLength 0→1→2→3→0→... */
__attribute__((used, externally_visible))
void _ChangeGameLength(MenuEntry *entry)
{
    gameLength++;
    if (gameLength == 4)
        gameLength = 0;
    _CallDrawMenuItem(entry);
}

/* ChangeAutoReplays: toggle autoReplays */
__attribute__((used, externally_visible))
void _ChangeAutoReplays(MenuEntry *entry)
{
    autoReplays ^= 1;
    _CallDrawMenuItem(entry);
}

/* ChangeAllPlTeamsEqual: toggle allPlayerTeamsEqual */
__attribute__((used, externally_visible))
void _ChangeAllPlTeamsEqual(MenuEntry *entry)
{
    allPlayerTeamsEqual ^= 1;
    _CallDrawMenuItem(entry);
}

/* ChangeAutoSaveHighlights: toggle autoSaveHighlights */
__attribute__((used, externally_visible))
void _ChangeAutoSaveHighlights(MenuEntry *entry)
{
    autoSaveHighlights ^= 1;
    _CallDrawMenuItem(entry);
}

/* ChangeMenuMusic: toggle menuMusic, restart music, redraw */
__attribute__((used, externally_visible))
void _ChangeMenuMusic(MenuEntry *entry)
{
    menuMusic ^= 1;
    _TryRestartingMusic();
    _CallDrawMenuItem(entry);
}

/* ================================================================
 * Simple menu exit/draw callbacks (Chunk 114)
 * ================================================================ */

extern uint16_t viewSquadSelectedPlayer;
extern uint16_t selectNationalTeamMenuResult;
extern uint16_t teamSelected;
extern uint8_t selectedFilename[];

/* CompetitionTopGoalScorersExit (line 54745): trivial exit */
__attribute__((used, externally_visible))
void _CompetitionTopGoalScorersExit(void)
{
    _SetExitMenuFlag();
}

/* TopGoalScorersExit (line 55243): trivial exit */
__attribute__((used, externally_visible))
void _TopGoalScorersExit(void)
{
    _SetExitMenuFlag();
}

/* BuyPlayerExit (line 75812): trivial exit */
__attribute__((used, externally_visible))
void _BuyPlayerExit(void)
{
    _SetExitMenuFlag();
}

/* ViewSquadExitContinue (line 73991): set no-selection, exit */
__attribute__((used, externally_visible))
void _ViewSquadExitContinue(void)
{
    viewSquadSelectedPlayer = 0xFFFF;
    _SetExitMenuFlag();
}

/* ViewSquad21AfterDraw (line 74007): draw entry 20 (0x460/56=20) */
__attribute__((used, externally_visible))
void _ViewSquad21AfterDraw(void)
{
    _DrawMenuItem(&menuEntries[20]);
}

/* SelectNationalTeamExit (line 78585): set no-selection, exit */
__attribute__((used, externally_visible))
void _SelectNationalTeamExit(void)
{
    selectNationalTeamMenuResult = 0xFFFF;
    _SetExitMenuFlag();
}

/* SelectNationalTeam25AfterDraw (line 78612): draw entry 24 (0x540/56=24) */
__attribute__((used, externally_visible))
void _SelectNationalTeam25AfterDraw(void)
{
    _DrawMenuItem(&menuEntries[24]);
}

/* SelTeamForTacticsExit (line 63673): set no-team, exit */
__attribute__((used, externally_visible))
void _SelTeamForTacticsExit(void)
{
    teamSelected = 0xFFFF;
    _SetExitMenuFlag();
}

/* AbortSaveFile (line 68003): clear filename, exit */
__attribute__((used, externally_visible))
void _AbortSaveFile(void)
{
    selectedFilename[0] = 0;
    _SetExitMenuFlag();
}

/* ================================================================
 * CareerOver + ViewSquad + National team callbacks (Chunk 115)
 * ================================================================ */

extern uint16_t currentSeasonShowing;
extern uint16_t numNewJobOffers;
extern uint16_t numJobOffers;
extern uint16_t youreSacked;
extern uint32_t squadTeamPtr;
extern uint8_t careerTeam[];

/* CareerOverInit (line 58154): set default menu entry based on career state */
__attribute__((used, externally_visible))
void _CareerOverInit(void)
{
    if (currentSeasonShowing == 0x13) {
        _SetCurrentEntry(6);
        return;
    }
    if (numNewJobOffers != 0 || numJobOffers != 0) {
        _SetCurrentEntry(5);
        return;
    }
    if (youreSacked != 0) {
        _SetCurrentEntry(8);
    }
}

/* ViewSquadOnSelect21 (line 73995): rotate players and redraw entry 20 */
__attribute__((used, externally_visible))
void _ViewSquadOnSelect21(void)
{
    _RotatePlayersLeft((uint8_t *)squadTeamPtr, 5);
    _CallDrawMenuItem(&menuEntries[20]);
}

/* SelectNationalTeamOnSelect25 (line 78600): rotate career team and redraw entry 24 */
__attribute__((used, externally_visible))
void _SelectNationalTeamOnSelect25(void)
{
    _RotatePlayersLeft(careerTeam, 5);
    _CallDrawMenuItem(&menuEntries[24]);
}

/* ================================================================
 * Chunk 116: Menu Navigation Callbacks
 * ================================================================ */

/* CommonMenuExit is now ported as _CommonMenuExit below */
extern void Career_FillCurrentSeasonInfo(void);  /* still in asm */
extern uint8_t managementRecordMenu[];
extern uint8_t viewCompetitionsMenu[];

/* SWOSMainMenuInit — calls CommonMenuExit on main menu init (SWOS2.s line 68835) */
__attribute__((used, externally_visible))
void _SWOSMainMenuInit(void)
{
    _CommonMenuExit();
}

/* ManagementRecordMenu — show management record menu (SWOS2.s line 59220) */
__attribute__((used, externally_visible))
void _ManagementRecordMenu(void)
{
    if (youreSacked == 0) {
        Career_FillCurrentSeasonInfo();
    }
    _ShowMenu(managementRecordMenu);
}

/* ViewCompetitionsMenuRetired — show view competitions menu (SWOS2.s line 58291) */
__attribute__((used, externally_visible))
void _ViewCompetitionsMenuRetired(void)
{
    _ShowMenu(viewCompetitionsMenu);
}

/* ================================================================
 * Chunk 118: CommonMenuExit
 * ================================================================ */

#define GAME_TYPE_DIY_COMPETITION    1
#define GAME_TYPE_PRESET_COMPETITION 2
#define GAME_TYPE_SEASON             3
#define GAME_TYPE_CAREER             4

extern uint16_t competitionOver;
extern uint8_t diyPresetNameOfCompetition[];
extern uint8_t aContinueA[];  /* "CONTINUE %a" */
extern uint8_t aReplayA[];    /* "REPLAY %a" */
extern uint8_t aSaveA[];      /* "SAVE %a" */
extern uint8_t aSeason_1[];   /* "SEASON" */
extern uint8_t aCareer_1[];   /* "CAREER" */
extern uint8_t saveFileTitle[];

extern void _PrintFormatted(const uint8_t *fmt, uint8_t *out,
                     uint16_t n0, uint16_t n1, uint16_t n2, uint16_t n3,
                     uint16_t n4, uint16_t n5, uint16_t n6,
                     const uint8_t *strA, const uint8_t *strB, const uint8_t *strC);

/* CommonMenuExit — set CONTINUE/REPLAY/SAVE entries on main menu return (SWOS2.s line 68793) */
__attribute__((used, externally_visible))
void _CommonMenuExit(void)
{
    uint8_t *nameStr;
    uint8_t *fmtStr;

    if (gameType == GAME_TYPE_DIY_COMPETITION) {
        nameStr = diyPresetNameOfCompetition;
        if (competitionOver == 1)
            fmtStr = aContinueA;
        else
            fmtStr = aReplayA;
    } else if (gameType == GAME_TYPE_PRESET_COMPETITION) {
        nameStr = diyPresetNameOfCompetition;
        if (competitionOver == 1)
            fmtStr = aContinueA;
        else
            fmtStr = aReplayA;
    } else if (gameType == GAME_TYPE_SEASON) {
        nameStr = aSeason_1;
        fmtStr = aContinueA;
    } else if (gameType == GAME_TYPE_CAREER) {
        nameStr = aCareer_1;
        fmtStr = aContinueA;
    } else {
        /* No active competition — hide CONTINUE and SAVE entries */
        menuEntries[12].isInvisible = 1;
        menuEntries[11].isInvisible = 1;
        /* Copy y from entry 11 to entry 13 */
        menuEntries[13].y = menuEntries[11].y;
        return;
    }

    /* Format "CONTINUE %a" or "REPLAY %a" into entry 11's string */
    uint8_t *dst = (uint8_t *)menuEntries[11].fg;
    _PrintFormatted(fmtStr, dst, 0, 0, 0, 0, 0, 0, 0, nameStr, 0, 0);

    /* Format "SAVE %a" into saveFileTitle */
    _PrintFormatted(aSaveA, saveFileTitle, 0, 0, 0, 0, 0, 0, 0, nameStr, 0, 0);

    /* Show both entries */
    menuEntries[12].isInvisible = 0;
    menuEntries[11].isInvisible = 0;
}

/* ================================================================
 * Chunk 119: Menu callbacks + ShowFriendlyMenu + ShowSelectRepresentationMenu
 * ================================================================ */

/* RANDOM/SEASONAL constants for ShowFriendlyMenu */
#define RANDOM_PITCH     0xFFFF
#define SEASON_SEP       7

extern uint8_t seed;              /* adjacent to randXorIndex — word write seeds both */
extern uint8_t randXorIndex;      /* adjacent byte after seed */
extern uint16_t diySelected;
extern uint8_t diyCompetitionMenu[];
extern uint8_t highlightsMenu[];
extern uint8_t optionsMenu[];
extern uint8_t diskMenu[];
extern uint8_t friendlyGameMenu[];
extern uint16_t pitchTypeOrSeason;
extern uint16_t friendlyPitchType;
extern uint16_t friendlySeason;
extern uint16_t minSubstitutes;
extern uint16_t maxSubstitutes;
extern uint16_t friendlyOk;
extern uint16_t selectNationalTeamMenuResult;
extern uint8_t selectNationalTeamMenu[];

extern void Friendly(void);
extern void EditCustomTeam(void);
extern void EditTactics(void);
extern void CheckContinueAbortPrompt(void);   /* sets Z flag — but called via asm bridge */
extern void ShowDesignDIYLeagueMenu(void);
extern void ShowDesignDIYCupMenu(void);
extern void ShowDesignDIYTournamentMenu(void);

/* FriendlyMenu — seed RNG, run Friendly, then update main menu (SWOS2.s line 68796) */
__attribute__((used, externally_visible))
void _FriendlyMenu(void)
{
    /* move.w (currentTick),(seed) — seeds both seed (high byte) + randXorIndex (low byte) */
    seed = (uint8_t)(currentTick >> 8);
    randXorIndex = (uint8_t)(currentTick & 0xFF);
    Friendly();
    _CommonMenuExit();
}

/* DIYCompetitionMenu — seed RNG, prompt, show DIY menu, dispatch design (SWOS2.s line 68803)
 * Note: CheckContinueAbortPrompt returns result via Z flag — need asm bridge */

/* EditCustomTeamMenu — edit custom teams, then update main menu (SWOS2.s line 69279) */
__attribute__((used, externally_visible))
void _EditCustomTeamMenu(void)
{
    EditCustomTeam();
    _CommonMenuExit();
}

/* EditTacticsMenu — edit tactics, then update main menu (SWOS2.s line 69285) */
__attribute__((used, externally_visible))
void _EditTacticsMenu(void)
{
    EditTactics();
    _CommonMenuExit();
}

/* HighlightsMenu — show highlights menu, then update main menu (SWOS2.s line 69291) */
__attribute__((used, externally_visible))
void _HighlightsMenu(void)
{
    _ShowMenu(highlightsMenu);
    _CommonMenuExit();
}

/* OptionsMenuSelected — show options menu, then update main menu (SWOS2.s line 69298) */
__attribute__((used, externally_visible))
void _OptionsMenuSelected(void)
{
    _ShowMenu(optionsMenu);
    _CommonMenuExit();
}

/* SaveDiskFilingMenu — show disk filing menu, then update main menu (SWOS2.s line 69305) */
__attribute__((used, externally_visible))
void _SaveDiskFilingMenu(void)
{
    _ShowMenu(diskMenu);
    _CommonMenuExit();
}

/* ShowFriendlyMenu — init friendly defaults, show menu (SWOS2.s line 50946) */
__attribute__((used, externally_visible))
void _ShowFriendlyMenu(void)
{
    inFriendlyMenu = 1;
    pitchTypeOrSeason = 0;
    friendlyPitchType = RANDOM_PITCH;
    friendlySeason = SEASON_SEP;
    minSubstitutes = 2;
    maxSubstitutes = 5;
    friendlyOk = 0;
    _ShowMenu(friendlyGameMenu);
    inFriendlyMenu = 0;
}

/* ShowSelectRepresentationMenu — show national team selection, return result (SWOS2.s line 77663) */
__attribute__((used, externally_visible))
uint16_t _ShowSelectRepresentationMenu(void)
{
    _ShowMenu(selectNationalTeamMenu);
    return selectNationalTeamMenuResult;
}

/* SetSelectTwoTeamsText — format "SELECT 2 TEAMS" for friendly menu (SWOS2.s line 51415) */
extern uint8_t aSelect0Teams[];   /* "SELECT %0 TEAMS" */

__attribute__((used, externally_visible))
void _SetSelectTwoTeamsText(void)
{
    uint8_t *str = (uint8_t *)menuEntries[10].fg;
    _PrintFormatted(aSelect0Teams, str, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* ================================================================
 * Chunk 121: FriendlyOK, ViewSelectedTeams, ShowDomesticPlayerMarket,
 *            ContestMenuScrollUp, ContestMenuScrollDown
 * ================================================================ */

extern uint16_t viewSelectedTeams;
extern uint16_t chooseTeamsResult;
extern uint16_t changeTeams;
extern uint16_t abortSelectTeams;
extern uint16_t exitSelectTeams;

extern void ScrollUpInGameList(void);
extern void ScrollDownInGameList(void);
extern void BuildGameListArrayFromTeamSelectionKeyCache(void);
extern void _CallDrawMultipleItemsRect(uint16_t count, MenuEntry *firstEntry);

/* FriendlyOK — set friendly ok flag, activate friendly menu (SWOS2.s line 51240) */
__attribute__((used, externally_visible))
void _FriendlyOK(void)
{
    friendlyOk = 1;
    _ActivateMenu(friendlyGameMenu);
}

/* ViewSelectedTeams — set view flag, clear results, exit menu (SWOS2.s line 66094) */
__attribute__((used, externally_visible))
void _ViewSelectedTeams(void)
{
    viewSelectedTeams = 1;
    chooseTeamsResult = 0;
    changeTeams = 0;
    abortSelectTeams = 0;
    exitSelectTeams = 0;
    _SetExitMenuFlag();
}

/* ContestMenuScrollUp — scroll up, rebuild, redraw 15 entries (SWOS2.s line 60082) */
__attribute__((used, externally_visible))
void _ContestMenuScrollUp(void)
{
    ScrollUpInGameList();
    BuildGameListArrayFromTeamSelectionKeyCache();
    _CallDrawMultipleItemsRect(15, &menuEntries[18]);
}

/* ContestMenuScrollDown — scroll down, rebuild, redraw 15 entries (SWOS2.s line 60114) */
__attribute__((used, externally_visible))
void _ContestMenuScrollDown(void)
{
    ScrollDownInGameList();
    BuildGameListArrayFromTeamSelectionKeyCache();
    _CallDrawMultipleItemsRect(15, &menuEntries[18]);
}
