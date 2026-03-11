/* game_timers.c — UpdateGameTimersAndCameraBreakMode + CheckIfBallOutOfPlay
 *
 * Chunk 49: 2 exported functions + internal helpers
 */

#include "game_timers.h"
#include "game_types.h"

/* ---- Extern globals ---- */
extern uint16_t playingPenalties;
extern uint16_t gameState;
extern uint16_t gameStatePl;
extern uint16_t penaltiesTimer;
extern uint16_t lastFrameTicks;
extern uint16_t inGameCounter;
extern uint16_t gameNotInProgressCounterWriteOnly;
extern uint16_t stoppageTimerTotal;
extern uint16_t stoppageTimerActive;
extern TeamGeneralInfo *lastTeamPlayedBeforeBreak;
extern uint16_t initialKickWriteOnlyTicks;
extern int16_t  resultTimer;
extern uint16_t timeVar;
extern int16_t  statsTimer;
extern uint16_t playGame;
extern uint16_t stoppageEventTimer;
extern uint16_t showFansCounter;
extern uint16_t breakCameraMode;
extern uint16_t breakState;
extern uint16_t goalScored;
extern uint16_t goalCameraMode;
extern uint16_t autoSaveHighlights;
extern uint16_t autoReplays;
extern uint16_t saveHighlightScene;
extern uint16_t userRequestedReplay;
extern uint16_t instantReplayFlag;
extern uint32_t currentScorer;
extern uint16_t foulXCoordinate;
extern uint16_t foulYCoordinate;
extern uint16_t cameraDirection;
extern uint8_t  playerTurnFlags;
extern uint16_t whichCard;
extern Sprite  *bookedPlayer;
extern uint16_t cameraCoordinatesValid;
extern uint16_t runSlower;
extern uint16_t refState;
extern uint16_t injuriesForever;
extern uint16_t inSubstitutesMenu;
extern uint16_t waitForPlayerToGoInTimer;
extern uint16_t cameraLeavingSubsTimer;
extern uint16_t ballOutOfGameTimer;
extern uint16_t stoppageAuxFlag;
extern uint16_t deadCameraBreakVar01;
extern uint16_t deadCameraBreakVar02;
extern uint16_t deadCameraBreakVar03;
extern uint16_t deadCameraBreakVar04;
extern uint16_t panicReasonCode;

extern TeamGeneralInfo topTeamData;
extern TeamGeneralInfo bottomTeamData;
extern Sprite          ballSprite;
extern Sprite          goalie1Sprite;
extern Sprite          goalie2Sprite;
extern Sprite         *team1SpritesTable[];
extern Sprite         *team2SpritesTable[];

extern uint8_t  pl1Fire;
extern uint8_t  pl2Fire;

/* CheckIfBallOutOfPlay globals */
extern uint16_t playRefereeWhistle;
extern Sprite  *lastPlayerPlayed;
extern uint32_t lastKeeperPlayed;
extern uint16_t statsTeam1Goals;
extern uint16_t statsTeam2Goals;
extern uint16_t statsTeam1GoalsCopy2;
extern uint16_t statsTeam2GoalsCopy2;
extern uint16_t teamNumThatScored;
extern uint32_t teamScoredDataPtr;
extern uint32_t teamScoredGamePtr;
extern uint16_t goalCounter;
extern uint16_t patternsGoalCounter;
extern uint16_t goalOut;
extern uint16_t forceLeftTeam;
extern uint16_t cameraXVelocity;
extern uint16_t cameraYVelocity;
extern TeamGeneralInfo *lastTeamPlayed;

extern uint8_t  playerNormalStandingAnimTable[];
extern uint8_t  aboutToThrowInAnimTable[];

/* ---- Extern function declarations ---- */
extern void _NextPenalty(void);
extern void _PrepareForInitialKick(void);
extern void _Player1StatusProc(void);
extern void _Player2StatusProc(void);
extern void _SetCameraMovingToShowerState(void);
extern void _FirstHalfJustEnded(void);
extern void _GoToHalftime(void);
extern void _PlayersLeavingPitch(void);
extern void _GameOver(void);
extern void _SetBallPosition(int16_t ballX, int16_t ballY);
extern void _ResetAnimatedPatternsForBothTeams(void);
extern void _CheckIfGoalkeeperClaimedTheBall(void);
extern void _SetPlayerAnimationTable(uint8_t *animTable, uint8_t *spritePtr);
extern void _PlayRefereeWhistleSample(void);
extern void _PanicHaltLoop(void);
extern void _GoalScored(int16_t teamGoalType, Sprite *lastPlayer);
extern void _PlayGoalReplayBottomSample(void);
extern void _PlayGoalKickOffSample(void);
extern uint16_t _Rand(void);
extern void _StopAllPlayers(void);


/* ---- Helper: negate result/time/stats timers ---- */
static void __attribute__((noinline)) negateTimers(void)
{
    resultTimer = -resultTimer;
    timeVar = -(int16_t)timeVar;
    statsTimer = -statsTimer;
}

/* ---- Helper: handle end_game_transmission ---- */
static void __attribute__((noinline)) handleEndGameTransmission(void)
{
    if (gameState == ST_RESULT_ON_HALFTIME) {
        negateTimers();
        _SetCameraMovingToShowerState();
        _PrepareForInitialKick();
        stoppageEventTimer = 0;
        return;
    }
    if (gameState == ST_RESULT_AFTER_THE_GAME) {
        negateTimers();
        playGame = 0;
        stoppageEventTimer = 0;
        return;
    }
    if (gameState == ST_STARTING_GAME) {
        showFansCounter = 0;
        _PrepareForInitialKick();
        stoppageEventTimer = 0;
        return;
    }
    if (gameState == ST_CAMERA_GOING_TO_SHOWERS) {
        _PrepareForInitialKick();
        stoppageEventTimer = 0;
        return;
    }
}

/* ---- Helper: check if fire was pressed to end game state ----
 * Returns 1 if end_game_transmission should be invoked, 0 if not. */
static int __attribute__((noinline)) checkFirePressedForEndGame(void)
{
    if (gameState < ST_STARTING_GAME || gameState > ST_GAME_ENDED)
        return 0;

    if (topTeamData.firePressed != 0)
        return 1;
    if (bottomTeamData.firePressed != 0)
        return 1;

    if (topTeamData.playerCoachNumber == 1 ||
        bottomTeamData.playerCoachNumber == 1) {
        _Player1StatusProc();
        if (pl1Fire != 0)
            return 1;
    }

    if (topTeamData.playerCoachNumber == 2 ||
        bottomTeamData.playerCoachNumber == 2) {
        _Player2StatusProc();
        if (pl2Fire != 0)
            return 1;
    }

    if (gameState == ST_RESULT_AFTER_THE_GAME) {
        _Player1StatusProc();
        if (pl1Fire != 0)
            return 1;
        _Player2StatusProc();
        if (pl2Fire != 0)
            return 1;
    }

    return 0;
}

/* ---- Helper: handle stopped state dispatch ---- */
static void __attribute__((noinline)) handleStoppedState(void)
{
    if (gameState == ST_RESULT_ON_HALFTIME) {
        negateTimers();
        _SetCameraMovingToShowerState();
        return;
    }
    if (gameState == ST_RESULT_AFTER_THE_GAME) {
        negateTimers();
        playGame = 0;
        return;
    }
    if (gameState == ST_STARTING_GAME) {
        _PrepareForInitialKick();
        return;
    }
    if (gameState == ST_CAMERA_GOING_TO_SHOWERS) {
        _PrepareForInitialKick();
        return;
    }
    if (gameState == ST_FIRST_HALF_ENDED) {
        _FirstHalfJustEnded();
        return;
    }
    if (gameState == ST_GOING_TO_HALFTIME) {
        negateTimers();
        _GoToHalftime();
        return;
    }
    if (gameState == ST_GAME_ENDED) {
        _PlayersLeavingPitch();
        return;
    }
    if (gameState == ST_PLAYERS_GOING_TO_SHOWER) {
        _GameOver();
        return;
    }
    if (gameState == ST_FIRST_EXTRA_STARTING) {
        _PrepareForInitialKick();
        return;
    }
    if (gameState == ST_FIRST_EXTRA_ENDED) {
        _PrepareForInitialKick();
        return;
    }

    /* Default: copy gameState to gameStatePl, set breakCameraMode */
    gameStatePl = gameState;

    if (gameState == ST_KEEPER_HOLDS_BALL) {
        if (inSubstitutesMenu == 0 && cameraLeavingSubsTimer == 0) {
            timeVar = 31000;
        }
    }

    breakCameraMode = 0;
}

/* ---- Helper: break camera mode state machine ---- */
static void __attribute__((noinline)) handleBreakCameraMode(TeamGeneralInfo *a6)
{
    Sprite *a5 = &ballSprite;
    int16_t i;
    Sprite **sprTable;
    Sprite *a1;

    if (inSubstitutesMenu != 0)
        return;
    if (waitForPlayerToGoInTimer != 0)
        return;

    switch (breakCameraMode) {

    case 0: /* wait for ball to stop */
        if (a5->deltaX.raw != 0)
            break;
        if (a5->deltaY.raw != 0)
            break;
        breakCameraMode = 1;
        if (gameState == ST_KEEPER_HOLDS_BALL)
            break;
        stoppageEventTimer = 50;
        if (goalCameraMode != 0)
            stoppageEventTimer = 75;
        break;

    case 1: /* handle replay/highlight, set ball position */
        if (playingPenalties == 0 && inSubstitutesMenu == 0) {
            if (goalCameraMode != 0) {
                if (autoSaveHighlights != 0)
                    saveHighlightScene = 1;
                if (autoReplays != 0) {
                    userRequestedReplay = 0;
                    instantReplayFlag = 1;
                }
            }
        }
        currentScorer = 0;
        if (playingPenalties != 0 && gameState != ST_PENALTIES)
            break;
        if (gameState != ST_KEEPER_HOLDS_BALL)
            _SetBallPosition((int16_t)foulXCoordinate, (int16_t)foulYCoordinate);
        breakCameraMode = 2;
        break;

    case 2: /* wait for card animation, reset players */
        if (whichCard != 0)
            break;
        if (cameraCoordinatesValid == 0)
            break;
        goalScored = 0;
        if (gameState == ST_KEEPER_HOLDS_BALL) {
            gameStatePl = 0x66;
            inGameCounter = 0;
            breakState = gameState;
        }
        if (topTeamData.resetControls == 0) {
            sprTable = topTeamData.spritesTable;
            for (i = 10; i >= 0; i--) {
                a1 = *sprTable++;
                a1->destReachedState = 1;
            }
        }
        if (bottomTeamData.resetControls == 0) {
            sprTable = bottomTeamData.spritesTable;
            for (i = 10; i >= 0; i--) {
                a1 = *sprTable++;
                a1->destReachedState = 1;
            }
        }
        if (gameState == ST_KEEPER_HOLDS_BALL) {
            goalie1Sprite.destReachedState = 3;
            goalie2Sprite.destReachedState = 3;
        }
        breakCameraMode = 4;
        break;

    case 4: /* wait for subs menu to close */
        if (inSubstitutesMenu != 0)
            break;
        breakCameraMode = 3;
        break;

    case 3: /* wait for all players to reach destination */
        if (refState != 0)
            break;
        if (injuriesForever != 0)
            break;
        sprTable = &team1SpritesTable[0];
        for (i = 21; i >= 0; i--) {
            a1 = *sprTable++;
            if (gameState != ST_PLAYERS_TO_INITIAL_POSITIONS &&
                gameState != ST_PENALTY &&
                gameState != ST_PENALTIES) {
                if (a1->onScreen == 0)
                    continue;
            }
            if (a1->destReachedState != 3)
                return;  /* early exit: not all players at dest */
        }
        runSlower = 0;
        if (goalCameraMode != 0)
            _ResetAnimatedPatternsForBothTeams();
        breakCameraMode = 5;
        break;

    case 5: /* clear card/booking state */
        whichCard = 0;
        bookedPlayer = (Sprite *)0;
        breakCameraMode = 6;
        break;

    case 6: /* clear ball-out-of-game timer, set timeVar */
        ballOutOfGameTimer = 0;
        if (gameState != ST_KEEPER_HOLDS_BALL)
            timeVar = 31000;
        breakCameraMode = 7;
        break;

    case 7: /* ball in play / throw-in setup */
        a6->ballInPlay = 1;
        ballOutOfGameTimer += 1;
        if (ballOutOfGameTimer >= 500) {
            deadCameraBreakVar01 += 1;
            deadCameraBreakVar02 = gameStatePl;
            deadCameraBreakVar03 = gameState;
            _CheckIfGoalkeeperClaimedTheBall();
            deadCameraBreakVar04 += 1;
            break;
        }
        if (a6->controlledPlayer == 0)
            break;
        a1 = a6->controlledPlayer;
        if (a1->playerState == PL_THROW_IN)
            _SetPlayerAnimationTable(aboutToThrowInAnimTable, (uint8_t *)a1);
        else
            _SetPlayerAnimationTable(playerNormalStandingAnimTable, (uint8_t *)a1);
        a6->ballOutOfPlay = 1;
        breakCameraMode = 8;
        if (gameState == ST_KEEPER_HOLDS_BALL)
            break;
        if (inSubstitutesMenu != 0)
            break;
        resultTimer = 31000;
        break;

    case 8: /* whistle and set waiting on player */
        stoppageAuxFlag = 0;
        if (gameState != ST_KEEPER_HOLDS_BALL)
            _PlayRefereeWhistleSample();
        goalCameraMode = 0;
        gameStatePl = ST_WAITING_ON_PLAYER;
        inGameCounter = 0;
        breakState = gameState;
        break;

    default:
        /* assert_failed: unknown breakCameraMode */
        panicReasonCode = 0x0B;
        _PanicHaltLoop();
        break;
    }
}


/* ===========================================================================
 * _UpdateGameTimersAndCameraBreakMode
 *
 * Main game timer dispatcher and break camera mode state machine.
 * Called once per frame from the game loop.
 * =========================================================================== */
__attribute__((used, externally_visible))
void _UpdateGameTimersAndCameraBreakMode(void)
{
    TeamGeneralInfo *a6;
    uint16_t d0;

    /* --- Penalty timer --- */
    if (playingPenalties != 0) {
        if (gameState != ST_PENALTIES) {
            penaltiesTimer += 1;
            if (penaltiesTimer == 100)
                _NextPenalty();
        }
    }

    /* --- Game in progress: update in-game counter --- */
    if (gameStatePl == ST_GAME_IN_PROGRESS) {
        inGameCounter += lastFrameTicks;
        return;
    }

    /* --- Game not in progress: update stoppage timers --- */
    gameNotInProgressCounterWriteOnly += 1;
    d0 = lastFrameTicks;
    stoppageTimerTotal += d0;

    if (gameStatePl == ST_WAITING_ON_PLAYER) {
        stoppageTimerActive += d0;
        a6 = lastTeamPlayedBeforeBreak;
        if (a6->playerNumber == 0) {
            if (stoppageTimerActive >= 750) {
                _PrepareForInitialKick();
                initialKickWriteOnlyTicks += 1;
            }
        }
        return;
    }

    /* --- Check fire for end-game transmission --- */
    if (checkFirePressedForEndGame()) {
        handleEndGameTransmission();
        return;
    }

    /* --- game_interrupted: stoppage event timer countdown --- */
    if (stoppageEventTimer != 0) {
        stoppageEventTimer -= lastFrameTicks;
        if ((int16_t)stoppageEventTimer > 0)
            return;
        stoppageEventTimer = 0;
    }

    /* event_triggered */
    a6 = lastTeamPlayedBeforeBreak;

    if (gameStatePl == ST_STOPPED) {
        handleStoppedState();
        return;
    }

    /* Game running: break camera mode state machine */
    handleBreakCameraMode(a6);
}


/* ---- Helper: write restart globals and stop players ---- */
static void __attribute__((noinline)) writeRestartGlobalsAndStopPlayers(
    TeamGeneralInfo *team, int16_t d1, int16_t d2, int16_t d3, uint8_t d4)
{
    if (forceLeftTeam == 1)
        team = &topTeamData;

    gameStatePl = ST_STOPPED;
    gameNotInProgressCounterWriteOnly = 0;
    foulXCoordinate = (uint16_t)d1;
    foulYCoordinate = (uint16_t)d2;
    cameraDirection = (uint16_t)d3;
    playerTurnFlags = d4;
    lastTeamPlayedBeforeBreak = team;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();
    cameraXVelocity = 0;
    cameraYVelocity = 0;
}

/* ---- Helper: handle goal scoring ---- */
static void __attribute__((noinline)) handleGoalScored(TeamGeneralInfo *teamData, uint16_t goalType, uint16_t teamNum)
{
    Sprite *a0;
    int16_t d0, d1;

    a0 = lastPlayerPlayed;
    if (lastKeeperPlayed != 0) {
        if (a0 == &goalie1Sprite || a0 == &goalie2Sprite)
            a0 = (Sprite *)(uint32_t)lastKeeperPlayed;
    }
    statsTeam1GoalsCopy2 = statsTeam1Goals;
    statsTeam2GoalsCopy2 = statsTeam2Goals;
    _GoalScored((int16_t)goalType, a0);
    goalCameraMode = 1;
    teamNumThatScored = teamNum;

    /* finalize_goal_replay_side */
    teamScoredDataPtr = (uint32_t)teamData;
    teamScoredGamePtr = (uint32_t)teamData->inGameTeamPtr;

    {
        TeamGeneralInfo *opTeam = teamData->opponentsTeam;
        int16_t camDir;
        uint8_t turnFlags;

        if (opTeam == &topTeamData) {
            camDir = 4;
            turnFlags = 124;
        } else {
            camDir = 0;
            turnFlags = 199;
        }

        _PlayGoalReplayBottomSample();
        playRefereeWhistle = 0;

        /* Goal counter bias system */
        d0 = (int16_t)_Rand();
        d0 = (uint16_t)d0 >> 1;
        d0 += 100;
        d1 = d0;

        if (playingPenalties == 0) {
            int16_t scoreDiff = (int16_t)statsTeam1Goals - (int16_t)statsTeam2Goals;

            if (scoreDiff == 0) {
                d1 = 200;
            } else {
                int16_t preDiff = (int16_t)statsTeam1GoalsCopy2 - (int16_t)statsTeam2GoalsCopy2;
                if (preDiff == 0) {
                    d1 = 300;
                } else if (preDiff == 1 || preDiff == -1) {
                    int16_t newDiff = (int16_t)statsTeam1Goals - (int16_t)statsTeam2Goals;
                    if (newDiff == 2 || newDiff == -2)
                        d1 = 200;
                    else
                        d1 = 100;
                } else {
                    d1 = 100;
                }
            }
            d1 += (int16_t)_Rand();
        }

        goalCounter = (uint16_t)d1;
        patternsGoalCounter = 1;

        gameState = ST_PLAYERS_TO_INITIAL_POSITIONS;
        breakCameraMode = 0xFFFF;
        writeRestartGlobalsAndStopPlayers(opTeam, 336, 449, camDir, turnFlags);
    }
}


/* ===========================================================================
 * _CheckIfBallOutOfPlay
 *
 * Detects when the ball has left the pitch boundaries and determines the
 * restart type: goal, corner kick, goal kick, or throw-in.
 * =========================================================================== */
__attribute__((used, externally_visible))
void _CheckIfBallOutOfPlay(void)
{
    int16_t d1, d2, d3;
    uint8_t d4;
    TeamGeneralInfo *a6;
    TeamStatisticsData *a0;

    playRefereeWhistle = 0;

    if (gameStatePl == ST_STOPPED) {
        /* exit_check_whistle */
        if (playRefereeWhistle != 0)
            _PlayRefereeWhistleSample();
        return;
    }

    playRefereeWhistle = 1;

    d1 = ballSprite.x.s.i;
    d2 = ballSprite.y.s.i;
    d3 = ballSprite.z.s.i;
    d1 -= 1;

    /* --- Check if ball is in goal window: Z <= 15, X in [302..366] --- */
    if (d3 <= 15 && d1 >= 302 && d1 <= 366) {
        if (d2 <= 449) {
            /* Ball above centerline — goal at top */
            a6 = &bottomTeamData;
            if (bottomTeamData.teamNumber == 1) {
                handleGoalScored(a6, 1, 1);
            } else {
                handleGoalScored(a6, 2, 2);
            }
        } else {
            /* Ball below centerline — goal at bottom */
            a6 = &topTeamData;
            if (topTeamData.teamNumber == 2) {
                handleGoalScored(a6, 2, 2);
            } else {
                handleGoalScored(a6, 1, 1);
            }
        }
        /* writeRestartGlobalsAndStopPlayers was called inside handleGoalScored */
        if (playRefereeWhistle != 0)
            _PlayRefereeWhistleSample();
        return;
    }

    /* not_in_goal_window: re-read ball position */
    d1 = ballSprite.x.s.i;
    d2 = ballSprite.y.s.i;
    d3 = ballSprite.z.s.i;
    d3 += 2;

    /* Check if ball is near goal area for goal-kick sample */
    if (ballSprite.speed >= 768 &&
        d1 >= 290 && d1 <= 381 && d3 <= 25) {
        _PlayGoalKickOffSample();
        playRefereeWhistle = 0;
    }

    /* --- Determine restart type based on ball position --- */
    a6 = &topTeamData;

    if (d2 < 129) {
        /* Ball past top end line */
        if (lastTeamPlayed == a6) {
            /* Same team kicked it out — corner for opponent */
            a6 = &bottomTeamData;
            if (d1 < 336) {
                /* set_corner_left_upper */
                d1 = 86; d2 = 134; d3 = 2; d4 = 0x1C;
                gameState = ST_CORNER_LEFT;
            } else {
                /* set_corner_right_upper */
                d1 = 585; d2 = 134; d3 = 6; d4 = 0x70;
                gameState = ST_CORNER_RIGHT;
            }
            breakCameraMode = 0xFFFF;
            a0 = a6->teamStatsPtr;
            a0->cornersWon += 1;
        } else {
            /* Opposite team — goal kick */
            if (d1 < 336) {
                /* setup_goal_out_right_top_half */
                d1 = 276; d2 = 154; d3 = 4; d4 = 0x7C;
                gameState = ST_GOAL_OUT_RIGHT;
            } else {
                /* setup_goal_out_left_top_half */
                d1 = 396; d2 = 154; d3 = 4; d4 = 0x7C;
                gameState = ST_GOAL_OUT_LEFT;
            }
            breakCameraMode = 0xFFFF;
            if (a6->playerNumber == 0)
                d4 &= 0xBB;
            goalOut = 1;
        }
        writeRestartGlobalsAndStopPlayers(a6, d1, d2, d3, d4);
        if (playRefereeWhistle != 0)
            _PlayRefereeWhistleSample();
        return;
    }

    a6 = &bottomTeamData;

    if (d2 > 769) {
        /* Ball past bottom end line */
        if (lastTeamPlayed == a6) {
            /* Same team kicked it out — corner for opponent */
            a6 = &topTeamData;
            if (d1 < 336) {
                /* set_corner_right_lower */
                if (forceLeftTeam == 1) {
                    /* set_corner_left_upper */
                    d1 = 86; d2 = 134; d3 = 2; d4 = 0x1C;
                    gameState = ST_CORNER_LEFT;
                } else {
                    d1 = 86; d2 = 764; d3 = 2; d4 = 7;
                    gameState = ST_CORNER_RIGHT;
                }
            } else {
                /* d1 >= 336 */
                if (forceLeftTeam == 1) {
                    /* set_corner_right_upper */
                    d1 = 585; d2 = 134; d3 = 6; d4 = 0x70;
                    gameState = ST_CORNER_RIGHT;
                } else {
                    d1 = 585; d2 = 764; d3 = 6; d4 = 0xC1;
                    gameState = ST_CORNER_LEFT;
                }
            }
            breakCameraMode = 0xFFFF;
            a0 = a6->teamStatsPtr;
            a0->cornersWon += 1;
        } else {
            /* Opposite team — goal kick */
            if (d1 < 336) {
                /* setup_goal_out_left_bottom_half */
                if (forceLeftTeam == 1) {
                    /* setup_goal_out_right_top_half */
                    d1 = 276; d2 = 154; d3 = 4; d4 = 0x7C;
                    gameState = ST_GOAL_OUT_RIGHT;
                } else {
                    d1 = 276; d2 = 744; d3 = 0; d4 = 0xC7;
                    gameState = ST_GOAL_OUT_LEFT;
                }
            } else {
                /* d1 >= 336 */
                if (forceLeftTeam == 1) {
                    /* setup_goal_out_left_top_half */
                    d1 = 396; d2 = 154; d3 = 4; d4 = 0x7C;
                    gameState = ST_GOAL_OUT_LEFT;
                } else {
                    d1 = 396; d2 = 744; d3 = 0; d4 = 0xC7;
                    gameState = ST_GOAL_OUT_RIGHT;
                }
            }
            breakCameraMode = 0xFFFF;
            if (a6->playerNumber == 0)
                d4 &= 0xBB;
            goalOut = 1;
        }
        writeRestartGlobalsAndStopPlayers(a6, d1, d2, d3, d4);
        if (playRefereeWhistle != 0)
            _PlayRefereeWhistleSample();
        return;
    }

    /* --- Ball is in throw-in band (Y: 129..769) --- */
    {
        TeamGeneralInfo *throwTeam = lastTeamPlayed;
        TeamGeneralInfo *opTeam = throwTeam->opponentsTeam;

        if (d1 >= 336) {
            /* Right side */
            d1 = 590;
            d3 = 6;
            d4 = 0xF1;

            if (opTeam != &topTeamData) {
                /* opponent is bottom team: zone based on Y */
                if (d2 < 342)       gameState = ST_THROW_IN_BACK_LEFT;
                else if (d2 < 556)  gameState = ST_THROW_IN_CENTER_LEFT;
                else                gameState = ST_THROW_IN_FORWARD_LEFT;
            } else {
                /* opponent is top team */
                if (d2 < 342)       gameState = ST_THROW_IN_FORWARD_RIGHT;
                else if (d2 < 556)  gameState = ST_THROW_IN_CENTER_RIGHT;
                else                gameState = ST_THROW_IN_BACK_RIGHT;
            }
        } else {
            /* Left side */
            d1 = 81;
            d3 = 2;
            d4 = 0x1F;

            if (opTeam != &topTeamData) {
                /* opponent is bottom team */
                if (d2 < 342)       gameState = ST_THROW_IN_BACK_RIGHT;
                else if (d2 < 556)  gameState = ST_THROW_IN_CENTER_RIGHT;
                else                gameState = ST_THROW_IN_FORWARD_RIGHT;
            } else {
                /* opponent is top team */
                if (d2 < 342)       gameState = ST_THROW_IN_FORWARD_LEFT;
                else if (d2 < 556)  gameState = ST_THROW_IN_CENTER_LEFT;
                else                gameState = ST_THROW_IN_BACK_LEFT;
            }
        }

        breakCameraMode = 0xFFFF;
        a6 = opTeam;  /* ASM: movea.l (a6),a6 — use the team GETTING the throw-in */
    }

    writeRestartGlobalsAndStopPlayers(a6, d1, d2, d3, d4);

    if (playRefereeWhistle != 0)
        _PlayRefereeWhistleSample();
}
