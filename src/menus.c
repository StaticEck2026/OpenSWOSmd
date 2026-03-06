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
#include "career_status.h"
#include "chairman_memo.h"
#include "play_match_menu.h"

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
extern void _PlayTrainingGame(void);
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
    _PlayTrainingGame();
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
 * Chunk 208: SquadUpdateGoalsMenuText + SquadCommon +
 *            SquadSwapPlayersAtIndices (3 → menus.c)
 * ================================================================ */

extern uint16_t removedPlayerStatAccum;
extern uint16_t squadPrevSelectedBreakPos;
extern MenuEntry *squadPrevHighlightedMenuEntryPtr;
extern uint8_t aGoals[];
extern uint8_t  currentMatchPlayers[];
extern uint16_t careerNumPlayers;
extern void _CallDrawMenuItem(MenuEntry *entry);
extern void _SetBenchPlayersNumbers(uint8_t *teamPtr);
extern uint8_t *_GetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal);
extern uint16_t _cGetPlayerAtIndexFileIdx;

/* SquadUpdateGoalsMenuText (SWOS2.s line 48686)
 * Computes total goals for all players in current squad and
 * writes "GOALS (N)" string into menu entry 16's fg buffer. */
__attribute__((used, externally_visible))
void _SquadUpdateGoalsMenuText(void)
{
    uint8_t *teamBase = (uint8_t *)squadTeamFilePtr;
    uint8_t *playerList = teamBase + 0x3C;    /* TeamFile.playerNumbers */
    uint16_t count = 16;
    uint16_t totalGoals = 0;

    if ((int16_t)selTeamNumber != -1 &&
        selTeamNumber == *(uint16_t *)teamBase) {
        playerList = currentMatchPlayers;
        count = careerNumPlayers;
        totalGoals = removedPlayerStatAccum;
    }

    /* Add field at TeamFile offset 0x16 (writesZeroHere) */
    totalGoals += (uint16_t)(uint8_t)teamBase[0x16];

    count--;   /* subq.w #1,d7 for dbf loop */
    uint16_t i;
    for (i = count + 1; i > 0; i--) {
        int16_t idx = (int16_t)(int8_t)*playerList++;
        uint8_t *blockPtr = teamBase + (uint32_t)(uint16_t)idx * 38;
        PlayerFile *pf = (PlayerFile *)(blockPtr + 76);
        totalGoals += (uint16_t)pf->goalStats[0];
        totalGoals += (uint16_t)pf->goalStats[1];
        totalGoals += (uint16_t)pf->goalStats[2];
        totalGoals += (uint16_t)pf->goalStats[3];
    }

    /* Build "GOALS (N)" string in entry 16's fg buffer */
    uint8_t *buf = (uint8_t *)menuEntries[16].fg;
    buf = _StringCopy((const uint8_t *)aGoals, buf);
    *buf++ = ' ';
    *buf++ = '(';
    _Int2Ascii(totalGoals, buf);
    while (*buf) buf++;
    *buf++ = ')';
    *buf = 0;
}

/* SquadSwapPlayersAtIndices (SWOS2.s line 49055)
 * Swaps two players at given break positions.  If the swap crosses
 * the starting 11 / substitutes boundary (one < 11, one >= 11, or
 * either >= 16), also swaps playerPositionsArray and shirt numbers.
 * Always swaps the player number array entry. */
static void squadSwapPlayersAtIndices(uint16_t prevFileIdx, uint16_t currFileIdx,
                                       uint16_t prevBreakPos, uint16_t currBreakPos)
{
    /* Check if full swap needed (cross-boundary or either >= 16) */
    int needFullSwap = 0;
    if (prevBreakPos >= 16 || currBreakPos >= 16) {
        needFullSwap = 1;
    } else if (prevBreakPos >= 11 && currBreakPos < 11) {
        needFullSwap = 1;
    } else if (currBreakPos >= 11 && prevBreakPos < 11) {
        needFullSwap = 1;
    }

    if (needFullSwap) {
        /* Full swap: get player pointers via GetPlayerAtIndex */
        uint16_t teamNum = *(uint16_t *)(uint32_t)squadTeamFilePtr;
        uint8_t *prevBlockPtr = _GetPlayerAtIndex(teamNum, prevBreakPos);
        uint16_t prevFIdx = _cGetPlayerAtIndexFileIdx;

        uint8_t *currBlockPtr = _GetPlayerAtIndex(teamNum, currBreakPos);
        uint16_t currFIdx = _cGetPlayerAtIndexFileIdx;

        /* Swap playerPositionsArray entries */
        uint8_t tmp = playerPositionsArray[currFIdx];
        playerPositionsArray[currFIdx] = playerPositionsArray[prevFIdx];
        playerPositionsArray[prevFIdx] = tmp;

        /* Swap shirt numbers (block_ptr + 0x4E = PlayerFile.shirtNumber) */
        PlayerFile *prevPf = (PlayerFile *)(prevBlockPtr + 76);
        PlayerFile *currPf = (PlayerFile *)(currBlockPtr + 76);
        int8_t tmpShirt = currPf->shirtNumber;
        currPf->shirtNumber = prevPf->shirtNumber;
        prevPf->shirtNumber = tmpShirt;
    }

    /* Always swap player number array entries at break positions */
    uint8_t *numArray;
    if (gameType == 4) {   /* GAME_TYPE_CAREER */
        numArray = currentMatchPlayers;
    } else {
        numArray = (uint8_t *)squadTeamFilePtr + 0x3C;  /* TeamFile.playerNumbers */
    }
    uint8_t tmpNum = numArray[prevBreakPos];
    numArray[prevBreakPos] = numArray[currBreakPos];
    numArray[currBreakPos] = tmpNum;
}

/* SquadCommon (SWOS2.s line 49009)
 * Menu callback for squad entry selection.  Computes the selected
 * slot from entry y-coordinate delta, resolves via
 * GetPlayerInfoBreakPosition, handles swap or select/deselect. */
__attribute__((used, externally_visible))
void _SquadCommon(void *entryPtr)
{
    MenuEntry *a5 = (MenuEntry *)entryPtr;
    uint16_t d6 = a5->ordinal - menuEntries[28].ordinal;
    uint16_t d1 = d6;

    uint8_t *teamPtr = (uint8_t *)squadTeamFilePtr;
    uint16_t outSlot, outPlayerIndex;
    _GetPlayerInfoBreakPosition(teamPtr, d1, &outSlot, &outPlayerIndex);
    d6 = outSlot;
    uint16_t d7 = outPlayerIndex;

    if ((int16_t)squadPrevSelectedPlayerIndex >= 0) {
        if (d7 != squadPrevSelectedPlayerIndex) {
            /* Swap the two players */
            squadSwapPlayersAtIndices(
                squadPrevSelectedPlayerIndex, d7,
                squadPrevSelectedBreakPos, d6);
            _SetBenchPlayersNumbers((uint8_t *)squadTeamFilePtr);
            ((uint8_t *)squadTeamFilePtr)[0x3B] = 1;
            _SquadCommitPositionsAndUpdateSkill();
            squadPrevSelectedPlayerIndex = 0xFFFF;
            _DrawMenu();
            return;
        }
        /* Same player selected twice → deselect */
        d7 = 0xFFFF;
    }

    squadPrevSelectedPlayerIndex = d7;
    squadPrevSelectedBreakPos = d6;

    MenuEntry *oldEntry = squadPrevHighlightedMenuEntryPtr;
    squadPrevHighlightedMenuEntryPtr = a5;
    if (oldEntry != (MenuEntry *)0) {
        _CallDrawMenuItem(oldEntry);
    }
    _CallDrawMenuItem(a5);
    _SquadMenuSavePlayerPositions();
    _SquadRedrawPlayerListAndCheckMark();
}

/* ================================================================
 * Chunk 209: SquadInit + SquadFinish (2 → menus.c)
 * ================================================================ */

extern uint16_t isNationalSquad;
extern uint16_t squadDisplayCount;
extern uint8_t careerTeam[];
extern uint32_t currentCompetitionPtr;
extern uint32_t secondaryCompetitionPtr;
extern uint32_t tertiaryCompetitionPtr;
extern uint32_t quaternaryCompetitionPtr;
extern uint32_t countriesTable[];
extern uint32_t playerPositionsStringTable[];
extern uint8_t aClubs[];
extern uint16_t isNationalSelector;
extern void _SetCurrentEntry(uint16_t entry);
extern uint8_t *_GetCompetitionAbbrevStr(uint8_t *competition);
extern void _GetBankBalance(uint8_t *dst);
extern uint8_t shortCountryNames[];

/* Helper: set the low nibble of the bg field's high word (68k big-endian color index) */
static void setBgColorIndex(MenuEntry *e, uint16_t colorIdx) {
    uint16_t bgHigh = (uint16_t)(e->bg >> 16);
    bgHigh = (bgHigh & 0xFFF0) | colorIdx;
    e->bg = ((uint32_t)bgHigh << 16) | (e->bg & 0xFFFF);
}

/* SquadUpdateMenuState (SWOS2.s line 48491)
 * Updates squad menu entry visibility, colors, and strings based on
 * current mode (view vs edit), competition state, and team status. */
__attribute__((used, externally_visible))
void _SquadUpdateMenuState(void)
{
    _UpdateBigCheckMark();
    uint8_t *a6 = (uint8_t *)squadTeamFilePtr;

    /* Entry 2: bg color based on team type byte (offset 4) */
    setBgColorIndex(&menuEntries[2], 12); /* ORANGE_TO_BROWN */
    if (a6[4] != 1) {
        setBgColorIndex(&menuEntries[2], 11); /* BLUE_TO_PURPLE */
        if (a6[4] != 2) {
            setBgColorIndex(&menuEntries[2], 13); /* BRIGHT_BLUE_TO_BLUE */
        }
    }

    /* Entry 3: set fg string to "(COUNTRY)" */
    {
        uint8_t *dst = (uint8_t *)menuEntries[3].fg;
        uint8_t countryIdx = a6[0];
        uint8_t *countryStr = (uint8_t *)countriesTable[countryIdx];
        *dst++ = '(';
        dst = _StringCopy(countryStr + 1, dst);
        *dst++ = ')';
        *dst = 0;
    }

    /* Entry 25: fg = team byte at offset 0x16 */
    menuEntries[25].fg = (uint32_t)(uint16_t)(uint8_t)a6[0x16];

    /* Entries 15, 13 initially invisible */
    menuEntries[15].isInvisible = 1;
    menuEntries[13].isInvisible = 1;
    if (isSquadViewMode == 0) {
        menuEntries[13].isInvisible = 0;
    }

    /* Entry 14 initially invisible */
    menuEntries[14].isInvisible = 1;
    {
        uint8_t *teamPtr = (uint8_t *)squadTeamFilePtr;
        if (teamPtr[0] >= 0x50 && teamPtr[0] <= 0x55) {
            /* do nothing — keep invisible */
        } else if (isSquadViewMode == 0) {
            menuEntries[14].isInvisible = 0;
        }
    }

    if (isSquadViewMode != 0) {
        /* ============ SQUAD VIEW MODE ============ */
        menuEntries[6].isInvisible = 1;

        if ((int16_t)selTeamNumber != -1 &&
            selTeamNumber == *(uint16_t *)a6) {
            menuEntries[26].isInvisible = 0;
            menuEntries[27].isInvisible = 0;
            menuEntries[27].fg = (uint32_t)removedPlayerStatAccum;
        } else {
            menuEntries[26].isInvisible = 1;
            menuEntries[27].isInvisible = 1;
        }

        menuEntries[24].isInvisible = 0;
        menuEntries[25].isInvisible = 0;

        /* Entries 16-22: visible by default */
        {
            int16_t i;
            for (i = 16; i <= 22; i++)
                menuEntries[i].isInvisible = 0;
        }

        menuEntries[8].isInvisible = 1;
        menuEntries[9].isInvisible = 1;

        if (isNationalSquad != 0) {
            /* National squad view: hide competition entries 16-22, stats entries */
            int16_t i;
            for (i = 16; i <= 22; i++)
                menuEntries[i].isInvisible = 1;
            menuEntries[26].isInvisible = 1;
            menuEntries[27].isInvisible = 1;
            menuEntries[24].isInvisible = 1;
            menuEntries[25].isInvisible = 1;
            goto afterEDE;
        }

        /* Check competition pointers for view mode */
        if ((quaternaryCompetitionPtr | secondaryCompetitionPtr | tertiaryCompetitionPtr) == 0) {
            menuEntries[18].isInvisible = 1;
            menuEntries[17].isInvisible = 1;
            menuEntries[19].isInvisible = 1;
            menuEntries[21].isInvisible = 1;
            goto afterEDE;
        }

        if (quaternaryCompetitionPtr == 0) {
            menuEntries[21].isInvisible = 1;
        }

        if (secondaryCompetitionPtr == 0) {
            menuEntries[17].isInvisible = 1;
            menuEntries[19].isInvisible = 1;
            goto afterEDE;
        }

        if (tertiaryCompetitionPtr == 0) {
            menuEntries[17].isInvisible = 1;
            goto afterEDE;
        }

        /* All three non-null: check national selector */
        if (isNationalSelector != 0 &&
            secondaryCompetitionPtr != 0 &&
            tertiaryCompetitionPtr != 0) {
            menuEntries[17].isInvisible = 1;
            menuEntries[20].isInvisible = 0;
            menuEntries[20].x = menuEntries[17].x;
            menuEntries[20].x += 11;
            goto afterEDE;
        }

        menuEntries[19].isInvisible = 1;
        goto afterEDE;

    } else {
        /* ============ SQUAD EDIT MODE ============ */
        menuEntries[6].isInvisible = 1;

        if (gameType == 4 &&
            squadTeamFilePtr == (uint32_t)careerTeam &&
            isNationalSelector == 0) {
            menuEntries[6].isInvisible = 0;
        } else {
            /* teamSkillDiff < 0: adjust entry 8 and 9 positions */
            if ((int16_t)teamSkillDiffAfterChange < 0) {
                menuEntries[8].x = 0xEE;
                menuEntries[8].dwWidth = 0x3C;
                menuEntries[9].x = 0xEE;
                menuEntries[9].dwWidth = 0x3C;

                if (isNationalSquad != 0) {
                    menuEntries[8].x -= 14;
                    menuEntries[8].y = 0xE5;
                    menuEntries[9].x -= 14;
                    menuEntries[9].y = 0xF5;
                }
            }
        }

        /* Entry 6: bg color and disabled state */
        setBgColorIndex(&menuEntries[6], 14); /* YELLOW_GREEN_BLACK */
        menuEntries[6].isDisabled = 0;

        {
            uint8_t *teamPtr = (uint8_t *)squadTeamFilePtr;
            if (teamPtr[0x17] & 1) {
                setBgColorIndex(&menuEntries[6], 9); /* PINK_TO_BROWN */
                menuEntries[6].isDisabled = 1;
            }
        }

        if ((int16_t)teamSkillDiffAfterChange >= 0) {
            menuEntries[6].x = 0xEE;
            menuEntries[6].dwWidth = 0x3C;
            menuEntries[9].x = 0xEE;
            menuEntries[9].dwWidth = 0x3C;
            menuEntries[9].y = 0xDD;

            {
                uint8_t *teamPtr = (uint8_t *)squadTeamFilePtr;
                if (teamPtr[4] != 1 &&
                    teamPtr[0] >= 0x50 && teamPtr[0] <= 0x55) {
                    menuEntries[9].x -= 14;
                }
            }
        }

        /* Entries 26,27 invisible */
        menuEntries[26].isInvisible = 1;
        menuEntries[27].isInvisible = 1;

        /* Entries 16-22 invisible */
        {
            int16_t i;
            for (i = 16; i <= 22; i++)
                menuEntries[i].isInvisible = 1;
        }

        /* Entries 8,9 visible */
        menuEntries[8].isInvisible = 0;
        menuEntries[9].isInvisible = 0;

        if ((int16_t)teamSkillDiffAfterChange < 0) {
            menuEntries[24].isInvisible = 1;
            menuEntries[25].isInvisible = 1;

            if (isNationalSquad != 0) {
                menuEntries[24].isInvisible = 0;
                menuEntries[25].isInvisible = 0;
                /* Make entries 16-22 visible again */
                {
                    int16_t i;
                    for (i = 16; i <= 22; i++)
                        menuEntries[i].isInvisible = 0;
                }
                menuEntries[13].isInvisible = 1;
                goto viewModeCompChecks;
            }
        } else {
            menuEntries[24].isInvisible = 0;
            menuEntries[25].isInvisible = 0;
        }
    }

afterEDE:
    /* Bank balance (entry 12) */
    if (isSquadViewMode == 0 &&
        gameType == 4 &&
        isNationalSelector == 0) {
        uint8_t *teamPtr = (uint8_t *)squadTeamFilePtr;
        if (*(uint16_t *)teamPtr == selTeamNumber) {
            menuEntries[12].isInvisible = 0;
            _GetBankBalance((uint8_t *)menuEntries[12].fg);
            goto afterBankBalance;
        }
    }
    menuEntries[12].isInvisible = 1;

afterBankBalance:
    /* Entry 2: fg = team name (offset 5), Entry 5: fg = manager (offset 0x24) */
    menuEntries[2].fg = (uint32_t)(a6 + 5);
    menuEntries[5].fg = (uint32_t)(a6 + 0x24);
    return;

viewModeCompChecks:
    /* Jumped to from edit mode national squad path — runs view mode competition checks */
    if ((quaternaryCompetitionPtr | secondaryCompetitionPtr | tertiaryCompetitionPtr) == 0) {
        menuEntries[18].isInvisible = 1;
        menuEntries[17].isInvisible = 1;
        menuEntries[19].isInvisible = 1;
        menuEntries[21].isInvisible = 1;
        goto afterEDE;
    }
    if (quaternaryCompetitionPtr == 0) {
        menuEntries[21].isInvisible = 1;
    }
    if (secondaryCompetitionPtr == 0) {
        menuEntries[17].isInvisible = 1;
        menuEntries[19].isInvisible = 1;
        goto afterEDE;
    }
    if (tertiaryCompetitionPtr == 0) {
        menuEntries[17].isInvisible = 1;
        goto afterEDE;
    }
    if (isNationalSelector != 0 &&
        secondaryCompetitionPtr != 0 &&
        tertiaryCompetitionPtr != 0) {
        menuEntries[17].isInvisible = 1;
        menuEntries[20].isInvisible = 0;
        menuEntries[20].x = menuEntries[17].x;
        menuEntries[20].x += 11;
        goto afterEDE;
    }
    menuEntries[19].isInvisible = 1;
    goto afterEDE;
}

/* ================================================================
 * Chunk 211: SquadAfterDrawCommon (1 → menus.c)
 * ================================================================ */

extern uint8_t playerPositionsArray[];
extern uint8_t oldPlayerPositionsArray[];
extern uint16_t squadRowMode;
extern uint16_t squadSelectedPlayerIndex;
extern uint32_t squadSelectedPlayerPtr;
extern uint8_t fundsBuffer[];
extern uint8_t careerUnavailablePlayerNames[];

/* Helper: set color index on both entries 46 and 48 */
static void setColorPair46_48(uint16_t colorIdx) {
    setBgColorIndex(&menuEntries[46], colorIdx);
    setBgColorIndex(&menuEntries[48], colorIdx);
}

/* SquadAfterDrawCommon (SWOS2.s line 48777)
 * AfterDraw callback for squad menu player rows. Sets row colors,
 * fills in player data for entries 46-63, then draws them. */
__attribute__((used, externally_visible))
void _SquadAfterDrawCommon(void *entryPtr)
{
    MenuEntry *a5 = (MenuEntry *)entryPtr;
    uint16_t d7 = a5->ordinal - 0x1C;
    uint8_t *teamBase = (uint8_t *)squadTeamFilePtr;
    uint16_t d5;

    /* Set row background colors based on row index and team type */
    if (d7 == 0x10) {
        d5 = 1;
        setColorPair46_48(8);
    } else {
        d5 = 0;
        if (d7 >= 11) {
            /* Substitutes */
            if (teamBase[4] == 1) {
                setColorPair46_48(12);
            } else {
                setColorPair46_48(11);
            }
        } else {
            /* Starting 11 */
            if (teamBase[4] == 1) {
                setColorPair46_48(10);
            } else if (d7 == 0) {
                setColorPair46_48(14); /* goalkeeper */
            } else {
                setColorPair46_48(13);
            }
        }
    }

    /* Get player info at this row */
    uint16_t d6_breakPos, d4_fileIdx;
    uint8_t *player = _GetPlayerInfoBreakPosition(teamBase, d7, &d6_breakPos, &d4_fileIdx);

    /* Check if injured */
    uint16_t injured = _IsPlayerInjured(player, (int16_t)squadRowMode);
    if (injured != 0) {
        setColorPair46_48(10);
    }

    /* Check if this is the previously selected player */
    if (d4_fileIdx == squadPrevSelectedPlayerIndex) {
        setColorPair46_48(6);
    }

    squadSelectedPlayerIndex = d4_fileIdx;
    squadSelectedPlayerPtr = (uint32_t)player;

    uint16_t yPos = a5->y;

    /* Entry 46: status sprite */
    MenuEntry *e = &menuEntries[46];
    e->y = yPos;
    _SetSquadMenuEntryStatusSprite(e, player, (uint16_t)squadRowMode);

    /* Check position arrow (up/down indicator) */
    if (d6_breakPos < 11) {
        uint8_t curPos = playerPositionsArray[d4_fileIdx];
        uint8_t oldPos = oldPlayerPositionsArray[d4_fileIdx];
        if (curPos < oldPos) {
            e->fg = 0xD1; /* down arrow */
        } else if (curPos > oldPos) {
            e->fg = 0xD0; /* up arrow */
        }
    }

    /* Entry 47: shirt number (hidden if d5==1 i.e. row 16) */
    e = &menuEntries[47];
    e->isInvisible = 1;
    if (d5 == 0) {
        e->fg = (uint32_t)(uint16_t)(uint8_t)player[0x4E];
        e->isInvisible = 0;
    }
    e->y = yPos;

    /* Entry 48: player name */
    e = &menuEntries[48];
    e->fg = (uint32_t)(player + 0x4F);
    e->y = yPos;

    /* Entry 49: position string */
    e = &menuEntries[49];
    {
        uint8_t posAndFace = player[0x66];
        uint16_t posIdx = (posAndFace & 0xE0) >> 3;
        e->fg = playerPositionsStringTable[posIdx / 4];
    }
    e->y = yPos;

    /* Entry 50: top skills string */
    e = &menuEntries[50];
    _FormTopSkillsString((uint8_t *)e->fg, player);
    e->y = yPos;

    /* Entries 51-54: invisible */
    menuEntries[51].isInvisible = 1; menuEntries[51].y = yPos;
    menuEntries[52].isInvisible = 1; menuEntries[52].y = yPos;
    menuEntries[53].isInvisible = 1; menuEntries[53].y = yPos;
    menuEntries[54].isInvisible = 1; menuEntries[54].y = yPos;

    /* Entry 54: nationality — show if team NOT in [0x50..0x55] and not view mode */
    {
        uint8_t *teamCheck = (uint8_t *)squadTeamFilePtr;
        if (teamCheck[0] >= 0x50 && teamCheck[0] <= 0x55) {
            /* team in range — skip nationality */
        } else if (isSquadViewMode == 0) {
            uint8_t *dst = (uint8_t *)menuEntries[54].fg;
            uint8_t nat = player[0x4C];
            uint16_t natIdx = (uint16_t)nat * 3;
            *dst++ = '(';
            dst[0] = shortCountryNames[natIdx];
            dst[1] = shortCountryNames[natIdx + 1];
            dst[2] = shortCountryNames[natIdx + 2];
            dst += 3;
            *dst++ = ')';
            *dst = 0;
            menuEntries[54].isInvisible = 0;
        }
    }
    menuEntries[54].y = yPos;

    /* Entry 55: goals/stats */
    e = &menuEntries[55];
    e->isInvisible = 1;
    {
        int16_t skillDiff = (int16_t)teamSkillDiffAfterChange;
        if (skillDiff < 0) {
            if (isNationalSquad == 0 && isSquadViewMode == 0) {
                uint16_t sum = (uint16_t)(uint8_t)player[0x6D]
                             + (uint16_t)(uint8_t)player[0x6E]
                             + (uint16_t)(uint8_t)player[0x6F]
                             + (uint16_t)(uint8_t)player[0x70];
                e->fg = (uint32_t)sum;
                e->isInvisible = 0;
            }
        } else {
            e->fg = (uint32_t)(uint16_t)(uint8_t)player[0x6D + skillDiff];
            e->isInvisible = 0;
        }
    }
    e->y = yPos;

    /* Entry 56: invisible */
    menuEntries[56].isInvisible = 1;
    menuEntries[56].y = yPos;

    /* Entry 57: invisible by default */
    menuEntries[57].isInvisible = 1;

    if (isSquadViewMode == 0) {
        uint8_t *teamGeneral = (uint8_t *)squadTeamFilePtr;
        uint16_t isNat = _IsTeamNational(teamGeneral);
        if (isNat != 0) {
            _InitPlayerValueDisplayStruct(player, teamGeneral);
        } else {
            _FormatPlayerValueOrStatusText(player, fundsBuffer);
            menuEntries[57].fg = (uint32_t)fundsBuffer;
        }
        menuEntries[57].isInvisible = 0;

        /* Color based on player status byte 0x71 */
        uint16_t colorIdx = 2;
        if (player[0x71] == 0x9B) {
            colorIdx = 13;
        } else if (player[0x71] == 0x9A) {
            colorIdx = 1;
        } else if (player[0x66] & 0x02) {
            colorIdx = 15;
        }
        {
            uint16_t sf = menuEntries[57].stringFlags;
            sf = (sf & 0xFFF0) | colorIdx;
            menuEntries[57].stringFlags = sf;
        }
    }
    menuEntries[57].y = yPos;

    /* Entry 58: invisible by default */
    menuEntries[58].isInvisible = 1;
    menuEntries[58].y = yPos;

    /* National squad view: show club names in entry 58 */
    if (isNationalSquad != 0 && isSquadViewMode != 0) {
        uint8_t *dst = (uint8_t *)menuEntries[58].fg;
        uint8_t *clubName = careerUnavailablePlayerNames + (uint16_t)(d4_fileIdx * 17);
        *dst++ = '(';
        dst = _StringCopy(clubName, dst);
        *dst++ = ')';
        *dst = 0;
        menuEntries[58].isInvisible = 0;
    }

    /* Entries 59-63: show competition goals when exactly one of
     * isNationalSquad/isSquadViewMode is set (XOR logic) */
    if ((isNationalSquad != 0) == (isSquadViewMode != 0)) {
        /* Both set or both clear: hide entries 59-63 */
        menuEntries[59].isInvisible = 1; menuEntries[59].y = yPos;
        menuEntries[60].isInvisible = 1; menuEntries[60].y = yPos;
        menuEntries[61].isInvisible = 1; menuEntries[61].y = yPos;
        menuEntries[62].isInvisible = 1; menuEntries[62].y = yPos;
        menuEntries[63].isInvisible = 1; menuEntries[63].y = yPos;
    } else {
        /* Show competition goal counts */
        /* Entry 59: first competition goals (if any secondary competitions exist) */
        e = &menuEntries[59];
        e->isInvisible = 1;
        if ((quaternaryCompetitionPtr | secondaryCompetitionPtr | tertiaryCompetitionPtr) != 0) {
            e->fg = (uint32_t)(uint16_t)(uint8_t)player[0x6D];
            e->isInvisible = 0;
        }
        e->y = yPos;

        /* Entry 60: secondary competition goals */
        e = &menuEntries[60];
        e->isInvisible = 1;
        if (secondaryCompetitionPtr != 0) {
            e->fg = (uint32_t)(uint16_t)(uint8_t)player[0x6E];
            e->isInvisible = 0;
        }
        e->y = yPos;

        /* Entry 61: tertiary competition goals */
        e = &menuEntries[61];
        e->isInvisible = 1;
        if (tertiaryCompetitionPtr != 0) {
            e->fg = (uint32_t)(uint16_t)(uint8_t)player[0x6F];
            e->isInvisible = 0;
        }
        e->y = yPos;

        /* Entry 62: quaternary competition goals */
        e = &menuEntries[62];
        e->isInvisible = 1;
        if (quaternaryCompetitionPtr != 0) {
            e->fg = (uint32_t)(uint16_t)(uint8_t)player[0x70];
            e->isInvisible = 0;
        }
        e->y = yPos;

        /* Entry 63: total goals (sum of all 4) */
        e = &menuEntries[63];
        {
            uint16_t total = (uint16_t)(uint8_t)player[0x6D]
                           + (uint16_t)(uint8_t)player[0x6E]
                           + (uint16_t)(uint8_t)player[0x6F]
                           + (uint16_t)(uint8_t)player[0x70];
            e->fg = (uint32_t)total;
            e->isInvisible = 0;
        }
        e->y = yPos;
    }

    /* Draw 18 entries starting from entry 46 */
    _DrawMultipleItems(18, &menuEntries[46]);
}

/* SquadFinish (SWOS2.s line 48543)
 * Called as the squad menu's onReturn/setup callback.
 * Adjusts menu entry positions, sets display count, sets up
 * competition abbreviation strings, etc. */
__attribute__((used, externally_visible))
void _SquadFinish(void)
{
    uint8_t *teamPtr = (uint8_t *)squadTeamFilePtr;

    /* If not national squad and team byte 0 in [0x50..0x55],
     * shift entries 13, 55, 24, 25 left by 13 */
    if (isNationalSquad == 0) {
        if (teamPtr[0] >= 0x50 && teamPtr[0] <= 0x55) {
            menuEntries[13].x -= 13;
            menuEntries[55].x -= 13;
            menuEntries[24].x -= 13;
            menuEntries[25].x -= 13;
        }
    }

    _CheckIsTeamNational();
    _SetBenchPlayersNumbers((uint8_t *)squadTeamFilePtr);

    /* Determine squad display count */
    uint16_t displayCount;
    if ((int16_t)selTeamNumber == -1 ||
        selTeamNumber != *(uint16_t *)teamPtr) {
        displayCount = 16;
    } else {
        displayCount = careerNumPlayers;
    }
    squadDisplayCount = displayCount;

    /* Reset selection state */
    squadPrevHighlightedMenuEntryPtr = (MenuEntry *)0;
    squadPrevSelectedPlayerIndex = 0xFFFF;

    uint8_t *a6 = (uint8_t *)squadTeamFilePtr;

    /* Entries 44, 45 visibility: hidden by default, shown for extra players */
    menuEntries[44].isInvisible = 1;
    menuEntries[45].isInvisible = 1;
    int16_t extraPlayers = (int16_t)squadDisplayCount - 16;
    if (extraPlayers != 0) {
        menuEntries[44].isInvisible = 0;
        if (extraPlayers != 1) {
            menuEntries[45].isInvisible = 0;
        }
    }

    /* Entry 21: set fg to country-specific position string */
    {
        uint8_t countryIdx = (uint8_t)selTeamNumber;
        uint8_t *countryEntry = (uint8_t *)countriesTable[countryIdx];
        int8_t langCode = (int8_t)countryEntry[0];
        int16_t langIdx = (int16_t)langCode;
        /* playerPositionsStringTable + $20 = index 8, then + langIdx */
        menuEntries[21].fg = playerPositionsStringTable[8 + langIdx];
    }

    if (isNationalSquad != 0) {
        /* Set competition abbreviation strings */
        uint8_t *abbrev = _GetCompetitionAbbrevStr((uint8_t *)currentCompetitionPtr);
        MenuEntry *target = &menuEntries[22]; /* default target if no secondary */

        if (secondaryCompetitionPtr != 0) {
            menuEntries[18].fg = (uint32_t)abbrev;
            abbrev = _GetCompetitionAbbrevStr((uint8_t *)secondaryCompetitionPtr);
            target = &menuEntries[19];

            if (tertiaryCompetitionPtr != 0) {
                menuEntries[19].fg = (uint32_t)abbrev;
                abbrev = _GetCompetitionAbbrevStr((uint8_t *)tertiaryCompetitionPtr);
                menuEntries[20].fg = (uint32_t)abbrev;
                menuEntries[20].isInvisible = 0;
                goto skipFgStore;
            }
        }
        target->fg = (uint32_t)abbrev;
    skipFgStore:;
    }

    isSquadViewMode = 0;
    _SquadUpdateMenuState();

    /* Entry 8: GOALS/CLUBS label visibility */
    menuEntries[8].isInvisible = 1;
    if ((int16_t)teamSkillDiffAfterChange < 0) {
        menuEntries[8].isInvisible = 0;
    }

    menuEntries[8].fg = (uint32_t)aGoals;
    if (isNationalSquad != 0) {
        menuEntries[8].fg = (uint32_t)aClubs;
    }

    /* If team type byte (offset 4) == 1 */
    a6 = (uint8_t *)squadTeamFilePtr;
    if (a6[4] == 1) {
        menuEntries[9].isInvisible = 1;
        menuEntries[10].isInvisible = 0;
        _SetCurrentEntry(10);
        if (a6[0] >= 0x50 && a6[0] <= 0x55) {
            menuEntries[10].x = 0xDF;
        }
    }

    _SquadUpdateGoalsMenuText();
    _SquadCommitPositionsAndUpdateSkill();
    _SquadMenuSavePlayerPositions();
}

/* SquadInit (SWOS2.s line 48425)
 * Called as the squad menu's onInit callback.
 * Sets isNationalSquad flag, adjusts menu entry x positions
 * based on which competition pointers are active and
 * teamSkillDiffAfterChange, then falls through to SquadFinish. */
__attribute__((used, externally_visible))
void _SquadInit(void)
{
    /* Set national squad flag */
    isNationalSquad = 0;
    if (squadTeamFilePtr == (uint32_t)careerTeam && isNationalSelector != 0) {
        isNationalSquad = 1;
    }

    /* If all three competition pointers are zero, shift entries right */
    if ((quaternaryCompetitionPtr | secondaryCompetitionPtr | tertiaryCompetitionPtr) == 0) {
        menuEntries[59].x += 20;
        menuEntries[60].x += 20;
        menuEntries[61].x += 20;
        menuEntries[62].x += 20;
        menuEntries[18].x += 20;
        menuEntries[19].x += 20;
        menuEntries[17].x += 20;
        menuEntries[21].x += 20;
    }

    /* If quaternary is null, shift more entries */
    if (quaternaryCompetitionPtr == 0) {
        menuEntries[59].x += 20;
        menuEntries[60].x += 20;
        menuEntries[61].x += 20;
        menuEntries[18].x += 20;
        menuEntries[19].x += 20;
        menuEntries[17].x += 20;
    }

    /* If tertiary is null */
    if (tertiaryCompetitionPtr == 0) {
        menuEntries[59].x += 20;
        menuEntries[60].x += 20;
        menuEntries[18].x += 20;
        menuEntries[19].x += 20;
    }

    /* If secondary is null */
    if (secondaryCompetitionPtr == 0) {
        menuEntries[59].x += 20;
        menuEntries[18].x += 20;
    }

    /* Negative teamSkillDiff: compute shift and apply to many entries */
    if ((int16_t)teamSkillDiffAfterChange < 0) {
        int16_t d1 = (menuEntries[59].x - 0xC7) >> 1;

        if (isNationalSquad != 0) {
            d1 += 8;
            menuEntries[16].x -= 8;
        }

        menuEntries[59].x -= d1;
        menuEntries[60].x -= d1;
        menuEntries[61].x -= d1;
        menuEntries[62].x -= d1;
        menuEntries[63].x -= d1;
        menuEntries[17].x -= d1;
        menuEntries[18].x -= d1;
        menuEntries[19].x -= d1;
        menuEntries[21].x -= d1;
        menuEntries[22].x -= d1;
        menuEntries[24].x -= d1;
        menuEntries[25].x -= d1;
        menuEntries[26].x -= d1;
        menuEntries[27].x -= d1;
    }

    /* Non-negative teamSkillDiff: adjust entries 24, 25, 9 */
    if ((int16_t)teamSkillDiffAfterChange >= 0) {
        menuEntries[24].x -= 0x32;
        menuEntries[24].y -= 0x0A;
        menuEntries[25].x -= 0x32;
        menuEntries[25].y -= 0x0A;
        menuEntries[9].x -= 0x1F;
    }

    /* Fall through to SquadFinish */
    _SquadFinish();
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
extern void _Career_FillCurrentSeasonInfo(void);
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
        _Career_FillCurrentSeasonInfo();
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
extern void _ShowDiyLeaguesTournamentMenu(uint16_t type, uint16_t viewOnly, uint16_t focusOverride);
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
        _ShowDiyLeaguesTournamentMenu(0, 0, 0);
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
            _ShowDiyLeaguesTournamentMenu(0, 0, 0);
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
            _ShowDiyLeaguesTournamentMenu(0, 0, 0);
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
            _ShowDiyLeaguesTournamentMenu(0, 0, 0);
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
            _ShowDiyLeaguesTournamentMenu(0, 0, 0);
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

/* ---- Chunk 147: SetFriendlyPitchOrSeason ---- */
/* SWOS2.s line 49546 — set menu entry 4's fg to pitch type or season string table pointer */
extern uint8_t friendlyPitchTypeTable[];
extern uint8_t friendly_season_table[];

__attribute__((used, externally_visible))
void _SetFriendlyPitchOrSeason(void)
{
    uint8_t *table = friendlyPitchTypeTable;
    if (pitchTypeOrSeason != 0)
        table = friendly_season_table;
    menuEntries[4].fg = (uint32_t)table;  /* 0xE0 / 56 = 4 */
}

/* ---- Chunk 147: PitchTypeOrSeasonProc ---- */
/* SWOS2.s line 49537 — toggle pitch/season mode, update display, redraw entries */
__attribute__((used, externally_visible))
void _PitchTypeOrSeasonProc(void)
{
    pitchTypeOrSeason ^= 1;
    _SetFriendlyPitchOrSeason();
    _CallDrawMultipleItems(2, &menuEntries[3]); /* 0xA8/56=3, count=2 */
}

/* ---- Chunk 147: FriendlyPitchTypeProc ---- */
/* SWOS2.s line 49558 — cycle pitch type (0-6, wrap to RANDOM=0xFFFF) or season (0-11).
 * Called as menu entry callback with a5=entry; redraws current entry after update. */
__attribute__((used, externally_visible))
void _FriendlyPitchTypeProc(MenuEntry *entry)
{
    if (pitchTypeOrSeason == 0) {
        friendlyPitchType++;
        if (friendlyPitchType == 7)
            friendlyPitchType = 0xFFFF; /* RANDOM */
    } else {
        friendlySeason++;
        if (friendlySeason >= 12)
            friendlySeason = 0;
    }
    _CallDrawMenuItem(entry);
}

/* ================================================================
 * Chunk 149: Friendly menu cluster
 * ================================================================ */

#define GAME_TYPE_NO_GAME  0
#define SEASONAL           0xFFFE
#define PINK_TO_BROWN_9    9
#define ENTRY_FRAME_AND_BACK_COLOR 2

extern uint16_t gameType;
extern uint8_t selTeamsPtr;
extern void _SaveSelectedTeams(void);
extern void _RestoreSelectedTeams(void);
extern void _ShowFriendlyMenu(void);
extern void _SetSelectTwoTeamsText(void);
extern void _Int2Ascii(uint16_t value, uint8_t *outBuf);
extern void _GoGetTeamsForPlay(uint8_t *gameTypeStr, uint8_t *playWhatStr,
                               uint16_t numTeams, uint16_t showClub,
                               uint16_t showNational, uint16_t showCustom);
extern void _InitTeams(void);
extern void _PlayFriendly(void);
extern uint16_t _Rand2(void);
extern void _SetCurrentEntry(uint16_t entry);

extern uint8_t aFriendly_1[];       /* "FRIENDLY" */
extern uint8_t aPlayFriendly[];     /* "PLAY FRIENDLY" */
extern uint16_t friendlyOk;
extern uint16_t minSubstitutes;
extern uint16_t maxSubstitutes;

/* Friendly: if no game in progress → show friendly menu, else ContestInProgress. */
__attribute__((used, externally_visible))
void _Friendly(void)
{
    if (gameType == GAME_TYPE_NO_GAME) {
        _ShowFriendlyMenu();
        return;
    }
    /* ContestInProgress: save state, show friendly, restore */
    _SaveSelectedTeams();
    uint16_t savedGameType = gameType;
    uint16_t savedSelTeam = selTeamNumber;
    selTeamNumber = 0xFFFF;
    gameType = GAME_TYPE_NO_GAME;
    _ShowFriendlyMenu();
    selTeamNumber = savedSelTeam;
    gameType = savedGameType;
    _RestoreSelectedTeams();
}

/* DrawNumbersOfSubstitutes: write min/max sub counts to menu entries. */
__attribute__((used, externally_visible))
void _DrawNumbersOfSubstitutes(void)
{
    _Int2Ascii(minSubstitutes, (uint8_t *)menuEntries[5].fg);   /* 0x118/56=5 */
    _Int2Ascii(maxSubstitutes, (uint8_t *)menuEntries[6].fg);   /* 0x150/56=6 */
    _SetFriendlyPitchOrSeason();
}

/* DisableEntries: disable entries 1-6 that have frame+back background. */
__attribute__((used, externally_visible))
void _DisableEntries(void)
{
    int16_t i;
    for (i = 0; i < 6; i++) {
        MenuEntry *e = &menuEntries[1 + i];  /* 0x38/56=1, loop 6 */
        if (e->background == ENTRY_FRAME_AND_BACK_COLOR) {
            uint16_t bgVal = *(uint16_t *)&e->bg;
            if ((bgVal & 0x0F) != 0) {
                bgVal = (bgVal & 0xFFF0) + PINK_TO_BROWN_9;
                *(uint16_t *)&e->bg = bgVal;
                e->isDisabled = 1;
            }
        }
    }
}

/* FriendlyMenuInit: init friendly menu — set header text if OK, fall through to Reinit. */
__attribute__((used, externally_visible))
void _FriendlyMenuInit(void)
{
    if (friendlyOk != 0)
        _SetSelectTwoTeamsText();
    _FriendlyMenuReinit();
}

/* FriendlyMenuReinit: update pitch/season display state and menu entry visibility. */
__attribute__((used, externally_visible))
void _FriendlyMenuReinit(void)
{
    if (pitchType != SEASONAL) {
        pitchTypeOrSeason = 0;
        friendlyPitchType = pitchType;
        /* Disable entry 3 (pitch/season toggle) */
        MenuEntry *e3 = &menuEntries[3];  /* 0xA8/56=3 */
        uint16_t bg3 = *(uint16_t *)&e3->bg;
        bg3 = (bg3 & 0xFFF0) + PINK_TO_BROWN_9;
        *(uint16_t *)&e3->bg = bg3;
        e3->isDisabled = 1;
        /* Disable entry 4 */
        MenuEntry *e4 = &menuEntries[4];  /* 0xE0/56=4 */
        uint16_t bg4 = *(uint16_t *)&e4->bg;
        bg4 = (bg4 & 0xFFF0) + PINK_TO_BROWN_9;
        *(uint16_t *)&e4->bg = bg4;
        e4->isDisabled = 1;
    }

    _DrawNumbersOfSubstitutes();

    if (friendlyOk == 0) {
        menuEntries[7].isInvisible = 1;   /* 0x188/56=7 */
        menuEntries[8].isInvisible = 0;   /* 0x1C0/56=8 */
        menuEntries[10].isInvisible = 1;  /* 0x230/56=10 */
        return;
    }

    _DisableEntries();
    menuEntries[8].isInvisible = 1;       /* 0x1C0/56=8 */
    _SetCurrentEntry(7);
}

/* ChooseTeamsMenu: set up friendly team selection. */
__attribute__((used, externally_visible))
void _ChooseTeamsMenu(void)
{
    _GoGetTeamsForPlay(aFriendly_1, aPlayFriendly, 2, 1, 1, 1);
    if (chooseTeamsResult == 0) {
        _SetExitMenuFlag();
        return;
    }
    _RandomlySwapTeams();
    _InitTeams();
    _PlayFriendly();
    _SetExitMenuFlag();
}

/* MinSubstitutes: increment minSubstitutes with wraparound to 1. */
__attribute__((used, externally_visible))
void _MinSubstitutes(void)
{
    if (maxSubstitutes == 0) return;
    minSubstitutes++;
    if (minSubstitutes > maxSubstitutes)
        minSubstitutes = 1;
    _DrawNumbersOfSubstitutes();
    _CallDrawMenuItem(&menuEntries[5]);   /* 0x118/56=5 */
}

/* MaxSubstitutes: increment maxSubstitutes (0-5), enforce minSubstitutes >= 1. */
__attribute__((used, externally_visible))
void _MaxSubstitutes(void)
{
    maxSubstitutes++;
    if (maxSubstitutes > 5) {
        maxSubstitutes = 0;
        minSubstitutes = 0;
    }
    if (maxSubstitutes == 1)
        minSubstitutes = 1;
    _DrawNumbersOfSubstitutes();
    _CallDrawMenuItem(&menuEntries[6]);   /* 0x150/56=6 */
    _CallDrawMenuItem(&menuEntries[5]);   /* 0x118/56=5 */
}

/* RandomlySwapTeams: 50% chance to swap team 1 and team 2 in selTeamsPtr. */
__attribute__((used, externally_visible))
void _RandomlySwapTeams(void)
{
    uint16_t r = _Rand2();
    if (!(r & 1)) return;
    /* Swap 684 bytes (TEAMFILE_SIZE) between team 0 and team 1 */
    uint16_t *a = (uint16_t *)&selTeamsPtr;
    uint16_t *b = (uint16_t *)(&selTeamsPtr + 684);
    int16_t i;
    for (i = 0; i < 342; i++) {
        uint16_t tmp = a[i];
        a[i] = b[i];
        b[i] = tmp;
    }
}

/* ================================================================
 * Chunk 163: ViewSquad Init/Common/AfterDraw
 * ================================================================ */

extern uint32_t viewSquadTitle;
extern uint32_t viewSquadFlags;
extern uint32_t viewSquadMaskLo;
extern uint32_t viewSquadMaskHi;
extern uint16_t viewSquadArg2;
extern uint16_t viewSquadArg3;
extern uint16_t viewSquadSelectedTeam;

extern uint8_t *_GetPlayerInfoBreakPosition(uint8_t *teamPtr, uint16_t requestedSlot,
                                              uint16_t *outSlot, uint16_t *outPlayerIndex);
extern void _BuildTransferPlayerMenuEntry(void *item, uint8_t *player,
                                           uint16_t y, int16_t idx,
                                           uint16_t style, uint8_t *extra);

/* ViewSquadInit (Chunk 163)
 * Initializes view squad menu: shows/hides entries based on roster size,
 * sets title/team name, handles pre-selection. */
__attribute__((used, externally_visible))
void _ViewSquadInit(void)
{
    uint16_t teamNum = selTeamNumber;
    if (teamNum != 0xFFFF) {
        uint8_t *teamPtr = (uint8_t *)squadTeamPtr;
        if (teamNum == *(uint16_t *)teamPtr) {
            if (careerNumPlayers != 16) {
                goto skipEntryShow;
            }
            menuEntries[20].isInvisible = 0; /* 0x460/56 = 20 */
            if (careerNumPlayers > 17) {
                menuEntries[21].isInvisible = 0; /* 0x498/56 = 21 */
            }
        }
    }
skipEntryShow:

    menuEntries[0].fg = viewSquadTitle;
    uint8_t *teamPtr2 = (uint8_t *)squadTeamPtr;
    menuEntries[1].fg = (uint32_t)(teamPtr2 + 5);

    if (viewSquadArg2) {
        menuEntries[2].isInvisible = 1;  /* 0x70/56 = 2 */
        menuEntries[3].isInvisible = 0;  /* 0xA8/56 = 3 */
        _SetCurrentEntry(3);
    }

    int16_t arg3 = (int16_t)viewSquadArg3;
    if (arg3 < 0)
        return;

    if ((uint16_t)arg3 < 16) {
        _SetCurrentEntry((uint16_t)arg3 + 4);
    } else {
        _SetCurrentEntry(20);
    }
}

/* ViewSquadCommon (Chunk 163)
 * a5 = current menu entry. Computes selected player/team, exits menu. */
__attribute__((used, externally_visible))
void _ViewSquadCommon(void *entryPtr)
{
    MenuEntry *a5 = (MenuEntry *)entryPtr;
    MenuEntry *entry4 = &menuEntries[4]; /* 0xE0/56 = 4 */
    uint16_t relIdx = a5->ordinal - entry4->ordinal;
    viewSquadSelectedTeam = relIdx;

    uint16_t outSlot, outPlayerIndex;
    _GetPlayerInfoBreakPosition((uint8_t *)squadTeamPtr, relIdx,
                                 &outSlot, &outPlayerIndex);
    viewSquadSelectedPlayer = outPlayerIndex;
    _SetExitMenuFlag();
}

/* ViewSquadAfterDrawCommon (Chunk 163)
 * a5 = current menu entry. Draws transfer-style player entry with
 * selection/flag masks overlaid. */
__attribute__((used, externally_visible))
void _ViewSquadAfterDrawCommon(void *entryPtr)
{
    MenuEntry *a5 = (MenuEntry *)entryPtr;
    MenuEntry *entry4 = &menuEntries[4]; /* 0xE0/56 = 4 */
    uint16_t d6 = a5->ordinal - entry4->ordinal;
    uint16_t d1 = d6;
    d6++;

    /* Special case: slot 16 (entry index 16 relative) → set d6 = 0xFFFF */
    if (d1 == 0x10)
        d6 = 0xFFFF;

    uint16_t outSlot, outPlayerIndex;
    uint8_t *player = _GetPlayerInfoBreakPosition(
        (uint8_t *)squadTeamPtr, d1, &outSlot, &outPlayerIndex);

    uint16_t style = 0;
    a5->isDisabled = 0;

    /* Check viewSquadMaskLo */
    if (viewSquadMaskLo & (1u << d1)) {
        a5->isDisabled = 1;
        style = 9;
    }
    /* Check viewSquadFlags — overrides maskLo */
    if (viewSquadFlags & (1u << d1)) {
        a5->isDisabled = 1;
        style = 1;
    }
    /* Check viewSquadMaskHi */
    if (viewSquadMaskHi & (1u << d1)) {
        style = 0xD;
    }
    /* viewSquadArg2 forces disabled */
    if (viewSquadArg2) {
        a5->isDisabled = 1;
    }

    uint16_t yCoord = a5->y;
    _BuildTransferPlayerMenuEntry(&menuEntries[23], player, yCoord,
                                   (int16_t)d6, style, 0);
    _DrawMultipleItems(12, &menuEntries[23]);
}

/* ================================================================
 * Chunk 164: ShowSquad / ShowSquadWithFlags
 * ================================================================ */

extern uint8_t viewSquadMenu[];

/* ShowSquad (Chunk 164)
 * Core squad viewing function: stores params to globals, shows viewSquadMenu,
 * returns selected player ordinal. viewSquadFlags is NOT cleared here —
 * the asm bridge for ShowSquad clears it, ShowSquadWithFlags sets it from d4.
 * Asm: a0=teamPtr, a1=title, d0=maskLo, d1=maskHi, d2=arg2, d3=arg3
 * Returns: d0 = viewSquadSelectedPlayer, d3 = viewSquadSelectedTeam */
__attribute__((used, externally_visible))
int16_t _ShowSquad(uint8_t *teamPtr, uint8_t *title, uint32_t maskLo,
                    uint32_t maskHi, uint16_t arg2, uint16_t arg3)
{
    squadTeamPtr = (uint32_t)teamPtr;
    viewSquadTitle = (uint32_t)title;
    viewSquadMaskLo = maskLo;
    viewSquadMaskHi = maskHi;
    viewSquadArg2 = arg2;
    viewSquadArg3 = arg3;
    _ShowMenu(viewSquadMenu);
    return (int16_t)viewSquadSelectedPlayer;
}

/* CheckNumberOfTeams — update "select N more/less teams" menu entries (SWOS2.s line 60199) */
extern uint8_t aSelect1MoreTea[];  /* "SELECT 1 MORE TEAM" */
extern uint8_t aSelect0MoreTea[];  /* "SELECT %0 MORE TEAMS" */
extern uint8_t aSelect1LessTea[];  /* "SELECT 1 LESS TEAM" */
extern uint8_t aSelect0LessTea[];  /* "SELECT %0 LESS TEAMS" */
extern uint8_t aError[];           /* "ERROR" */

__attribute__((used, externally_visible))
void _CheckNumberOfTeams(void)
{
    /* Entry indices:
     * 86 = "change teams" button (offset 0x1308/56)
     * 87 = "play" button (offset 0x1378/56)
     * 88 = "select N more/less" text (offset 0x13B0/56)
     */
    if (selectTeamsNoLimit) {
        menuEntries[87].isInvisible = 1;
        menuEntries[88].isInvisible = 1;
        menuEntries[86].isInvisible = 1;
        return;
    }

    int16_t diff = (int16_t)(numSelectedTeams - numNeededTeams);
    if (diff == 0) {
        /* Exact count: show "play" button, hide "select more/less" */
        menuEntries[87].isInvisible = 0;
        menuEntries[88].isInvisible = 1;
        menuEntries[87].content = 2;
        menuEntries[87].fg = playWhatStr;
    } else {
        /* Wrong count: hide "play", show "select more/less" */
        menuEntries[87].isInvisible = 1;
        menuEntries[88].isInvisible = 0;
    }

    /* Entry 86 ("change teams") visible only if at least 1 team selected */
    menuEntries[86].isInvisible = (numSelectedTeams == 0) ? 1 : 0;

    /* Build status text in entry 88's string buffer */
    uint8_t *outBuf = (uint8_t *)menuEntries[88].fg;
    uint8_t *fmt;
    uint16_t absDiff;

    if (diff == 0) {
        /* Should not reach here (handled above), but assembly has "ERROR" path */
        _StringCopy(aError, outBuf);
        return;
    } else if (diff < 0) {
        absDiff = (uint16_t)(-diff);
        if (absDiff == 1) {
            _StringCopy(aSelect1MoreTea, outBuf);
            return;
        }
        fmt = aSelect0MoreTea;
    } else {
        absDiff = (uint16_t)diff;
        if (absDiff == 1) {
            _StringCopy(aSelect1LessTea, outBuf);
            return;
        }
        fmt = aSelect0LessTea;
    }
    _PrintFormatted(fmt, outBuf, absDiff, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}


/* ==========================================================================
 * Chunk 201 — ChooseTeamsDialog + ChooseTeamsDialogWithNationality
 * (SWOS2.s lines 54900-54963)
 * ========================================================================== */

extern uint16_t selectedTeamCountryNumber;
extern uint32_t selectedTeamPtr;
extern uint8_t  aStatistics[];

/* ChooseTeamsDialog — set up team selection globals and show choose-teams menu.
 * If teamsType >= 0 after menu, calls SelectTeamsFinalMenu(255).
 * Assembly: d2=showClub, d3=showNational, d4=showOrigCustom, a0=title, a1=onSelect.
 * Returns via globals: selectedTeamCountryNumber, selectedTeamPtr. */
__attribute__((used, externally_visible))
void _ChooseTeamsDialog(uint16_t showClub, uint16_t showNational, uint16_t showOrigCustom,
                        uint8_t *title, void *onSelect)
{
    showClubTeams = showClub;
    showNationalTeams = showNational;
    showOrigCustomTeams = showOrigCustom;
    gameTypeStr = 0;
    chooseTeamsHeader1stPart = (uint32_t)title;
    chooseTeamsFunc = (uint32_t)onSelect;
    careerSquadScreenDidSelectPlayerFlag = 0;
    /* seq instruction: sets high byte of word to 0xFF when a1==0 (big-endian) */
    if (onSelect == (void *)0)
        careerSquadScreenDidSelectPlayerFlag = 0xFF00;
    playWhatStr = (uint32_t)aStatistics;
    numNeededTeams = 1;
    selectTeamsNoLimit = 1;
    selectedTeamCountryNumber = 0xFFFF;
    selectedTeamPtr = 0;
    _ShowMenu(chooseTeamsMenu);
    if ((int16_t)teamsType >= 0) {
        loadTeam = 1;
        showCupsAndOther = 0;
        playSeason = 0;
        _CSelectTeamsFinalMenu(255);
    }
}

/* ChooseTeamsDialogWithNationality — same as ChooseTeamsDialog but passes
 * nationality to SelectTeamsFinalMenu instead of 255.
 * Assembly: d0=nationality, d2=showClub, d3=showNational, d4=showOrigCustom,
 * a0=title, a1=onSelect.
 * Returns via globals: selectedTeamCountryNumber, selectedTeamPtr. */
__attribute__((used, externally_visible))
void _ChooseTeamsDialogWithNationality(uint16_t nationality, uint16_t showClub,
                                       uint16_t showNational, uint16_t showOrigCustom,
                                       uint8_t *title, void *onSelect)
{
    showClubTeams = showClub;
    showNationalTeams = showNational;
    showOrigCustomTeams = showOrigCustom;
    gameTypeStr = 0;
    chooseTeamsHeader1stPart = (uint32_t)title;
    chooseTeamsFunc = (uint32_t)onSelect;
    careerSquadScreenDidSelectPlayerFlag = 0;
    /* seq instruction: sets high byte of word to 0xFF when a1==0 (big-endian) */
    if (onSelect == (void *)0)
        careerSquadScreenDidSelectPlayerFlag = 0xFF00;
    playWhatStr = (uint32_t)aStatistics;
    numNeededTeams = 1;
    selectTeamsNoLimit = 1;
    selectedTeamCountryNumber = 0xFFFF;
    selectedTeamPtr = 0;
    _ShowMenu(chooseTeamsMenu);
    if ((int16_t)teamsType >= 0) {
        loadTeam = 1;
        showCupsAndOther = 0;
        playSeason = 0;
        _CSelectTeamsFinalMenu(nationality);
    }
}


/* ==========================================================================
 * Chunk 202 — SelectTeamToManage
 * (SWOS2.s lines 50038-50050)
 * ========================================================================== */

extern uint8_t aSelectTeamToMa[];

/* SelectTeamToManage — show club team selection dialog for career mode.
 * Wraps ChooseTeamsDialog with showClub=1, showNational=0, showOrigCustom=0.
 * Results available in globals: selectedTeamCountryNumber, selectedTeamPtr. */
__attribute__((used, externally_visible))
void _SelectTeamToManage(void)
{
    _ChooseTeamsDialog(1, 0, 0, aSelectTeamToMa, (void *)0);
}


/* ==========================================================================
 * Chunk 204 — Career_ShowAddInternationalPlayerDialog
 * (SWOS2.s lines 64815-64835)
 * ========================================================================== */

extern uint16_t careerAddIntlPlayerResult;
extern uint32_t careerAddIntlPlayerSelectedSrcPlayerPtr;
extern uint32_t careerAddIntlPlayerSelectedSrcPlayerNamePtr;
extern uint8_t  nationalTeamFileId;
extern uint8_t  aAddInternation[];
extern uint8_t  aAddInternation_0[];
extern void AddInternationalPlayerHome(void);
extern void AddInternationalPlayerAbroad(void);

/* Additional externs for Chunk 205 */
extern uint16_t careerAddIntlPlayerSelectedTeamNumber;
extern uint16_t careerAddIntlPlayerSelectedPlayerOrdinal;
extern uint8_t  nationalTeamNationality;
extern uint16_t careerUnavailablePlayerList[];
extern uint16_t careerUnavailablePlayerLookupTable[];
extern uint8_t  currentMatchPlayers[];
extern uint8_t *_GetTeamPointer(uint16_t teamId);
extern int16_t  _ShowSquad(uint8_t *teamPtr, uint8_t *title, uint32_t maskLo,
                            uint32_t maskHi, uint16_t arg2, uint16_t arg3);
extern void _Career_CopyUnavailablePlayerToCareerTeamSlot(uint8_t *srcPlayer,
    uint8_t *dstTeam, uint8_t *srcName, uint16_t srcTeamNumber,
    uint16_t srcPlayerIndex, uint16_t dstSlotIndex);

/* Career_ShowAddInternationalPlayerDialog — show dialog to add international player.
 * If homeOrAbroadFlag==0: use nationality filter (home), else show all (abroad).
 * Results stored in globals: careerAddIntlPlayerResult,
 * careerAddIntlPlayerSelectedSrcPlayerPtr, careerAddIntlPlayerSelectedSrcPlayerNamePtr. */
__attribute__((used, externally_visible))
void _Career_ShowAddInternationalPlayerDialog(uint16_t homeOrAbroadFlag)
{
    careerAddIntlPlayerResult = 0xFFFF;
    if (homeOrAbroadFlag == 0) {
        /* Home: filter by nationality */
        _ChooseTeamsDialogWithNationality((uint16_t)nationalTeamFileId, 1, 0, 0,
                                          aAddInternation, (void *)AddInternationalPlayerHome);
    } else {
        /* Abroad: show all club teams */
        _ChooseTeamsDialog(1, 0, 0, aAddInternation_0, (void *)AddInternationalPlayerAbroad);
    }
}


/* ==========================================================================
 * Chunk 205 — Career_AddInternationalPlayerCommon,
 *             AddInternationalPlayerHome, AddInternationalPlayerAbroad
 * (SWOS2.s lines 64829-64965)
 *
 * Called as chooseTeamsFunc callback with a0 = selected team entry pointer.
 * Displays a squad view filtered by nationality/unavailability, lets the
 * user pick a player, then copies that player into the career team.
 * ========================================================================== */

/* Career_AddInternationalPlayerCommon
 * selectedTeam: pointer to team selection data (first word = team number)
 * title: menu title string ("ADD INTERNATIONAL PLAYER (HOME/ABROAD)") */
__attribute__((used, externally_visible))
void _Career_AddInternationalPlayerCommon(uint8_t *selectedTeam, uint8_t *title)
{
    uint16_t teamNumber = *(uint16_t *)selectedTeam;
    careerAddIntlPlayerSelectedTeamNumber = teamNumber;

    uint8_t *teamData = _GetTeamPointer(teamNumber);
    TeamFileEx *teamEx = (TeamFileEx *)teamData;
    uint8_t nat = nationalTeamNationality;
    uint16_t teamNum = teamEx->team.globalTeamNumber;

    /* Loop 1: Mark unavailable players with injury flag in cardsInjuries.
     * For each player matching the national team nationality, check if
     * they appear in careerUnavailablePlayerList (4 entries). If found,
     * set injury bits in cardsInjuries. */
    for (uint16_t d2 = 0; d2 < 16; d2++) {
        if (teamEx->players[d2].nationality == (int8_t)nat) {
            uint16_t *entry = careerUnavailablePlayerList;
            for (int16_t d4 = 3; d4 >= 0; d4--) {
                if (entry[0] == teamNum && entry[1] == d2) {
                    uint8_t ci = teamEx->players[d2].cardsInjuries;
                    ci = (ci & 0xFC) | 0x08;
                    teamEx->players[d2].cardsInjuries = ci;
                    break;
                }
                entry += 2;  /* next 4-byte entry */
            }
        }
    }

    /* Loop 2: Build nationality mask — bit set means player does NOT
     * match the national team nationality (greyed out in squad view). */
    uint32_t natMask = 0;
    for (uint16_t d2 = 0; d2 < 16; d2++) {
        if (teamEx->players[d2].nationality != (int8_t)nat) {
            natMask |= (1UL << d2);
        }
    }

    /* Loop 3: Build unavailable mask from careerUnavailablePlayerLookupTable
     * (20 entries). If the entry's team number matches, mark that player
     * index bit in both masks. */
    uint32_t unavailMask = 0;
    uint16_t *lookup = careerUnavailablePlayerLookupTable;
    for (int16_t i = 19; i >= 0; i--) {
        if (lookup[0] == careerAddIntlPlayerSelectedTeamNumber) {
            uint16_t pidx = lookup[1];
            natMask |= (1UL << pidx);
            unavailMask |= (1UL << pidx);
        }
        lookup += 2;  /* next 4-byte entry */
    }

    /* Show squad selection screen */
    careerAddIntlPlayerResult = 0xFFFF;
    careerSquadScreenDidSelectPlayerFlag = 0;
    viewSquadFlags = unavailMask;
    int16_t selected = _ShowSquad(teamData, title, natMask, 0, 0, 0xFFFF);

    if (selected < 0)
        return;

    careerSquadScreenDidSelectPlayerFlag = 1;
    careerAddIntlPlayerSelectedPlayerOrdinal = (uint16_t)selected;

    /* Calculate source player pointers using block pointer convention:
     * block_ptr = teamBase + ordinal * sizeof(PlayerFile)
     * PlayerFile fields at block_ptr + sizeof(TeamFile) */
    uint8_t *srcBlock = teamData + (uint16_t)((uint16_t)selected * sizeof(PlayerFile));
    careerAddIntlPlayerSelectedSrcPlayerPtr = (uint32_t)srcBlock;
    careerAddIntlPlayerSelectedSrcPlayerNamePtr = (uint32_t)(teamData + 5);

    /* Get position type of selected source player */
    uint16_t srcPos = (teamEx->players[selected].positionAndFace & 0xE0) >> 5;

    /* Check if career team is full (max 20 players) */
    if (careerNumPlayers >= 20) {
        careerAddIntlPlayerResult = 1;
        return;
    }

    /* Search for an empty, position-compatible slot in the career team */
    uint16_t dstSlot, dstFileIdx;
    for (int16_t d7 = 0; d7 < 20; d7++) {
        uint8_t *careerPlayer = _GetPlayerInfoBreakPosition(
            careerTeam, (uint16_t)d7, &dstSlot, &dstFileIdx);
        PlayerFile *pf = (PlayerFile *)(careerPlayer + sizeof(TeamFile));

        /* Check if slot is empty (name starts with 0xFF) */
        if ((uint8_t)pf->playerName[0] != 0xFF)
            continue;

        /* Position compatibility:
         * goalkeeper (0) → only goalkeeper slots
         * outfield (>0) → only outfield slots */
        uint16_t dstPos = (pf->positionAndFace & 0xE0) >> 5;
        if (srcPos == 0) {
            if (dstPos != 0)
                continue;
        } else {
            if (dstPos == 0)
                continue;
        }

        /* Found compatible slot. If slot > 16, shift currentMatchPlayers
         * to insert the value at position 16. Use volatile to prevent
         * GCC from optimizing this into a memmove call (no libc). */
        if (dstSlot > 16) {
            volatile uint8_t *cmp = currentMatchPlayers;
            uint8_t saved = cmp[dstSlot];
            uint16_t idx = dstSlot;
            while (idx != 16) {
                cmp[idx] = cmp[idx - 1];
                idx--;
            }
            cmp[16] = saved;
        }

        /* Copy the international player into the career team slot */
        _Career_CopyUnavailablePlayerToCareerTeamSlot(
            srcBlock, careerPlayer,
            teamData + 5,  /* srcName = team name pointer */
            careerAddIntlPlayerSelectedTeamNumber,
            careerAddIntlPlayerSelectedPlayerOrdinal,
            dstFileIdx);

        careerNumPlayers++;
        careerAddIntlPlayerResult = 0;
        return;
    }

    /* No compatible empty slot found */
    careerAddIntlPlayerResult = 1;
}

/* AddInternationalPlayerHome — callback for home nationality team selection.
 * selectedTeam passed in a0 from menu callback (chooseTeamsFunc). */
__attribute__((used, externally_visible))
void _AddInternationalPlayerHome(uint8_t *selectedTeam)
{
    _Career_AddInternationalPlayerCommon(selectedTeam, aAddInternation);
}

/* AddInternationalPlayerAbroad — callback for abroad team selection.
 * selectedTeam passed in a0 from menu callback (chooseTeamsFunc). */
__attribute__((used, externally_visible))
void _AddInternationalPlayerAbroad(uint8_t *selectedTeam)
{
    _Career_AddInternationalPlayerCommon(selectedTeam, aAddInternation_0);
}


/* ==========================================================================
 * Chunk 206 — National Team Selection family
 * (SWOS2.s lines 65499-65745)
 *
 * SelectNationalTeamInit, SelectNationalTeamFinish,
 * Career_RebuildInternationalPlayerSelectionMasks,
 * NationalTeamAddPlayer, NationalTeamRemovePlayers,
 * SelectNationalTeamAbort, SelectNationalTeamCommon,
 * SelectNationalTeamAfterDrawCommon
 * ========================================================================== */

extern uint32_t careerIntlPlayersRemoveMask;
extern uint32_t careerIntlPlayersWrongTypeMask;
extern uint32_t careerIntlPlayersSelectableMask;
extern uint16_t careerNationalTeamHasPendingAddPlayer;
extern uint16_t selectNationalTeamMenuResult;
extern uint16_t careerNationalTeamAddPlayerType;
extern uint8_t  careerIntlPlayerSelectedPlayerDataCopy[];
extern uint8_t  careerIntlPlayerSelectedTeamName[];
extern uint8_t  nationalTeamAddPlayerMenu[];
extern uint8_t  careerUnavailablePlayerNames[];
extern uint16_t selTeamNumber;

extern uint8_t *_GetPlayer(uint16_t teamNumber, uint16_t playerFileIndex);
extern void _Career_UpdateNationalTeamMenuEntries(void);
extern void _Career_UpdateRemovePlayersMenuEntry(void);
extern void _Career_RemoveInternationalPlayersByMask(uint32_t removeMask);

/* SelectNationalTeamInit — onInit for the national team selection menu.
 * Clears remove mask and pending add flag. If squad not full, enables
 * "add player" button (entry 3). */
__attribute__((used, externally_visible))
void _SelectNationalTeamInit(void)
{
    careerIntlPlayersRemoveMask = 0;
    careerNationalTeamHasPendingAddPlayer = 0;
    if (careerNumPlayers != 20) {
        _SetCurrentEntry(3);
    }
}

/* SelectNationalTeamFinish — onReturn for the national team selection menu.
 * Adjusts entry layout, disables play button, sets string flag,
 * then updates menu state. */
__attribute__((used, externally_visible))
void _SelectNationalTeamFinish(void)
{
    menuEntries[38].x -= 25;
    menuEntries[7].isDisabled = 1;
    uint16_t sf = menuEntries[1].stringFlags;
    sf |= (1 << 13);
    menuEntries[1].stringFlags = sf;
    _Career_UpdateNationalTeamMenuEntries();
    _Career_RebuildInternationalPlayerSelectionMasks();
    _Career_UpdateRemovePlayersMenuEntry();
}

/* Career_RebuildInternationalPlayerSelectionMasks
 * Scans career team slots (0-19). For each occupied slot, checks if the
 * player's position type is compatible with the pending add player's type.
 * Builds two masks:
 *   careerIntlPlayersSelectableMask: bit CLEAR = selectable
 *   careerIntlPlayersWrongTypeMask: bit SET = wrong position type */
__attribute__((used, externally_visible))
void _Career_RebuildInternationalPlayerSelectionMasks(void)
{
    /* loc_147376 is a block pointer for the pending player data copy.
     * In asm, loc_147376 = careerIntlPlayerSelectedPlayerDataCopy - 76.
     * The block pointer convention: adding 0x66 reaches positionAndFace. */
    PlayerFile *pendingPf = (PlayerFile *)careerIntlPlayerSelectedPlayerDataCopy;
    uint16_t pendingPos = (pendingPf->positionAndFace & 0xE0) >> 5;

    uint32_t wrongMask = 0;
    uint32_t selectMask = 0xFFFFFFFF;  /* all bits set = all selectable */

    for (int16_t d1 = 19; d1 >= 0; d1--) {
        uint8_t *player = _GetPlayer(selTeamNumber, (uint16_t)d1);
        PlayerFile *pf = (PlayerFile *)(player + sizeof(TeamFile));

        /* Skip empty slots */
        if ((uint8_t)pf->playerName[0] == 0xFF)
            continue;

        /* Check position compatibility if pending add is active */
        if (careerNationalTeamHasPendingAddPlayer) {
            uint16_t slotPos = (pf->positionAndFace & 0xE0) >> 5;
            if (pendingPos == 0) {
                /* Pending is GK: only GK slots are compatible */
                if (slotPos != 0) {
                    wrongMask |= (1UL << d1);
                    continue;
                }
            } else {
                /* Pending is outfield: only non-GK slots */
                if (slotPos == 0) {
                    wrongMask |= (1UL << d1);
                    continue;
                }
            }
        }

        /* Slot is selectable: clear its bit */
        selectMask &= ~(1UL << d1);
    }

    careerIntlPlayersSelectableMask = selectMask;
    careerIntlPlayersWrongTypeMask = wrongMask;
}

/* NationalTeamAddPlayer — shows the home/abroad sub-menu, then lets user
 * select a player. If player selected but no room in career team (result==1),
 * copies player data for pending addition. */
__attribute__((used, externally_visible))
void _NationalTeamAddPlayer(void)
{
    careerIntlPlayersRemoveMask = 0;
    _ShowMenu(nationalTeamAddPlayerMenu);
    int16_t addType = (int16_t)careerNationalTeamAddPlayerType;

    if (addType >= 0) {
        _Career_ShowAddInternationalPlayerDialog((uint16_t)addType);

        if (careerAddIntlPlayerResult == 1) {
            /* No room right now — store as pending add */
            _StringCopy((uint8_t *)(uint32_t)careerAddIntlPlayerSelectedSrcPlayerNamePtr,
                         careerIntlPlayerSelectedTeamName);

            /* Copy 38 bytes of PlayerFile data from source player */
            uint8_t *src = (uint8_t *)(uint32_t)careerAddIntlPlayerSelectedSrcPlayerPtr + sizeof(TeamFile);
            volatile uint8_t *dst = (volatile uint8_t *)careerIntlPlayerSelectedPlayerDataCopy;
            for (int16_t i = 37; i >= 0; i--) {
                *dst++ = *src++;
            }

            careerNationalTeamHasPendingAddPlayer = 1;
            _SetCurrentEntry(5);
            _Career_UpdateRemovePlayersMenuEntry();
        }
    }

    _Career_UpdateNationalTeamMenuEntries();
    _Career_RebuildInternationalPlayerSelectionMasks();
    _DrawMenu();
}

/* NationalTeamRemovePlayers — removes marked international players and
 * updates the UI. */
__attribute__((used, externally_visible))
void _NationalTeamRemovePlayers(void)
{
    _Career_RemoveInternationalPlayersByMask(careerIntlPlayersRemoveMask);
    careerIntlPlayersRemoveMask = 0;
    _Career_UpdateNationalTeamMenuEntries();
    _Career_RebuildInternationalPlayerSelectionMasks();
    _Career_UpdateRemovePlayersMenuEntry();
    _SetCurrentEntry(4);
    if (careerNumPlayers != 20) {
        _SetCurrentEntry(3);
    }
    _DrawMenu();
}

/* SelectNationalTeamAbort — cancels pending add and resets UI. */
__attribute__((used, externally_visible))
void _SelectNationalTeamAbort(void)
{
    careerNationalTeamHasPendingAddPlayer = 0;
    _Career_UpdateNationalTeamMenuEntries();
    _Career_RebuildInternationalPlayerSelectionMasks();
    _Career_UpdateRemovePlayersMenuEntry();
    _SetCurrentEntry(3);
    _DrawMenu();
}

/* SelectNationalTeamCommon — handles clicking a player slot in the
 * national team menu. If a pending add is in progress, places the
 * pending player into the selected slot. Otherwise toggles the
 * remove-mark on the selected player.
 * entry = a5 from asm (current menu entry). */
__attribute__((used, externally_visible))
void _SelectNationalTeamCommon(void *entryPtr)
{
    MenuEntry *a5 = (MenuEntry *)entryPtr;
    /* Get player index from current entry's ordinal relative to entry 8 */
    uint16_t d1 = a5->ordinal - menuEntries[8].ordinal;

    uint16_t slot, fileIdx;
    uint8_t *careerPlayer = _GetPlayerInfoBreakPosition(
        careerTeam, d1, &slot, &fileIdx);

    if (careerNationalTeamHasPendingAddPlayer) {
        /* Place pending player into selected slot.
         * loc_147376 = careerIntlPlayerSelectedPlayerDataCopy - 76 is the
         * "block pointer" for the pending player data copy. */
        uint8_t *pendingBlock = careerIntlPlayerSelectedPlayerDataCopy - sizeof(TeamFile);

        _Career_CopyUnavailablePlayerToCareerTeamSlot(
            pendingBlock, careerPlayer,
            careerIntlPlayerSelectedTeamName,
            careerAddIntlPlayerSelectedTeamNumber,
            careerAddIntlPlayerSelectedPlayerOrdinal,
            fileIdx);

        careerNationalTeamHasPendingAddPlayer = 0;
        _SetCurrentEntry(3);
        _Career_UpdateNationalTeamMenuEntries();
        _Career_UpdateRemovePlayersMenuEntry();
        _Career_RebuildInternationalPlayerSelectionMasks();
        _DrawMenu();
    } else {
        /* Toggle remove mark on this player */
        selectNationalTeamMenuResult = fileIdx;
        careerIntlPlayersRemoveMask ^= (1UL << d1);
        _Career_UpdateRemovePlayersMenuEntry();
        _DrawMenu();
    }
}

/* SelectNationalTeamAfterDrawCommon — after-draw callback for each player
 * slot in the national team menu. Draws player info row with appropriate
 * coloring based on position compatibility, selection state, and remove marks.
 *
 * Uses the "block pointer" convention for BuildTransferPlayerMenuEntry:
 * block_ptr + 0x4C = PlayerFile.nationality, etc.
 * entry = a5 from asm (current menu entry). */
__attribute__((used, externally_visible))
void _SelectNationalTeamAfterDrawCommon(void *entryPtr)
{
    MenuEntry *a5 = (MenuEntry *)entryPtr;
    int16_t d6;
    uint16_t d5;
    uint8_t *a6_player;  /* player block pointer for BuildTransferPlayerMenuEntry */
    uint8_t *a4_extra;   /* extra name pointer */

    if (a5->ordinal == 7) {
        /* Entry 7 = pending add player display slot */
        if (!careerNationalTeamHasPendingAddPlayer)
            return;

        /* Use the pending player data copy as block pointer.
         * loc_147376 = careerIntlPlayerSelectedPlayerDataCopy - 76. */
        a6_player = careerIntlPlayerSelectedPlayerDataCopy - sizeof(TeamFile);
        d6 = -1;  /* 0xFFFF */
        d5 = 0x0D;
        a4_extra = careerIntlPlayerSelectedTeamName;
    } else {
        /* Normal player slot */
        uint16_t relIdx = a5->ordinal - menuEntries[8].ordinal;
        d6 = (int16_t)(relIdx + 1);
        uint16_t d1 = relIdx;

        /* Entry for last slot (ordinal delta == 16) gets d6 = -1 */
        if (d1 == 16)
            d6 = -1;

        uint16_t slot, fileIdx;
        uint8_t *careerPlayer = _GetPlayerInfoBreakPosition(
            careerTeam, d1, &slot, &fileIdx);
        a6_player = careerPlayer;

        /* Default color based on position type */
        d5 = 0;
        PlayerFile *pf = (PlayerFile *)(careerPlayer + sizeof(TeamFile));
        uint16_t posType = (pf->positionAndFace & 0xE0) >> 5;
        if (posType == 0)
            d5 = 0x0E;  /* goalkeeper color */

        /* Disable entry if wrong position type */
        a5->isDisabled = 0;
        if (careerIntlPlayersWrongTypeMask & (1UL << d1)) {
            a5->isDisabled = 1;
            d5 = 9;
        }

        /* Disable entry if not selectable */
        if (careerIntlPlayersSelectableMask & (1UL << d1)) {
            a5->isDisabled = 1;
        }

        /* If not in pending add mode, check remove mask */
        if (!careerNationalTeamHasPendingAddPlayer) {
            if (careerIntlPlayersRemoveMask & (1UL << d1)) {
                d5 = 0x0D;
            }
        }

        /* Set extra name pointer from careerUnavailablePlayerNames */
        a4_extra = careerUnavailablePlayerNames + d1 * 17;
    }

    /* Call BuildTransferPlayerMenuEntry and draw */
    uint16_t y = a5->y;
    _BuildTransferPlayerMenuEntry(
        (void *)&menuEntries[27],
        a6_player,
        y,
        d6,
        d5,
        a4_extra);

    /* Hide entry 37 */
    menuEntries[37].isInvisible = 1;

    _DrawMultipleItems(12, &menuEntries[27]);
}
