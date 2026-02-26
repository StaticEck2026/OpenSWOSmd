#include "game_init.h"
#include "game_types.h"

/* ---- External globals (in SWOS2.s code/BSS sections) ---- */
extern uint32_t currentScorer;
extern Sprite  *lastPlayerBeforeGoalkeeper;
extern uint16_t goalScored;
extern uint16_t runSlower;
extern uint16_t whichCard;
extern Sprite  *bookedPlayer;
extern uint16_t playerHadBall;
extern uint32_t lastKeeperPlayed;
extern TeamGeneralInfo *lastTeamPlayed;
extern Sprite  *lastPlayerPlayed;
extern uint16_t penalty;
extern uint16_t goalCameraMode;
extern uint16_t goalOut;
extern uint16_t gameNotInProgressCounterWriteOnly;
extern uint16_t fireBlocked;
extern TeamGeneralInfo *lastTeamPlayedBeforeBreak;
extern uint16_t stoppageTimerTotal;
extern uint16_t stoppageTimerActive;
extern uint16_t stoppageEventTimer;
extern uint16_t inGameCounter;
extern uint16_t gameStatePl;
extern uint16_t gameState;
extern uint16_t breakState;
extern uint16_t breakCameraMode;
extern uint16_t teamPlayingUp;
extern uint16_t topTeamPlayerNo;
extern uint16_t bottomTeamPlayerNo;
extern uint16_t topTeamCoachNo;
extern uint16_t bottomTeamCoachNo;
extern uint16_t pl1Coach;
extern uint16_t pl2Coach;
extern TeamGeneralInfo topTeamData;
extern TeamGeneralInfo bottomTeamData;
extern Sprite           *team1SpritesTable[];
extern Sprite           *team2SpritesTable[];
extern TeamGame         *topTeamPtr;
extern TeamGame         *bottomTeamPtr;
extern TeamStatisticsData team1StatsData;
extern TeamStatisticsData team2StatsData;
extern uint16_t pl1Tactics;
extern uint16_t pl2Tactics;

/* InitGameSaveTeams globals */
extern TeamGame  topTeamInGame;
extern TeamGame  bottomTeamInGame;
extern TeamGame  topTeamSaved;
extern TeamGame  bottomTeamSaved;
extern uint16_t pitchNumberAndType;
extern uint8_t  pl1Fire;
extern uint8_t  pl2Fire;
extern uint16_t paletteIndex;

/* ---- External function declarations ---- */
extern void _Player1StatusProc(void);
extern void _Player2StatusProc(void);
extern void _FadeOutToBlack(void);
extern void _LoadPitchMap(uint16_t index);
extern void _FixupAndColorPitchFile(void);
extern void _ConvertColorsAndTextSprites(TeamGame *team1, TeamGame *team2);

/* ================================================================
 * InitTeamsData  (SWOS2.s line 19585)
 *
 * Clears all per-match game state globals, sets gameState to
 * ST_GAME_IN_PROGRESS, then initializes topTeamData and
 * bottomTeamData TeamGeneralInfo structs. Runs a 2-iteration
 * loop: first iteration fills the "top" slot, second fills
 * "bottom". If teamPlayingUp != 1, a0/a1 are swapped so the
 * struct that gets filled first is bottomTeamData instead.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitTeamsData(void)
{
    /* Clear per-match game state */
    currentScorer = 0;
    lastPlayerBeforeGoalkeeper = (Sprite *)0;
    goalScored = 0;
    runSlower = 0;
    whichCard = 0;
    bookedPlayer = (Sprite *)0;
    playerHadBall = 0;
    lastKeeperPlayed = 0;
    lastTeamPlayed = (TeamGeneralInfo *)0;
    lastPlayerPlayed = (Sprite *)0;
    penalty = 0;
    goalCameraMode = 0;
    goalOut = 0;
    gameNotInProgressCounterWriteOnly = 0;
    fireBlocked = 0;
    lastTeamPlayedBeforeBreak = (TeamGeneralInfo *)0;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    stoppageEventTimer = 0;
    inGameCounter = 0;
    gameStatePl = ST_GAME_IN_PROGRESS;
    gameState = ST_GAME_IN_PROGRESS;
    breakState = 0;
    breakCameraMode = 0xFFFF;

    /* 2-iteration loop: first for top team (d7=1, d5=1), second for bottom (d7=0, d5=2) */
    uint16_t          playerNo    = topTeamPlayerNo;
    uint16_t          coachNo     = topTeamCoachNo;
    uint16_t          isPlCoach   = pl1Coach;
    TeamGeneralInfo  *a0          = &topTeamData;       /* destination struct */
    TeamGeneralInfo  *a1          = &bottomTeamData;    /* opponent struct */
    Sprite          **spritesTab  = team1SpritesTable;
    TeamGame         *teamPtr     = topTeamPtr;
    TeamStatisticsData *statsPtr  = &team1StatsData;
    uint16_t         *tactics     = &pl1Tactics;
    uint16_t          teamNum     = 1;

    if (teamPlayingUp != 1) {
        /* swap a0/a1 */
        TeamGeneralInfo *tmp = a0;
        a0 = a1;
        a1 = tmp;
    }

    int16_t iter;
    for (iter = 1; iter >= 0; iter--) {
        /* Fill struct at a0 */
        a0->opponentsTeam                      = a1;
        a0->inGameTeamPtr                      = teamPtr;
        a0->teamStatsPtr                       = statsPtr;
        a0->playerNumber                       = (int16_t)playerNo;
        a0->playerCoachNumber                   = (int16_t)coachNo;
        a0->isPlCoach                           = (int16_t)isPlCoach;
        a0->spritesTable                       = spritesTab;
        a0->teamNumber                         = (int16_t)teamNum;
        a0->tactics                            = (int16_t)*tactics;
        a0->goalkeeperPlaying                  = 0;
        a0->resetControls                      = 0;
        a0->updatePlayerIndex                  = 10;
        a0->controlledPlayer                   = 0;
        a0->passToPlayerPtr                    = 0;
        a0->passingKickingPlayer               = 0;
        a0->playerHasBall                      = 0;
        a0->goalkeeperDivingRight              = 0;
        a0->goalkeeperDivingLeft               = 0;
        a0->ballOutOfPlayOrKeeper              = 0;
        a0->goaliePlayingOrOut                 = 0;
        a0->passingBall                        = 0;
        a0->passingToPlayer                    = 0;
        a0->playerSwitchTimer                  = 0;
        a0->ballInPlay                         = 0;
        a0->ballOutOfPlay                      = 0;
        a0->passKickTimer                      = 0;
        a0->ballCanBeControlled                = 0;
        a0->field_72                           = 0;
        a0->ballControllingPlayerDirection     = (int16_t)0xFFFF;
        a0->field_74                           = 0;
        a0->wonTheBallTimer                    = 0;
        a0->spinTimer                          = (int16_t)0xFFFF;
        a0->field_3C                           = 0;
        a0->goalkeeperSavedCommentTimer        = 0;
        a0->lastHeadingTacklingPlayer          = 0;
        a0->field_4E                           = 0;
        a0->headerOrTackle                     = 0;
        a0->shooting                           = 0;
        a0->passInProgress                     = 0;

        /* Set up for second iteration (bottom team) */
        playerNo    = bottomTeamPlayerNo;
        coachNo     = bottomTeamCoachNo;
        isPlCoach   = pl2Coach;
        a0          = &bottomTeamData;
        a1          = &topTeamData;
        spritesTab  = team2SpritesTable;
        teamPtr     = bottomTeamPtr;  /* bottom team pointer */
        statsPtr    = &team2StatsData;
        tactics     = &pl2Tactics;
        teamNum     = 2;

        if (teamPlayingUp != 1) {
            /* swap a0/a1 for second iteration too */
            TeamGeneralInfo *tmp = a0;
            a0 = a1;
            a1 = tmp;
        }
    }
}

/* ================================================================
 * InitGameSaveTeams  (SWOS2.s line 11941)
 *
 * Saves both team in-game structs (1018 bytes each) to snapshot
 * buffers, stores team pointers, loads pitch, converts colors
 * and sprites, waits for both fire buttons released, fades out,
 * sets paletteIndex=2.
 *
 * NOTE: In the original asm, this function falls through into
 * InitGameRestoreTeams via "bra.w *+4". The asm stub handles
 * that fall-through; this C function only covers the save portion.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _InitGameSaveTeams(TeamGame *leftTeam, TeamGame *rightTeam)
{
    /* Copy both team structs (1018 bytes each) to snapshot buffers */
    uint8_t *src1 = (uint8_t *)&topTeamInGame;
    uint8_t *dst1 = (uint8_t *)&topTeamSaved;
    uint8_t *src2 = (uint8_t *)&bottomTeamInGame;
    uint8_t *dst2 = (uint8_t *)&bottomTeamSaved;
    int16_t i;
    for (i = 1017; i >= 0; i--) {
        *dst1++ = *src1++;
        *dst2++ = *src2++;
    }

    /* Save team pointers */
    topTeamPtr = leftTeam;
    bottomTeamPtr = rightTeam;

    /* Load pitch map (high byte of pitchNumberAndType is the index) */
    _LoadPitchMap(pitchNumberAndType >> 8);
    _FixupAndColorPitchFile();
    _ConvertColorsAndTextSprites(leftTeam, rightTeam);

    /* Wait for both fire buttons released */
    for (;;) {
        _Player1StatusProc();
        if (pl1Fire)
            continue;
        _Player2StatusProc();
        if (!pl2Fire)
            break;
    }

    _FadeOutToBlack();
    paletteIndex = 2;
}
