/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "match_setup.h"
#include "game_types.h"
#include "deterministic.h"

/* ---- External globals (in SWOS2.s code/BSS sections) ---- */
extern Sprite             ballSprite;
extern TeamGeneralInfo    topTeamData;
extern TeamGeneralInfo    bottomTeamData;
extern Sprite             refereeSprite;
extern Sprite            *team1SpritesTable[11];
extern Sprite            *team2SpritesTable[11];
extern TeamGame           topTeamInGame;
extern TeamGame           bottomTeamInGame;
extern TeamGame           topTeamSaved;
extern TeamGame           bottomTeamSaved;
extern TeamStatisticsData team1StatsData;
extern TeamStatisticsData team2StatsData;

/* Match state */
extern uint16_t gameState;
extern uint16_t gameStatePl;
extern uint16_t breakCameraMode;
extern uint16_t gameNotInProgressCounterWriteOnly;
extern TeamGeneralInfo *lastTeamPlayedBeforeBreak;
extern uint16_t stoppageTimerTotal;
extern uint16_t stoppageTimerActive;
extern uint16_t stoppageEventTimer;
extern uint16_t halfNumber;
extern uint16_t teamStarting;
extern uint16_t teamPlayingUp;
extern uint16_t cameraDirection;
extern uint8_t  playerTurnFlags;
extern uint16_t lastCameraDirection;
extern uint8_t  lastPlayerTurnFlags;
extern uint16_t cameraXVelocity;
extern uint16_t cameraYVelocity;
extern uint16_t cameraMoveToShowerLatch;
extern uint16_t hideBall;
extern volatile uint16_t playGame;
extern uint16_t trainingGame;
extern uint16_t goalCounter;
extern uint16_t endOfHalfWriteOnly;
extern uint16_t sfxQueueWriteIdx;
extern uint16_t showFansCounter;
extern uint16_t resultTimer;
extern uint16_t timeVar;
extern uint16_t refState;
extern uint16_t foulXCoordinate;
extern uint16_t foulYCoordinate;
extern uint16_t playRefereeWhistle;

/* InitGameRestoreTeams globals */
extern uint16_t gameLengthInGame;
extern uint16_t playerCardChance;
extern uint16_t overlayActiveFlag;
extern uint16_t gameRandValue;
extern uint16_t paletteIndex;
extern volatile uint16_t playHighlightFlag;
extern uint8_t  gamePalette[];
extern uint32_t paletteTable[];
extern uint16_t pitchBallSpeedFactor;
extern uint16_t ballSpeedBounceFactor;
extern uint16_t ballBounceFactor;
extern uint16_t pitchNumberAndType;
extern uint16_t playingPenalties;
extern uint16_t dontShowScorers;
extern uint16_t statsTimer;
extern uint16_t showStats;
extern uint16_t showingUserStats;
extern uint16_t subsTeamsSwapped;
extern uint16_t inSubstitutesMenu;
extern uint16_t cameraLeavingSubsTimer;
extern uint16_t benchOff;
extern uint16_t leavingSubsMenu;
extern uint16_t waitForPlayerToGoInTimer;
extern uint16_t substituteInProgress;
extern uint16_t statsTeam1Goals;
extern uint16_t team1GoalsDigit1;
extern uint16_t team1GoalsDigit2;
extern uint16_t statsTeam2Goals;
extern uint16_t team2GoalsDigit1;
extern uint16_t team2GoalsDigit2;
extern uint16_t team1TotalGoals;
extern uint16_t team2TotalGoals;
extern uint16_t secondLeg;
extern uint16_t team1GoalsFirstLeg;
extern uint16_t team2GoalsFirstLeg;
extern uint16_t team1NumSubs;
extern uint16_t team2NumSubs;
extern uint16_t team1GoalkeeperReplaced;
extern uint16_t team2GoalkeeperReplaced;

/* StartPenalties globals */
extern uint16_t penaltiesState;
extern uint16_t savedTeam1Goals;
extern uint16_t savedTeam2Goals;
extern uint16_t team1PenaltyGoals;
extern uint16_t team2PenaltyGoals;
extern uint16_t team1PenaltyShooterIndex;
extern uint16_t team2PenaltyShooterIndex;
extern uint16_t team1PenaltyAttempts;
extern uint16_t team2PenaltyAttempts;

/* InitAnimatedPatterns */
extern int16_t  animatedPatterns[];
extern uint16_t animPatternsIndex;
extern uint16_t patternsGoalCounter;

/* Data tables (in SWOS2.s code section, PC-relative) */
extern uint32_t playerCardChancesPerGameLength[];
extern int16_t  gameLenSecondsTable[];
extern uint32_t pitchPalettePtrs[];
extern int16_t  kPitchBallSpeedInfluence[];
extern int16_t  kBallSpeedBounceFactorTable[];
extern int16_t  kBallBounceFactorTable[];
extern uint16_t timeDelta;
extern TeamGame *topTeamPtr;
extern TeamGame *bottomTeamPtr;
extern uint16_t pl1Tactics;
extern uint16_t pl2Tactics;
extern AnimTable_12 refWaitingAnimTable;
extern AnimTable playerNormalStandingAnimTable;

/* ---- External C functions (already ported) ---- */
extern void _InitTeamsData(void);
extern void _InitDisplaySprites(void);
extern void _InitCurrentGoalHighlight(void);
extern void _ResetTime(void);
extern void _ResetResultSprites(void);
extern void _ClearSaveFlagForGoalSprites(void);
extern void _ResetGoalAttempt(void);
extern void _CrowdResetMatchState(void);
extern void _CopyPlayerNameLengths(void);
extern void _InitializePlayerSpriteFrameIndices(void);
extern void _InitAdvertisements(void);
extern void _PlayPenaltySample(void);
extern void _PlaySample1IfNotTraining(void);
extern void _FadeOutToBlack(void);
extern void _UpdatePlanePtrsAndBlitParams(void);
extern uint16_t _Rand(void);

/* ---- C-to-asm bridges (for unported asm functions) ---- */
extern void _SetPlayerAnimationTable(AnimTable *animTable, Sprite *sprite);
extern void _InitRefereeAnimationTable(uint8_t *animTable);
extern void _ApplyTeamTactics(uint8_t *playerInfos);
extern void _NextPenalty(void);
extern void _ReplayUpdateCameraAndSwapBuffers(int16_t x, int16_t y);

/* ================================================================
 * SetBallPosition  (SWOS2.s line 10505)
 *
 * Stop ball and set position to (ballX, ballY), z=0.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetBallPosition(int16_t ballX, int16_t ballY)
{
    ballSprite.speed      = 0;
    ballSprite.x.s.i      = ballX;
    ballSprite.x.s.frac   = 0;
    ballSprite.y.s.i      = ballY;
    ballSprite.y.s.frac   = 0;
    ballSprite.destX      = ballX;
    ballSprite.destY      = ballY;
    ballSprite.z.raw      = 0;
    ballSprite.deltaZ.raw = 0;
}

/* ================================================================
 * resetPlayerPassingKicking  (SWOS2.s line 22797)
 *
 * Clears team passing/kicking state fields.
 * ================================================================ */
static void resetPlayerPassingKicking(TeamGeneralInfo *team)
{
    team->ballInPlay           = 0;
    team->ballOutOfPlay        = 0;
    team->controlledPlayer     = 0;
    team->passToPlayerPtr      = 0;
    team->passingBall          = 0;
    team->passingToPlayer      = 0;
    team->playerSwitchTimer    = 0;
    team->passingKickingPlayer = 0;
}

/* ================================================================
 * ResetBothTeamsPlayerPassingKicking  (SWOS2.s line 22781)
 *
 * Saves camera direction and turn flags, resets passing/kicking
 * for both teams, clears goalkeeperPlaying on bottom team.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ResetBothTeamsPlayerPassingKicking(void)
{
    lastCameraDirection = cameraDirection;
    lastPlayerTurnFlags = playerTurnFlags;
    resetPlayerPassingKicking(&topTeamData);
    resetPlayerPassingKicking(&bottomTeamData);
    bottomTeamData.goalkeeperPlaying = 0;
}

/* ================================================================
 * stopPlayers  (SWOS2.s line 22842)
 *
 * For each of the 11 players on a team, if the player is in
 * NORMAL/HAPPY/SAD state and not sent away, set dest = current pos.
 * ================================================================ */
static void stopPlayers(TeamGeneralInfo *team)
{
    Sprite * const *spritesTable = team->spritesTable;
    int16_t i;
    for (i = 10; i >= 0; i--) {
        Sprite *sprite = spritesTable[i];
        uint8_t state = sprite->playerState;
        if (state == PL_HAPPY || state == PL_SAD || state == PL_NORMAL) {
            if ((uint16_t)sprite->sentAway == 0) {
                sprite->destX = sprite->x.s.i;
                sprite->destY = sprite->y.s.i;
            }
        }
    }
}

/* ================================================================
 * StopAllPlayers  (SWOS2.s line 22810)
 *
 * Saves camera direction and turn flags, stops both teams' players,
 * resets passing/kicking for both teams, clears goalkeeperPlaying.
 * ================================================================ */
__attribute__((used, externally_visible))
void _StopAllPlayers(void)
{
    lastCameraDirection = cameraDirection;
    lastPlayerTurnFlags = playerTurnFlags;

    stopPlayers(&topTeamData);
    resetPlayerPassingKicking(&topTeamData);

    stopPlayers(&bottomTeamData);
    resetPlayerPassingKicking(&bottomTeamData);
    bottomTeamData.goalkeeperPlaying = 0;
}

/* ================================================================
 * RemoveReferee  (SWOS2.s line 11441)
 *
 * Makes referee invisible, resets position and animation state.
 * ================================================================ */
__attribute__((used, externally_visible))
void _RemoveReferee(void)
{
    refState                    = 0;
    refereeSprite.visible       = 0;
    refereeSprite.x.s.i         = 276;
    refereeSprite.y.s.i         = 439;
    refereeSprite.destX         = refereeSprite.x.s.i;
    refereeSprite.destY         = refereeSprite.y.s.i;
    refereeSprite.z.raw         = 0;
    refereeSprite.speed         = 0;
    refereeSprite.playerDownTimer = 0;
    refereeSprite.frameIndex    = (int16_t)0xFFFF;
    refereeSprite.cycleFramesTimer = 1;
    refereeSprite.imageIndex    = (int16_t)0xFFFF;
    refereeSprite.direction     = 0;
    refereeSprite.onScreen      = 1;
    _InitRefereeAnimationTable((uint8_t *)&refWaitingAnimTable); /* TODO: update when _InitRefereeAnimationTable takes AnimTable* */
}

/* ================================================================
 * InitAnimatedPatterns  (SWOS2.s line 56110)
 *
 * Clears the 8-entry animated patterns array and resets counters.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitAnimatedPatterns(void)
{
    int16_t i;
    for (i = 0; i < 8; i++) {
        animatedPatterns[i] = (int16_t)0xFFFF;
    }
    patternsGoalCounter = 0;
    animPatternsIndex = 0;
}

/* ================================================================
 * InitPlayersBeforeEnteringPitch  (SWOS2.s line 23194)
 *
 * Positions all 22 players at their starting coordinates, resets
 * sprite state, and sets up animation tables.
 * ================================================================ */
static const int16_t kTeamsStartingCoordinates[44] = {
    0x012C, 0x0045,  /* element[0]  */
    0x0118, 0x002E,  /* element[1]  */
    0x0104, 0x0022,  /* element[2]  */
    0x00F0, 0x0018,  /* element[3]  */
    0x00DC, 0x0010,  /* element[4]  */
    0x00C8, 0x0009,  /* element[5]  */
    0x00B4, 0x0003,  /* element[6]  */
    0x00A0, -2,      /* element[7]  */
    0x008C, -8,      /* element[8]  */
    0x0078, -9,      /* element[9]  */
    0x0064, -11,     /* element[10] */
    0x012C, -65,     /* element[11] */
    0x0118, -42,     /* element[12] */
    0x0104, -30,     /* element[13] */
    0x00F0, -20,     /* element[14] */
    0x00DC, -12,     /* element[15] */
    0x00C8, -5,      /* element[16] */
    0x00B4, 1,       /* element[17] */
    0x00A0, 6,       /* element[18] */
    0x008C, 12,      /* element[19] */
    0x0078, 13,      /* element[20] */
    0x0064, 15       /* element[21] */
};

__attribute__((used, externally_visible))
void _InitPlayersBeforeEnteringPitch(void)
{
    _RemoveReferee();
    /* nullsub_21 — no-op */

    Sprite **spritesTableA;
    if (teamPlayingUp == 1) {
        spritesTableA = team1SpritesTable;
    } else {
        spritesTableA = team2SpritesTable;
    }

    const int16_t *coords = kTeamsStartingCoordinates;
    int16_t pass;
    for (pass = 1; pass >= 0; pass--) {
        int16_t idx;
        for (idx = 10; idx >= 0; idx--) {
            Sprite *sprite = *spritesTableA++;
            sprite->x.s.i  = *coords++ + 591;
            sprite->y.s.i  = *coords++ + 449;

            /* Add random jitter 0..7 to X */
            uint8_t r = _Rand();
            sprite->x.s.i += (int16_t)(r & 7);

            sprite->destX            = sprite->x.s.i;
            sprite->destY            = sprite->y.s.i;
            sprite->z.s.i            = 0;  /* ASM: clr.w SPRITE_Z — only clears integer part */
            sprite->speed            = 0;
            sprite->playerState      = 0;
            sprite->playerDownTimer  = 0;
            sprite->frameIndex       = (int16_t)0xFFFF;
            sprite->cycleFramesTimer = 1;
            sprite->imageIndex       = (int16_t)0xFFFF;
            sprite->direction        = 0;
            sprite->onScreen         = 1;

            if (gameState == ST_STARTING_GAME) {
                sprite->sentAway    = 0;
                sprite->cards       = 0;
                sprite->injuryLevel = 0;
            }

            _SetPlayerAnimationTable(
                &playerNormalStandingAnimTable,
                sprite
            );
        }

        /* Second pass: other team (coords continues from where it left off) */
        if (teamPlayingUp == 2) {
            spritesTableA = team1SpritesTable;
        } else {
            spritesTableA = team2SpritesTable;
        }
    }
}

/* ================================================================
 * Common helper: set "break" state with standard field assignments
 * ================================================================ */
static void setBreakState(uint16_t newGameState, uint16_t newGameStatePl,
                          uint16_t stoppageTimer)
{
    hideBall = 0;
    stoppageEventTimer = stoppageTimer;
    gameState = newGameState;
    breakCameraMode = 0xFFFF;
    gameStatePl = newGameStatePl;
    gameNotInProgressCounterWriteOnly = 0;
    cameraDirection = 0xFFFF;
    lastTeamPlayedBeforeBreak = &topTeamData;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
}

static void clearCameraVelocity(void)
{
    cameraXVelocity = 0;
    cameraYVelocity = 0;
}

/* ================================================================
 * StartingMatch  (SWOS2.s line 22465)
 * ================================================================ */
__attribute__((used, externally_visible))
void _StartingMatch(void)
{
    halfNumber = 1;
    _SetBallPosition(1672, 449);
    hideBall = 0;
    _InitTeamsData();
    stoppageEventTimer = 100;
    gameState = ST_STARTING_GAME;
    breakCameraMode = 0xFFFF;
    gameStatePl = 101;
    gameNotInProgressCounterWriteOnly = 0;
    cameraDirection = 0xFFFF;
    lastTeamPlayedBeforeBreak = &topTeamData;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();
    clearCameraVelocity();
    cameraMoveToShowerLatch = 1;
    _InitPlayersBeforeEnteringPitch();
}

/* ================================================================
 * SetCameraMovingToShowerState  (SWOS2.s line 22490)
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetCameraMovingToShowerState(void)
{
    halfNumber = 2;
    /* Invert teamPlayingUp: neg + add 3 (1->2, 2->1) */
    teamPlayingUp = (uint16_t)(3 - teamPlayingUp);
    teamStarting = (uint16_t)(3 - teamStarting);

    _SetBallPosition(1672, 449);
    hideBall = 0;
    _InitTeamsData();
    stoppageEventTimer = 100;
    gameState = ST_CAMERA_GOING_TO_SHOWERS;
    breakCameraMode = 0xFFFF;
    gameStatePl = 101;
    gameNotInProgressCounterWriteOnly = 0;
    cameraDirection = 0xFFFF;
    lastTeamPlayedBeforeBreak = &topTeamData;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();
    clearCameraVelocity();
    cameraMoveToShowerLatch = 1;

    if (playGame != 0) {
        _PlayPenaltySample();
        sfxQueueWriteIdx = 350;
    }

    _InitPlayersBeforeEnteringPitch();
}

/* ================================================================
 * EndFirstHalf  (SWOS2.s line 22523)
 * ================================================================ */
__attribute__((used, externally_visible))
void _EndFirstHalf(void)
{
    setBreakState(ST_FIRST_HALF_ENDED, ST_STOPPED, 100);
    _ResetBothTeamsPlayerPassingKicking();
    clearCameraVelocity();
    if (playGame != 0 && goalCounter == 0) {
        endOfHalfWriteOnly = 25;
    }
}

/* ================================================================
 * FirstHalfJustEnded  (SWOS2.s line 22547)
 * ================================================================ */
__attribute__((used, externally_visible))
void _FirstHalfJustEnded(void)
{
    hideBall = 0;
    stoppageEventTimer = 250;
    gameState = ST_GOING_TO_HALFTIME;
    breakCameraMode = 0xFFFF;
    gameStatePl = ST_STOPPED;
    gameNotInProgressCounterWriteOnly = 0;
    cameraDirection = 0xFFFF;
    lastTeamPlayedBeforeBreak = &topTeamData;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();
    clearCameraVelocity();
}

/* ================================================================
 * EndOfGame  (SWOS2.s line 22565)
 * ================================================================ */
__attribute__((used, externally_visible))
void _EndOfGame(void)
{
    setBreakState(ST_GAME_ENDED, ST_STOPPED, 150);
    _ResetBothTeamsPlayerPassingKicking();
    clearCameraVelocity();
    if (playGame != 0 && goalCounter == 0) {
        endOfHalfWriteOnly = 25;
    }
}

/* ================================================================
 * PlayersLeavingPitch  (SWOS2.s line 22589)
 * ================================================================ */
__attribute__((used, externally_visible))
void _PlayersLeavingPitch(void)
{
    hideBall = 0;
    stoppageEventTimer = 250;
    gameState = ST_PLAYERS_GOING_TO_SHOWER;
    breakCameraMode = 0xFFFF;
    gameStatePl = 0x65;
    gameNotInProgressCounterWriteOnly = 0;
    cameraDirection = 0xFFFF;
    lastTeamPlayedBeforeBreak = &topTeamData;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();
    clearCameraVelocity();
}

/* ================================================================
 * GoToHalftime  (SWOS2.s line 22607)
 * ================================================================ */
__attribute__((used, externally_visible))
void _GoToHalftime(void)
{
    _SetBallPosition(1672, 449);
    resultTimer = 30000;
    timeVar = 32000;
    stoppageEventTimer = 700;
    gameState = ST_RESULT_ON_HALFTIME;
    breakCameraMode = 0xFFFF;
    gameStatePl = ST_STOPPED;
    gameNotInProgressCounterWriteOnly = 0;
    cameraDirection = 0xFFFF;
    lastTeamPlayedBeforeBreak = &topTeamData;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();
    clearCameraVelocity();
}

/* ================================================================
 * GameOver  (SWOS2.s line 22629)
 * ================================================================ */
__attribute__((used, externally_visible))
void _GameOver(void)
{
    _FadeOutToBlack();

    int16_t viewX = 168;
    int16_t viewY = 80;
    if (trainingGame == 0) {
        viewY = 16;
    }
    _ReplayUpdateCameraAndSwapBuffers(viewX, viewY);
    _UpdatePlanePtrsAndBlitParams();

    _SetBallPosition(1672, 449);
    resultTimer = 30000;
    timeVar = 32000;
    stoppageEventTimer = 1500;
    gameState = ST_RESULT_AFTER_THE_GAME;
    breakCameraMode = 0xFFFF;
    gameStatePl = ST_STOPPED;
    gameNotInProgressCounterWriteOnly = 0;
    cameraDirection = 0xFFFF;
    lastTeamPlayedBeforeBreak = &topTeamData;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();
    clearCameraVelocity();
}

/* ================================================================
 * StartFirstExtraTime  (SWOS2.s line 22660)
 * ================================================================ */
__attribute__((used, externally_visible))
void _StartFirstExtraTime(void)
{
    halfNumber = 1;

    /* Random teamPlayingUp and teamStarting */
    teamPlayingUp = (uint16_t)((_Rand() & 1) + 1);
    teamStarting = (uint16_t)((_Rand() & 1) + 1);

    hideBall = 0;
    _InitTeamsData();
    stoppageEventTimer = 100;
    gameState = ST_FIRST_EXTRA_STARTING;
    breakCameraMode = 0xFFFF;
    gameStatePl = ST_STOPPED;
    gameNotInProgressCounterWriteOnly = 0;
    cameraDirection = 0xFFFF;

    /* Select kickoff team: if teamStarting == teamPlayingUp, use top, else bottom */
    if (teamStarting == teamPlayingUp) {
        lastTeamPlayedBeforeBreak = &topTeamData;
    } else {
        lastTeamPlayedBeforeBreak = &bottomTeamData;
    }

    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();
    clearCameraVelocity();
}

/* ================================================================
 * EndFirstExtraTime  (SWOS2.s line 22694)
 * ================================================================ */
__attribute__((used, externally_visible))
void _EndFirstExtraTime(void)
{
    halfNumber = 2;
    teamPlayingUp = (uint16_t)(3 - teamPlayingUp);
    teamStarting = (uint16_t)(3 - teamStarting);

    hideBall = 0;
    _InitTeamsData();
    stoppageEventTimer = 100;
    gameState = ST_FIRST_EXTRA_ENDED;
    breakCameraMode = 0xFFFF;
    gameStatePl = ST_STOPPED;
    gameNotInProgressCounterWriteOnly = 0;
    cameraDirection = 0xFFFF;

    if (teamStarting == teamPlayingUp) {
        lastTeamPlayedBeforeBreak = &topTeamData;
    } else {
        lastTeamPlayedBeforeBreak = &bottomTeamData;
    }

    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();
    clearCameraVelocity();
}

/* ================================================================
 * PrepareForInitialKick  (SWOS2.s line 22724)
 * ================================================================ */
__attribute__((used, externally_visible))
void _PrepareForInitialKick(void)
{
    _SetBallPosition(336, 449);

    TeamGeneralInfo *kickoffTeam = &topTeamData;
    int16_t direction = 4;             /* south */
    uint8_t turnFlags = 0x7C;          /* only allow E, SE, S, SW, W */

    if (teamStarting != teamPlayingUp) {
        kickoffTeam = &bottomTeamData;
        direction = 0;                 /* north */
        turnFlags = 0xC7;              /* only allow E, NE, N, NW, W */
    }

    gameState = ST_PLAYERS_TO_INITIAL_POSITIONS;
    breakCameraMode = 0xFFFF;
    gameStatePl = ST_STOPPED;
    gameNotInProgressCounterWriteOnly = 0;
    foulXCoordinate = 0x150;
    foulYCoordinate = 0x1C1;
    cameraDirection = direction;
    playerTurnFlags = turnFlags;
    lastTeamPlayedBeforeBreak = kickoffTeam;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();
    clearCameraVelocity();
}

/* ================================================================
 * StartPenalties  (SWOS2.s line 19467)
 * ================================================================ */
__attribute__((used, externally_visible))
void _StartPenalties(void)
{
    penaltiesState = 0xFFFF;
    savedTeam1Goals = statsTeam1Goals;
    savedTeam2Goals = statsTeam2Goals;
    statsTeam1Goals = 0;
    team1GoalsDigit1 = 0;
    team1GoalsDigit2 = 0;
    statsTeam2Goals = 0;
    team2GoalsDigit1 = 0;
    team2GoalsDigit2 = 0;
    team1PenaltyGoals = 0;
    team2PenaltyGoals = 0;

    teamPlayingUp = (uint16_t)((_Rand() & 1) + 1);
    teamStarting = (uint16_t)((_Rand() & 1) + 1);

    team1PenaltyShooterIndex = 11;
    team2PenaltyShooterIndex = 11;
    team1PenaltyAttempts = 0;
    team2PenaltyAttempts = 0;
    playingPenalties = 1;
    dontShowScorers = 1;

    /* Clear cards/sentaway for both teams */
    int16_t i;
    for (i = 10; i >= 0; i--) {
        Sprite *sprite = team1SpritesTable[i];
        sprite->cards   = 0;
        sprite->sentAway = 0;
    }
    for (i = 10; i >= 0; i--) {
        Sprite *sprite = team2SpritesTable[i];
        sprite->cards   = 0;
        sprite->sentAway = 0;
    }

    _NextPenalty();
}

/* ================================================================
 * InitGameRestoreTeams  (SWOS2.s line 11915)
 *
 * Master match initialization: restores team data from snapshots,
 * applies tactics, sets up palette/pitch factors, clears all
 * per-match stats/state, then calls StartingMatch.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _InitGameRestoreTeams(void)
{
    _InitAdvertisements();
    _CopyPlayerNameLengths();

    /* Pick player card chance from table based on game length */
    uint16_t lenIdx = gameLengthInGame;
    uint16_t *cardChanceTable = (uint16_t *)playerCardChancesPerGameLength[lenIdx];
    uint8_t r = _Rand();
    playerCardChance = cardChanceTable[(r & 0x1E) >> 1];

    overlayActiveFlag = 0;

    /* Restore team data from snapshots (1018 bytes each) */
    uint8_t *src1 = (uint8_t *)&topTeamSaved;
    uint8_t *dst1 = (uint8_t *)&topTeamInGame;
    uint8_t *src2 = (uint8_t *)&bottomTeamSaved;
    uint8_t *dst2 = (uint8_t *)&bottomTeamInGame;
    int16_t i;
    for (i = 1017; i >= 0; i--) {
        *dst1++ = *src1++;
        *dst2++ = *src2++;
    }

    _InitializePlayerSpriteFrameIndices();

#ifdef DETERMINISTIC_MODE
    {
        extern uint16_t currentGameTick;
        extern uint16_t currentTick;
        currentGameTick = 0;
        currentTick = 0;
        _InitDeterministicMode();
    }
#endif
    gameRandValue = _Rand();

    /* Apply tactics for both teams */
    _ApplyTeamTactics((uint8_t *)&topTeamInGame);
    _ApplyTeamTactics((uint8_t *)&bottomTeamInGame);

    paletteIndex = 2;
    /* nullsub_5 — no-op */

    _CrowdResetMatchState();

    /* Random teamPlayingUp and teamStarting */
    teamPlayingUp = (uint16_t)((_Rand() & 1) + 1);
    teamStarting = (uint16_t)((_Rand() & 1) + 1);

    /* Set time delta from game length table */
    timeDelta = *(uint16_t *)((uint8_t *)gameLenSecondsTable + (gameLengthInGame << 1));

    if (playHighlightFlag != 0) {
        goto apply_palette_and_pitch;
    }
    /* bne.w *+4 — when playHighlightFlag != 0, skip to palette setup */

apply_palette_and_pitch:
    {
        /* Apply pitch palette */
        uint16_t pitchIdx = pitchNumberAndType & 0xFF;

        /* paletteTable[2] = &gamePalette (offset 8 in paletteTable is index 2) */
        paletteTable[2] = (uint32_t)gamePalette;

#ifdef AGA
        {
            /* AGA: apply 9 pitch entries via paletteIndices, write full 256-color palette to copper */
            extern uint8_t paletteIndices[];
            const uint16_t *palPtr = (const uint16_t *)pitchPalettePtrs[pitchIdx];
            int16_t pi;
            for (pi = 0; pi < 9; pi++)
            {
                uint16_t idx = paletteIndices[pi];
                ((uint16_t *)gamePalette)[idx] = palPtr[pi];
            }
        }
#else
        {
            /* OCS: copy 3 palette entries to fixed positions */
            uint16_t *palPtr = (uint16_t *)pitchPalettePtrs[pitchIdx];
            *(uint16_t *)(gamePalette) = palPtr[0];
            *(uint16_t *)(gamePalette + 0xE) = palPtr[1];
            *(uint16_t *)(gamePalette + 0x12) = palPtr[2];
        }
#endif

        /* Pitch physics factors (pitchIdx >> 1 for word-indexed tables) */
        uint16_t physIdx = pitchIdx;
        pitchBallSpeedFactor = kPitchBallSpeedInfluence[physIdx];
        ballSpeedBounceFactor = kBallSpeedBounceFactorTable[physIdx];
        ballBounceFactor = kBallBounceFactorTable[physIdx];
    }

    /* Clear match state */
    playingPenalties = 0;
    dontShowScorers = 0;
    statsTimer = 0;
    showStats = 0;
    showingUserStats = 0;
    subsTeamsSwapped = 0;
    inSubstitutesMenu = 0;
    cameraLeavingSubsTimer = 0;
    benchOff = 0;
    leavingSubsMenu = 0;
    waitForPlayerToGoInTimer = 0;
    substituteInProgress = 0;

    /* Clear goal scores */
    statsTeam1Goals = 0;
    team1GoalsDigit1 = 0;
    team1GoalsDigit2 = 0;
    statsTeam2Goals = 0;
    team2GoalsDigit1 = 0;
    team2GoalsDigit2 = 0;
    team1TotalGoals = 0;
    team2TotalGoals = 0;

    if (secondLeg != 0) {
        team1TotalGoals = team1GoalsFirstLeg;
        team2TotalGoals = team2GoalsFirstLeg;
    }

    /* Clear substitution and goalkeeper replacement counts */
    team1NumSubs = 0;
    team2NumSubs = 0;
    team1GoalkeeperReplaced = 0;
    team2GoalkeeperReplaced = 0;

    /* Clear team stats */
    team1StatsData.ballPossession = 0;
    team1StatsData.cornersWon     = 0;
    team1StatsData.foulsConceded  = 0;
    team1StatsData.bookings       = 0;
    team1StatsData.sendingsOff    = 0;
    team1StatsData.goalAttempts   = 0;
    team1StatsData.onTarget       = 0;

    team2StatsData.ballPossession = 0;
    team2StatsData.cornersWon     = 0;
    team2StatsData.foulsConceded  = 0;
    team2StatsData.bookings       = 0;
    team2StatsData.sendingsOff    = 0;
    team2StatsData.goalAttempts   = 0;
    team2StatsData.onTarget       = 0;

    _InitDisplaySprites();
    _InitCurrentGoalHighlight();
    _ResetTime();
    _ResetResultSprites();
    _ClearSaveFlagForGoalSprites();
    _ResetGoalAttempt();
    _InitAnimatedPatterns();
    _StartingMatch();
    _PlaySample1IfNotTraining();

    if (playGame != 0) {
        _PlayPenaltySample();
        sfxQueueWriteIdx = 500;
        if (trainingGame == 0) {
            showFansCounter = 100;
        }
    }
}
