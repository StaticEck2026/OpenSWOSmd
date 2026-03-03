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
#include "chairman_memo.h"

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
    _ScrollUpInGameList();
    _BuildGameListArrayFromTeamSelectionKeyCache();
    _CallDrawMultipleItemsRect(15, &menuEntries[18]);
}

/* ContestMenuScrollDown — scroll down, rebuild, redraw 15 entries (SWOS2.s line 60114) */
__attribute__((used, externally_visible))
void _ContestMenuScrollDown(void)
{
    _ScrollDownInGameList();
    _BuildGameListArrayFromTeamSelectionKeyCache();
    _CallDrawMultipleItemsRect(15, &menuEntries[18]);
}

/* ================================================================
 * Chunk 122: Contest Menu Helpers
 * ================================================================ */

/* Color constants for menu entry background manipulation */
#define COLOR_ORANGE_TO_BROWN_4    4
#define COLOR_PINK_TO_BROWN_7      7
#define COLOR_BLUE_TO_BLACK_8      8
#define COLOR_PINK_TO_BROWN_9      9
#define COLOR_BLUE_TO_PURPLE_11   11

/* Chairman message IDs */
#define NO_MORE_TIME_TO_NEGOTIATE  3

/* ---- External globals (contest/career state) ---- */
extern uint16_t teamSelectionKeyCache_ListIndex;
extern uint16_t currentGame;
extern uint16_t teamSelectionKeyCache_ListCount;
extern uint16_t teamSelectionKeyCacheCount;
extern uint16_t teamSelectionKeyCacheIndex;
extern uint16_t teamSelectionKeyCacheEnd;
extern int16_t  savedContestListIndex;
extern int16_t  savedContestGameOffset;
extern uint32_t savedContestEntryPtr;
extern uint32_t savedContestRoundPtr;
extern uint16_t gameListY;
extern uint16_t contestEntryHeight;
extern uint32_t gameListArray[];       /* 14 × 2 uint32_t entries = 112 bytes */
extern uint16_t columnMarker;
extern uint16_t cglmVar3;
extern uint16_t seasonFinished;
extern uint16_t careerFinished;
extern uint16_t retired;
extern uint16_t clubBusinessActive;
extern uint16_t timeToNegotiate;
extern uint16_t teamNoOfferAccepted;
extern uint16_t jobOffersFromRetired;
extern uint16_t seasonNumberOfSquads;
extern uint16_t isNationalSelector;
extern uint16_t careerNumPlayers;
extern uint32_t irqCookieObfuscated;
extern uint8_t  managerNameBuffer[];
extern uint8_t  managerSurnameBuffer[];
extern uint8_t  seasonPlaying;
extern uint16_t selTeamNumber;
extern uint8_t  teamSelectionKeyCache[];
extern uint16_t numOffers;
extern uint16_t currentMessage;
extern uint16_t primaryCompSquadViewParam;
extern uint16_t seasonSquadTeamNumbers[];
extern uint32_t selectedContest;
extern uint16_t contestLeagueNumber;

/* String constants */
extern uint8_t aPlayMatch[];        /* "PLAY MATCH" */
extern uint8_t aFinishSeason[];     /* "FINISH SEASON" */
extern uint8_t aNextMatch[];        /* "NEXT MATCH" */
extern uint8_t aSquads[];           /* "SQUADS" */
extern uint8_t aNoOffers[];         /* "NO OFFERS" */
extern uint8_t aNewJob[];           /* "NEW JOB" */
extern uint8_t aNoJobOffers[];      /* "NO JOB OFFERS" */
extern uint8_t aSquadSelected[];    /* "SQUAD SELECTED" */
extern uint8_t aChangeSquad[];      /* "CHANGE SQUAD" */
extern uint8_t aSelectSquad[];      /* "SELECT SQUAD" */
extern uint8_t a01Season[];         /* "%0/%1 SEASON" */
extern uint8_t a001Season[];        /* "%0/0%1 SEASON" */
extern uint8_t aSquad[];            /* "SQUAD" */

/* Menu packed data (still in asm) */
extern uint8_t contestMenu[];
extern uint8_t careerOverMenu[];
extern uint8_t jobOffersMenu[];
extern uint8_t gotJobOfferMenu[];
extern uint8_t transferMarketMenu[];

/* Functions (asm or already ported) */
extern uint8_t *_TeamSelectionKeyCache_GetListPtr(uint16_t listIndex);
extern uint8_t *_TeamSelectionKeyCache_GetListElementPtr(uint16_t listIndex, uint16_t elementIndex);
extern uint8_t *_TeamSelectionKeyCache_GetCurrentListPtr(void);
extern uint8_t *_VerifyTeamLoaded(uint16_t teamNum);
extern uint16_t _GetStringLength(const uint8_t *str);
extern uint8_t *_StringCopy(const uint8_t *src, uint8_t *dst);
extern int16_t  _CheckForTransferOffers(void);
extern uint16_t _IsTournamentAllowingSquadChangesIfNotCareer(void);
extern void _DrawMultipleItems(uint16_t maxItems, MenuEntry *firstEntry);
extern void Career_PostMatchUpdate(void);        /* still in asm */
extern void _CCareer_ViewOrEditCompetitionSlot(uint8_t *comp, uint16_t sel); /* C bridge for asm */
extern void SeasonFinished_ComputeSeasonReview(void); /* still in asm */
extern void SelectAndShowSquadToView(void);      /* still in asm, requires a0/a1/d0/d1 */
extern void ChooseCompetitionMenu(void);         /* asm stub → _ChooseCompetitionMenu */

/* ScrollUpInGameList — decrement currentGame/listIndex with bounds (SWOS2.s line 60083) */
__attribute__((used, externally_visible))
void _ScrollUpInGameList(void)
{
    int16_t game = (int16_t)currentGame - 1;
    if (game >= 0) {
        currentGame = (uint16_t)game;
        return;
    }
    /* currentGame underflowed, try moving to previous list entry */
    int16_t idx = (int16_t)teamSelectionKeyCache_ListIndex - 1;
    if (idx < 0) {
        teamSelectionKeyCache_ListIndex = 0;
        currentGame = 0;
        return;
    }
    teamSelectionKeyCache_ListIndex = (uint16_t)idx;
    uint8_t *entry = _TeamSelectionKeyCache_GetListPtr((uint16_t)idx);
    int8_t numGames = *(int8_t *)(entry + 5);
    int16_t newGame;
    if (numGames == 0)
        newGame = 0;
    else
        newGame = numGames - 1;
    currentGame = (uint16_t)newGame;
}

/* ScrollDownInGameList — increment currentGame/listIndex with bounds (SWOS2.s line 60109) */
__attribute__((used, externally_visible))
void _ScrollDownInGameList(void)
{
    uint8_t *entry = _TeamSelectionKeyCache_GetListPtr(teamSelectionKeyCache_ListIndex);
    uint8_t numGames = *(uint8_t *)(entry + 5);
    if (numGames != 0) {
        uint16_t next = currentGame + 1;
        if ((uint8_t)next != numGames) {
            currentGame = next;
            return;
        }
    }
    /* Move to next list entry */
    uint16_t nextIdx = teamSelectionKeyCache_ListIndex + 1;
    if (nextIdx == teamSelectionKeyCache_ListCount)
        return;
    teamSelectionKeyCache_ListIndex = nextIdx;
    currentGame = 0;
}

/* RestoreContestScrollPosition — restore saved scroll pos (SWOS2.s line 60132) */
__attribute__((used, externally_visible))
void _RestoreContestScrollPosition(void)
{
    teamSelectionKeyCache_ListIndex = (uint16_t)savedContestListIndex;
    if (savedContestListIndex >= 0) {
        currentGame = (uint16_t)savedContestGameOffset;
    } else {
        teamSelectionKeyCache_ListIndex = teamSelectionKeyCacheIndex;
        currentGame = 0;
        _ScrollUpInGameList();
    }
    /* Scroll up 3 extra times to show context */
    _ScrollUpInGameList();
    _ScrollUpInGameList();
    _ScrollUpInGameList();
}

/* SelectActiveContestAndRound — find current active contest entry (SWOS2.s line 60149) */
__attribute__((used, externally_visible))
void _SelectActiveContestAndRound(void)
{
    if (gameType != GAME_TYPE_CAREER) {
        /* DIY/preset/season without squads */
        if (seasonNumberOfSquads == 0) {
            if (seasonFinished != 0)
                goto setInvalid;
            uint8_t *a0 = _TeamSelectionKeyCache_GetCurrentListPtr();
            savedContestListIndex = (int16_t)teamSelectionKeyCacheIndex;
            savedContestEntryPtr = (uint32_t)a0;
            savedContestGameOffset = 0;
            savedContestRoundPtr = (uint32_t)a0;
            return;
        }
    }
    /* Career mode (or season with squads) */
    if (careerFinished != 0)
        goto setInvalid;
    {
        uint8_t *a0 = _TeamSelectionKeyCache_GetCurrentListPtr();
        uint8_t *base = teamSelectionKeyCache;
        uint16_t d0 = teamSelectionKeyCacheIndex;
        for (;;) {
            uint8_t numGames = *(uint8_t *)(a0 + 5);
            uint8_t remaining = *(uint8_t *)(a0 + 6);
            if (numGames != 0 && remaining != 0) {
                savedContestListIndex = (int16_t)d0;
                savedContestEntryPtr = (uint32_t)a0;
                int16_t gameOff = (int16_t)numGames - (int16_t)remaining;
                savedContestGameOffset = gameOff;
                uint32_t byteOff = (uint16_t)gameOff * 12u;
                savedContestRoundPtr = (uint32_t)(a0 + byteOff);
                return;
            }
            d0++;
            a0 += *(uint16_t *)(a0 + 2); /* advance by entry size */
            if (d0 == teamSelectionKeyCacheCount)
                break;
        }
    }
setInvalid:
    savedContestListIndex = -1;
    savedContestEntryPtr = 0xFFFFFFFF;
}

/* CheckClubBusiness — toggle blinking on CLUB BUSINESS entry (SWOS2.s line 60196)
 * Note: asm used absolute address ($297E).w which was entry 6 in original binary.
 * We use menuEntries[6] instead. */
__attribute__((used, externally_visible))
void _CheckClubBusiness(void)
{
    if (gameType != GAME_TYPE_CAREER)
        return;
    MenuEntry *entry = &menuEntries[6];
    uint16_t flags = entry->stringFlags;
    if (clubBusinessActive != 0)
        flags |= (1 << 13);
    else
        flags &= ~(1 << 13);
    entry->stringFlags = flags;
}

/* CheckJobOffers — update JOB OFFERS entry state (SWOS2.s line 60214) */
__attribute__((used, externally_visible))
void _CheckJobOffers(void)
{
    if (gameType != GAME_TYPE_CAREER)
        return;
    MenuEntry *entry = &menuEntries[5]; /* JOB OFFERS entry */
    /* Reset background color */
    entry->bg = (entry->bg & 0xFFF0) + COLOR_BLUE_TO_PURPLE_11;
    entry->isDisabled = 0;
    uint16_t flags = entry->stringFlags;
    flags &= ~(1 << 13);

    if (teamNoOfferAccepted != 0xFFFF) {
        /* Got an accepted offer — show "NEW JOB" */
        entry->fg = (uint32_t)aNewJob;
        entry->stringFlags = flags;
        return;
    }
    if (numNewJobOffers != 0) {
        /* New offers available — set blinking */
        flags |= (1 << 13);
        entry->stringFlags = flags;
        return;
    }
    if (numJobOffers != 0) {
        /* Old offers still available */
        entry->stringFlags = flags;
        return;
    }
    /* No offers at all */
    entry->fg = (uint32_t)aNoJobOffers;
    entry->isDisabled = 1;
    entry->bg = (entry->bg & 0xFFF0) + COLOR_PINK_TO_BROWN_7;
    entry->stringFlags = flags;
}

/* ContestMenuOnReturn_SetButtonsAndDefaultSelection (SWOS2.s line 60254) */
__attribute__((used, externally_visible))
void _ContestMenuOnReturn_SetButtonsAndDefaultSelection(MenuEntry *a5)
{
    _ContestSetButtons();
    uint16_t ord = *(uint16_t *)((uint8_t *)a5 + 2); /* a5->ordinal at offset 2 */
    if (ord == 0x0D || ord == 0x0A) {
        if (seasonFinished != 0) {
            _SetCurrentEntry(7);
        }
    }
}

/* SeasonLowerButtons — reposition season lower buttons (SWOS2.s line 60346) */
__attribute__((used, externally_visible))
void _SeasonLowerButtons(void)
{
    MenuEntry *entry13 = &menuEntries[13];
    MenuEntry *entry14 = &menuEntries[14];
    MenuEntry *entry15 = &menuEntries[15];
    entry14->y = entry13->y;
    entry15->y = entry13->y + 13;
    menuEntries[13].isInvisible = 1;
}

/* CheckTransfers — update TRANSFERS entry state (SWOS2.s line 60358) */
__attribute__((used, externally_visible))
void _CheckTransfers(void)
{
    if (gameType != GAME_TYPE_CAREER)
        return;

    if (isNationalSelector != 0) {
        if (_IsTournamentAllowingSquadChangesIfNotCareer() != 0)
            goto disableEntry;
    } else {
        if (teamSelectionKeyCacheEnd != 0)
            goto disableEntry;
    }

    /* Enable transfers entry */
    {
        MenuEntry *entry = &menuEntries[4];
        entry->bg = (entry->bg & 0xFFF0) + COLOR_BLUE_TO_BLACK_8;
        entry->isDisabled = 0;
        if (timeToNegotiate != 0) {
            if (_CheckForTransferOffers() == 0) {
                /* Has pending offers — set blinking */
                uint16_t flags = entry->stringFlags;
                flags |= (1 << 13);
                entry->stringFlags = flags;
            }
        }
    }
    goto clearBlink;

disableEntry:
    {
        MenuEntry *entry = &menuEntries[4];
        entry->bg = (entry->bg & 0xFFF0) + COLOR_PINK_TO_BROWN_9;
        entry->isDisabled = 1;
    }
clearBlink:
    {
        MenuEntry *entry = &menuEntries[4];
        uint16_t flags = entry->stringFlags;
        flags &= ~(1 << 13);
        entry->stringFlags = flags;
    }
}

/* BuildGameListArrayFromTeamSelectionKeyCache (SWOS2.s line 60406) */
__attribute__((used, externally_visible))
void _BuildGameListArrayFromTeamSelectionKeyCache(void)
{
    uint8_t *elemPtr;
    /* Get list element pointer: a0=list entry, a1=element within it */
    /* The asm calls TeamSelectionKeyCache_GetListElementPtr which returns a0 (list) via a0, a1 via a1.
       In C, the function returns a1 but we also need a0. Use the list ptr function. */
    uint16_t listIdx = teamSelectionKeyCache_ListIndex;
    uint16_t gameIdx = currentGame;
    uint8_t *listEntry = _TeamSelectionKeyCache_GetListPtr(listIdx);
    uint8_t *gameEntry = listEntry + gameIdx * 12;

    uint32_t *out = gameListArray;
    uint16_t remaining = 14; /* 0xD + 1 = 14 entries (dbf counts from 13) */

    for (;;) {
        if (listIdx == teamSelectionKeyCache_ListCount) {
            /* Fill remaining slots with NULL */
            while (remaining > 0) {
                *out++ = 0;
                *out++ = 0;
                remaining--;
            }
            return;
        }
        for (;;) {
            *out++ = (uint32_t)listEntry;
            *out++ = (uint32_t)gameEntry;
            uint8_t numGames = *(uint8_t *)(listEntry + 5);
            if (numGames == 0)
                break;
            gameIdx++;
            if ((uint8_t)gameIdx == numGames)
                break;
            gameEntry += 12;
            remaining--;
            if (remaining == 0)
                return;
        }
        /* Move to next list entry */
        listIdx++;
        gameIdx = 0;
        listEntry += *(uint16_t *)(listEntry + 2);
        gameEntry = listEntry;
        remaining--;
        if (remaining == 0)
            return;
    }
}

/* PrintNameAndSeason — show manager name, team, and season year (SWOS2.s line 60445) */
__attribute__((used, externally_visible))
void _PrintNameAndSeason(void)
{
    if (gameType != GAME_TYPE_CAREER) {
        menuEntries[0].isInvisible = 1;
        /* 60/56 = entry offset 60 relative to entry 0 = entry1.isInvisible (offset 4 of entry 1 = byte 60 from start) */
        /* Actually: offset 60 from menuEntries[0] = menuEntries[1].isInvisible (entry 1, byte 56+4=60) */
        menuEntries[1].isInvisible = 1;
        /* offset 116 from menuEntries[0] = menuEntries[2].isInvisible (entry 2, byte 112+4=116) */
        menuEntries[2].isInvisible = 1;
        return;
    }
    menuEntries[0].isInvisible = 0;
    menuEntries[1].isInvisible = 0;
    menuEntries[2].isInvisible = 0;
    menuEntries[3].isInvisible = 1;  /* $A8 / 56 = 3, offset from base $A8 = 168 = 3*56=168, field offset 4 → isInvisible */

    /* Build manager name string into entry 0's string buffer */
    uint8_t *dst = (uint8_t *)menuEntries[0].fg;
    uint16_t nameLen = _GetStringLength(managerNameBuffer);
    uint16_t surnameLen = _GetStringLength(managerSurnameBuffer);
    uint16_t totalLen = nameLen + surnameLen;
    uint8_t *src = managerNameBuffer;

    if (totalLen > 12) {
        /* Name too long — abbreviate first name to initial */
        if (src[0] != 0) {
            *dst++ = src[0];
            *dst++ = ' ';
        }
    } else {
        dst = _StringCopy(src, dst);
        *dst++ = ' ';
    }
    _StringCopy(managerSurnameBuffer, dst);

    /* Set team name in entry 1 */
    uint8_t *teamData = _VerifyTeamLoaded(selTeamNumber);
    uint8_t *teamName = teamData + 5;
    uint8_t *entry1Str = (uint8_t *)menuEntries[1].fg;
    _StringCopy(teamName, entry1Str);

    /* Set season year in entry 2 */
    uint8_t *entry2Str = (uint8_t *)menuEntries[2].fg;
    uint16_t year = (uint16_t)(seasonPlaying & 0xFF);
    uint16_t d1 = year;
    year += 1900;
    if (year < 1996)
        year += 100;
    d1 += 1;
    if (d1 >= 100)
        d1 -= 100;
    uint8_t *fmt;
    if (d1 >= 10)
        fmt = a01Season;    /* "%0/%1 SEASON" */
    else
        fmt = a001Season;   /* "%0/0%1 SEASON" */
    _PrintFormatted(fmt, entry2Str, year, d1, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* SetNationalSelectorButtons — configure national team selector buttons (SWOS2.s line 60517) */
__attribute__((used, externally_visible))
void _SetNationalSelectorButtons(void)
{
    if (gameType != GAME_TYPE_CAREER)
        return;
    if (isNationalSelector == 0)
        return;

    /* Set transfers/squad entry text */
    MenuEntry *transfersEntry = &menuEntries[4]; /* TRANSFERS */
    transfersEntry->fg = (uint32_t)aSquadSelected;

    if (_IsTournamentAllowingSquadChangesIfNotCareer() == 0) {
        transfersEntry->fg = (uint32_t)aChangeSquad;
        if (careerNumPlayers != 20)
            transfersEntry->fg = (uint32_t)aSelectSquad;
    }

    /* Entry 12 (national selector actions) */
    MenuEntry *entry12 = &menuEntries[12];
    entry12->bg = (entry12->bg & 0xFFF0) + COLOR_ORANGE_TO_BROWN_4;
    entry12->isDisabled = 0;

    if (_IsTournamentAllowingSquadChangesIfNotCareer() != 0) {
        entry12->bg = (entry12->bg & 0xFFF0) + COLOR_PINK_TO_BROWN_9;
        entry12->isDisabled = 1;
    }

    /* Hide entry 6 (CLUB BUSINESS) for national selector */
    menuEntries[6].isInvisible = 1;

    if (careerNumPlayers == 20)
        return;

    /* Not enough players — disable most entries */
    MenuEntry *entry8 = &menuEntries[8];  /* SQUAD */
    entry8->bg = (entry8->bg & 0xFFF0) + COLOR_PINK_TO_BROWN_9;
    entry8->isDisabled = 1;

    MenuEntry *entry7 = &menuEntries[7];  /* PLAY MATCH */
    entry7->bg = (entry7->bg & 0xFFF0) + COLOR_PINK_TO_BROWN_9;
    entry7->isDisabled = 1;

    MenuEntry *entry10 = &menuEntries[10]; /* NEXT MATCH */
    entry10->bg = (entry10->bg & 0xFFF0) + COLOR_PINK_TO_BROWN_9;
    entry10->isDisabled = 1;

    MenuEntry *entry11 = &menuEntries[11]; /* VIEW COMPETITIONS */
    entry11->bg = (entry11->bg & 0xFFF0) + COLOR_PINK_TO_BROWN_9;
    entry11->isDisabled = 1;

    entry12->bg = (entry12->bg & 0xFFF0) + COLOR_PINK_TO_BROWN_9;
    entry12->isDisabled = 1;
}

/* ContestSetButtons — set play/finish/next match button states (SWOS2.s line 60270) */
__attribute__((used, externally_visible))
void _ContestSetButtons(void)
{
    MenuEntry *entry7 = &menuEntries[7]; /* PLAY MATCH */
    entry7->fg = (uint32_t)aPlayMatch;
    entry7->stringFlags &= ~(1 << 13); /* clear blinking */

    if (gameType == GAME_TYPE_SEASON) {
        /* Season mode */
        MenuEntry *entry13 = &menuEntries[13];
        entry13->bg = (entry13->bg & 0xFFF0) + COLOR_ORANGE_TO_BROWN_4;
        entry13->isDisabled = 0;
        if (seasonNumberOfSquads == 0) {
            entry7->fg = (uint32_t)aNextMatch;
            _SeasonLowerButtons();
            if (seasonFinished != 0)
                goto setFinishSeason;
            return;
        }
        if (seasonFinished != 0)
            goto lowerAndFinish;
        if (careerFinished != 0)
            goto setFinishSeason;
        return;
    }

    /* Career mode */
    if (seasonFinished != 0)
        goto careerFinished_label;

    /* Season not finished */
    menuEntries[10].isInvisible = 0;
    {
        MenuEntry *entry10 = &menuEntries[10];
        entry10->bg = (entry10->bg & 0xFFF0) + COLOR_ORANGE_TO_BROWN_4;
    }
    if (careerFinished != 0) {
        entry7->fg = (uint32_t)aFinishSeason;
        return;
    }
    /* Check if match available — set blinking if so */
    if (isNationalSelector != 0)
        return;
    if (teamSelectionKeyCacheEnd == 0)
        return;
    entry7->stringFlags |= (1 << 13);
    return;

careerFinished_label:
    entry7->fg = (uint32_t)aFinishSeason;
    {
        MenuEntry *entry10 = &menuEntries[10];
        entry10->bg = (entry10->bg & 0xFFF0) + COLOR_PINK_TO_BROWN_9;
        entry10->isDisabled = 1;
    }
    return;

lowerAndFinish:
    _SeasonLowerButtons();
setFinishSeason:
    entry7->fg = (uint32_t)aFinishSeason;
}

/* ContestMenuInit — init callback for contest menu (SWOS2.s line 59786) */
__attribute__((used, externally_visible))
void _ContestMenuInit(void)
{
    /* Tamper check: irqCookieObfuscated XOR 0xCFD1F861 must equal 0x1786 */
    uint32_t cookie = irqCookieObfuscated ^ 0xCFD1F861;
    if (cookie != 0x1786)
        return; /* tamper detected — skip init (menu will be broken) */

    if (retired != 0) {
        _ActivateMenu(careerOverMenu);
        return;
    }

    _ContestSetButtons();
    _ContestMenuOnReturn();

    if (gameType != GAME_TYPE_CAREER)
        return;
    if (isNationalSelector == 0)
        return;
    if (careerNumPlayers == 0x14)
        return;
    _SetCurrentEntry(4);
}

/* ContestMenuOnReturn — after-draw callback for contest menu (SWOS2.s line 59811) */
__attribute__((used, externally_visible))
void _ContestMenuOnReturn(void)
{
    if (gameType == GAME_TYPE_SEASON) {
        /* Season mode: hide career-only entries, show season-only entries */
        menuEntries[10].isInvisible = 1;  /* NEXT MATCH (career) */
        menuEntries[13].isInvisible = 0;  /* VIEW COMPETITIONS (season) */
        menuEntries[11].isInvisible = 1;  /* VIEW COMPETITIONS (career) */
        menuEntries[14].isInvisible = 0;  /* EXIT (season) */
        menuEntries[9].isInvisible = 1;   /* EXIT (career) */
        menuEntries[15].isInvisible = 0;  /* SCROLL arrows (season) */

        /* Adjust entry heights */
        menuEntries[7].dwHeight -= 1;
        menuEntries[8].dwHeight -= 1;

        /* If only 1 squad, change "SQUAD" text to "SQUADS" */
        if (seasonNumberOfSquads == 1) {
            menuEntries[8].fg = (uint32_t)aSquads;
        }

        /* Hide career-specific entries */
        menuEntries[4].isInvisible = 1;   /* TRANSFERS */
        menuEntries[12].isInvisible = 1;  /* national selector */
        menuEntries[6].isInvisible = 1;   /* CLUB BUSINESS */
        menuEntries[5].isInvisible = 1;   /* JOB OFFERS */
        /* fall through to career setup (shared code for game list) */
    } else if (gameType != GAME_TYPE_CAREER) {
        return;
    }

    /* Career/Season common: set up game list area */
    MenuEntry *entry18 = &menuEntries[18];
    gameListY = entry18->y + 2;
    contestEntryHeight = entry18->dwHeight - 4;

    _PrintNameAndSeason();
    /* ContestMenuOnReturn_NoOp() — no-op, skip */

    teamSelectionKeyCache_ListIndex = 0;
    currentGame = 0;
    teamSelectionKeyCache_ListCount = teamSelectionKeyCacheCount;

    _SelectActiveContestAndRound();
    _RestoreContestScrollPosition();
    _BuildGameListArrayFromTeamSelectionKeyCache();
    _ContestSetButtons();
    _CheckTransfers();
    _CheckClubBusiness();
    _CheckJobOffers();
    _SetNationalSelectorButtons();
}

/* ==========================================================================
 * Chunk 123 — Contest Menu Action Handlers
 * (PlayMatchMenu, NextMatch, ViewCompetitionsMenu, ClubBusinessMenu,
 *  JobOffersMenu, SeasonFinished, TransfersMenu, ViewWorldMenu,
 *  jmp_SetExitMenuFlag)
 * ========================================================================== */

extern void _CareerPlayMatch(void);
extern void _Career_PlayMatchMenuLoop(void);
extern void _UpdateCareerAfterMatch(void);
extern void _Career_NextMatch(void);
extern void _MaybeSackManagerAfterSeasonReview(void);
extern void _Career_HandleRetirement(void);
extern void _ShowClubBussinesMenu(void);

/* Forward declarations for functions in this chunk */
void _NextMatch(MenuEntry *callerEntry);
void _SeasonFinished(void);

/* PlayMatchMenu — contest PLAY MATCH action (SWOS2.s line 59796)
 * Called as menu onSelect callback; a5 = caller's entry.
 * Asm bridge passes a5 as parameter. */
__attribute__((used, externally_visible))
void _PlayMatchMenu(MenuEntry *callerEntry)
{
    if (seasonFinished != 0) {
        _SeasonFinished();
        return;
    }

    if (gameType != GAME_TYPE_CAREER) {
        if (seasonNumberOfSquads == 0) {
            _NextMatch(callerEntry);
            return;
        }
    }

    if (careerFinished != 0) {
        _CareerPlayMatch();
        _SeasonFinished();
        return;
    }

    _Career_PlayMatchMenuLoop();

    if (gameType != GAME_TYPE_SEASON) {
        _UpdateCareerAfterMatch();
        if (youreSacked != 0) {
            Career_PostMatchUpdate();
            _ActivateMenu(contestMenu);
            return;
        }
        _CheckClubBusiness();
    }

    /* loc_13BB64 */
    _TeamSelectionKeyCache_GetCurrentListPtr();
    _SelectActiveContestAndRound();
    _ContestMenuOnReturn_SetButtonsAndDefaultSelection(callerEntry);
    _RestoreContestScrollPosition();
    _BuildGameListArrayFromTeamSelectionKeyCache();
    _CheckTransfers();
    /* ContestMenuOnReturn_NoOp() — no-op, skip */

    if (careerFinished != 0 || seasonFinished != 0) {
        fireResetFlag = 1;
    }
}

/* NextMatch — NEXT MATCH action for season mode (SWOS2.s line 59845)
 * Asm bridge passes a5 as parameter. */
__attribute__((used, externally_visible))
void _NextMatch(MenuEntry *callerEntry)
{
    _Career_NextMatch();

    if (gameType != GAME_TYPE_SEASON) {
        _UpdateCareerAfterMatch();
        if (youreSacked != 0) {
            Career_PostMatchUpdate();
            _ActivateMenu(contestMenu);
            return;
        }
        _CheckClubBusiness();
    }

    /* loc_13BBE6 */
    _TeamSelectionKeyCache_GetCurrentListPtr();
    _ContestMenuOnReturn_SetButtonsAndDefaultSelection(callerEntry);
    _SelectActiveContestAndRound();
    _RestoreContestScrollPosition();
    _BuildGameListArrayFromTeamSelectionKeyCache();
    _CheckTransfers();
    /* ContestMenuOnReturn_NoOp() — no-op, skip */

    if (careerFinished != 0 || seasonFinished != 0) {
        fireResetFlag = 1;
    }
}

/* ViewCompetitionsMenu — show competitions viewer (SWOS2.s line 59880) */
__attribute__((used, externally_visible))
void _ViewCompetitionsMenu(void)
{
    _ShowMenu(viewCompetitionsMenu);
    _TeamSelectionKeyCache_GetCurrentListPtr();
    _ContestSetButtons();
    _CheckTransfers();
    /* ContestMenuOnReturn_NoOp — no-op, skip */
}

/* ClubBusinessMenu — show club business (SWOS2.s line 59925) */
__attribute__((used, externally_visible))
void _ClubBusinessMenu(void)
{
    _ShowClubBussinesMenu();
}

/* JobOffersMenu — show job offers (SWOS2.s line 59930) */
__attribute__((used, externally_visible))
void _JobOffersMenu(void)
{
    if (teamNoOfferAccepted == 0xFFFF) {
        /* No offer accepted yet */
        if (numNewJobOffers == 0 && numJobOffers == 0)
            return;

        jobOffersFromRetired = 0;
        _ShowMenu(jobOffersMenu);
    } else {
        /* Already have an accepted offer */
        _ShowMenu(gotJobOfferMenu);
    }

    /* After either menu, check if entry 5 is disabled */
    MenuEntry *entry5 = &menuEntries[5];
    if (entry5->isDisabled != 0) {
        _SetCurrentEntry(7);
    }
}

/* SeasonFinished — handle end of season (SWOS2.s line 59956) */
__attribute__((used, externally_visible))
void _SeasonFinished(void)
{
    if (gameType != GAME_TYPE_CAREER) {
        _InitCareerVariables(0);
        _SetExitMenuFlag();
        return;
    }

    /* Career mode */
    if (isNationalSelector != 0) {
        SeasonFinished_ComputeSeasonReview();
        _MaybeSackManagerAfterSeasonReview();
        if (youreSacked != 0) {
            Career_PostMatchUpdate();
            _ActivateMenu(contestMenu);
            return;
        }
    }

    /* loc_13BD4C */
    _Career_HandleRetirement();
    _ActivateMenu(contestMenu);
}

/* TransfersMenu — show transfers or national team selection (SWOS2.s line 59981) */
__attribute__((used, externally_visible))
void _TransfersMenu(void)
{
    if (isNationalSelector != 0) {
        _ShowSelectRepresentationMenu();
        return;
    }

    if (timeToNegotiate == 0) {
        currentMessage = NO_MORE_TIME_TO_NEGOTIATE;
        _ShowChairmanMessage();
        return;
    }

    _ShowMenu(transferMarketMenu);
}

/* ViewWorldMenu — view world competitions (SWOS2.s line 59996) */
__attribute__((used, externally_visible))
void _ViewWorldMenu(void)
{
    ChooseCompetitionMenu();
    if (chooseTeamsResult == 0)
        return;

    _CCareer_ViewOrEditCompetitionSlot((uint8_t *)selectedContest, contestLeagueNumber);
    _ContestMenuOnReturn();
}

/* jmp_SetExitMenuFlag — trivial wrapper (SWOS2.s line 60009) */
__attribute__((used, externally_visible))
void _jmp_SetExitMenuFlag(void)
{
    _SetExitMenuFlag();
}

/* ==========================================================================
 * Chunk 124 — Competition Menu Functions
 * (DIYCompetitionMenu, PresetCompetitionMenu, SeasonMenu, CareerMenu,
 *  StartContest, SaveContest, LoadOldCompetitionMenu)
 * ========================================================================== */

extern int16_t _CheckContinueAbortPrompt(uint8_t *header);
extern void _ShowDesignDIYLeagueMenu(void);
extern void _ShowDesignDIYCupMenu(void);
extern void _ShowDesignDIYTournamentMenu(void);
extern void SelectTeamsForPresetCompetition(void);   /* asm stub → _SelectTeamsForPresetCompetition */
extern void _CopySelectedTeamNumbers(uint16_t count, uint8_t *dest);
extern void _CInitDIYCompetitionFromTableEntry(void); /* bridge: reads selectedContest + contestLeagueNumber */
extern void _CInitDIYCupFromRecord(void);             /* bridge: reads selectedContest */
extern void _CInitDIYLeagueFromRecord(void);           /* bridge: reads selectedContest */
extern void _InitTeams(void);
extern void _asm_ShowDiyLeaguesTournamentMenu(uint16_t d0, uint16_t d1, uint16_t d2);
extern void _asm_ShowDIYCupMenuWithParams(uint16_t d0, uint16_t d1, uint16_t d2);
extern void _DIY_EditCompetitionSlot(uint16_t paramA, uint16_t paramB, uint16_t paramC);
extern void _DIY_RestoreCompetitionFileBufferFromDIYCompetition(void);
extern void _DIY_CopyCompetitionStartToCompetitionFileBuffer_Alt(void);
extern void _DIY_CopyDiyFileBufferCopyToCompetitionFileBuffer(void);
extern void ChooseSeasonMenu(void);                    /* asm stub → _ChooseSeasonMenu */
extern void _CInitNewSeason(void);                     /* bridge: reads selectedContest + contestLeagueNumber */
extern int16_t _Asset_PromptInsertSaveDiskDf0(uint8_t *title);
extern int16_t _SelectFileToSaveDialog(uint32_t filterExt, uint8_t *defaultFilename, uint8_t *menuTitleStr);
extern int16_t _GetFilenameAndExtension(uint32_t preferredExt, uint8_t *defaultFilename, uint8_t *menuTitleStr);
extern void _ShowInsertDisk2Dialog(uint8_t *header);
extern int16_t _SaveDIYFile(uint8_t *filename);
extern int16_t _SavePresetCompetitionFile(uint8_t *filename);
extern int16_t _SaveCareerFile(uint8_t *filename);
extern int16_t _LoadDIYFile(uint8_t *filename);
extern int16_t _LoadPresetFile(uint8_t *filename);
extern int16_t _LoadCareerFile(uint8_t *filename);
extern void _SaveDIYTournament(void);
extern void _RestoreDIYTournament(void);
extern void _SaveDIYLeague(void);
extern void _RestoreDIYLeague(void);
extern void _DIY_CopyCompetitionFileBufferToCompetitionStart(void);
extern void _DIY_CopyCompetitionStartToCompetitionFileBuffer(void);
extern void _DIY_SaveCompetitionFileBufferToDIYCompetition(void);
extern void _DIY_CopyCompetitionFileBufferToDiyFileBufferCopy(void);
extern void _DIY_CopyCompetitionFileBufferToCompetitionStart_Alt(void);
extern void _DIY_ResetAndLoadCurrentCompetitionSlot(void);
extern void _DIY_InitCupFromTeamGridAndValidateTeams(void);
extern void _DIY_ResetCompetitionProgressAndShuffle(void);

extern uint16_t diySelected;
extern uint8_t  competitionFileBuffer[];
extern uint8_t  diyCompetitionMenu[];
extern uint8_t  careerMenu[];
extern uint8_t  aDiyCompetition_0[];
extern uint8_t  aPresetCompetit_0[];
extern uint8_t  aSeason_1[];
extern uint8_t  aCareer_1[];
extern uint8_t  aLoadOldCompeti[];
extern uint8_t  aNotACompetitio[];
extern uint8_t  competitionFilename[];
extern uint8_t  selectedFilename[];
extern uint32_t extension;
extern uint16_t diyLeagueTeamIds[];
extern uint16_t diyCupTeamIds[];
extern uint16_t diyTournamentTeamIds[];
extern uint8_t  saveFileTitle[];

/* DIYCompetitionMenu — DIY competition type selection (SWOS2.s line 68119) */
__attribute__((used, externally_visible))
void _DIYCompetitionMenu(void)
{
    *(volatile uint16_t *)(volatile void *)&seed = currentTick;  /* word write seeds both seed and adjacent randXorIndex */
    if (_CheckContinueAbortPrompt(aDiyCompetition_0) != 0)
        goto exit;

    _ShowMenu(diyCompetitionMenu);
    if ((int16_t)diySelected < 0)
        goto exit;

    if (diySelected == 0) {
        _ShowDesignDIYLeagueMenu();
    } else if (diySelected == 1) {
        _ShowDesignDIYCupMenu();
    } else {
        _ShowDesignDIYTournamentMenu();
    }

    if (gameType != GAME_TYPE_NO_GAME) {
        _SetCurrentEntry(11);
    }

exit:
    _CommonMenuExit();
}

/* PresetCompetitionMenu — preset competition setup (SWOS2.s line 68149) */
__attribute__((used, externally_visible))
void _PresetCompetitionMenu(void)
{
    *(volatile uint16_t *)(volatile void *)&seed = currentTick;  /* word write seeds both seed and adjacent randXorIndex */
    if (_CheckContinueAbortPrompt(aPresetCompetit_0) != 0) {
        _CommonMenuExit();
        return;
    }

    SelectTeamsForPresetCompetition();
    if (chooseTeamsResult == 0) {
        _CommonMenuExit();
        return;
    }

    _InitCareerVariables(2);
    uint8_t *contest = (uint8_t *)selectedContest;
    uint8_t compType = contest[1];

    if (compType == 0) {
        /* League */
        _CopySelectedTeamNumbers(numSelectedTeams, (uint8_t *)diyLeagueTeamIds);
        _CInitDIYCompetitionFromTableEntry();  /* bridge reads selectedContest + contestLeagueNumber */
        _InitTeams();
        _asm_ShowDiyLeaguesTournamentMenu(0, 0, 0);
        _DIY_RestoreCompetitionFileBufferFromDIYCompetition();
        _CommonMenuExit();
    } else if (compType == 1) {
        /* Cup */
        _CopySelectedTeamNumbers(numSelectedTeams, (uint8_t *)diyCupTeamIds);
        _CInitDIYCupFromRecord();  /* bridge reads selectedContest */
        _InitTeams();
        _asm_ShowDIYCupMenuWithParams(0, 0, 0);
        _DIY_CopyCompetitionStartToCompetitionFileBuffer_Alt();
        _CommonMenuExit();
    } else {
        /* Tournament */
        _CopySelectedTeamNumbers(numSelectedTeams, (uint8_t *)diyTournamentTeamIds);
        _CInitDIYLeagueFromRecord();  /* bridge reads selectedContest */
        _InitTeams();
        _DIY_EditCompetitionSlot(0, 0, 0);
        _DIY_CopyDiyFileBufferCopyToCompetitionFileBuffer();
    }

    _CommonMenuExit();
    if (gameType != GAME_TYPE_NO_GAME) {
        _SetCurrentEntry(11);
    }
}

/* SeasonMenu — season mode setup (SWOS2.s line 68221) */
__attribute__((used, externally_visible))
void _SeasonMenu(void)
{
    *(volatile uint16_t *)(volatile void *)&seed = currentTick;  /* word write seeds both seed and adjacent randXorIndex */
    if (_CheckContinueAbortPrompt(aSeason_1) != 0) {
        _CommonMenuExit();
        return;
    }

    ChooseSeasonMenu();
    if (chooseTeamsResult == 0) {
        _CommonMenuExit();
        return;
    }

    _InitCareerVariables(3);
    _CInitNewSeason();  /* bridge reads selectedContest + contestLeagueNumber */
    _ShowMenu(contestMenu);
    _CommonMenuExit();
    if (gameType != GAME_TYPE_NO_GAME) {
        _SetCurrentEntry(11);
    }
}

/* CareerMenu — career mode menu (SWOS2.s line 68250) */
__attribute__((used, externally_visible))
void _CareerMenu(void)
{
    *(volatile uint16_t *)(volatile void *)&seed = currentTick;  /* word write seeds both seed and adjacent randXorIndex */
    if (_CheckContinueAbortPrompt(aCareer_1) != 0) {
        _CommonMenuExit();
        return;
    }

    _ShowMenu(careerMenu);
    _CommonMenuExit();
    if (gameType != GAME_TYPE_NO_GAME) {
        _SetCurrentEntry(11);
    }
    _CommonMenuExit();
}

/* StartContest — start or resume contest based on game type (SWOS2.s line 68268)
 * Large dispatcher with branches for DIY, preset, season, career modes,
 * and sub-branches for league/cup/tournament and new/in-progress states. */
__attribute__((used, externally_visible))
void _StartContest(void)
{
    contestStarted = 0;
    uint16_t gt = gameType;

    if (gt == GAME_TYPE_DIY_COMPETITION) {
        if (competitionOver == 1)
            goto diy_over;      /* loc_140FDC */
        goto diy_not_over;      /* loc_1410E4 */
    } else if (gt == GAME_TYPE_PRESET_COMPETITION) {
        if (competitionOver == 1)
            goto preset_over;   /* loc_141050 */
        goto preset_not_over;   /* loc_14117C */
    } else if (gt == GAME_TYPE_SEASON) {
        /* loc_1410C0 */
        _ShowMenu(contestMenu);
        _CommonMenuExit();
        goto final_check;
    } else {
        /* loc_1410D2 — career */
        _ShowMenu(contestMenu);
        _CommonMenuExit();
        goto final_check;
    }

diy_over: /* loc_140FDC — DIY competition over */
    {
        uint16_t ct = *(uint16_t *)competitionFileBuffer;
        if (ct == 0) {
            /* League: loc_141030 */
            _SaveDIYLeague();
            _asm_ShowDiyLeaguesTournamentMenu(0, 0, 0);
            _RestoreDIYLeague();
            _CommonMenuExit();
            goto final_check;
        } else if (ct == 1) {
            /* Cup: loc_141010 */
            _DIY_CopyCompetitionFileBufferToCompetitionStart();
            _asm_ShowDIYCupMenuWithParams(0, 0, 0);
            _DIY_CopyCompetitionStartToCompetitionFileBuffer();
            _CommonMenuExit();
            goto final_check;
        } else {
            /* Tournament */
            _SaveDIYTournament();
            _DIY_EditCompetitionSlot(0, 0, 0);
            _RestoreDIYTournament();
            _CommonMenuExit();
            goto final_check;
        }
    }

preset_over: /* loc_141050 — preset competition over */
    {
        uint16_t ct = *(uint16_t *)competitionFileBuffer;
        if (ct == 0) {
            /* League: loc_1410A0 */
            _DIY_SaveCompetitionFileBufferToDIYCompetition();
            _asm_ShowDiyLeaguesTournamentMenu(0, 0, 0);
            _DIY_RestoreCompetitionFileBufferFromDIYCompetition();
            _CommonMenuExit();
            goto final_check;
        } else if (ct == 1) {
            /* Cup: loc_141080 */
            _DIY_CopyCompetitionFileBufferToCompetitionStart_Alt();
            _asm_ShowDIYCupMenuWithParams(0, 0, 0);
            _DIY_CopyCompetitionStartToCompetitionFileBuffer_Alt();
            _CommonMenuExit();
            goto final_check;
        } else {
            /* Tournament */
            _DIY_CopyCompetitionFileBufferToDiyFileBufferCopy();
            _DIY_EditCompetitionSlot(0, 0, 0);
            _DIY_CopyDiyFileBufferCopyToCompetitionFileBuffer();
            _CommonMenuExit();
            goto final_check;
        }
    }

diy_not_over: /* loc_1410E4 — DIY not over yet */
    {
        uint16_t ct = *(uint16_t *)competitionFileBuffer;
        if (ct == 0) {
            /* League: loc_141150 */
            _SaveDIYLeague();
            _InitTeams();
            _DIY_ResetCompetitionProgressAndShuffle();
            _asm_ShowDiyLeaguesTournamentMenu(0, 0, 0);
            _RestoreDIYLeague();
            _CommonMenuExit();
            goto final_check;
        } else if (ct == 1) {
            /* Cup: loc_141124 */
            _DIY_CopyCompetitionFileBufferToCompetitionStart();
            _InitTeams();
            _DIY_InitCupFromTeamGridAndValidateTeams();
            _asm_ShowDIYCupMenuWithParams(0, 0, 0);
            _DIY_CopyCompetitionStartToCompetitionFileBuffer();
            _CommonMenuExit();
            goto final_check;
        } else {
            /* Tournament */
            _SaveDIYTournament();
            _InitTeams();
            _DIY_ResetAndLoadCurrentCompetitionSlot();
            _DIY_EditCompetitionSlot(0, 0, 0);
            _RestoreDIYTournament();
            _CommonMenuExit();
            goto final_check;
        }
    }

preset_not_over: /* loc_14117C — preset not over yet */
    {
        uint16_t ct = *(uint16_t *)competitionFileBuffer;
        if (ct == 0) {
            /* League: loc_1411E4 */
            _DIY_SaveCompetitionFileBufferToDIYCompetition();
            _InitTeams();
            _DIY_ResetCompetitionProgressAndShuffle();
            _asm_ShowDiyLeaguesTournamentMenu(0, 0, 0);
            _DIY_RestoreCompetitionFileBufferFromDIYCompetition();
            _CommonMenuExit();
            goto final_check;
        } else if (ct == 1) {
            /* Cup: loc_1411B8 */
            _DIY_CopyCompetitionFileBufferToCompetitionStart_Alt();
            _InitTeams();
            _DIY_InitCupFromTeamGridAndValidateTeams();
            _asm_ShowDIYCupMenuWithParams(0, 0, 0);
            _DIY_CopyCompetitionStartToCompetitionFileBuffer_Alt();
            _CommonMenuExit();
            goto final_check;
        } else {
            /* Tournament */
            _DIY_CopyCompetitionFileBufferToDiyFileBufferCopy();
            _InitTeams();
            _DIY_ResetAndLoadCurrentCompetitionSlot();
            _DIY_EditCompetitionSlot(0, 0, 0);
            _DIY_CopyDiyFileBufferCopyToCompetitionFileBuffer();
            _CommonMenuExit();
            goto final_check;
        }
    }

final_check: /* loc_141210 */
    _CommonMenuExit();
    if (gameType == GAME_TYPE_NO_GAME) {
        _SetCurrentEntry(13);
    }
}

/* SaveContest — save competition to disk (SWOS2.s line 68443) */
__attribute__((used, externally_visible))
void _SaveContest(void)
{
    if (_Asset_PromptInsertSaveDiskDf0(saveFileTitle) != 0)
        goto cleanup;

    {
        uint16_t gt = gameType;
        uint32_t ext;
        int16_t result;

        if (gt == GAME_TYPE_DIY_COMPETITION) {
            ext = 0x2E444959; /* '.DIY' */
        } else if (gt == GAME_TYPE_PRESET_COMPETITION) {
            ext = 0x2E505245; /* '.PRE' */
        } else if (gt == GAME_TYPE_SEASON) {
            ext = 0x2E534541; /* '.SEA' */
        } else {
            ext = 0x2E434152; /* '.CAR' */
        }

        if (_SelectFileToSaveDialog(ext, competitionFilename, saveFileTitle) != 0)
            goto done;

        if (gt == GAME_TYPE_DIY_COMPETITION) {
            result = _SaveDIYFile(selectedFilename);
        } else if (gt == GAME_TYPE_PRESET_COMPETITION) {
            result = _SavePresetCompetitionFile(selectedFilename);
        } else if (gt == GAME_TYPE_SEASON) {
            result = _SaveCareerFile(selectedFilename);   /* j_SaveCareerFile -> SaveCareerFile */
        } else {
            result = _SaveCareerFile(selectedFilename);
        }

        if (result != 0)
            goto done;

        contestStarted = 1;
        _StringCopy(selectedFilename, competitionFilename);
    }

done:
    _ShowInsertDisk2Dialog(saveFileTitle);
cleanup:
    _CommonMenuExit();
}

/* LoadOldCompetitionMenu — load competition from disk (SWOS2.s line 68524) */
__attribute__((used, externally_visible))
void _LoadOldCompetitionMenu(void)
{
    if (_CheckContinueAbortPrompt(aLoadOldCompeti) != 0) {
        _CommonMenuExit();
        return;
    }

    if (_Asset_PromptInsertSaveDiskDf0(aLoadOldCompeti) != 0) {
        _CommonMenuExit();
        return;
    }

    if (_GetFilenameAndExtension(0, aLoadOldCompeti, aLoadOldCompeti) != 0)
        goto fail;

    {
        int16_t result;
        if (extension == 0x2E444959) {          /* '.DIY' */
            result = _LoadDIYFile(selectedFilename);
        } else if (extension == 0x2E505245) {   /* '.PRE' */
            result = _LoadPresetFile(selectedFilename);
        } else if (extension == 0x2E534541) {   /* '.SEA' */
            result = _LoadCareerFile(selectedFilename);  /* j_LoadCareerFile */
        } else if (extension == 0x2E434152) {   /* '.CAR' */
            result = _LoadCareerFile(selectedFilename);
        } else {
            _ShowErrorMenu(aNotACompetitio);
            goto fail;
        }

        if (result != 0) {
            _InitCareerVariables(0);
            goto fail;
        }

        _StringCopy(selectedFilename, competitionFilename);
    }

    /* Success path: loc_14147C */
    _ShowInsertDisk2Dialog(aLoadOldCompeti);
    _CommonMenuExit();
    _StartContest();
    return;

fail: /* loc_14149A */
    _ShowInsertDisk2Dialog(aLoadOldCompeti);
    _CommonMenuExit();
}

/* ==========================================================================
 * Chunk 125 — Choose/Select Team Menu Functions
 * (ChooseCompetitionMenu, SelectTeamsForPresetCompetition, ChooseSeasonMenu,
 *  GoGetTeamsForPlay)
 * ========================================================================== */

extern void _CSelectTeamsFinalMenu(uint16_t mode); /* bridge: loads d0 from param, calls SelectTeamsFinalMenu */

extern uint16_t teamsType;
extern uint32_t gameTypeStr;
extern uint32_t chooseTeamsHeader1stPart;
extern uint32_t chooseTeamsFunc;
extern uint16_t careerSquadScreenDidSelectPlayerFlag;
extern uint16_t selectTeamsNoLimit;
extern uint16_t loadTeam;
extern uint16_t showCupsAndOther;
extern uint16_t playSeason;
extern uint16_t chooseCompetitionOnly;
extern uint16_t choosingPreset;
extern uint16_t showClubTeams;
extern uint16_t showNationalTeams;
extern uint16_t showOrigCustomTeams;
extern uint32_t playWhatStr;
extern uint16_t numNeededTeams;
extern uint8_t  aChooseCompetit[];
extern uint8_t  aChoosePresetCo[];
extern uint8_t  aChooseSeason[];
extern uint8_t  chooseTeamsMenu[];

/* ChooseCompetitionMenu — choose competition setup (SWOS2.s line 63765) */
__attribute__((used, externally_visible))
void _ChooseCompetitionMenu(void)
{
    gameTypeStr = 0;
    chooseTeamsHeader1stPart = (uint32_t)aChooseCompetit;
    chooseTeamsFunc = 0;
    careerSquadScreenDidSelectPlayerFlag = 0;
    selectTeamsNoLimit = 1;
    teamsType = 0xFFFF;
    loadTeam = 0;
    showCupsAndOther = 1;
    playSeason = 0;
    chooseCompetitionOnly = 1;
    _CSelectTeamsFinalMenu(255);
}

/* SelectTeamsForPresetCompetition — choose preset competition (SWOS2.s line 63782) */
__attribute__((used, externally_visible))
void _SelectTeamsForPresetCompetition(void)
{
    gameTypeStr = 0;
    chooseTeamsHeader1stPart = (uint32_t)aChoosePresetCo;
    chooseTeamsFunc = 0;
    careerSquadScreenDidSelectPlayerFlag = 0;
    selectTeamsNoLimit = 1;
    teamsType = 0xFFFF;
    loadTeam = 0;
    showCupsAndOther = 1;
    playSeason = 0;
    chooseCompetitionOnly = 0;
    choosingPreset = 1;
    _CSelectTeamsFinalMenu(255);
    choosingPreset = 0;
}

/* ChooseSeasonMenu — choose season setup (SWOS2.s line 63804) */
__attribute__((used, externally_visible))
void _ChooseSeasonMenu(void)
{
    gameTypeStr = 0;
    chooseTeamsHeader1stPart = (uint32_t)aChooseSeason;
    chooseTeamsFunc = 0;
    careerSquadScreenDidSelectPlayerFlag = 0;
    selectTeamsNoLimit = 1;
    teamsType = 0xFFFF;
    loadTeam = 0;
    showCupsAndOther = 0;
    playSeason = 1;
    chooseCompetitionOnly = 0;
    _CSelectTeamsFinalMenu(255);
}

/* GoGetTeamsForPlay — set up team selection globals and show menu (SWOS2.s line 63830)
 * Called from asm via register bridge (d0=numTeams, d2=showClub, d3=showNational,
 * d4=showOrigCustom, a0=gameTypeStrParam, a1=playWhatStrParam). */
__attribute__((used, externally_visible))
void _GoGetTeamsForPlay(uint8_t *gameTypeStrParam, uint8_t *playWhatStrParam,
                        uint16_t numTeams, uint16_t showClub,
                        uint16_t showNational, uint16_t showOrigCustom)
{
    showClubTeams = showClub;
    showNationalTeams = showNational;
    showOrigCustomTeams = showOrigCustom;
    gameTypeStr = (uint32_t)gameTypeStrParam;
    playWhatStr = (uint32_t)playWhatStrParam;
    chooseTeamsHeader1stPart = 0;
    numSelectedTeams = 0;
    numNeededTeams = numTeams;
    chooseTeamsFunc = 0;
    careerSquadScreenDidSelectPlayerFlag = 0;
    selectTeamsNoLimit = 0;
    _ShowMenu(chooseTeamsMenu);
    if ((int16_t)teamsType < 0)
        return;
    showCupsAndOther = 0;
    loadTeam = 1;
    playSeason = 0;
    chooseCompetitionOnly = 0;
    _CSelectTeamsFinalMenu(255);
    _ShowInsertDisk2Dialog((uint8_t *)0);
}


/* ================================================================
 * Chunk 135 — Career Over callbacks, Stats dispatchers,
 *             Select Teams callbacks
 * ================================================================ */

/* Additional externs for Chunk 135 */
extern uint8_t  gotJobOfferMenu[];
extern uint8_t  jobOffersMenu[];
extern uint8_t  a0EndSeason[];          /* "%0 END SEASON" */
extern uint8_t  aRetire[];              /* "RETIRE" */
extern uint8_t  aNewJob[];              /* "NEW JOB" */
extern uint8_t  aNoJobOffers[];         /* "NO JOB OFFERS" */
extern uint16_t selectedTeamIndex;
extern uint16_t selectedLeagueIndex;
extern uint32_t selectedTeamText;
extern uint16_t lastSelectedTeam;
extern uint16_t startingTeamIndex;
extern uint16_t numTeamsColumn1;
extern uint16_t numTeamsColumn2;
extern uint16_t numTeamsColumn3;
extern uint16_t teamDepthLevel;
extern uint16_t topOfStackMark;
extern uint16_t diyCompetitionType;

extern void _ShowLeagueInfo(void);
extern void _ShowCupInfo(void);
extern void _ShowTournamentInfo(void);

/* Already declared elsewhere in menus.c: */
/* extern uint16_t teamNoOfferAccepted, jobOffersFromRetired; */
/* extern uint16_t numNewJobOffers, numJobOffers, youreSacked; */
/* extern uint16_t currentSeasonShowing, contestStarted; */
/* extern uint8_t managerNameBuffer[], managerSurnameBuffer[], seasonPlaying; */
/* extern uint16_t viewSelectedTeams, chooseTeamsResult, changeTeams, abortSelectTeams, exitSelectTeams; */


/* ================================================================
 * CareerOverUpdateJobOffersEntry  (SWOS2.s line 55578)
 *
 * Updates the job offers menu entry (#5 at offset $118) visibility,
 * color, and text based on career state.
 * ================================================================ */
static void CareerOverUpdateJobOffersEntry(void)
{
    MenuEntry *e;

    /* If season 19 (max), hide the entry entirely */
    if (currentSeasonShowing == 19)
    {
        menuEntries[0x118 / 56].isInvisible = 1;
        return;
    }

    /* Set entry to yellow-green-black color scheme */
    e = &menuEntries[0x118 / 56];
    {
        uint16_t bg = *(uint16_t *)((uint8_t *)e + 30);  /* BG+BACKANDFRAMECOLOR at offset 30 */
        bg = (bg & 0xFFF0) + 14;  /* YELLOW_GREEN_BLACK_14 */
        *(uint16_t *)((uint8_t *)e + 30) = bg;
    }
    e->isDisabled = 0;

    /* Check stringFlags bit 13 (flashing/highlight) */
    {
        uint16_t flags = e->stringFlags;
        flags &= ~(1 << 13);  /* clear bit 13 */

        if (teamNoOfferAccepted == 0xFFFF)
        {
            /* No offer accepted — check if there are any offers */
            if (numNewJobOffers != 0 || numJobOffers != 0)
            {
                flags |= (1 << 13);  /* set bit 13 (flash) */
                e->stringFlags = flags;
                return;
            }
            /* No offers at all */
            e->fg = (uint32_t)aNoJobOffers;
            e->isDisabled = 1;
            {
                uint16_t bg = *(uint16_t *)((uint8_t *)e + 30);
                bg = (bg & 0xFFF0) + 9;  /* PINK_TO_BROWN_9 */
                *(uint16_t *)((uint8_t *)e + 30) = bg;
            }
            e->stringFlags = flags;
            return;
        }
        else
        {
            /* Offer accepted — show "NEW JOB" */
            e->fg = (uint32_t)aNewJob;
            e->stringFlags = flags;
            return;
        }
    }
}


/* ================================================================
 * CareerOverFinish  (SWOS2.s line 55487)
 *
 * Menu finish (onReturn) callback for career over menu.
 * Sets up manager name, team name, season year, and entry states.
 * ================================================================ */
__attribute__((used, externally_visible))
void _CareerOverFinish(void)
{
    MenuEntry *titleEntry = &menuEntries[0];
    uint8_t *dest = (uint8_t *)titleEntry->fg;
    uint8_t *src;
    uint16_t nameLen, surnameLen, totalLen;

    /* Build "FIRSTNAME SURNAME" or "F SURNAME" into title entry */
    src = managerNameBuffer;
    nameLen = _GetStringLength(src);
    src = managerSurnameBuffer;
    surnameLen = _GetStringLength(src);
    totalLen = nameLen + surnameLen;

    src = managerNameBuffer;
    if (totalLen > 12)
    {
        /* Name too long — abbreviate first name to initial */
        if (*src != 0)
        {
            *dest++ = *src++;
            *dest++ = ' ';
        }
    }
    else
    {
        /* Full first name fits */
        dest = _StringCopy(src, dest);
        *dest++ = ' ';
    }

    /* Append surname */
    src = managerSurnameBuffer;
    dest = _StringCopy(src, dest);

    /* Set team name in entry 1 (offset $38/56 = 1) */
    {
        uint16_t teamNum = selTeamNumber;
        uint8_t *teamPtr = _VerifyTeamLoaded(teamNum);
        uint8_t *teamName = teamPtr + 5;
        MenuEntry *e1 = &menuEntries[1];
        dest = (uint8_t *)e1->fg;
        _StringCopy(teamName, dest);
    }

    /* Set season year in entry 2 (offset $70/56 = 2) */
    {
        MenuEntry *e2 = &menuEntries[2];
        dest = (uint8_t *)e2->fg;
        uint16_t year = (uint16_t)seasonPlaying + 1900;
        if (year < 1996)
            year += 100;
        year += 1;
        src = a0EndSeason;
        _PrintFormatted(src, dest, year, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    CareerOverUpdateJobOffersEntry();

    /* Season 19: final season special handling */
    if (currentSeasonShowing == 19)
    {
        menuEntries[0x188 / 56].isInvisible = 1;
        {
            MenuEntry *e150 = &menuEntries[0x150 / 56];
            e150->fg = (uint32_t)aRetire;
        }
        menuEntries[0xA8 / 56].isInvisible = 0;
        return;
    }

    /* Check if sacked with no offers → disable continue/save */
    if (teamNoOfferAccepted == 0xFFFF && youreSacked != 0
        && numNewJobOffers == 0 && numJobOffers == 0)
    {
        /* Disable "CONTINUE CAREER" entry (at offset $E0) */
        {
            MenuEntry *eE0 = &menuEntries[0xE0 / 56];
            eE0->isDisabled = 1;
            uint16_t bg = *(uint16_t *)((uint8_t *)eE0 + 30);
            bg = (bg & 0xFFF0) + 9;  /* PINK_TO_BROWN_9 */
            *(uint16_t *)((uint8_t *)eE0 + 30) = bg;
        }
        /* Disable "SAVE CAREER" entry (at offset $150) */
        {
            MenuEntry *e150 = &menuEntries[0x150 / 56];
            e150->isDisabled = 1;
            uint16_t bg = *(uint16_t *)((uint8_t *)e150 + 30);
            bg = (bg & 0xFFF0) + 9;  /* PINK_TO_BROWN_9 */
            *(uint16_t *)((uint8_t *)e150 + 30) = bg;
        }
        menuEntries[0x188 / 56].isInvisible = 1;
        menuEntries[0xA8 / 56].isInvisible = 0;
    }

    /* If sacked with no accepted offer → also disable save */
    if (youreSacked != 0 && teamNoOfferAccepted == 0xFFFF)
    {
        MenuEntry *e150 = &menuEntries[0x150 / 56];
        e150->isDisabled = 1;
        uint16_t bg = *(uint16_t *)((uint8_t *)e150 + 30);
        bg = (bg & 0xFFF0) + 9;  /* PINK_TO_BROWN_9 */
        *(uint16_t *)((uint8_t *)e150 + 30) = bg;
    }
}


/* ================================================================
 * JobOffersMenuRetired  (SWOS2.s line 55623)
 *
 * Shows job offers from the career over menu.
 * Handles different states: new/existing offers, no offers, etc.
 * ================================================================ */
__attribute__((used, externally_visible))
void _JobOffersMenuRetired(void)
{
    if (teamNoOfferAccepted != 0xFFFF)
    {
        /* Already accepted an offer — show the "got job" menu */
        _ShowMenu(gotJobOfferMenu);
        goto update_entry;
    }

    /* Check if there are any offers to show */
    if (numNewJobOffers == 0 && numJobOffers == 0)
        return;

    /* Show job offers */
    jobOffersFromRetired = 1;
    _ShowMenu(jobOffersMenu);

    if (teamNoOfferAccepted == 0xFFFF)
    {
        /* No offer was accepted */
        if (youreSacked != 0)
        {
            /* Sacked and no remaining offers → exit career */
            uint16_t totalOffers = numNewJobOffers + numJobOffers;
            if (totalOffers == 0)
            {
                _InitCareerVariables(0);
                _SetExitMenuFlag();
                return;
            }
        }
    }

update_entry:
    /* Check if job offers entry is disabled → set current entry to 8 */
    {
        MenuEntry *e = &menuEntries[0x118 / 56];
        if (e->isDisabled != 0)
            _SetCurrentEntry(8);
    }
}


/* ================================================================
 * ExitCareerMenu  (SWOS2.s line 55700)
 *
 * Exit handler for the career over menu.
 * If sacked with no offers, clears career state first.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ExitCareerMenu(void)
{
    if (teamNoOfferAccepted == 0xFFFF && youreSacked != 0
        && numNewJobOffers == 0 && numJobOffers == 0)
    {
        _InitCareerVariables(0);
    }
    _SetExitMenuFlag();
}


/* ================================================================
 * CompetitionInfo  (SWOS2.s line 55254)
 *
 * Dispatches to the correct info display based on competition type.
 * ================================================================ */
__attribute__((used, externally_visible))
void _CompetitionInfo(void)
{
    if (diyCompetitionType == 2)
        _ShowTournamentInfo();
    else if (diyCompetitionType != 0)
        _ShowCupInfo();
    else
        _ShowLeagueInfo();
}


/* ================================================================
 * StatsExit  (SWOS2.s line 55275)
 *
 * Trivial exit handler — just calls SetExitMenuFlag.
 * ================================================================ */
__attribute__((used, externally_visible))
void _StatsExit(void)
{
    _SetExitMenuFlag();
}


/* ================================================================
 * ChooseTeamsPlayGame  (SWOS2.s line 62910)
 *
 * "Play Game" button handler in choose teams menu.
 * Sets result flags and exits menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ChooseTeamsPlayGame(void)
{
    selectedTeamIndex = 0xFFFF;
    selectedLeagueIndex = 0xFFFF;
    chooseTeamsResult = 1;
    viewSelectedTeams = 0;
    changeTeams = 0;
    abortSelectTeams = 0;
    exitSelectTeams = 0;
    _SetExitMenuFlag();
}


/* ================================================================
 * ExitSelectTeams  (SWOS2.s line 62922)
 *
 * Exit/abort handler for the select teams menu.
 * Behavior depends on teamDepthLevel vs topOfStackMark.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ExitSelectTeams(void)
{
    selectedTeamIndex = 0xFFFF;
    selectedLeagueIndex = 0xFFFF;
    chooseTeamsResult = 0;
    viewSelectedTeams = 0;
    changeTeams = 0;

    {
        uint16_t depth = teamDepthLevel;
        if (depth == 0 || depth == topOfStackMark)
        {
            abortSelectTeams = 1;
            exitSelectTeams = 0;
        }
        else
        {
            exitSelectTeams = 1;
            abortSelectTeams = 0;
        }
    }
    _SetExitMenuFlag();
}


/* ================================================================
 * SelectTeamsCommon  (SWOS2.s line 62942)
 *
 * Handler for selecting a team from the 3-column list.
 * Converts menu ordinal to team index accounting for column layout.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SelectTeamsCommon(MenuEntry *entry)
{
    uint8_t *namePtr = (uint8_t *)entry->fg;

    /* Ignore empty (space-prefixed) entries */
    if (*namePtr == ' ')
        return;

    {
        uint16_t ord = entry->ordinal;
        uint16_t idx;

        lastSelectedTeam = ord;
        idx = ord - 21 + startingTeamIndex;

        /* Convert column ordinal to flat index */
        if (idx >= 22)
        {
            if (idx >= 44)
            {
                idx -= 22;
                idx += numTeamsColumn2;
            }
            idx -= 22;
            idx += numTeamsColumn1;
        }

        selectedTeamIndex = idx;
        selectedLeagueIndex = 0xFFFF;
        selectedTeamText = entry->fg;
        chooseTeamsResult = 0;
        viewSelectedTeams = 0;
        changeTeams = 0;
        abortSelectTeams = 0;
        exitSelectTeams = 0;
        _SetExitMenuFlag();
    }
}


/* ================================================================
 * SelectLeaguesCommon  (SWOS2.s line 62974)
 *
 * Handler for selecting a league from the list.
 * Converts ordinal (1-based) to league index.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SelectLeaguesCommon(MenuEntry *entry)
{
    uint8_t *namePtr = (uint8_t *)entry->fg;

    if (*namePtr == ' ')
        return;

    {
        uint16_t ord = entry->ordinal;
        uint16_t idx;

        lastSelectedTeam = ord;
        idx = ord - 1;
        if (idx >= 10)
            idx -= 10;

        selectedLeagueIndex = idx;
        selectedTeamIndex = 0xFFFF;
        selectedTeamText = entry->fg;
        chooseTeamsResult = 0;
        viewSelectedTeams = 0;
        changeTeams = 0;
        abortSelectTeams = 0;
        exitSelectTeams = 0;
        _SetExitMenuFlag();
    }
}


/* ================================================================
 * Chunk 136 — ViewCompetitions Init/OnReturn, Career Menu Init/Finish,
 *             Nationality handlers, SelTeamForTactics
 * ================================================================ */

/* Additional externs for Chunk 136 */
extern uint32_t currentCompetitionPtr;
extern uint8_t  tourCopy[];
extern uint8_t  friendlyCopy[];
extern uint32_t secondaryCompetitionPtr;
extern uint32_t tertiaryCompetitionPtr;
extern uint32_t quaternaryCompetitionPtr;
extern uint8_t  diyPresetNameOfCompetition[];
extern uint8_t  secondaryCompetitionName[];
extern uint8_t  tertiaryCompetitionName[];
extern uint8_t  quaternaryCompetitionName[];
extern uint8_t  plNationality;
extern uint8_t  nationalityOrdinal;
extern uint8_t  nationalityOrdinalsArray[];
extern uint8_t  shortCountryNames[];
extern uint8_t  aOther[];               /* "OTHER" */
extern uint16_t controlMask;
extern uint16_t selTeamsColumn1;
extern uint16_t selTeamsColumn2;
extern uint16_t teamSelected;

extern void _SetDefaultNameAndSurname(void);
extern void _CheckSex(void);
extern void _SetPlayerCoachFieldColor(void);


/* ================================================================
 * SetNationalityField  (SWOS2.s line 55866)
 *
 * Sets the nationality 3-char code in the career menu entry.
 * Entry 7 (offset $188/56) fg string pointer is the destination.
 * ================================================================ */
static void SetNationalityField(void)
{
    MenuEntry *e = &menuEntries[0x188 / 56];
    uint8_t *dest = (uint8_t *)e->fg;
    uint8_t nat = plNationality;

    if (nat == 0xFF)
    {
        /* Unknown nationality — write "OTHER" */
        _StringCopy(aOther, dest);
        return;
    }

    /* Copy 3-char country code from table */
    {
        uint16_t idx = (uint16_t)nat;
        uint16_t off = idx * 3;
        uint8_t *src = shortCountryNames + off;
        dest[0] = src[0];
        dest[1] = src[1];
        dest[2] = src[2];
        dest[3] = 0;
    }
}


/* ================================================================
 * ViewCompetitionsOnReturn  (SWOS2.s line 56227)
 *
 * Called when the ViewCompetitions menu is restored.
 * Sets up competition name text pointers and shuffles Y coordinates
 * when hiding unavailable competition slots.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ViewCompetitionsOnReturn(void)
{
    /* Set text pointers for competition name entries */
    menuEntries[1].fg = (uint32_t)diyPresetNameOfCompetition;
    menuEntries[2].fg = (uint32_t)secondaryCompetitionName;
    menuEntries[3].fg = (uint32_t)tertiaryCompetitionName;
    menuEntries[4].fg = (uint32_t)quaternaryCompetitionName;

    /* If primary competition is tour/friendly, hide entry 1 and shift Y coords up */
    if (currentCompetitionPtr == (uint32_t)tourCopy
        || currentCompetitionPtr == (uint32_t)friendlyCopy)
    {
        menuEntries[1].isInvisible = 1;
        /* Shift Y coordinates: 5←4, 4←3, 3←2, 2←1 */
        menuEntries[5].y = menuEntries[4].y;
        menuEntries[4].y = menuEntries[3].y;
        menuEntries[3].y = menuEntries[2].y;
        menuEntries[2].y = menuEntries[1].y;
    }

    /* If secondary competition is null/tour/friendly, hide entry 2 and shift */
    if (secondaryCompetitionPtr == 0
        || secondaryCompetitionPtr == (uint32_t)friendlyCopy
        || secondaryCompetitionPtr == (uint32_t)tourCopy)
    {
        menuEntries[2].isInvisible = 1;
        /* Shift Y coordinates: 5←4, 4←3, 3←2 */
        menuEntries[5].y = menuEntries[4].y;
        menuEntries[4].y = menuEntries[3].y;
        menuEntries[3].y = menuEntries[2].y;
    }

    /* If tertiary competition is null/tour/friendly, hide entry 3 and shift */
    if (tertiaryCompetitionPtr == 0
        || tertiaryCompetitionPtr == (uint32_t)friendlyCopy
        || tertiaryCompetitionPtr == (uint32_t)tourCopy)
    {
        menuEntries[3].isInvisible = 1;
        /* Shift Y coordinates: 5←4, 4←3 */
        menuEntries[5].y = menuEntries[4].y;
        menuEntries[4].y = menuEntries[3].y;
    }

    /* If quaternary competition is null, hide entry 4 and shift */
    if (quaternaryCompetitionPtr == 0)
    {
        menuEntries[4].isInvisible = 1;
        menuEntries[5].y = menuEntries[4].y;
    }

    /* Hide "Top Goal Scorers" entry (5) if national selector or has team selection cache */
    if (isNationalSelector != 0 || teamSelectionKeyCacheIndex != 0)
    {
        menuEntries[5].isInvisible = 1;
    }

    /* Hide "Squad" entry (6) if not SEASON game type (4) */
    if (gameType != 4)
    {
        menuEntries[6].isInvisible = 1;
    }
}


/* ================================================================
 * ViewCompetitionsInit  (SWOS2.s line 56209)
 *
 * Initializes the ViewCompetitions menu — sets starting entry based
 * on whether current competition is tour/friendly.
 * Falls through to ViewCompetitionsOnReturn.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ViewCompetitionsInit(void)
{
    if (currentCompetitionPtr == (uint32_t)tourCopy
        || currentCompetitionPtr == (uint32_t)friendlyCopy)
    {
        _SetCurrentEntry(5);

        if (isNationalSelector != 0 || teamSelectionKeyCacheIndex != 0)
        {
            _SetCurrentEntry(7);
        }
    }

    _ViewCompetitionsOnReturn();
}


/* ================================================================
 * CareerMenuFinish  (SWOS2.s line 55847)
 *
 * OnReturn callback for the career menu. Updates sex, player-coach
 * field color, and nationality display.
 * ================================================================ */
__attribute__((used, externally_visible))
void _CareerMenuFinish(void)
{
    uint8_t nat;
    uint8_t *table;
    uint8_t idx;

    _CheckSex();
    _SetPlayerCoachFieldColor();

    /* Find nationalityOrdinal for the current plNationality */
    nat = plNationality;
    table = nationalityOrdinalsArray;
    idx = 0;
    while (*table != nat)
    {
        table++;
        idx++;
    }
    nationalityOrdinal = idx;

    SetNationalityField();
}


/* ================================================================
 * CareerMenuInit  (SWOS2.s line 55842)
 *
 * OnInit callback for the career menu.
 * Calls SetDefaultNameAndSurname, then falls through to CareerMenuFinish.
 * ================================================================ */
__attribute__((used, externally_visible))
void _CareerMenuInit(void)
{
    _SetDefaultNameAndSurname();
    _CareerMenuFinish();
}


/* ================================================================
 * ChangeNationality  (SWOS2.s line 55889)
 *
 * Menu callback for left/right on the nationality field.
 * Cycles through nationalities and redraws.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ChangeNationality(MenuEntry *entry)
{
    uint16_t ctrl = controlMask;
    int8_t ord;

    /* Right or Down → increment */
    if (ctrl == 0x81 || ctrl == 0x101)
    {
        ord = (int8_t)nationalityOrdinal;
        ord++;
        if (ord == 64)
            ord = 0;
    }
    /* Left or Up → decrement */
    else if (ctrl == 0x41 || ctrl == 0x201)
    {
        ord = (int8_t)nationalityOrdinal;
        ord--;
        if (ord < 0)
            ord = 63;
    }
    else
    {
        return;
    }

    nationalityOrdinal = (uint8_t)ord;
    {
        int16_t idx = (int16_t)(int8_t)ord;  /* ext.w d0 */
        plNationality = nationalityOrdinalsArray[idx];
    }
    SetNationalityField();
    _CallDrawMenuItem(entry);
}


/* ================================================================
 * SelTeamForTacticsCommon  (SWOS2.s line 60202)
 *
 * Handler for selecting a team from the tactics team selection list.
 * Converts menu ordinal to flat team index via column layout.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SelTeamForTacticsCommon(MenuEntry *entry)
{
    uint16_t idx = entry->ordinal;
    idx -= 1;

    if (idx >= 22)
    {
        if (idx >= 44)
        {
            idx -= 22;
            idx += selTeamsColumn2;
        }
        idx -= 22;
        idx += selTeamsColumn1;
    }

    teamSelected = idx;
    _SetExitMenuFlag();
}


/* ================================================================
 * Chunk 137 — SelTeamForTactics_ReflowTeamListLayout
 * ================================================================ */

/* Additional externs for Chunk 137 */
extern uint16_t selTeamsColumn3;
extern uint16_t numTeamsCountryBuffer;
extern uint16_t selTeamsListTopY;
extern uint16_t selTeamsListBottomY;


/* ================================================================
 * SelTeamForTactics_ReflowTeamListLayout  (SWOS2.s line 60065)
 *
 * Calculates the Y positions for the 3-column team selection list
 * in the edit tactics team picker. Hides title if >18 teams,
 * adjusts bottom entry if >20 teams.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SelTeamForTactics_ReflowTeamListLayout(void)
{
    uint16_t maxCol;
    uint16_t topY, bottomY, totalHeight, listHeight, startY;
    MenuEntry *e;
    int16_t col, row;

    /* Find the maximum column height */
    maxCol = selTeamsColumn1;
    if (selTeamsColumn2 > maxCol)
        maxCol = selTeamsColumn2;
    if (selTeamsColumn3 > maxCol)
        maxCol = selTeamsColumn3;

    /* If more than 20 teams, adjust bottom entry */
    if (maxCol > 18)
    {
        if (maxCol > 20)
        {
            /* Entry at offset $EA8 = entry 67 */
            e = &menuEntries[0xEA8 / 56];
            e->y += 7;
            e->dwHeight -= 7;
        }
        /* Hide title entry (entry 0) */
        menuEntries[0].isInvisible = 1;
    }

    /* Calculate list top Y */
    e = &menuEntries[0];
    topY = e->y;
    if (e->isInvisible == 0)
        topY += e->dwHeight;
    selTeamsListTopY = topY;

    /* Calculate list bottom Y */
    e = &menuEntries[0xEA8 / 56];
    selTeamsListBottomY = e->y;
    if (numTeamsCountryBuffer == 64)
    {
        selTeamsListBottomY += e->dwHeight;
    }

    /* Center the list vertically */
    listHeight = (uint16_t)(maxCol * 9);
    totalHeight = selTeamsListBottomY - selTeamsListTopY;
    startY = ((totalHeight - listHeight) >> 1) + selTeamsListTopY;

    /* Set Y positions for all 3 columns × 22 rows (entries starting at offset $38 = entry 1) */
    e = &menuEntries[1];
    for (col = 2; col >= 0; col--)
    {
        uint16_t y = startY;
        for (row = 0x15; row >= 0; row--)
        {
            e->y = y;
            y += 9;
            e++;
        }
    }
}


/* ================================================================
 * Chunk 139 — Team Selection Type Handlers + League/Country Names
 * ================================================================ */

/* Externs for Chunk 139 */
extern uint16_t teamsType;
extern uint16_t chooseTeamsResult;
extern uint16_t viewSelectedTeams;
extern uint16_t changeTeams;
extern uint16_t abortSelectTeams;
extern uint16_t exitSelectTeams;
extern uint16_t numLeaguesInCountry;
extern uint32_t teamNames[];          /* array of string pointers */
extern uint16_t chooseTeamsNumTeams;
extern uint32_t countryNames[];       /* array of string pointers */
extern uint16_t teamsLowerBound;

extern void _SetExitMenuFlag(void);

/* Constants */
#define CLUB_TEAMS             1
#define ORIGINAL_CUSTOM_TEAMS  2
#define EDITED_CUSTOM_TEAMS    3
#define PINK_TO_BROWN_7        7


/* ================================================================
 * ClubTeams  (SWOS2.s line 62995)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ClubTeams(void)
{
    teamsType = CLUB_TEAMS;
    chooseTeamsResult = 0;
    viewSelectedTeams = 0;
    changeTeams = 0;
    abortSelectTeams = 0;
    exitSelectTeams = 0;
    _SetExitMenuFlag();
}


/* ================================================================
 * NationalTeams  (SWOS2.s line 63006)
 * ================================================================ */
__attribute__((used, externally_visible))
void _NationalTeams(void)
{
    teamsType = 0;
    chooseTeamsResult = 0;
    viewSelectedTeams = 0;
    changeTeams = 0;
    abortSelectTeams = 0;
    exitSelectTeams = 0;
    _SetExitMenuFlag();
}


/* ================================================================
 * OriginalCustomTeams  (SWOS2.s line 63018)
 * ================================================================ */
__attribute__((used, externally_visible))
void _OriginalCustomTeams(void)
{
    teamsType = ORIGINAL_CUSTOM_TEAMS;
    chooseTeamsResult = 0;
    viewSelectedTeams = 0;
    changeTeams = 0;
    abortSelectTeams = 0;
    exitSelectTeams = 0;
    _SetExitMenuFlag();
}


/* ================================================================
 * EditedCustomTeams  (SWOS2.s line 63029)
 * ================================================================ */
__attribute__((used, externally_visible))
void _EditedCustomTeams(void)
{
    teamsType = EDITED_CUSTOM_TEAMS;
    chooseTeamsResult = 0;
    viewSelectedTeams = 0;
    changeTeams = 0;
    abortSelectTeams = 0;
    exitSelectTeams = 0;
    _SetExitMenuFlag();
}


/* ================================================================
 * ExitChooseTeams  (SWOS2.s line 63040)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ExitChooseTeams(void)
{
    teamsType = 0xFFFF;
    chooseTeamsResult = 0;
    viewSelectedTeams = 0;
    changeTeams = 0;
    abortSelectTeams = 1;
    exitSelectTeams = 0;
    _SetExitMenuFlag();
}


/* ================================================================
 * SetLeagueNames  (SWOS2.s line 62387)
 *
 * Populates menu entries for league names in team selection.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetLeagueNames(void)
{
    MenuEntry *e;
    uint32_t *names;
    int16_t count;

    if (numLeaguesInCountry == 0)
        return;

    names = teamNames;
    /* Entry at $268/56 = 11 (the first lea at $38 is dead code in asm) */
    e = &menuEntries[11];

    count = (int16_t)numLeaguesInCountry - 1;

    do {
        e->isInvisible = 0;
        e->fg = *names++;

        /* If name starts with '.', set special background color */
        {
            const uint8_t *str = (const uint8_t *)e->fg;
            if (*str == '.')
            {
                /* Write PINK_TO_BROWN_7 to high word of bg (68k move.w to bg offset) */
                *(uint16_t *)&e->bg = PINK_TO_BROWN_7;
            }
        }

        e++;
    } while (--count >= 0);
}


/* ================================================================
 * SetCountryNames  (SWOS2.s line 62412)
 *
 * Populates menu entries for country team names in team selection.
 * Handles column-based layout and makes non-showing entries invisible.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetCountryNames(void)
{
    uint32_t *names = countryNames;
    MenuEntry *e = &menuEntries[21]; /* $498/56 = 21 */
    uint16_t d6 = startingTeamIndex;

    for (;;)
    {
        e->isInvisible = 1;

        if (d6 >= chooseTeamsNumTeams)
            goto next_entry;

        {
            uint16_t lower = teamsLowerBound;

            for (;;)
            {
                uint16_t d1 = lower;
                uint16_t ord = e->ordinal;
                uint16_t d0_q, d0_r;

                /* (ordinal - 21) / 22 → quotient in low word, remainder in high word after swap */
                {
                    uint32_t divResult = (uint32_t)(ord - 21) / 22;
                    uint32_t modResult = (uint32_t)(ord - 21) % 22;
                    d0_q = (uint16_t)divResult;
                    d0_r = (uint16_t)modResult;
                }

                if (d0_q == 0)
                    d1 += numTeamsColumn1;
                else if (d0_q == 1)
                    d1 += numTeamsColumn2;
                else
                {
                    d1 = lower;
                    d1 += numTeamsColumn3;
                }

                /* Check if remainder (row) is within [lower, d1) */
                if (d0_r >= lower && d0_r < d1)
                {
                    /* Show this entry */
                    e->isInvisible = 0;
                    e->fg = *names++;

                    /* If name starts with '.', set special background color */
                    {
                        const uint8_t *str = (const uint8_t *)e->fg;
                        if (*str == '.')
                        {
                            *(uint16_t *)&e->bg = PINK_TO_BROWN_7;
                        }
                    }
                    break; /* goto next_entry */
                }

                /* Skip entry — make next entry invisible and continue */
                e++;
                if (e->ordinal == 87)
                    return;
                e->isInvisible = 1;
                /* Continue inner loop with same lower bound */
            }
        }

next_entry:
        e++;
        if (e->ordinal == 87)
            return;
        d6++;
    }
}

/* ---- Chunk 141: SetMenuEntryPlayerFace ---- */
#define TEAMFILE_PLAYERS           76
#define PLAYERFILE_POSITIONANDFACE 26
#define SPR_PL_FACE_WHITE_GUY     186

__attribute__((used, externally_visible))
void _SetMenuEntryPlayerFace(MenuEntry *entry, uint8_t *teamData) {
    uint16_t d0 = teamData[TEAMFILE_PLAYERS + PLAYERFILE_POSITIONANDFACE];
    d0 &= 0x18;
    d0 >>= 3;
    d0 += SPR_PL_FACE_WHITE_GUY;
    *(uint16_t *)&entry->fg = d0;
}

/* ---- Chunk 142: SaveCareer ---- */
/* SWOS2.s line 55472 — save career to disk */
__attribute__((used, externally_visible))
void _SaveCareer(void)
{
    uint8_t *title = (uint8_t *)"SAVE CAREER";

    if (_Asset_PromptInsertSaveDiskDf0(title) != 0)
        return;

    if (_SelectFileToSaveDialog(0x2E434152, competitionFilename, title) != 0)
        goto show_insert;

    if (_SaveCareerFile(selectedFilename) != 0)
        goto show_insert;

    contestStarted = 1;
    _StringCopy(selectedFilename, competitionFilename);

show_insert:
    _ShowInsertDisk2Dialog(title);
}

/* ---- Chunk 143: ExitOptions ---- */
/* SWOS2.s line 46546 — options menu exit callback with prerelease easter egg */
/* showPrereleaseCounter declared extern at line 1027 (asm variable at SWOS2.s:46562) */

__attribute__((used, externally_visible))
void _ExitOptions(void)
{
    if (controlMask == 0x20) {
        _SetExitMenuFlag();
        return;
    }
    if (controlMask == 0x30) {
        showPrereleaseCounter++;
        if (showPrereleaseCounter == 15) {
            menuEntries[14].isInvisible = 0;
            _CallDrawMenuItem(&menuEntries[14]);
        }
    }
}

/* ---- Chunk 143: SetSquadMenuEntryStatusSprite ---- */
/* SWOS2.s line 54885 — set player status sprite on squad menu entry */
/* Writes sprite ID to high word of entry->fg based on player injury/status bits.
   flag==0: during-match status bits (mask 0x1C), flag!=0: after-match bits (mask 0x03) */
extern uint16_t _ShouldApplyRandomPlayerStatusAfterSim(void);

__attribute__((used, externally_visible))
void _SetSquadMenuEntryStatusSprite(MenuEntry *entry, uint8_t *player, uint16_t flag)
{
    uint16_t d1 = player[0x66] & 0x18;
    d1 >>= 3; /* face index 0-3 */
    uint16_t d2, d3;
    uint16_t sprite;

    if (flag == 0) {
        /* Path 0: during-match status */
        d2 = player[0x67] & 0x1C;
        d3 = player[0x67] & 0xE0;

        if (d2 == 0) {
            if (d3 == 0) {
                *(uint16_t *)&entry->fg = d1 + 0xBA;
                return;
            }
            if (d3 == 0x20) {
                *(uint16_t *)&entry->fg = d1 + 0x99;
                return;
            }
            goto injury_0;
        }
        /* d2 != 0 */
        if (d3 == 0 || d3 == 0x20) {
            sprite = (d2 - 4) / 4 + 0x91;
            goto write_fg_bg_0;
        }
        if (d2 == 4) {
            goto injury_0;
        }
        {
            uint16_t c0 = (d2 - 8) >> 2;
            uint16_t c1 = (d3 - 0x40) >> 5;
            if (c0 >= c1) {
                sprite = (d2 - 4) / 4 + 0x91;
                goto write_fg_bg_0;
            }
        }
    injury_0:
        if (player >= careerTeam && player < (uint8_t *)&careerNumPlayers) {
            if (_ShouldApplyRandomPlayerStatusAfterSim() == 0) {
                sprite = 0x98;
                goto write_fg_bg_0;
            }
        }
        sprite = (d3 - 0x40) / 32 + 0x9C;
    write_fg_bg_0:
        *(uint16_t *)&entry->fg = sprite;
        *(uint16_t *)&entry->bg &= 0xFFF0;
        return;
    }

    /* Path 1: after-match status (flag != 0) */
    d2 = player[0x67] & 0x03;
    d3 = player[0x67] & 0xE0;

    if (d2 == 0) {
        if (d3 == 0) {
            *(uint16_t *)&entry->fg = d1 + 0xBA;
            return;
        }
        if (d3 == 0x20) {
            *(uint16_t *)&entry->fg = d1 + 0x99;
            return;
        }
        goto injury_1;
    }
    /* d2 != 0 */
    if (d3 == 0 || d3 == 0x20) {
        sprite = d2 - 1 + 0x92;
        goto write_fg_bg_1;
    }
    {
        uint16_t c0 = d2 - 1;
        uint16_t c1 = (d3 - 0x40) >> 5;
        if (c0 >= c1) {
            sprite = d2 - 1 + 0x92;
            goto write_fg_bg_1;
        }
    }
injury_1:
    if (player >= careerTeam && player < (uint8_t *)&careerNumPlayers) {
        if (_ShouldApplyRandomPlayerStatusAfterSim() == 0) {
            sprite = 0x98;
            goto write_fg_bg_1;
        }
    }
    sprite = (d3 - 0x40) / 32 + 0x9C;
write_fg_bg_1:
    *(uint16_t *)&entry->fg = sprite;
    *(uint16_t *)&entry->bg &= 0xFFF0;
}

/* ---- Chunk 143: SetSquadMenuEntryStatusSprite_AfterSim ---- */
/* SWOS2.s line 55022 — after-sim variant: check random status first */
__attribute__((used, externally_visible))
void _SetSquadMenuEntryStatusSprite_AfterSim(MenuEntry *entry, uint8_t *player)
{
    if (_ShouldApplyRandomPlayerStatusAfterSim() == 0) {
        _SetSquadMenuEntryStatusSprite(entry, player, 0);
        return;
    }
    /* After sim path — extract face and status for sprite */
    uint16_t d1 = player[0x66] & 0x18;
    d1 >>= 3;
    uint16_t d3 = player[0x67] & 0xE0;

    if (d3 == 0) {
        *(uint16_t *)&entry->fg = d1 + 0xBA;
        return;
    }
    if (d3 == 0x20) {
        *(uint16_t *)&entry->fg = d1 + 0x99;
        return;
    }
    /* Injured */
    *(uint16_t *)&entry->fg = (d3 - 0x40) / 32 + 0x9C;
}
