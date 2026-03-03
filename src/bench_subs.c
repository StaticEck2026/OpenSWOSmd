/* bench_subs.c — Chunk 39: Bench / Substitution Controls
 *
 * Ported from SWOS2.s lines 12131-13961:
 *   BenchCheckControls, LeaveSubsMenu, BenchMenuHandleControls,
 *   TrainingSwapBenchTeams, IsThisPlayerOkToSelect, FirePressedInSubsMenu,
 *   ShowFormationMenu, SubsBenchOff, SelectPlayerToSubstituteMenuHandler,
 *   CheckForThrowInAndGoalkeepersBall, SubstitutePlayer,
 *   CallUpdateSubstitutePlayerIndices, state3_handle_fire_press,
 *   MarkPlayersMenuHandler, HandleFormationMenuControls,
 *   UpdateSubstitutePlayerIndices, UpdatePlayerToBeSubstitutedPosition,
 *   IsPlayerOkToSubstitute, HandleFormationMenuMovement,
 *   FindInitialPlayerToBeSubstituted, SwapPlayerNameData,
 *   GetBenchPlayerPointer
 *
 * Per-frame controls for in-game substitution/formation/marking menus.
 * BenchCheckControls is called from GameLoop each frame.
 */

#include "bench_subs.h"
#include "game_types.h"
#include "game_state.h"
#include "goalkeeper.h"
#include "match_sim.h"
#include "player_anim.h"
#include "player_sprites.h"

/* ---- external asm globals ---- */
extern uint16_t inSubstitutesMenu;
extern uint32_t benchTeam;          /* pointer stored as uint32_t */
extern uint32_t benchTeamGame;      /* pointer stored as uint32_t */
extern uint16_t subsTeamsSwapped;
extern uint16_t subsDirection;
extern uint8_t  subsFire;
extern uint16_t subsFireTimer;
extern uint16_t subsState;
extern uint16_t benchPlayerIndex;
extern uint16_t subsBlockFire;
extern uint16_t subsBlockDirections;
extern uint16_t subsMovementDelayTimer;
extern uint16_t subsPrevDirection;
extern uint16_t benchOff;
extern uint16_t cameraCoordinatesValid;
extern uint16_t waitForPlayerToGoInTimer;
extern uint16_t cameraLeavingSubsTimer;
extern uint16_t substituteInProgress;
extern uint16_t refState;
extern uint16_t injuriesForever;
extern uint16_t statsTimer;
extern uint16_t gameStatePl;
extern uint16_t gameState;
extern uint16_t whichCard;
extern uint16_t playingPenalties;
extern uint16_t bench1Called;
extern uint16_t bench2Called;
extern uint16_t frameCount;
extern uint16_t trainingGame;
extern uint16_t subsTrainingTopTeam;
extern uint16_t benchY;
extern uint16_t opponentBenchY;
extern uint16_t plComingYTmp;
extern uint16_t plComingY;
extern uint16_t benchWriteOnly01;
extern uint16_t benchXLimit;
extern uint16_t subsMenuX;
extern uint16_t formationMenuX;
extern uint16_t plLeavingX;
extern uint16_t plComingX;
extern uint16_t benchWriteOnly02;
extern uint16_t deadBenchVar;
extern uint16_t leavingSubsMenu;
extern uint16_t overlayActiveFlag;
extern uint16_t writeOnly01;
extern uint16_t subsWriteOnly;
extern uint16_t goToSubsTimer;
extern uint16_t pl1Direction;
extern uint16_t pl2Direction;
extern uint8_t  pl1Fire;
extern uint8_t  pl2Fire;
extern uint16_t subsPrevPl1Direction;
extern uint16_t subsPrevPl2Direction;
extern uint16_t subsPl1TapCount;
extern uint16_t subsPl2TapCount;
extern uint16_t pl1BenchTimeoutCounter;
extern uint16_t pl2BenchTimeoutCounter;
extern uint16_t pl1BenchBlockWhileHoldingDirection;
extern uint16_t pl2BenchBlockWhileHoldingDirection;
extern int16_t  timeVar;
extern int16_t  resultTimer;
extern uint16_t selectedFormationEntry;
extern uint16_t plToBeSubstitutedPos;
extern uint16_t plToBeSubstitutedOrd;
extern uint16_t plToEnterGameIndex;
extern uint16_t selectedPlayerInSubs;
extern uint16_t team1NumSubs;
extern uint16_t team2NumSubs;
extern uint16_t team1GoalkeeperReplaced;
extern uint16_t team2GoalkeeperReplaced;
extern uint16_t team1MostFrequentFace;
extern uint16_t team2MostFrequentFace;
extern uint16_t maxSubsPerTeam;
extern uint16_t gameMaxSubstitutes;
extern uint32_t substitutedPlSprite;  /* pointer stored as uint32_t */
extern uint32_t teamThatSubstitutes;  /* pointer stored as uint32_t */
extern uint16_t substitutedPlX;
extern uint16_t substitutedPlY;
extern uint16_t substitutedPlDestX;
extern uint16_t substitutedPlDestY;
extern uint16_t plSubstitutedX;
extern uint16_t plSubstitutedY;
extern uint16_t pl1Tactics;
extern uint16_t pl2Tactics;
extern uint16_t hideBall;
extern uint16_t cameraXVelocity;
extern uint16_t cameraYVelocity;
extern TeamGeneralInfo *lastTeamPlayedBeforeBreak;

extern TeamGeneralInfo topTeamData;
extern TeamGeneralInfo bottomTeamData;
extern TeamGame       *topTeamPtr;
extern TeamGame       *bottomTeamPtr;
extern TeamGame topTeamInGame;
extern TeamGame bottomTeamInGame;
extern uint8_t  plOrdinalsTable[];
extern uint8_t  plNamesLengthTableCopy[];
extern uint16_t ingameTeamPlayerOffsets[];
extern uint32_t positionsTable[];
extern uint8_t  playerNormalStandingAnimTable[];

/* ---- Game state constants (ST_GAME_IN_PROGRESS, ST_KEEPER_HOLDS_BALL,
 *   ST_STARTING_GAME, ST_GAME_ENDED, ST_STOPPED all from game_types.h) ---- */

/* ---- Position constants ---- */
#define POS_RIGHT_BACK          1
#define POS_LEFT_BACK           2
#define POS_DEFENDER            3
#define POS_RIGHT_WING          4
#define POS_LEFT_WING           5
#define POS_MIDFIELDER          6

/* ---- Sprite constants ---- */
#define SPR_SELECT_SUBSTITUTE_ARROW 1170
#define SPR_SUBSTITUTES             1174
#define SPR_FORMATION               1175
#define SPR_RED_CROSS_INJURY        1176
#define SPR_RED_CARD_SMALL          139
#define SPR_YELLOW_CARD_SMALL       140

/* ---- TeamGame color offsets (used for kit color reads via field names) ---- */
/* prShirtCol(2), prStripesCol(4), prShortsCol(6), prSocksCol(8) */

/*
 * Helper: get a typed PlayerInfo pointer from a TeamGame and a raw byte offset
 * from ingameTeamPlayerOffsets[].
 *
 * ingameTeamPlayerOffsets[p] stores the byte offset within TeamGame to a position
 * 42 bytes BEFORE the actual PlayerInfo data. Adding 42 aligns to PlayerInfo start.
 * Equivalently, this is &teamGame->players[slot] where the slot index is encoded
 * in the offset.
 */
static inline PlayerInfo *piFromOffset(TeamGame *tg, uint16_t off)
{
    return (PlayerInfo *)((uint8_t *)tg + off + 42);
}

/* ---- external data ---- */
extern uint16_t cameraX;
extern uint16_t cameraY;
extern uint16_t initialSubsState;
extern uint16_t drawTrainingTopHalfOfTheBench;
extern uint16_t currentGameTick;
extern uint16_t useIndividualPlayerSkinColor;
extern uint16_t panicReasonCode;
extern uint16_t smallCharsTable[];
extern uint16_t ourCoachFrames[];
extern uint16_t opponentCoachFrames[];
extern uint16_t benchColors[];
extern uint16_t subsMenuBackgroundColor;
extern uint16_t subsMenuHighlightColor;
extern uint16_t subsMenuEntryFrameColor;
extern uint8_t  playerNumberString[];
extern uint16_t playerFacesTable[];
extern uint32_t playerPositionsStringTable[];
extern uint32_t tacticsStringTable[];
extern uint32_t teamTacticsTable[];
extern uint8_t  aFormation[];

/* ---- external functions ---- */
extern void _Player1StatusProc(void);
extern void _Player2StatusProc(void);
extern void _InitCurrentGoalHighlight(void);
extern void _CheckIfGoalkeeperClaimedTheBall(void);
extern void _InitializePlayerSpriteFrameIndices(void);
extern void _CApplyTeamTactics(TeamGame *teamGame);
extern void _SetPlayerAnimationTable(uint8_t *animTable, Sprite *sprite);
extern void _StopAllPlayers(void);
extern void _CDrawSprite16Pixels(uint16_t spriteIndex, uint16_t x, uint16_t y);
extern void _CDrawSpriteInCameraView(uint16_t spriteIndex, uint16_t x, uint16_t y);
extern void _CDrawSpriteHotspotInCameraView(uint16_t spriteIndex, uint16_t x, uint16_t y);
extern void _FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
extern uint16_t _DrawMenuText(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable);
extern uint16_t _DrawMenuTextCentered(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable);
extern void _PanicHaltLoop(void);

/* ---- forward declarations ---- */
static void leaveSubsMenu(void);
static void benchMenuHandleControls(void);
static void trainingSwapBenchTeams(void);
static int16_t isThisPlayerOkToSelect(void);
static void firePressedInSubsMenu(void);
static void showFormationMenu(void);
static void subsBenchOff(void);
static void selectPlayerToSubstituteMenuHandler(void);
static void checkForThrowInAndGoalkeepersBall(void);
static void substitutePlayer(void);
static void state3HandleFirePress(void);
static void markPlayersMenuHandler(void);
static void handleFormationMenuControls(void);
static void updateSubstitutePlayerIndices(void);
static void updatePlayerToBeSubstitutedPosition(void);
static uint16_t isPlayerOkToSubstitute(void);
static void handleFormationMenuMovement(void);
static void findInitialPlayerToBeSubstituted(void);
static void swapPlayerNameData(int16_t enterIndex, int16_t leaveIndex);
static PlayerInfo *getBenchPlayerPointer(uint16_t index, uint16_t *outOrdinal);
static void drawOpponentsBench(void);
static void drawBenchPlayersAndCoach(TeamGame *teamGame, TeamGeneralInfo *team, uint16_t x, uint16_t y);
static void drawSubstitutesMenu(void);
static void drawSubstitutesMenuEntry(uint16_t y, uint16_t baseColor, int16_t selectedIndex,
                                     uint8_t *charsTable, TeamGame *teamGame, uint16_t *playerOffsets);
static void drawFormationMenu(void);
static void drawFormationMenuEntry(uint8_t *charsTable, uint16_t y, uint16_t entryColor, uint16_t entryIndex);

/* ================================================================
 * _BenchCheckControls
 * (SWOS2.s line 12131)
 *
 * Per-frame substitution controls check. Called from GameLoop.
 * ================================================================ */
void __attribute__((used, externally_visible))
_BenchCheckControls(void)
{
    TeamGeneralInfo *a6;

    if (inSubstitutesMenu != 0) {
        if ((int16_t)inSubstitutesMenu < 0)
            return;

        /* menu is active, read input from appropriate player */
        a6 = (TeamGeneralInfo *)benchTeam;
        if (subsTeamsSwapped != 0)
            a6 = a6->opponentsTeam;

        {
            if (a6->playerNumber == 1)
                goto read_pl1_input;
            if (a6->playerNumber == 2)
                goto read_pl2_input;
            if (a6->playerCoachNumber != 1)
                goto read_pl2_input;
        }

    read_pl1_input:
        _Player1StatusProc();
        subsDirection = pl1Direction;
        subsFire = pl1Fire;
        goto menu_active_phase;

    read_pl2_input:
        _Player2StatusProc();
        subsDirection = pl2Direction;
        subsFire = pl2Fire;
        goto menu_active_phase;
    }

    /* inSubstitutesMenu == 0: check if bench should activate */
    if (benchOff != 0) {
        if (cameraCoordinatesValid != 0)
            benchOff = 0;
    }

    if (waitForPlayerToGoInTimer != 0) {
        waitForPlayerToGoInTimer--;
        return;
    }

    if (cameraLeavingSubsTimer != 0) {
        cameraLeavingSubsTimer--;
        return;
    }

    if (substituteInProgress != 0)
        return;
    if (refState != 0)
        return;
    if (injuriesForever != 0)
        return;
    if (statsTimer != 0)
        return;

    if (gameStatePl == ST_GAME_IN_PROGRESS)
        goto reset_bench_tap_state;
    if (whichCard != 0)
        goto reset_bench_tap_state;
    if (playingPenalties != 0)
        goto reset_bench_tap_state;

    if (gameState >= ST_STARTING_GAME && gameState <= ST_GAME_ENDED)
        goto reset_bench_tap_state;

    /* check if either player has called the bench */
    a6 = &topTeamData;
    if (bench1Called != 0)
        goto activate_bench;
    a6 = &bottomTeamData;
    if (bench2Called != 0)
        goto activate_bench;

    /* alternate between top/bottom team each frame */
    if (frameCount & 1) {
        a6 = &topTeamData;
    } else {
        a6 = &bottomTeamData;
    }

    /* check if team has a human player */
    if ((uint16_t)a6->playerNumber != 0) {
        subsDirection = (uint16_t)a6->direction;
        subsFire = a6->firePressed;
        goto check_double_tap;
    }

    if ((uint16_t)a6->playerCoachNumber == 0)
        return;

    if ((uint16_t)a6->playerCoachNumber != 2) {
        _Player1StatusProc();
        subsDirection = pl1Direction;
        subsFire = pl1Fire;
        goto check_double_tap;
    }

    _Player2StatusProc();
    subsDirection = pl2Direction;
    subsFire = pl2Fire;
    goto check_double_tap;

reset_bench_tap_state:
    subsPrevPl1Direction = 0xFFFF;
    subsPl1TapCount = 0;
    pl1BenchTimeoutCounter = 0;
    subsPrevPl2Direction = 0xFFFF;
    subsPl2TapCount = 0;
    pl2BenchTimeoutCounter = 0;
    bench1Called = 0;
    bench2Called = 0;
    return;

check_double_tap:
    /* double-tap detection for top team (a6 == &topTeamData) or bottom team */
    if (a6 == &topTeamData) {
        /* player 1 double-tap detection */
        uint16_t d0 = subsDirection;
        if ((int16_t)d0 >= 0) {
            if (pl1BenchBlockWhileHoldingDirection != 0)
                return;
            if ((int16_t)subsPrevPl1Direction >= 0) {
                if (d0 != subsPrevPl1Direction)
                    goto pl1_reset_tap;
                subsPl1TapCount++;
                pl1BenchTimeoutCounter = 0;
                if (subsPl1TapCount == 2)
                    goto activate_bench;
            }
            subsPrevPl1Direction = subsDirection;
            pl1BenchBlockWhileHoldingDirection = 1;
            return;
        }
        /* direction released */
        pl1BenchBlockWhileHoldingDirection = 0;
        pl1BenchTimeoutCounter++;
        if (pl1BenchTimeoutCounter != 15)
            return;
    pl1_reset_tap:
        subsPrevPl1Direction = 0xFFFF;
        subsPl1TapCount = 0;
        pl1BenchTimeoutCounter = 0;
        return;
    } else {
        /* player 2 double-tap detection */
        uint16_t d0 = subsDirection;
        if ((int16_t)d0 >= 0) {
            if (pl2BenchBlockWhileHoldingDirection != 0)
                return;
            if ((int16_t)subsPrevPl2Direction >= 0) {
                if (d0 != subsPrevPl2Direction)
                    goto pl2_reset_tap;
                subsPl2TapCount++;
                pl2BenchTimeoutCounter = 0;
                if (subsPl2TapCount == 2)
                    goto activate_bench;
            }
            subsPrevPl2Direction = subsDirection;
            pl2BenchBlockWhileHoldingDirection = 1;
            return;
        }
        /* direction released */
        pl2BenchBlockWhileHoldingDirection = 0;
        pl2BenchTimeoutCounter++;
        if (pl2BenchTimeoutCounter != 15)
            return;
    pl2_reset_tap:
        subsPrevPl2Direction = 0xFFFF;
        subsPl2TapCount = 0;
        pl2BenchTimeoutCounter = 0;
        return;
    }

activate_bench:
    {
        int16_t d1, d2;
        TeamGame *a0;

        bench1Called = 0;
        bench2Called = 0;
        benchTeam = (uint32_t)a6;
        a0 = topTeamPtr;
        if ((uint16_t)a6->teamNumber == 2)
            a0 = bottomTeamPtr;

        benchTeamGame = (uint32_t)a0;
        plToBeSubstitutedPos = 0xFFFF;
        subsState = 0;
        benchPlayerIndex = 0;
        subsBlockFire = 1;
        subsBlockDirections = 1;
        inSubstitutesMenu = 30;
        goToSubsTimer = 15;

        /* negate timers */
        timeVar = -timeVar;
        resultTimer = -resultTimer;
        statsTimer = -((int16_t)statsTimer);

        d1 = 389;   /* top bench Y */
        d2 = 485;   /* bottom bench Y */

        if (trainingGame != 0) {
            d1 += 67;    /* training game bench coordinates */
            d2 -= 29;
            subsTrainingTopTeam = 0;
            /* exg d1,d2 */
            { int16_t tmp = d1; d1 = d2; d2 = tmp; }
            if (a0 == &topTeamInGame) {
                /* benchTeamGame == topTeamInGame: keep as-is */
            } else {
                subsTrainingTopTeam = 1;
            }
            goto set_bench_coords;
        }

        /* check bit 1 of 2nd char of top team name to decide bench side */
        if ((uint8_t)topTeamInGame.teamName[0] & 2) {
            int16_t tmp = d1; d1 = d2; d2 = tmp;
        }
        if (a0 != &topTeamInGame) {
            int16_t tmp = d1; d1 = d2; d2 = tmp;
        }

    set_bench_coords:
        benchY = (uint16_t)d1;
        opponentBenchY = (uint16_t)d2;
        plComingYTmp = 449;
        plComingY = plComingYTmp;
        benchWriteOnly01 = benchY + 50;
        benchXLimit = 27;
        subsMenuX = 100;
        formationMenuX = 100;
        plLeavingX = 39;
        plComingX = 26;
        benchWriteOnly02 = 35;

        checkForThrowInAndGoalkeepersBall();
        return;
    }

menu_active_phase:
    /* bench menu is active; handle timers and input dispatch */
    {
        int16_t d7;
        a6 = (TeamGeneralInfo *)benchTeam;

        if (cameraCoordinatesValid == 0) {
            goToSubsTimer = 15;
            return;
        }

        if (goToSubsTimer != 0) {
            goToSubsTimer--;
            return;
        }

        if ((TeamGeneralInfo *)benchTeam != a6)  /* cmpa.l benchTeam,a6 */
            return;
        if ((int16_t)inSubstitutesMenu < 30)
            return;
        if (substituteInProgress != 0) {
            if ((int16_t)substituteInProgress < 0) {
                substitutePlayer();
            }
            return;
        }

        d7 = (int16_t)subsDirection;
        if (d7 < 0)
            goto clear_block;
        if (subsBlockDirections != 0) {
            if ((uint16_t)d7 != subsPrevDirection)
                goto clear_block;
            if (subsState == 0)
                return;
            if (subsPrevDirection != 0 && subsPrevDirection != 4)
                return;
            subsMovementDelayTimer++;
            if (subsMovementDelayTimer != 2)
                return;
        }

    clear_block:
        subsBlockDirections = 0;
        subsMovementDelayTimer = 0;
        subsPrevDirection = (uint16_t)d7;

        {
            uint16_t d0 = 0;
            if (subsFire != 0) {
                d0 = subsFireTimer + 1;
            }
            subsFireTimer = d0;
        }

        if (subsBlockFire != 0) {
            if (subsFire != 0)
                return;
            subsBlockFire = 0;
        }

        if (inSubstitutesMenu == 30) {
            benchMenuHandleControls();
            return;
        }
    }
}

/* ================================================================
 * leaveSubsMenu (SWOS2.s line 12406)
 * ================================================================ */
static void leaveSubsMenu(void)
{
    _InitCurrentGoalHighlight();
    benchOff = 1;
    cameraLeavingSubsTimer = 50;
    leavingSubsMenu = 1;
    _CheckIfGoalkeeperClaimedTheBall();
    bench1Called = 0;
    bench2Called = 0;
    inSubstitutesMenu = 0;
    subsTeamsSwapped = 0;
    overlayActiveFlag = 0;
    writeOnly01 = 1;
    subsState = 0;
    plToBeSubstitutedPos = 0xFFFF;
    pl1BenchTimeoutCounter = 0;
    pl2BenchTimeoutCounter = 0;
    subsPrevPl1Direction = 0xFFFF;
    subsPrevPl2Direction = 0xFFFF;
    subsPl1TapCount = 0;
    subsPl2TapCount = 0;
    subsWriteOnly = 2;

    /* loc_10CF6E: clear deadBenchVar */
    deadBenchVar = 0;
}

/* ================================================================
 * benchMenuHandleControls (SWOS2.s line 12431)
 * ================================================================ */
static void benchMenuHandleControls(void)
{
    TeamGeneralInfo *a0;

    if (substituteInProgress != 0)
        return;

    if (subsState != 0) {
        if (subsState == 1) {
            selectPlayerToSubstituteMenuHandler();
            return;
        }
        if (subsState == 3) {
            state3HandleFirePress();
            return;
        }
        if (subsState == 2) {
            handleFormationMenuControls();
            return;
        }
        if (subsState == 4) {
            markPlayersMenuHandler();
            return;
        }
        return;
    }

    /* subsState == 0: top-level bench menu */
    a0 = (TeamGeneralInfo *)benchTeam;
    if (a0 == &topTeamData) {
        if (bench1Called != 0) {
            leaveSubsMenu();
            return;
        }
    } else {
        if (bench2Called != 0) {
            leaveSubsMenu();
            return;
        }
    }

    {
        uint16_t d0 = subsDirection;

        if (trainingGame != 0 && subsTrainingTopTeam != 0) {
            /* inverted controls for training top team */
            if (d0 == 0) goto move_down;
            if (d0 == 4) goto move_up;
            goto check_left_right;
        }

        if (d0 == 0) goto move_up;
        if (d0 == 4) goto move_down;

    check_left_right:
        if (d0 == 2) { leaveSubsMenu(); return; }
        if (d0 == 6) { leaveSubsMenu(); return; }
        if (subsFire != 0) {
            firePressedInSubsMenu();
            return;
        }
        return;

    move_down:
        if (gameState == ST_KEEPER_HOLDS_BALL)
            return;
        subsBlockDirections = 1;
        if (benchPlayerIndex == 5)
            return;
        /* dead code: clr.w benchPlayerIndex; bra exit_rts removed */
        do {
            benchPlayerIndex++;
            if (benchPlayerIndex == 6) goto scan_backward;
            if (isThisPlayerOkToSelect() >= 0) return;
        } while (1);

    move_up:
        if (trainingGame == 0) {
            /* normal: check keeper holds ball */
        } else {
            if (benchPlayerIndex == 0) goto training_swap;
        }
        if (gameState == ST_KEEPER_HOLDS_BALL && trainingGame == 0)
            return;
        subsBlockDirections = 1;
        if (benchPlayerIndex == 0) {
            if (trainingGame != 0) {
            training_swap:
                trainingSwapBenchTeams();
            }
            return;
            /* dead code: benchPlayerIndex=6, fallthrough removed */
        }

    scan_backward:
        do {
            benchPlayerIndex--;
            if (benchPlayerIndex == 0) return;
            if (isThisPlayerOkToSelect() >= 0) return;
        } while (1);
    }
}

/* ================================================================
 * trainingSwapBenchTeams (SWOS2.s line 12526)
 * ================================================================ */
static void trainingSwapBenchTeams(void)
{
    uint16_t tmpY;
    TeamGeneralInfo *a6;
    TeamGame *a0;

    subsTeamsSwapped ^= 1;
    subsTrainingTopTeam ^= 1;

    tmpY = benchY;
    benchY = opponentBenchY;
    opponentBenchY = tmpY;

    a6 = (TeamGeneralInfo *)benchTeam;
    a6 = a6->opponentsTeam;
    benchTeam = (uint32_t)a6;

    a0 = topTeamPtr;
    if ((uint16_t)a6->teamNumber == 2)
        a0 = bottomTeamPtr;
    benchTeamGame = (uint32_t)a0;
}

/* ================================================================
 * isThisPlayerOkToSelect (SWOS2.s line 12549)
 * Returns: negative = not OK, >= 0 = OK
 * ================================================================ */
static int16_t isThisPlayerOkToSelect(void)
{
    uint16_t d0, d1;
    TeamGeneralInfo *a0;

    d0 = team1NumSubs;
    d1 = team1GoalkeeperReplaced;
    a0 = (TeamGeneralInfo *)benchTeam;
    if ((uint16_t)a0->teamNumber != 1) {
        d0 = team2NumSubs;
        d1 = team2GoalkeeperReplaced;
    }

    if (d0 == maxSubsPerTeam)
        return -1;
    /* dead code block at 10D1DE-10D1FC skipped (unreachable after bra.s) */

    /* check player status */
    {
        uint16_t idx = benchPlayerIndex + 10;
        TeamGame *btg = (TeamGame *)benchTeamGame;
        PlayerInfo *pi = &btg->players[idx];

        if (pi->skip != 0)
            return -1;
        if ((int8_t)pi->position < 0)
            return (int16_t)(int8_t)pi->position; /* returns negative */
        if ((uint8_t)pi->numYellowCards >= 2)
            return -1;

        return 0;
    }
}

/* ================================================================
 * firePressedInSubsMenu (SWOS2.s line 12591)
 * ================================================================ */
static void firePressedInSubsMenu(void)
{
    uint16_t d0 = benchPlayerIndex;

    if (d0 == 0)
        goto marks_menu;

    d0--;
    if ((int16_t)d0 < 0)
        return;

    {
        uint16_t offset = ingameTeamPlayerOffsets[d0 + 11];
        PlayerInfo *pi = piFromOffset((TeamGame *)benchTeamGame, offset);

        if ((int8_t)pi->position < 0)
            return;
        if ((uint8_t)pi->numYellowCards >= 2)
            return;
    }

    subsState = 1;
    subsBlockFire = 1;
    overlayActiveFlag = 1;
    plToEnterGameIndex = benchPlayerIndex + 10;

    if (gameMaxSubstitutes > 5 && plToEnterGameIndex == 11) {
        plToBeSubstitutedPos = 0;
        return;
    }

    findInitialPlayerToBeSubstituted();
    return;

marks_menu:
    selectedPlayerInSubs = 0xFFFF;
    subsState = 4;
    subsBlockFire = 1;
    overlayActiveFlag = 1;
    plToBeSubstitutedOrd = 0xFFFF;
    plToBeSubstitutedPos = 0xFFFF;
    selectedPlayerInSubs = 0xFFFF;
}

/* ================================================================
 * showFormationMenu (SWOS2.s line 12634)
 * ================================================================ */
static void showFormationMenu(void)
{
    TeamGeneralInfo *a0 = (TeamGeneralInfo *)benchTeam;
    subsState = 2;
    subsBlockFire = 1;
    overlayActiveFlag = 1;
    selectedFormationEntry = (uint16_t)a0->tactics;
}

/* ================================================================
 * subsBenchOff (SWOS2.s line 12644)
 * ================================================================ */
static void subsBenchOff(void)
{
    subsState = 0;
    subsBlockDirections = 1;
    subsBlockFire = 1;
    overlayActiveFlag = 0;
    subsWriteOnly = 2;
}

/* ================================================================
 * selectPlayerToSubstituteMenuHandler (SWOS2.s line 12654)
 * ================================================================ */
static void selectPlayerToSubstituteMenuHandler(void)
{
    if (subsFire == 0) {
        updateSubstitutePlayerIndices();
        return;
    }

    overlayActiveFlag = 0;
    substituteInProgress = 1;

    {
        uint16_t *numSubsPtr;
        uint16_t *gkReplacedPtr;
        TeamGeneralInfo *a1 = (TeamGeneralInfo *)benchTeam;

        if ((uint16_t)a1->teamNumber == 1) {
            numSubsPtr = &team1NumSubs;
            gkReplacedPtr = &team1GoalkeeperReplaced;
        } else {
            numSubsPtr = &team2NumSubs;
            gkReplacedPtr = &team2GoalkeeperReplaced;
        }

        (*numSubsPtr)++;
        if (plToEnterGameIndex == 11)
            (*gkReplacedPtr)++;
    }

    subsBlockFire = 1;
    subsState = 0;
    subsWriteOnly = 2;

    {
        TeamGeneralInfo *a0 = (TeamGeneralInfo *)benchTeam;
        Sprite **sprTable;
        Sprite *sprPtr;

        teamThatSubstitutes = (uint32_t)a0;
        sprTable = a0->spritesTable;
        sprPtr = sprTable[plToBeSubstitutedPos];
        substitutedPlSprite = (uint32_t)sprPtr;

        sprPtr->cards = 0;
        substitutedPlX = (uint16_t)sprPtr->x.s.i;
        substitutedPlY = (uint16_t)sprPtr->y.s.i;
        substitutedPlDestX = plLeavingX;
        substitutedPlDestY = plComingYTmp;
        plSubstitutedX = 39;
        plSubstitutedY = 449;
    }
}

/* ================================================================
 * checkForThrowInAndGoalkeepersBall (SWOS2.s line 12695)
 * ================================================================ */
static void checkForThrowInAndGoalkeepersBall(void)
{
    TeamGeneralInfo *a0 = lastTeamPlayedBeforeBreak;
    Sprite *a1 = a0->controlledPlayer;

    if (a1 != (Sprite *)0) {
        if (a1->playerState == PL_THROW_IN) {
            hideBall = 0;
            a1->playerState = PL_NORMAL;
            _SetPlayerAnimationTable(playerNormalStandingAnimTable, a1);
        }
    }

    _CheckIfGoalkeeperClaimedTheBall();
}

/* ================================================================
 * substitutePlayer (SWOS2.s line 12712)
 * ================================================================ */
static void __attribute__((optimize("no-tree-loop-distribute-patterns")))
substitutePlayer(void)
{
    Sprite *sprPtr = (Sprite *)substitutedPlSprite;
    int16_t d7, d6;
    TeamGame *a0;
    uint16_t off7, off6;
    int16_t i;

    sprPtr->injuryLevel = 0;
    sprPtr->sentAway = 0;

    d7 = (int16_t)plToBeSubstitutedPos;
    d6 = (int16_t)plToEnterGameIndex;
    a0 = (TeamGame *)benchTeamGame;

    /* reset marked player if it's the one going out */
    if ((uint16_t)a0->markedPlayer == (uint16_t)d7) {
        a0->markedPlayer = (int16_t)0xFFFF;
    }
    if ((uint16_t)a0->markedPlayer == (uint16_t)d6) {
        a0->markedPlayer = (int16_t)0xFFFF;
        if (d7 != 0)
            a0->markedPlayer = (int16_t)(uint16_t)d7;
    }

    swapPlayerNameData(d6, d7);

    off6 = ingameTeamPlayerOffsets[d6];
    off7 = ingameTeamPlayerOffsets[d7];
    a0 = (TeamGame *)benchTeamGame;

    /* mark leaving player as unavailable:
     * ingameTeamPlayerOffsets[p] + 42 = start of PlayerInfo p in TeamGame;
     * position field is at offset 4 within PlayerInfo */
    {
        uint8_t *tgBytes = (uint8_t *)a0;
        tgBytes[off7 + 42 + 4] = 0xFF;  /* PlayerInfo.position = 0xFF */
    }

    /* swap player info data (61 bytes) */
    {
        uint8_t *tgBytes = (uint8_t *)a0;
        for (i = 60; i >= 0; i--) {
            uint8_t tmp = tgBytes[off6 + 42 + i];
            tgBytes[off6 + 42 + i] = tgBytes[off7 + 42 + i];
            tgBytes[off7 + 42 + i] = tmp;
        }
    }

    _InitializePlayerSpriteFrameIndices();
    _CApplyTeamTactics((TeamGame *)benchTeamGame);

    waitForPlayerToGoInTimer = 100;
    subsBlockFire = 1;

    leaveSubsMenu();
}

/* ================================================================
 * state3HandleFirePress (SWOS2.s line 12762)
 * ================================================================ */
static void state3HandleFirePress(void)
{
    if (subsFire != 0) {
        overlayActiveFlag = 0;
        return;
        /* dead code after return: send-off logic, never reached
         * (original asm has bra.w exit_rts before it) */
    }

    updateSubstitutePlayerIndices();
}

/* ================================================================
 * markPlayersMenuHandler (SWOS2.s line 12782)
 * ================================================================ */
static void __attribute__((optimize("no-tree-loop-distribute-patterns")))
markPlayersMenuHandler(void)
{
    TeamGame *a0;
    uint16_t off;

    /* check for long fire press -> toggle marked player */
    a0 = (TeamGame *)benchTeamGame;
    off = ingameTeamPlayerOffsets[plToBeSubstitutedPos];
    {
        PlayerInfo *pi = piFromOffset(a0, off);
        if ((uint8_t)pi->numYellowCards < 2) {
            if (subsFireTimer == 8) {
                uint16_t d0 = plToBeSubstitutedPos;
                a0 = (TeamGame *)benchTeamGame;
                if ((uint16_t)a0->markedPlayer == d0)
                    d0 = 0xFFFF;
                a0->markedPlayer = (int16_t)d0;
                selectedPlayerInSubs = 0xFFFF;
                return;
            }
        }
    }

    if (subsFire != 0) {
        if ((int16_t)plToBeSubstitutedOrd < 0) {
            /* formation menu */
            subsWriteOnly = 2;
            showFormationMenu();
            return;
        }

        if (plToBeSubstitutedOrd == 0)
            return;

        if ((int16_t)selectedPlayerInSubs < 0) {
            /* select this player */
            if (subsFireTimer != 1)
                return;
            selectedPlayerInSubs = plToBeSubstitutedPos;
            return;
        }

        /* swap two players */
        {
            uint16_t selPos = selectedPlayerInSubs;
            uint16_t curPos = plToBeSubstitutedPos;

            if (selPos == curPos) {
                if (subsFireTimer != 1)
                    return;
                selectedPlayerInSubs = 0xFFFF;
                return;
            }

            {
                int16_t d7s = (int16_t)selPos;
                int16_t d6s = (int16_t)curPos;
                TeamGame *btg;
                uint16_t off6, off7;

                /* update marked player if needed */
                btg = (TeamGame *)benchTeamGame;
                if ((uint16_t)btg->markedPlayer == (uint16_t)d6s) {
                    btg->markedPlayer = (int16_t)(uint16_t)d7s;
                } else if ((uint16_t)btg->markedPlayer == (uint16_t)d7s) {
                    btg->markedPlayer = (int16_t)(uint16_t)d6s;
                }

                swapPlayerNameData(d6s, d7s);

                off6 = ingameTeamPlayerOffsets[d6s];
                off7 = ingameTeamPlayerOffsets[d7s];
                btg = (TeamGame *)benchTeamGame;

                /* swap player data (61 bytes at PlayerInfo start) */
                {
                    int16_t k;
                    uint8_t *tgBytes = (uint8_t *)btg;
                    for (k = 60; k >= 0; k--) {
                        uint8_t tmp = tgBytes[off6 + 42 + k];
                        tgBytes[off6 + 42 + k] = tgBytes[off7 + 42 + k];
                        tgBytes[off7 + 42 + k] = tmp;
                    }
                }

                /* swap sprites */
                {
                    TeamGeneralInfo *tgi = (TeamGeneralInfo *)benchTeam;
                    Sprite **sprTable = tgi->spritesTable;
                    Sprite *spr1 = sprTable[d6s];
                    Sprite *spr2 = sprTable[d7s];
                    int16_t k;
                    int16_t ordTmp;

                    /* swap 110 bytes of sprite data */
                    {
                        uint8_t *s1 = (uint8_t *)spr1;
                        uint8_t *s2 = (uint8_t *)spr2;
                        for (k = 109; k >= 0; k--) {
                            uint8_t tmp = s1[k];
                            s1[k] = s2[k];
                            s2[k] = tmp;
                        }
                    }

                    /* preserve original ordinals (swap back) */
                    ordTmp = spr1->playerOrdinal;
                    spr1->playerOrdinal = spr2->playerOrdinal;
                    spr2->playerOrdinal = ordTmp;
                }

                _InitializePlayerSpriteFrameIndices();
                _CApplyTeamTactics((TeamGame *)benchTeamGame);

                selectedPlayerInSubs = 0xFFFF;
                overlayActiveFlag = 0;
                subsBenchOff();
                return;
            }
        }
    }

    /* no fire: update selection indices */
    updateSubstitutePlayerIndices();
}

/* ================================================================
 * handleFormationMenuControls (SWOS2.s line 12889)
 * ================================================================ */
static void handleFormationMenuControls(void)
{
    if (subsFire != 0) {
        TeamGeneralInfo *a0 = (TeamGeneralInfo *)benchTeam;
        overlayActiveFlag = 0;
        a0->tactics = (int16_t)selectedFormationEntry;

        if ((uint16_t)a0->teamNumber == 1) {
            pl1Tactics = selectedFormationEntry;
        } else {
            pl2Tactics = selectedFormationEntry;
        }

        _CApplyTeamTactics((TeamGame *)benchTeamGame);
        subsBenchOff();
        return;
    }

    handleFormationMenuMovement();
}

/* ================================================================
 * updateSubstitutePlayerIndices (SWOS2.s line 12916)
 * ================================================================ */
static void updateSubstitutePlayerIndices(void)
{
    uint16_t d7 = subsDirection;

    if (d7 == 2) { subsBenchOff(); return; }
    if (d7 == 6) { subsBenchOff(); return; }

    /* for state != 4 and goalie sub (index 11): skip if maxSubs <= 5 */
    if (subsState != 4) {
        if (plToEnterGameIndex == 11) {
            if (gameMaxSubstitutes <= 5)
                return;
        }
    }

    d7 = subsDirection;
    if (d7 == 0) goto select_prev;
    if (d7 == 4) goto select_next;
    return;

select_next:
    subsBlockDirections = 1;

    if (subsState == 4) {
        if ((int16_t)plToBeSubstitutedOrd < 0) {
            plToBeSubstitutedOrd = 1;
            updatePlayerToBeSubstitutedPosition();
            return;
        }
    }

    if (plToBeSubstitutedOrd == 10)
        return;

    do {
        plToBeSubstitutedOrd++;
        if (plToBeSubstitutedOrd == 11)
            goto dec_scan_backward;
        updatePlayerToBeSubstitutedPosition();
        if (isPlayerOkToSubstitute() == 0) return;
    } while (1);

select_prev:
    subsBlockDirections = 1;

    if (subsState == 4) {
        if ((int16_t)plToBeSubstitutedOrd < 0)
            return;
        if (plToBeSubstitutedOrd == 1) {
            plToBeSubstitutedOrd = 0xFFFF;
            plToBeSubstitutedPos = 0xFFFF;
            return;
        }
    }

    if (plToBeSubstitutedOrd == 0)
        return;

dec_scan_backward:
    do {
        plToBeSubstitutedOrd--;
        if ((int16_t)plToBeSubstitutedOrd < 0)
            goto select_next_inc;
        updatePlayerToBeSubstitutedPosition();
        if (isPlayerOkToSubstitute() == 0) return;
    } while (1);

select_next_inc:
    do {
        plToBeSubstitutedOrd++;
        if (plToBeSubstitutedOrd == 11)
            goto dec_scan_backward;
        updatePlayerToBeSubstitutedPosition();
        if (isPlayerOkToSubstitute() == 0) return;
    } while (1);
}

/* ================================================================
 * updatePlayerToBeSubstitutedPosition (SWOS2.s line 12978)
 * ================================================================ */
static void updatePlayerToBeSubstitutedPosition(void)
{
    uint16_t ordinal;
    getBenchPlayerPointer(plToBeSubstitutedOrd, &ordinal);
    plToBeSubstitutedPos = ordinal;
}

/* ================================================================
 * isPlayerOkToSubstitute (SWOS2.s line 12989)
 * Returns: 0 = OK to substitute, 1 = do not substitute
 * ================================================================ */
static uint16_t isPlayerOkToSubstitute(void)
{
    if (subsState == 4) {
        if (plToBeSubstitutedOrd == 0)
            return 0;   /* OK - formation slot */
        return 1;       /* not OK */
    }

    {
        TeamGame *btg = (TeamGame *)benchTeamGame;
        uint16_t off = ingameTeamPlayerOffsets[plToBeSubstitutedPos];
        PlayerInfo *pi = piFromOffset(btg, off);

        /* red-carded players can be substituted */
        if ((uint8_t)pi->numYellowCards >= 2)
            return 0;

        /* in state 3: check disallowSubs */
        if (subsState == 3) {
            if ((int8_t)pi->field_B < 0)
                return 0;
        }

        return 1;   /* not OK */
    }
}

/* ================================================================
 * handleFormationMenuMovement (SWOS2.s line 13017)
 * ================================================================ */
static void handleFormationMenuMovement(void)
{
    uint16_t d7 = subsDirection;

    if (d7 == 0) goto formation_left;
    if (d7 == 4) goto formation_right;
    if (d7 == 2) { subsBenchOff(); return; }
    if (d7 == 6) { subsBenchOff(); return; }
    return;

formation_right:
    subsBlockDirections = 1;
    if ((int16_t)selectedFormationEntry < 0) {
        selectedFormationEntry = 0;
        return;
    }
    if (selectedFormationEntry == 0x11)
        return;
    selectedFormationEntry++;
    return;

formation_left:
    subsBlockDirections = 1;
    if ((int16_t)selectedFormationEntry < 0) {
        selectedFormationEntry = 7;
        return;
    }
    if (selectedFormationEntry == 0)
        return;
    selectedFormationEntry--;
}

/* ================================================================
 * findInitialPlayerToBeSubstituted (SWOS2.s line 13055)
 * ================================================================ */
static void findInitialPlayerToBeSubstituted(void)
{
    TeamGame *btg = (TeamGame *)benchTeamGame;
    uint16_t enterOff;
    int8_t targetPos;
    int16_t firstAvail = -1;   /* d6 */
    int16_t exactMatch = -1;   /* d5 */
    int16_t groupMatch = -1;   /* d4 */
    int16_t d1;

    enterOff = ingameTeamPlayerOffsets[plToEnterGameIndex];
    targetPos = piFromOffset(btg, enterOff)->position;

    for (d1 = 0; d1 < 11; d1++) {
        uint16_t ordinal;
        PlayerInfo *pi = getBenchPlayerPointer((uint16_t)d1, &ordinal);

        if ((uint8_t)pi->numYellowCards >= 2)
            goto next_player;

        /* record first available if not yet found */
        if (firstAvail < 0)
            firstAvail = d1;

        /* try group match if not yet found */
        if (groupMatch < 0) {
            int8_t plPos = pi->position;
            if (targetPos == POS_DEFENDER || targetPos == POS_RIGHT_BACK || targetPos == POS_LEFT_BACK) {
                if (plPos == POS_DEFENDER || plPos == POS_RIGHT_BACK || plPos == POS_LEFT_BACK)
                    groupMatch = d1;
            } else if (targetPos == POS_MIDFIELDER || targetPos == POS_RIGHT_WING || targetPos == POS_LEFT_WING) {
                if (plPos == POS_MIDFIELDER || plPos == POS_RIGHT_WING || plPos == POS_LEFT_WING)
                    groupMatch = d1;
            }
        }

        /* try exact position match if not yet found */
        if (exactMatch < 0) {
            if (pi->position == targetPos)
                exactMatch = d1;
        }

    next_player:
        ;
    }

    /* priority: exact > group > first available */
    {
        int16_t result = exactMatch;
        if (result < 0) {
            result = groupMatch;
            if (result < 0)
                result = firstAvail;
        }
        plToBeSubstitutedOrd = (uint16_t)result;

        {
            uint16_t ordinal;
            getBenchPlayerPointer((uint16_t)result, &ordinal);
            plToBeSubstitutedPos = ordinal;
        }
    }
}

/* ================================================================
 * swapPlayerNameData (SWOS2.s line 13140)
 * ================================================================ */
static void swapPlayerNameData(int16_t enterIndex, int16_t leaveIndex)
{
    uint8_t *ordTable = plOrdinalsTable;
    uint8_t *lenTable = plNamesLengthTableCopy;
    uint8_t tmp;

    if ((uint32_t)benchTeamGame != (uint32_t)&topTeamInGame) {
        ordTable += 16;
        lenTable += 16;
    }

    /* swap ordinals */
    tmp = ordTable[enterIndex];
    ordTable[enterIndex] = ordTable[leaveIndex];
    ordTable[leaveIndex] = tmp;

    /* swap name lengths */
    tmp = lenTable[enterIndex];
    lenTable[enterIndex] = lenTable[leaveIndex];
    lenTable[leaveIndex] = tmp;
}

/* ================================================================
 * getBenchPlayerPointer (SWOS2.s line 13941)
 *
 * Given a player index (0..10), looks up the player's position
 * in the current formation/tactics, then returns a pointer to
 * the player's info in the in-game team data.
 * Also returns the player's ordinal (0-based) via outOrdinal.
 * ================================================================ */
static PlayerInfo *getBenchPlayerPointer(uint16_t index, uint16_t *outOrdinal)
{
    uint16_t tactics;
    TeamGeneralInfo *a0;
    uint8_t *formOrder;
    uint8_t d0;
    uint16_t off;

    tactics = pl1Tactics;
    a0 = (TeamGeneralInfo *)benchTeam;
    if ((uint16_t)a0->teamNumber != 1)
        tactics = pl2Tactics;

    formOrder = (uint8_t *)positionsTable[tactics];
    d0 = formOrder[index];
    d0--;  /* convert 1-based to 0-based */

    off = ingameTeamPlayerOffsets[d0];

    *outOrdinal = (uint16_t)d0;
    return piFromOffset((TeamGame *)benchTeamGame, off);
}

/* ================================================================
 * _DrawBenchAndSubsMenu (SWOS2.s line 13159)
 *
 * Main drawing entry for bench/substitutes menu.
 * Called from GameLoop each frame when bench is active.
 * ================================================================ */
void __attribute__((used, externally_visible))
_DrawBenchAndSubsMenu(void)
{
    uint16_t d0, d1, d2;

    if (inSubstitutesMenu != 0)
        goto draw_bench;

    if (leavingSubsMenu == 0)
        return;

    d0 = benchXLimit;
    d0 += 8;
    if (d0 > cameraX)
        goto draw_bench;

    leavingSubsMenu = 0;
    return;

draw_bench:
    d0 = benchXLimit;
    d0 += 8;
    if (d0 <= cameraX)
        return;

    initialSubsState = subsState;

    if (trainingGame != 0) {
        if (subsTrainingTopTeam != 0)
            goto skip_opponent_1;
    }
    drawOpponentsBench();

skip_opponent_1:
    d1 = benchXLimit;
    d2 = benchY;
    d2 -= 21;
    d2 += 4;
    d0 = benchPlayerIndex;
    d1 -= 12;
    d0 *= 7;

    if (trainingGame != 0) {
        if (subsTrainingTopTeam != 0)
            d0 = (uint16_t)(-(int16_t)d0);
    }

    if (trainingGame != 0) {
        d0 -= 7;
        if (subsTrainingTopTeam == 0)
            d0 += 14;
    }

    d2 += d0;

    if (goToSubsTimer == 0 && substituteInProgress == 0 && subsState == 0) {
        uint16_t save_d1 = d1;
        _CDrawSpriteInCameraView(SPR_SELECT_SUBSTITUTE_ARROW, d1, d2);
        d1 = save_d1;
    }

    d1 += 12;
    d2 = benchY;
    drawTrainingTopHalfOfTheBench = subsTrainingTopTeam;
    drawBenchPlayersAndCoach((TeamGame *)benchTeamGame, (TeamGeneralInfo *)benchTeam, d1, d2);

    if (trainingGame != 0) {
        if (subsTrainingTopTeam != 0)
            drawOpponentsBench();
    }

    if (subsState == 1)
        drawSubstitutesMenu();
    else if (subsState == 3)
        drawSubstitutesMenu();
    else if (subsState == 2)
        drawFormationMenu();
    else if (subsState == 4)
        drawSubstitutesMenu();
}

/* ================================================================
 * drawOpponentsBench (SWOS2.s line 13239)
 *
 * Draws the opponent's bench players.
 * ================================================================ */
static void drawOpponentsBench(void)
{
    uint16_t d1, d2;
    TeamGame *a0;
    TeamGeneralInfo *a1;
    uint16_t savedState;

    d1 = benchXLimit;
    d2 = opponentBenchY;

    a0 = &topTeamInGame;
    if (benchTeamGame != (uint32_t)&bottomTeamInGame)
        a0 = &bottomTeamInGame;

    a1 = (TeamGeneralInfo *)benchTeam;
    a1 = a1->opponentsTeam; /* movea.l 0(a1),a1 — get opponents team ptr */

    savedState = subsState;
    subsState = 0xFFFF;
    drawTrainingTopHalfOfTheBench = subsTrainingTopTeam;
    drawTrainingTopHalfOfTheBench ^= 1;
    drawBenchPlayersAndCoach(a0, a1, d1, d2);
    subsState = savedState;
}

/* ================================================================
 * drawBenchPlayersAndCoach (SWOS2.s line 13266)
 *
 * Draws coach sprite + bench player sprites.
 * teamGame = TeamGame pointer (team that requested management)
 * team     = TeamGeneralInfo pointer (team data)
 * d1 = x, d2 = y
 * ================================================================ */
static void drawBenchPlayersAndCoach(TeamGame *teamGame, TeamGeneralInfo *team, uint16_t d1, uint16_t d2)
{
    uint16_t d5 = 0, d6 = 0;
    uint16_t d3, d0, d7;
    PlayerInfo *pi;

    if ((uint16_t)team->teamNumber != 1) {
        d5 = 7;
        d6 = 12;
    }

    d2 -= 0x13;
    d2 += 4;

    /* select coach sprite base */
    d3 = 0x49E;
    if (initialSubsState == 4 || initialSubsState == 2) {
        if (benchPlayerIndex == 0)
            d3 = 0x4A1;
    }

    /* animate coach from frame table */
    d0 = currentGameTick;
    d0 &= 0x1E0;
    d0 >>= 4;
    {
        uint16_t *coachFrames = (uint16_t *)ourCoachFrames;
        if ((int16_t)initialSubsState < 0)
            coachFrames = (uint16_t *)opponentCoachFrames;
        d0 = coachFrames[d0 >> 1];
    }
    d0 += d3;
    d0 += d5;

    /* draw coach sprite */
    if (trainingGame != 0 && drawTrainingTopHalfOfTheBench != 0)
        goto skip_coach;

    {
        uint16_t coach_d2 = d2;
        if (trainingGame != 0) {
            coach_d2 -= 7;
            if (subsTrainingTopTeam == 0)
                coach_d2 += 14;
        }
        _CDrawSpriteHotspotInCameraView(d0, d1, coach_d2);
    }

    if (trainingGame != 0) {
        /* no adjustment needed after the draw — we used a local copy */
    }

skip_coach:
    d2 += 7;
    if (trainingGame != 0)
        d2 += 7;

    /* advance to player 11 (first substitute) */
    pi = &teamGame->players[11];

    if (trainingGame != 0 && drawTrainingTopHalfOfTheBench != 0) {
        d2 -= 0x38;
        pi += 4; /* advance by 4 more players = player 15 */
    }

    for (d7 = 1; d7 < 6; d7++) {
        /* check if player is valid to draw */
        if (pi->skip != 0)
            goto next_player;
        if ((int8_t)pi->position < 0)
            goto next_player;
        if ((uint8_t)pi->numYellowCards >= 2)
            goto next_player;

        if ((int16_t)subsState < 0)
            goto draw_normal;

        if (substituteInProgress != 0) {
            d3 = plToEnterGameIndex;
            d3 -= 10;
            if (d3 == d7)
                goto draw_selected;
            goto draw_normal;
        }

        if (subsState == 1) {
            if (d7 == benchPlayerIndex)
                goto draw_selected;
        }
        goto draw_normal;

    draw_selected:
        d0 = 0x4B5;
        if (gameMaxSubstitutes > 5) {
            if (d7 != 1)
                goto sprite_chosen;
            goto use_gk_sprite;
        }
        if (pi->position != 0)
            goto sprite_chosen;
    use_gk_sprite:
        d0 = 0x4B2;
        goto sprite_chosen;

    draw_normal:
        d0 = 0x4AF;
        if (gameMaxSubstitutes > 5) {
            if (d7 != 1)
                goto sprite_chosen;
            goto use_gk_normal;
        }
        if (pi->position != 0)
            goto sprite_chosen;
    use_gk_normal:
        d0 = 0x4AC;

    sprite_chosen:
        if (useIndividualPlayerSkinColor == 0) {
            /* asm uses lea (team2GoalkeeperReplaced),a1 then indexes by teamNumber*2.
             * Layout: [0]=team2GoalkeeperReplaced, [1]=team1MostFrequentFace, [2]=team2MostFrequentFace.
             * So teamNumber 1 → team1MostFrequentFace, teamNumber 2 → team2MostFrequentFace. */
            uint16_t teamNum = (uint16_t)team->teamNumber;
            d3 = (teamNum == 1) ? team1MostFrequentFace : team2MostFrequentFace;
        } else {
            d3 = (uint16_t)(int16_t)(int8_t)pi->face;
        }
        d0 += d3;
        d0 += d6;
        _CDrawSpriteHotspotInCameraView(d0, d1, d2);

    next_player:
        d2 += 7;
        if (trainingGame != 0 && drawTrainingTopHalfOfTheBench != 0) {
            pi -= 2; /* go backward: +1 - 2 = -1 */
        } else {
            pi++;
        }
    }
}

/* ================================================================
 * drawSubstitutesMenu (SWOS2.s line 13413)
 *
 * Draws the substitutes selection menu overlay.
 * ================================================================ */
static void drawSubstitutesMenu(void)
{
    uint16_t d1, d2, d3, d4, d7;
    TeamGame *a2 = (TeamGame *)benchTeamGame;
    uint8_t *charsTable = (uint8_t *)smallCharsTable;
    uint16_t *playerOffsets = ingameTeamPlayerOffsets;

    /* fill benchColors from team kit colors */
    {
        uint16_t *bc = benchColors;
        bc[0] = (uint16_t)a2->prShirtCol;
        bc[1] = (uint16_t)a2->prStripesCol;
        bc[2] = (uint16_t)a2->prShortsCol;
        bc[3] = (uint16_t)a2->prSocksCol;
        bc[4] = 3;
        bc[5] = 1;
        bc[6] = 3;
        bc[7] = 1;
        bc[8] = 0xFFFF;
    }

    /* find first color not 2 or 15 */
    {
        uint16_t *scan = benchColors;
        do { d1 = *scan++; } while (d1 == 2 || d1 == 15);
    }

    /* find second distinct color not 2 or 15 */
    {
        uint16_t *scan = benchColors;
        do { d2 = *scan++; } while (d2 == 2 || d2 == 15 || d2 == d1);
    }

    /* find third color */
    {
        uint16_t *scan = benchColors;
        do { d3 = *scan++; } while (d3 == d1 || d3 == d2);
    }

    subsMenuBackgroundColor = d1;
    subsMenuHighlightColor = d2;
    if ((int16_t)d3 < 0)
        d3 = d2;
    subsMenuEntryFrameColor = d3;

    d2 = 36;

    /* draw "FORMATION" entry for state 3 */
    if (subsState == 3)
        goto skip_formation_entry;

    if (subsState == 4) {
        d4 = subsMenuBackgroundColor;
        if ((int16_t)plToBeSubstitutedOrd < 0)
            d4 = subsMenuHighlightColor;
        drawSubstitutesMenuEntry(d2, d4, (int16_t)0xFFFF, charsTable, a2, playerOffsets);
        goto skip_formation_entry;
    }

    /* state 1: draw with plToEnterGameIndex as selected */
    d7 = plToEnterGameIndex;
    d4 = subsMenuBackgroundColor;
    drawSubstitutesMenuEntry(d2, d4, (int16_t)d7, charsTable, a2, playerOffsets);

skip_formation_entry:
    d2 += 15;

    /* draw title box - shadow */
    {
        uint16_t saved_d2 = d2;
        uint16_t bx = subsMenuX - cameraX + 43;
        _FillRectangle(bx, d2, 41, 33, 8);
        _FillRectangle(bx - 2, d2 - 2, 41, 33, subsMenuBackgroundColor);
        _FillRectangle(bx - 1, d2 - 1, 39, 31, subsMenuHighlightColor);

        /* draw title sprite */
        {
            uint16_t tx = bx - 1 + 2;
            uint16_t ty = d2 - 1 + 2;
            uint16_t spr = SPR_SUBSTITUTES;
            if (subsState != 1 && subsState != 4) {
                panicReasonCode = 0x4554;
                _PanicHaltLoop();
            }
            _CDrawSprite16Pixels(spr, tx, ty);
        }
        d2 = saved_d2;
    }

    d2 += 0x25; /* 37 */

    /* draw each player entry */
    for (d7 = 0; d7 < 11; d7++) {
        uint16_t ordinal;
        uint16_t save_d7 = d7;

        getBenchPlayerPointer(d7, &ordinal);
        d7 = ordinal;

        d4 = subsMenuBackgroundColor;
        if (d7 == plToBeSubstitutedPos)
            d4 = subsMenuHighlightColor;

        if (subsState == 4) {
            if (d7 == selectedPlayerInSubs) {
                d4 = 10;
                if (d4 == subsMenuBackgroundColor || d4 == subsMenuHighlightColor) {
                    d4 = 11;
                    if (d4 == subsMenuBackgroundColor || d4 == subsMenuHighlightColor)
                        d4 = 14;
                }
            }
        }

        drawSubstitutesMenuEntry(d2, d4, (int16_t)d7, charsTable, a2, playerOffsets);
        d7 = save_d7;
        d2 += 10;
    }
}

/* ================================================================
 * drawSubstitutesMenuEntry (SWOS2.s line 13566)
 *
 * Draws a single entry in the substitutes menu.
 * d2 = y, d4 = base color, d7 = selected player index (-1 = "FORMATION")
 * a1 = chars table, a2 = team game, a4 = player offsets
 * ================================================================ */
static void drawSubstitutesMenuEntry(uint16_t y, uint16_t baseColor, int16_t selectedIndex,
                                     uint8_t *charsTable, TeamGame *teamGame, uint16_t *playerOffsets)
{
    uint16_t x0, d6, d5;
    int16_t d7 = selectedIndex;

    /* draw shadow rectangle */
    x0 = subsMenuX - cameraX;
    _FillRectangle(x0, y, 126, 11, 8);

    /* draw frame rectangle */
    _FillRectangle(x0 - 2, y - 2, 126, 11, subsMenuEntryFrameColor);

    /* draw background rectangle */
    _FillRectangle(x0 - 1, y - 1, 124, 9, baseColor);

    /* "FORMATION" entry (d7 == -1 in state 4) */
    if (subsState == 4 && d7 == -1) {
        uint16_t cx = subsMenuX - cameraX + 63;
        y += 1;
        _DrawMenuTextCentered(cx, y, 2, aFormation, charsTable);
        y -= 1;
        return;
    }

    /* get player offset from offsets table */
    d6 = playerOffsets[(uint16_t)d7];

    /* get shirt number — PlayerInfo is at offset (d6 + 42) from TeamGame base */
    {
        PlayerInfo *pi = piFromOffset(teamGame, d6);
        d5 = (uint16_t)(int16_t)(int8_t)pi->shirtNumber;

        /* draw face sprite if individual skin color enabled */
        if (useIndividualPlayerSkinColor != 0) {
            uint16_t faceIdx = (uint16_t)(int16_t)(int8_t)pi->face;
            uint16_t faceSpr = playerFacesTable[faceIdx];
            uint16_t fx = subsMenuX - cameraX;

            /* check if this player is marked — blink face sprite */
            {
                TeamGame *btg = (TeamGame *)benchTeamGame;
                if ((uint16_t)btg->markedPlayer == (uint16_t)d7) {
                    if ((frameCount & 4) == 0)
                        goto skip_face;
                }
            }
            _CDrawSprite16Pixels(faceSpr, fx, y);
        }
    skip_face:;
    }

    /* check for permanent injury (FFFE) */
    {
        TeamGeneralInfo *bt = (TeamGeneralInfo *)benchTeam;
        Sprite **sprites = bt->spritesTable;
        Sprite *spr = sprites[(uint16_t)d7];
        if (spr->injuryLevel == (int16_t)0xFFFE) {
            uint16_t ix = subsMenuX - cameraX;
            if (useIndividualPlayerSkinColor != 0)
                ix += 9;
            _CDrawSprite16Pixels(SPR_RED_CROSS_INJURY, ix, y);
            goto draw_name;
        }
    }

    /* draw shirt number */
    {
        uint16_t nx = subsMenuX - cameraX + 4;
        if (useIndividualPlayerSkinColor != 0)
            nx += 9;

        {
            uint8_t numStr[4];
            uint8_t *p = numStr;
            uint8_t num = (uint8_t)d5;
            if (num >= 10) {
                *p++ = '1';
                num -= 10;
            }
            *p = num + '0';
            p++;
            *p = 0;

            y += 1;
            _DrawMenuTextCentered(nx, y, 2, numStr, charsTable);
        }
    }

    /* draw card icon if applicable (only for players 0-10) */
    if ((uint16_t)d7 < 11) {
        TeamGeneralInfo *bt = (TeamGeneralInfo *)benchTeam;
        Sprite **sprites = bt->spritesTable;
        Sprite *spr = sprites[(uint16_t)d7];
        int16_t cards = spr->cards;
        if (cards != 0) {
            uint16_t cardSpr;
            if (cards < 0) {
                cardSpr = SPR_RED_CARD_SMALL;
            } else {
                cardSpr = SPR_YELLOW_CARD_SMALL;
            }
            y -= 1;
            {
                uint16_t cx = subsMenuX - cameraX;
                if (useIndividualPlayerSkinColor != 0)
                    cx += 9;
                _CDrawSprite16Pixels(cardSpr, cx, y);
            }
            y += 1;
        }
    }

draw_name:
    /* draw player short name */
    {
        PlayerInfo *pi = piFromOffset(teamGame, d6);
        uint16_t nameX = subsMenuX - cameraX + 9;
        if (useIndividualPlayerSkinColor != 0)
            nameX += 9;
        _DrawMenuText(nameX, y, 2, (uint8_t *)pi->shortName, charsTable);
    }

    /* draw position string */
    {
        PlayerInfo *pi = piFromOffset(teamGame, d6);
        uint8_t *posStr = (uint8_t *)playerPositionsStringTable[(uint16_t)(int16_t)(int8_t)pi->position];
        uint16_t posX = subsMenuX - cameraX + 116;
        _DrawMenuTextCentered(posX, y, 2, posStr, charsTable);
    }
    y -= 1;

    /* draw injury indicator */
    {
        PlayerInfo *pi = piFromOffset(teamGame, d6);
        uint8_t injByte = (uint8_t)pi->injuriesBitfield;
        if (injByte == 0)
            return;
        if (injByte == 0x20)
            goto draw_injury;
        {
            uint16_t level = (injByte & 0xE0) >> 5;
            uint16_t frame = frameCount & 7;
            if (level < frame)
                return;
        }
    draw_injury:
        {
            uint16_t ix = subsMenuX - cameraX + 101;
            _CDrawSprite16Pixels(SPR_RED_CROSS_INJURY, ix, y);
        }
    }
}

/* ================================================================
 * drawFormationMenu (SWOS2.s line 13757)
 *
 * Draws the formation selection menu.
 * ================================================================ */
static void drawFormationMenu(void)
{
    uint16_t d1, d2, d3, d7;
    TeamGame *a2 = (TeamGame *)benchTeamGame;
    uint8_t *charsTable = (uint8_t *)smallCharsTable;

    /* fill benchColors from team kit colors (same as drawSubstitutesMenu) */
    {
        uint16_t *bc = benchColors;
        bc[0] = (uint16_t)a2->prShirtCol;
        bc[1] = (uint16_t)a2->prStripesCol;
        bc[2] = (uint16_t)a2->prShortsCol;
        bc[3] = (uint16_t)a2->prSocksCol;
        bc[4] = 3;
        bc[5] = 1;
        bc[6] = 3;
        bc[7] = 1;
        bc[8] = 0xFFFF;
    }

    /* find first color not 2 or 15 */
    {
        uint16_t *scan = benchColors;
        do { d1 = *scan++; } while (d1 == 2 || d1 == 15);
    }

    /* find second distinct color */
    {
        uint16_t *scan = benchColors;
        do { d2 = *scan++; } while (d2 == 2 || d2 == 15 || d2 == d1);
    }

    /* find third color */
    {
        uint16_t *scan = benchColors;
        do { d3 = *scan++; } while (d3 == d1 || d3 == d2);
    }

    subsMenuBackgroundColor = d1;
    subsMenuHighlightColor = d2;
    if ((int16_t)d3 < 0)
        d3 = d2;
    subsMenuEntryFrameColor = d3;

    d2 = 11;
    d2 += 15;

    /* draw title box */
    {
        uint16_t saved_d2 = d2;
        uint16_t bx = formationMenuX - cameraX + 10;
        _FillRectangle(bx, d2, 41, 33, 8);
        _FillRectangle(bx - 2, d2 - 2, 41, 33, subsMenuBackgroundColor);
        _FillRectangle(bx - 1, d2 - 1, 39, 31, subsMenuHighlightColor);

        /* draw title sprite */
        {
            uint16_t tx = bx - 1 + 2;
            uint16_t ty = d2 - 1 + 2;
            _CDrawSprite16Pixels(SPR_FORMATION, tx, ty);
        }
        d2 = saved_d2;
    }

    d2 += 37;

    /* draw each formation entry */
    for (d7 = 0; d7 < 18; d7++) {
        uint16_t color = subsMenuBackgroundColor;
        if (d7 == selectedFormationEntry)
            color = subsMenuHighlightColor;
        drawFormationMenuEntry(charsTable, d2, color, d7);
        d2 += 10;
    }
}

/* ================================================================
 * drawFormationMenuEntry (SWOS2.s line 13858)
 *
 * Draws a single formation menu entry.
 * d2 = y, d4 = entry color, d7 = entry index, a1 = chars table
 * ================================================================ */
static void drawFormationMenuEntry(uint8_t *charsTable, uint16_t y, uint16_t entryColor, uint16_t entryIndex)
{
    uint16_t x0;
    uint8_t *textStr;

    /* draw shadow */
    x0 = formationMenuX - cameraX;
    _FillRectangle(x0, y, 60, 11, 8);

    /* draw frame */
    _FillRectangle(x0 - 2, y - 2, 60, 11, subsMenuEntryFrameColor);

    /* draw background */
    _FillRectangle(x0 - 1, y - 1, 58, 9, entryColor);

    /* get formation name string */
    if (entryIndex < 12) {
        textStr = (uint8_t *)tacticsStringTable[entryIndex];
    } else {
        textStr = (uint8_t *)teamTacticsTable[entryIndex];
    }

    /* draw centered text */
    {
        uint16_t tx = formationMenuX - cameraX + 30;
        _DrawMenuTextCentered(tx, y, 2, textStr, charsTable);
    }
}

/* ================================================================
 * Chunk 117: ShowFormationMenu + SubsBenchOff
 * ================================================================ */

/* ShowFormationMenu — set state to show formation selection overlay (SWOS2.s line 10015) */
__attribute__((used, externally_visible))
void _ShowFormationMenu(void)
{
    subsState = 2;
    subsBlockFire = 1;
    overlayActiveFlag = 1;
    TeamGeneralInfo *team = (TeamGeneralInfo *)benchTeam;
    selectedFormationEntry = (uint16_t)team->tactics;
}

/* SubsBenchOff — dismiss bench/subs overlay (SWOS2.s line 10025) */
__attribute__((used, externally_visible))
void _SubsBenchOff(void)
{
    subsState = 0;
    subsBlockDirections = 1;
    subsBlockFire = 1;
    overlayActiveFlag = 0;
    subsWriteOnly = 2;
}

/* ================================================================
 * Chunk 153: GetBenchPlayerPointer  (SWOS2.s line 11315)
 *
 * Maps a bench player index (0-10) to an in-game player pointer.
 * Uses the current tactics to select the formation position table,
 * looks up the player ordinal, then computes the offset into the
 * team's in-game player data.
 *
 * Returns: ordinal (uint16_t), stores pointer in benchPlayerResultA0
 * ================================================================ */

/* Result pointer for asm bridge (asm reads this into A0) */
uint8_t *benchPlayerResultA0;

__attribute__((used, externally_visible))
uint16_t _GetBenchPlayerPointer(uint16_t index)
{
    uint16_t tactics = pl1Tactics;
    TeamGeneralInfo *team = (TeamGeneralInfo *)benchTeam;
    if (team->teamNumber != 1)
        tactics = pl2Tactics;

    uint8_t *posTable = (uint8_t *)positionsTable[tactics];
    uint16_t ordinal = (uint16_t)posTable[index] - 1;

    uint16_t offset = ingameTeamPlayerOffsets[ordinal];
    benchPlayerResultA0 = (uint8_t *)benchTeamGame + offset;

    return ordinal;
}

/* ================================================================
 * UpdatePlayerToBeSubstitutedPosition  (SWOS2.s line 10351)
 *
 * Reads plToBeSubstitutedOrd, calls GetBenchPlayerPointer to get
 * the position ordinal, and stores result in plToBeSubstitutedPos.
 * ================================================================ */
__attribute__((used, externally_visible))
void _UpdatePlayerToBeSubstitutedPosition(void)
{
    plToBeSubstitutedPos = _GetBenchPlayerPointer(plToBeSubstitutedOrd);
}

/* ================================================================
 * IsPlayerOkToSubstitute  (SWOS2.s line 10359)
 *
 * Checks if the currently highlighted player can be substituted.
 * Returns: 1 = ok to substitute, 0 = do not substitute.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _IsPlayerOkToSubstitute(void)
{
    if (subsState == 4) {
        /* In state 4, only ordinal 0 (goalkeeper) is ok */
        if (plToBeSubstitutedOrd == 0)
            return 1;
        return 0;
    }

    /* Check candidate player's status */
    uint16_t offset = ingameTeamPlayerOffsets[plToBeSubstitutedPos];
    PlayerInfo *pi = piFromOffset((TeamGame *)benchTeamGame, offset);

    /* Red-carded player (numYellowCards >= 2) — ok to sub off */
    if ((uint8_t)pi->numYellowCards >= 2)
        return 1;

    /* In state 3, check disallow subs flag (bit 7 of field_B) */
    if (subsState == 3) {
        if ((int8_t)pi->field_B < 0)
            return 1;
    }

    return 0;
}

/* ================================================================
 * FindInitialPlayerToBeSubstituted  (SWOS2.s line 10425)
 *
 * Searches the team for a player matching the position of the player
 * about to enter the game. Priority: exact match > group match > first
 * available. Sets plToBeSubstitutedOrd and plToBeSubstitutedPos.
 * ================================================================ */
__attribute__((used, externally_visible))
void _FindInitialPlayerToBeSubstituted(void)
{
    TeamGame *tg = (TeamGame *)benchTeamGame;

    /* Get position of the player entering the game */
    uint16_t enterOff = ingameTeamPlayerOffsets[plToEnterGameIndex];
    int8_t targetPos = piFromOffset(tg, enterOff)->position;

    int16_t firstAvailable = -1;   /* d6 */
    int16_t exactMatch = -1;       /* d5 */
    int16_t groupMatch = -1;       /* d4 */

    for (uint16_t i = 0; i < 11; i++) {
        _GetBenchPlayerPointer(i);
        PlayerInfo *pi = (PlayerInfo *)(benchPlayerResultA0 + 42);

        /* Skip red-carded players */
        if ((uint8_t)pi->numYellowCards >= 2)
            continue;

        /* Record first available (if not yet found) */
        if (firstAvailable < 0)
            firstAvailable = (int16_t)i;

        /* Group match check (only if no group match found yet) */
        if (groupMatch < 0) {
            int8_t pp = pi->position;

            if (targetPos == POS_DEFENDER || targetPos == POS_RIGHT_BACK || targetPos == POS_LEFT_BACK) {
                /* Defender group: target is DEF/RB/LB */
                if (pp == POS_DEFENDER || pp == POS_RIGHT_BACK || pp == POS_LEFT_BACK)
                    groupMatch = (int16_t)i;
            } else if (targetPos == POS_MIDFIELDER || targetPos == POS_RIGHT_WING || targetPos == POS_LEFT_WING) {
                /* Midfielder group: target is MID/RW/LW */
                if (pp == POS_MIDFIELDER || pp == POS_RIGHT_WING || pp == POS_LEFT_WING)
                    groupMatch = (int16_t)i;
            }
        }

        /* Exact position match (only if no exact match found yet) */
        if (exactMatch < 0) {
            if (pi->position == targetPos)
                exactMatch = (int16_t)i;
        }
    }

    /* Pick best match: exact > group > first available */
    int16_t selected;
    if (exactMatch >= 0)
        selected = exactMatch;
    else if (groupMatch >= 0)
        selected = groupMatch;
    else
        selected = firstAvailable;

    plToBeSubstitutedOrd = (uint16_t)selected;
    plToBeSubstitutedPos = _GetBenchPlayerPointer((uint16_t)selected);
}

/* ================================================================
 * HandleFormationMenuMovement  (SWOS2.s line 10368)
 *
 * Handles left/right/up/down input while in the formation menu.
 * Left/right cycle through selectedFormationEntry (0-17).
 * Up/down call SubsBenchOff to exit.
 * ================================================================ */
__attribute__((used, externally_visible))
void _HandleFormationMenuMovement(void)
{
    uint16_t dir = subsDirection;

    if (dir == 0) {
        /* Left press */
        subsBlockDirections = 1;
        if ((int16_t)selectedFormationEntry >= 0) {
            if (selectedFormationEntry == 0)
                return;
            selectedFormationEntry--;
            return;
        }
        selectedFormationEntry = 7;
        return;
    }

    if (dir == 4) {
        /* Right press */
        subsBlockDirections = 1;
        if ((int16_t)selectedFormationEntry >= 0) {
            if (selectedFormationEntry == 0x11)
                return;
            selectedFormationEntry++;
            return;
        }
        selectedFormationEntry = 0;
        return;
    }

    if (dir == 2 || dir == 6) {
        /* Up or down press — exit formation menu */
        _SubsBenchOff();
        return;
    }

    /* Other directions: do nothing */
}

/* ================================================================
 * SwapPlayerNameData  (SWOS2.s line 10416)
 *
 * Swaps two entries in the plOrdinalsTable and plNamesLengthTableCopy
 * arrays. If the bench team is the bottom team, uses offset +16.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SwapPlayerNameData(uint16_t enterIndex, uint16_t leaveIndex)
{
    uint8_t *ordinals = (uint8_t *)&plOrdinalsTable;
    uint8_t *nameLens = (uint8_t *)&plNamesLengthTableCopy;

    if (benchTeamGame != (uint32_t)&topTeamInGame) {
        ordinals += 16;
        nameLens += 16;
    }

    /* Swap ordinals */
    uint8_t tmp = ordinals[enterIndex];
    ordinals[enterIndex] = ordinals[leaveIndex];
    ordinals[leaveIndex] = tmp;

    /* Swap name lengths */
    tmp = nameLens[enterIndex];
    nameLens[enterIndex] = nameLens[leaveIndex];
    nameLens[leaveIndex] = tmp;
}

/* ================================================================
 * Chunk 156: TrainingSwapBenchTeams + LeaveSubsMenu +
 *            CheckForThrowInAndGoalkeepersBall
 * ================================================================ */

/* ---- TrainingSwapBenchTeams  (SWOS2.s line 9907) ----
 * Swaps the active bench team during a training match.
 * Toggles swap flags, swaps Y positions, switches to opponent team,
 * and updates benchTeamGame pointer.
 */
__attribute__((used, externally_visible))
void _TrainingSwapBenchTeams(void)
{
    subsTeamsSwapped ^= 1;
    subsTrainingTopTeam ^= 1;

    /* Swap bench Y positions */
    uint16_t tmp = benchY;
    benchY = opponentBenchY;
    opponentBenchY = tmp;

    /* Switch to opponent team */
    TeamGeneralInfo *team = (TeamGeneralInfo *)benchTeam;
    team = team->opponentsTeam;
    benchTeam = (uint32_t)team;

    /* Set benchTeamGame based on team number */
    TeamGame *gamePtr = topTeamPtr;
    if (team->teamNumber == 2)
        gamePtr = bottomTeamPtr;
    benchTeamGame = (uint32_t)gamePtr;
}

/* ---- LeaveSubsMenu  (SWOS2.s line 9787) ----
 * Clean exit from substitution menu. Resets all subs-related state,
 * re-initializes goal highlight, triggers bench-off camera transition.
 * Falls through to loc_10CF6E which clears deadBenchVar.
 */
__attribute__((used, externally_visible))
void _LeaveSubsMenu(void)
{
    _InitCurrentGoalHighlight();
    benchOff = 1;
    cameraLeavingSubsTimer = 50;
    leavingSubsMenu = 1;
    _CheckIfGoalkeeperClaimedTheBall();
    bench1Called = 0;
    bench2Called = 0;
    inSubstitutesMenu = 0;
    subsTeamsSwapped = 0;
    overlayActiveFlag = 0;
    writeOnly01 = 1;
    subsState = 0;
    plToBeSubstitutedPos = 0xFFFF;
    pl1BenchTimeoutCounter = 0;
    pl2BenchTimeoutCounter = 0;
    subsPrevPl1Direction = 0xFFFF;
    subsPrevPl2Direction = 0xFFFF;
    subsPl1TapCount = 0;
    subsPl2TapCount = 0;
    subsWriteOnly = 2;
    deadBenchVar = 0;  /* from loc_10CF6E tail */
}

/* ---- CheckForThrowInAndGoalkeepersBall  (SWOS2.s line 10068) ----
 * End-of-break handler: if the controlled player was in throw-in state,
 * transition them back to normal. Then check if goalkeeper claimed ball.
 */
__attribute__((used, externally_visible))
void _CheckForThrowInAndGoalkeepersBall(void)
{
    Sprite *player = lastTeamPlayedBeforeBreak->controlledPlayer;
    if (player != 0) {
        if (player->playerState == PL_THROW_IN) {
            hideBall = 0;
            player->playerState = PL_NORMAL;
            _SetPlayerAnimationTable(playerNormalStandingAnimTable, player);
        }
    }
    _CheckIfGoalkeeperClaimedTheBall();
}

/* ================================================================
 * Chunk 157: IsThisPlayerOkToSelect + FirePressedInSubsMenu +
 *            HandleFormationMenuControls + UpdateSubstitutePlayerIndices +
 *            CallUpdateSubstitutePlayerIndices + state3_handle_fire_press
 * ================================================================ */

/* ---- IsThisPlayerOkToSelect  (SWOS2.s line 9896) ----
 * Checks if the bench player at benchPlayerIndex is valid for selection.
 * Returns 0 = OK, -1 = not OK. Caller checks sign flag (bmi).
 */
__attribute__((used, externally_visible))
int16_t _IsThisPlayerOkToSelect(void)
{
    uint16_t numSubs = team1NumSubs;
    TeamGeneralInfo *team = (TeamGeneralInfo *)benchTeam;
    if (team->teamNumber != 1)
        numSubs = team2NumSubs;

    /* Max substitutions reached? */
    if (numSubs == maxSubsPerTeam)
        return -1;

    /* Compute player data pointer: (benchPlayerIndex + 10) * 61 */
    uint16_t playerIdx = benchPlayerIndex + 10;
    uint32_t offset = (uint32_t)playerIdx * PLAYERINFO_SIZE;
    PlayerInfo *pi = (PlayerInfo *)((uint8_t *)benchTeamGame + offset + 42);

    /* Already substituted? (pi->skip repurposed as substitution flag) */
    if (pi->skip != 0)
        return -1;

    /* Inactive player? (negative position) */
    if (pi->position < 0)
        return -1;

    /* Red card or worse? */
    if ((uint8_t)pi->numYellowCards >= 2)
        return -1;

    return 0;
}

/* ---- FirePressedInSubsMenu  (SWOS2.s line 9938) ----
 * Handles fire button press in bench menu (subsState == 0).
 * benchPlayerIndex == 0: enters mark-players mode (state 4).
 * benchPlayerIndex >= 1: starts substitution (state 1).
 */
__attribute__((used, externally_visible))
void _FirePressedInSubsMenu(void)
{
    uint16_t idx = benchPlayerIndex;

    if (idx == 0) {
        /* Mark players mode */
        selectedPlayerInSubs = 0xFFFF;
        subsState = 4;
        subsBlockFire = 1;
        overlayActiveFlag = 1;
        plToBeSubstitutedOrd = 0xFFFF;
        plToBeSubstitutedPos = 0xFFFF;
        selectedPlayerInSubs = 0xFFFF;
        return;
    }

    /* Check if bench player is valid for substitution */
    uint16_t ordinal = idx + 10;
    uint16_t offset = ingameTeamPlayerOffsets[ordinal];
    PlayerInfo *pi = piFromOffset((TeamGame *)benchTeamGame, offset);

    /* Inactive player? */
    if (pi->position < 0)
        return;

    /* Red card? */
    if ((uint8_t)pi->numYellowCards >= 2)
        return;

    /* Start substitution process */
    subsState = 1;
    subsBlockFire = 1;
    overlayActiveFlag = 1;
    plToEnterGameIndex = idx + 10;

    if (gameMaxSubstitutes > 5 && plToEnterGameIndex == 11) {
        plToBeSubstitutedPos = 0;
        return;
    }

    _FindInitialPlayerToBeSubstituted();
}

/* ---- HandleFormationMenuControls  (SWOS2.s line 10216) ----
 * Handles controls while in formation selection menu (subsState == 2).
 * Fire applies the selected formation; otherwise delegates to movement.
 */
__attribute__((used, externally_visible))
void _HandleFormationMenuControls(void)
{
    if (subsFire == 0) {
        /* No fire — handle movement */
        _HandleFormationMenuMovement();
        return;
    }

    /* Fire pressed — apply selected formation */
    overlayActiveFlag = 0;
    TeamGeneralInfo *team = (TeamGeneralInfo *)benchTeam;
    team->tactics = (int16_t)selectedFormationEntry;

    if (team->teamNumber == 1)
        pl1Tactics = selectedFormationEntry;
    else
        pl2Tactics = selectedFormationEntry;

    _ApplyTeamTactics((TeamGame *)benchTeamGame);
    _SubsBenchOff();
}

/* ---- UpdateSubstitutePlayerIndices  (SWOS2.s line 10243) ----
 * Cycles through substitute candidates based on direction input.
 * Direction 0 = previous, 4 = next, 2/6 = exit bench.
 * Scans forward/backward with wrap-around to find valid candidates.
 */
__attribute__((used, externally_visible))
void _UpdateSubstitutePlayerIndices(void)
{
    uint16_t dir = subsDirection;

    /* Left/right = exit bench */
    if (dir == 2 || dir == 6) {
        _SubsBenchOff();
        return;
    }

    /* Guard: not state 4, entering player 11 with <=5 max subs */
    if (subsState != 4) {
        if (plToEnterGameIndex == 11 && gameMaxSubstitutes <= 5)
            return;
    }

    dir = subsDirection;
    if (dir == 4)
        goto select_next;
    if (dir == 0)
        goto select_prev;
    return;

select_next:
    subsBlockDirections = 1;
    if (subsState == 4 && (int16_t)plToBeSubstitutedOrd < 0) {
        plToBeSubstitutedOrd = 1;
        _UpdatePlayerToBeSubstitutedPosition();
        return;
    }
    if (plToBeSubstitutedOrd == 10)
        return;
    goto inc_forward;

select_prev:
    subsBlockDirections = 1;
    if (subsState == 4) {
        if ((int16_t)plToBeSubstitutedOrd < 0)
            return;
        if (plToBeSubstitutedOrd == 1) {
            plToBeSubstitutedOrd = 0xFFFF;
            plToBeSubstitutedPos = 0xFFFF;
            return;
        }
    }
    if (plToBeSubstitutedOrd == 0)
        return;
    goto dec_backward;

inc_forward:
    plToBeSubstitutedOrd++;
    if (plToBeSubstitutedOrd == 11)
        goto dec_backward;
    _UpdatePlayerToBeSubstitutedPosition();
    if (_IsPlayerOkToSubstitute() != 0)
        goto inc_forward;
    return;

dec_backward:
    plToBeSubstitutedOrd--;
    if ((int16_t)plToBeSubstitutedOrd < 0)
        goto inc_forward;
    _UpdatePlayerToBeSubstitutedPosition();
    if (_IsPlayerOkToSubstitute() != 0)
        goto dec_backward;
}

/* ---- CallUpdateSubstitutePlayerIndices  (SWOS2.s line 10083) ----
 * Thin wrapper that calls UpdateSubstitutePlayerIndices.
 */
__attribute__((used, externally_visible))
void _CallUpdateSubstitutePlayerIndices(void)
{
    _UpdateSubstitutePlayerIndices();
}

/* ---- state3_handle_fire_press  (SWOS2.s line 10089) ----
 * Handles fire button in subsState == 3 (substitute confirmation).
 * Fire pressed: clears overlay and exits.
 * Fire not pressed: delegates to UpdateSubstitutePlayerIndices.
 * Note: dead code after bra.w in original asm is omitted.
 */
__attribute__((used, externally_visible))
void _state3_handle_fire_press(void)
{
    if (subsFire != 0) {
        overlayActiveFlag = 0;
        return;
    }
    _UpdateSubstitutePlayerIndices();
}

/* ================================================================
 * Chunk 158: SelectPlayerToSubstituteMenuHandler + SubstitutePlayer +
 *            MarkPlayersMenuHandler
 * ================================================================ */

/* ---- SelectPlayerToSubstituteMenuHandler  (SWOS2.s line 9922) ----
 * Handles fire button in subsState == 1 (player selected for substitution).
 * Fire: confirms substitution — increments sub counters, sets up sprite
 * animation for the leaving player, records positions.
 * No fire: delegates to CallUpdateSubstitutePlayerIndices.
 */
__attribute__((used, externally_visible))
void _SelectPlayerToSubstituteMenuHandler(void)
{
    if (subsFire == 0) {
        _CallUpdateSubstitutePlayerIndices();
        return;
    }

    overlayActiveFlag = 0;
    substituteInProgress = 1;

    /* Increment substitution counters for the correct team */
    TeamGeneralInfo *team = (TeamGeneralInfo *)benchTeam;
    if (team->teamNumber == 1) {
        team1NumSubs++;
        if (plToEnterGameIndex == 11)
            team1GoalkeeperReplaced++;
    } else {
        team2NumSubs++;
        if (plToEnterGameIndex == 11)
            team2GoalkeeperReplaced++;
    }

    subsBlockFire = 1;
    subsState = 0;
    subsWriteOnly = 2;

    /* Set up substitution sprite and animation data */
    uint16_t pos = plToBeSubstitutedPos;
    teamThatSubstitutes = benchTeam;
    Sprite **table = team->spritesTable;
    Sprite *sprite = table[pos];
    substitutedPlSprite = (uint32_t)sprite;
    sprite->cards = 0;
    substitutedPlX = sprite->x.s.i;
    substitutedPlY = sprite->y.s.i;
    substitutedPlDestX = plLeavingX;
    substitutedPlDestY = plComingYTmp;
    plSubstitutedX = 39;
    plSubstitutedY = 449;
}

/* ---- SubstitutePlayer  (SWOS2.s line 9968) ----
 * Performs the actual player substitution:
 *   - Clears injury/sent-away on the leaving player's sprite
 *   - Updates marked player tracking
 *   - Swaps name/ordinal data between the two players
 *   - Marks leaving player's position as inactive (0xFF)
 *   - Byte-swaps 61 bytes of PlayerInfo between the two slots
 *   - Rebuilds sprite frame indices, re-applies tactics
 *   - Leaves the subs menu
 */
__attribute__((used, externally_visible))
void _SubstitutePlayer(void)
{
    Sprite *sprite = (Sprite *)substitutedPlSprite;
    sprite->injuryLevel = 0;
    sprite->sentAway = 0;

    uint16_t leaving = plToBeSubstitutedPos;  /* d7 — on-field player going off */
    uint16_t entering = plToEnterGameIndex;   /* d6 — bench player coming on */
    TeamGame *tg = (TeamGame *)benchTeamGame;

    /* Update marked player tracking */
    if (tg->markedPlayer == (int16_t)leaving)
        tg->markedPlayer = -1;
    if (tg->markedPlayer == (int16_t)entering) {
        tg->markedPlayer = -1;
        if (leaving != 0)
            tg->markedPlayer = (int16_t)leaving;
    }

    _SwapPlayerNameData(entering, leaving);

    /* Look up raw byte offsets */
    uint16_t enterOff = ingameTeamPlayerOffsets[entering];
    uint16_t leaveOff = ingameTeamPlayerOffsets[leaving];

    /* Mark leaving player's position as inactive before swap */
    uint8_t *base = (uint8_t *)tg;
    base[leaveOff + 46] = 0xFF;  /* PLAYERINFOHEADER_POSITION */

    /* Swap 61 bytes of PlayerInfo data (offset 42 from player block start) */
    uint8_t *pE = base + enterOff + 42;
    uint8_t *pL = base + leaveOff + 42;
    for (int16_t i = 0; i < 61; i++) {
        uint8_t tmp = pE[i];
        pE[i] = pL[i];
        pL[i] = tmp;
    }

    _InitializePlayerSpriteFrameIndices();
    _ApplyTeamTactics((TeamGame *)benchTeamGame);
    waitForPlayerToGoInTimer = 100;
    subsBlockFire = 1;
    _LeaveSubsMenu();
}

/* ---- MarkPlayersMenuHandler  (SWOS2.s line 10022) ----
 * Handles controls in subsState == 4 (mark/swap players mode).
 *
 * Fire hold (8 frames) on valid player: toggles mark on that player.
 * Fire press with negative ord: enters formation menu.
 * Fire with two selected players: swaps their positions, data, and sprites.
 * No fire: delegates to UpdateSubstitutePlayerIndices.
 */
__attribute__((used, externally_visible))
void _MarkPlayersMenuHandler(void)
{
    /* Check if current player can be marked (no red card) and fire held long enough */
    uint16_t offset = ingameTeamPlayerOffsets[plToBeSubstitutedPos];
    PlayerInfo *pi = piFromOffset((TeamGame *)benchTeamGame, offset);
    TeamGame *tg = (TeamGame *)benchTeamGame;

    if ((uint8_t)pi->numYellowCards < 2 && subsFireTimer == 8) {
        /* Toggle mark on current player */
        uint16_t pos = plToBeSubstitutedPos;
        if (tg->markedPlayer == (int16_t)pos)
            pos = 0xFFFF;  /* unmark */
        tg->markedPlayer = (int16_t)pos;
        selectedPlayerInSubs = 0xFFFF;
        return;
    }

    /* Fire not pressed — delegate to movement/scanning */
    if (subsFire == 0) {
        _UpdateSubstitutePlayerIndices();
        return;
    }

    /* Fire pressed with negative ord → formation menu */
    if ((int16_t)plToBeSubstitutedOrd < 0) {
        subsWriteOnly = 2;
        _ShowFormationMenu();
        return;
    }

    /* Ord == 0 → exit */
    if (plToBeSubstitutedOrd == 0)
        return;

    /* No player selected yet? */
    if ((int16_t)selectedPlayerInSubs < 0) {
        /* First fire press → select current player */
        if (subsFireTimer == 1)
            selectedPlayerInSubs = plToBeSubstitutedPos;
        return;
    }

    /* Same player selected again? */
    if (selectedPlayerInSubs == plToBeSubstitutedPos) {
        /* Deselect on short press */
        if (subsFireTimer == 1)
            selectedPlayerInSubs = 0xFFFF;
        return;
    }

    /* Two different players selected — swap them */
    uint16_t player1 = selectedPlayerInSubs;    /* d7 — first selected */
    uint16_t player2 = plToBeSubstitutedPos;    /* d6 — second selected */

    /* Update marked player to follow the swap */
    if (tg->markedPlayer == (int16_t)player2) {
        tg->markedPlayer = (int16_t)player1;
    } else if (tg->markedPlayer == (int16_t)player1) {
        tg->markedPlayer = (int16_t)player2;
    }

    _SwapPlayerNameData(player2, player1);

    /* Swap PlayerInfo data (61 bytes) */
    uint16_t off1 = ingameTeamPlayerOffsets[player2];
    uint16_t off2 = ingameTeamPlayerOffsets[player1];
    uint8_t *base = (uint8_t *)tg;
    uint8_t *p1 = base + off1 + 42;
    uint8_t *p2 = base + off2 + 42;
    for (int16_t i = 0; i < 61; i++) {
        uint8_t tmp = p1[i];
        p1[i] = p2[i];
        p2[i] = tmp;
    }

    /* Swap Sprite data (110 bytes, preserving playerOrdinal) */
    TeamGeneralInfo *benchTm = (TeamGeneralInfo *)benchTeam;
    Sprite *sprite1 = benchTm->spritesTable[player2];
    Sprite *sprite2 = benchTm->spritesTable[player1];
    uint8_t *s1 = (uint8_t *)sprite1;
    uint8_t *s2 = (uint8_t *)sprite2;
    for (int16_t i = 0; i < 110; i++) {
        uint8_t tmp = s1[i];
        s1[i] = s2[i];
        s2[i] = tmp;
    }
    /* Restore playerOrdinal — each sprite keeps its own ordinal */
    int16_t tmpOrd = sprite1->playerOrdinal;
    sprite1->playerOrdinal = sprite2->playerOrdinal;
    sprite2->playerOrdinal = tmpOrd;

    _InitializePlayerSpriteFrameIndices();
    _ApplyTeamTactics((TeamGame *)benchTeamGame);
    selectedPlayerInSubs = 0xFFFF;
    overlayActiveFlag = 0;
    _SubsBenchOff();
}

/* ================================================================
 * Chunk 159: BenchMenuHandleControls
 * ================================================================ */

/* ---- BenchMenuHandleControls  (SWOS2.s line 9792) ----
 * Main per-frame dispatcher for all bench menu controls.
 * Routes to state-specific handlers based on subsState.
 * In idle state (0): handles direction, fire, training team swap.
 */
__attribute__((used, externally_visible))
void _BenchMenuHandleControls(void)
{
    if (substituteInProgress != 0)
        return;

    /* Dispatch based on current subs menu state */
    if (subsState != 0) {
        if (subsState == 1) { _SelectPlayerToSubstituteMenuHandler(); return; }
        if (subsState == 3) { _state3_handle_fire_press(); return; }
        if (subsState == 2) { _HandleFormationMenuControls(); return; }
        if (subsState == 4) { _MarkPlayersMenuHandler(); return; }
        return;
    }

    /* subsState == 0: bench idle — check if bench was dismissed */
    if (benchTeam == (uint32_t)&topTeamData) {
        if (bench1Called != 0) { _LeaveSubsMenu(); return; }
    } else {
        if (bench2Called != 0) { _LeaveSubsMenu(); return; }
    }

    uint16_t dir = subsDirection;

    /* Training top-team reverses up/down direction mapping */
    if (trainingGame != 0 && subsTrainingTopTeam != 0) {
        if (dir == 0) goto handle_inc;
        if (dir == 4) goto handle_dec;
    } else {
        if (dir == 0) goto handle_dec;
        if (dir == 4) goto handle_inc;
    }

    /* Left/right = leave, fire = start substitution */
    if (dir == 2 || dir == 6) { _LeaveSubsMenu(); return; }
    if (subsFire != 0) { _FirePressedInSubsMenu(); return; }
    return;

handle_inc:
    /* Scroll up in bench list (increment benchPlayerIndex) */
    if (gameState == ST_KEEPER_HOLDS_BALL) return;
    subsBlockDirections = 1;
    if (benchPlayerIndex == 5) return;  /* already at max */
    goto inc_scan;

inc_scan:
    benchPlayerIndex++;
    if (benchPlayerIndex == 6) goto dec_scan;  /* wrap around */
    if (_IsThisPlayerOkToSelect() < 0) goto inc_scan;
    return;

handle_dec:
    /* Scroll down in bench list (decrement benchPlayerIndex) */
    if (trainingGame == 0 || benchPlayerIndex != 0) {
        if (gameState == ST_KEEPER_HOLDS_BALL) return;
    }
    subsBlockDirections = 1;
    if (benchPlayerIndex != 0) goto dec_scan;
    /* At index 0 in training: swap teams */
    if (trainingGame != 0) _TrainingSwapBenchTeams();
    return;

dec_scan:
    benchPlayerIndex--;
    if (benchPlayerIndex == 0) return;
    if (_IsThisPlayerOkToSelect() < 0) goto dec_scan;
}
