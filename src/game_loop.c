/* game_loop.c — Game Loop, MainKeysCheck, Highlights Playback, AssignFakeGoalsToScorers
 *
 * Chunk 41: 10 functions (7 exported + 2 static + 1 helper)
 */

#include "game_loop.h"
#include "game_types.h"
#include "display.h"

/* ---- Extern globals (data section, exported from asm/Data.c) ---- */
extern uint16_t gameCanceled;
extern uint16_t instantReplayFlag;
extern uint16_t saveHighlightScene;
extern uint16_t fadeAndSaveHighlightScene;
extern uint16_t spaceReplayTimer;
extern uint16_t subsWriteOnly;
extern uint16_t playGame;
extern uint16_t playHighlightFlag;
extern uint16_t normalReplayRunning;
extern uint16_t userRequestedReplay;
extern uint16_t overlayFlagBackup;
extern uint16_t paused;
extern uint16_t overlayActiveFlag;
extern volatile uint16_t currentTick;
extern uint16_t currentKey;
extern uint16_t frameCount;
extern uint16_t fadeState;
extern uint16_t fadeInitFlag;
extern uint16_t goIntoHighlightsLoop;
extern uint16_t replayState;
extern uint16_t showingUserStats;
extern uint16_t trainingGame;
extern uint16_t inSubstitutesMenu;
extern uint16_t gameState;
extern uint16_t gameStatePl;
extern uint16_t crowdChantsOn;
extern uint16_t spinBigS;
extern uint16_t currentPlayerIndicatorPosition;
extern uint16_t bench1Called;
extern uint16_t bench2Called;
extern uint16_t fireBlocked;
extern uint16_t goalCameraMode;
extern uint16_t isGameFriendly;
extern uint16_t replaySelected;
extern uint16_t team1NumAllowedInjuries;
extern uint16_t team2NumAllowedInjuries;
extern uint16_t extraTimeState;
extern uint16_t penaltiesState;
extern uint16_t writeOnly01;
extern uint32_t gameTime;
extern uint16_t cameraX;
extern uint16_t cameraY;
extern volatile int16_t waitingInterrupt;
extern uint32_t irqCookieObfuscated;
extern uint16_t g_quitToDos;

extern uint8_t pl1Fire;
extern uint8_t pl2Fire;
extern TeamGeneralInfo *teamScoredDataPtr;

extern TeamGame topTeamInGame;
extern TeamGame bottomTeamInGame;
extern TeamGeneralInfo topTeamData;
extern TeamGeneralInfo bottomTeamData;

extern uint16_t statsTeam1Goals;
extern uint16_t statsTeam2Goals;
extern uint16_t statsTeam1GoalsCopy;
extern uint16_t statsTeam2GoalsCopy;
extern uint16_t team1TotalGoals;
extern uint16_t team2TotalGoals;
extern uint16_t pl1Tactics;
extern uint16_t pl2Tactics;
extern uint32_t playersPositionsByTactics[18];

extern uint16_t hilSceneNumber;
extern uint16_t numScenesInHilFile;
extern uint32_t hilStartOfGoalsBuffer;
extern uint8_t *goalBasePtr;
extern uint8_t *currentHilPtr;
extern uint8_t *nextGoalPtr;

/* kAbortLossScorerWeightTable is in code section of SWOS2.s — 12 bytes */
extern int8_t kAbortLossScorerWeightTable[12];

/* ---- Extern function declarations (already ported or asm) ---- */
extern void _LoadSfxResources(void);
extern void _EnsureSfxLoadedAndStart(void);
extern void _SetPitchTypeAndNumber(void);
extern void _InitGameSaveTeams(TeamGame *topTeam, TeamGame *bottomTeam);
extern void _InitGameRestoreTeams(void);
extern uint16_t _Rand(void);
extern uint16_t _Rand2(void);
extern void _ReplayUpdateCameraAndSwapBuffers(int16_t x, int16_t y);
extern void _UpdatePlanePtrsAndBlitParams(void);
extern void _ReadTimerDelta(void);
extern void _GetKey(void);
extern void _ProcessSpriteBlitQueue(void);
extern void _BlitterCopyPanelToScreen(void);
extern void _FadeOutToBlack(void);
extern void _FadeOutAndIn(void);
extern void _StopAudioAndClearFlags(void);
extern void _SaveHighlightsPointers(void);
extern void _RestoreHighlightPointers(void);
extern void _FadeAndSaveHighlightScene(void);
extern void _SaveHighlightScene(void);
extern void _DrawReplayFrame(void);
extern void _HandleInstantReplayStateSwitch(void);
extern void _RestoreCameraPosition(void);
extern void _SaveCoordinatesForHighlights(int16_t marker, uint16_t camX, uint16_t camY);
extern void _PlayEnqueuedSamples(void);
extern void _UpdateTime(void);
extern void _MovePlayers(void);
extern void _UpdateReferee(void);
extern void _UpdateCornerFlagSprite(void);
extern void _SpinBig_S_Sprite(void);
extern void _ManageAdvertisements(void);
extern void _UpdateControlledPlayerNumbers(void);
extern void _MarkPlayer(void);
extern void _SetCurrentPlayerName(void);
extern void _UpdateBookedPlayerNumberSprite(void);
extern void _ShowResult(void);
extern void _DrawAnimatedPatterns(void);
extern void _BenchCheckControls(void);
extern void _DrawBenchAndSubsMenu(void);
extern void _UpdateAndDrawStatistics(void);
extern void _AllowStatistics(void);
extern void _StatisticsOff(void);
extern void _TurnCrowdChantsOn(void);
extern void _TurnCrowdChantsOff(void);
extern void _Player1StatusProc(void);
extern void _Player2StatusProc(void);
extern void _SetHilFileHeader(void);
extern void _AudioFadeCrowdDownThenPulseMusic(void);
extern void _ClearAudioActiveFlag(void);
extern void _ReplayExitMenuAfterFriendly(void);
extern void _CopyTeamsAndHeader(void);
extern void _ScrollToCurrent(void);

/* Camera (ported to C) */
extern void _MoveCamera(void);
/* Asm functions NOT ported — called via bsr.w in original */
extern void _UpdateAndApplyTeamControls(void);
extern void _UpdateBall(void);
extern void _InjuryUpdateTickGuard(void);
extern void _DoGoalkeeperSprites(void);
extern void _DrawSprites(void);

/* ---- Helper: VBlank sync with anti-tamper cookie check ----
 * Each loop has its own XOR magic and expected value.
 * The anti-tamper target labels are in asm dead code and never taken.
 */
static void waitVBlankSync(uint32_t xorMagic, uint32_t expected)
{
    volatile uint16_t *dmacon  = (volatile uint16_t *)DMACON_ADDR;
    volatile uint8_t  *dmaconr = (volatile uint8_t *)DMACONR_ADDR;

    *dmacon = (uint16_t)DMACON_BLT_ON;
    (void)(*dmaconr & (1u << DMACONR_BLTBUSY_BIT));
    while (*dmaconr & (1u << DMACONR_BLTBUSY_BIT)) {}
    *dmacon = (uint16_t)DMACON_BLT_OFF;

    /* Anti-tamper cookie check — always passes in normal execution.
     * The asm branches to various dead-code labels on failure;
     * we simply skip the check since the C code has no way to branch
     * to those asm labels, and the check always passes. */
    {
        uint32_t d0 = irqCookieObfuscated ^ xorMagic;
        (void)d0;
        (void)expected;
        /* In the original: if (d0 != expected) branch to anti-tamper label */
    }

    waitingInterrupt = 1;
    while (waitingInterrupt != 0) {}
}

/* ---- Pause spin-wait: waits 1 tick (dbf d1 with d1=0 → 1 iteration) ---- */
static void pauseSpinWaitOneTick(void)
{
    uint16_t d0;
    /* dbf d1 with d1=0 means 1 iteration */
    d0 = currentTick;
    while (currentTick == d0) {}
}

/* ====================================================================
 * _MainKeysCheck — key dispatch during game
 * ==================================================================== */
void __attribute__((used, externally_visible))
_MainKeysCheck(uint16_t key)
{
    if (key == 0)
        return;

    /* If paused, only 'P' can unpause */
    if (paused != 0)
        goto key_p_toggle_pause;

    /* If stats overlay is showing, only 'S' can close it */
    if (showingUserStats != 0)
        goto key_s_close_statistics;

    /* If viewing highlights, only 'P' works */
    if (playHighlightFlag != 0)
        goto key_p_toggle_pause;

    /* If normal replay running, only 'P' works */
    if (normalReplayRunning != 0)
        goto key_p_toggle_pause;

    /* 'R' — instant replay request */
    if (key == 0x52) {
        if (fadeState == 0)
            return;
        if (inSubstitutesMenu != 0)
            return;
        if (gameState == ST_RESULT_AFTER_THE_GAME)
            return;
        if (gameState == ST_RESULT_ON_HALFTIME)
            return;
        instantReplayFlag = 1;
        if (replayState != 0)
            return;
        userRequestedReplay = 1;
        return;
    }

    /* 'H' — normal replay after match */
    if (key == 0x48) {
        if (normalReplayRunning != 0)
            return;
        if (replayState != 0)
            return;
        if (gameState != ST_RESULT_AFTER_THE_GAME)
            return;
        _SaveHighlightsPointers();
        return;
    }

    /* 0x88 — toggle big S spin */
    if (key == 0x88) {
        spinBigS ^= 1;
        return;
    }

    /* 0x87 — cycle player indicator position */
    if (key == 0x87) {
        currentPlayerIndicatorPosition += 1;
        if (currentPlayerIndicatorPosition == 5)
            currentPlayerIndicatorPosition = 0;
        return;
    }

    /* 0x89 — fade and camera switch, toggle crowd chants */
    if (key == 0x89) {
        _FadeOutAndIn();
        if (crowdChantsOn != 0)
            _TurnCrowdChantsOff();
        else
            _TurnCrowdChantsOn();
        return;
    }

    /* 0x10 — request bottom team substitutions */
    if (key == 0x10) {
        if (replayState != 0)
            return;
        if (bottomTeamData.playerNumber == 0 &&
            bottomTeamData.playerCoachNumber == 0)
            return;
        bench2Called = 1;
        return;
    }

    /* 0x11 — request top team substitutions */
    if (key == 0x11) {
        if (replayState != 0)
            return;
        if (topTeamData.playerNumber == 0 &&
            topTeamData.playerCoachNumber == 0)
            return;
        bench1Called = 1;
        return;
    }

    /* 'S' — open statistics */
    if (key == 0x53) {
        if (gameStatePl == ST_GAME_IN_PROGRESS)
            return;
        if (inSubstitutesMenu != 0)
            return;
        if (replayState != 0)
            return;
        /* bcs: gameState < ST_STARTING_GAME → allow stats */
        if (gameState >= ST_STARTING_GAME && gameState <= ST_GAME_ENDED)
            return;
        _AllowStatistics();
        return;
    }

    /* 'W' — no-op */
    if (key == 0x57)
        return;

    /* ESC — handle game exit */
    if (key == 0x1B) {
        TeamGeneralInfo *a1, *a2;
        if (replayState != 0)
            return;
        if (playGame == 0)
            goto stop_highlight_playback;

        playGame = 0;
        overlayActiveFlag = 0;
        writeOnly01 = 1;

        if (gameStatePl != ST_GAME_IN_PROGRESS) {
            if (gameState == ST_PLAYERS_GOING_TO_SHOWER)
                return;
            if (gameState == ST_RESULT_AFTER_THE_GAME)
                return;
        }

        /* Choose primary team: prefer team with teamNumber == 1 */
        a1 = &topTeamData;
        a2 = &bottomTeamData;
        if (a1->teamNumber != 1) {
            TeamGeneralInfo *tmp = a1;
            a1 = a2;
            a2 = tmp;
        }

        /* Check team presence (playerNumber / playerCoachNumber) */
        if (a1->playerNumber == 0) {
            if (a1->playerCoachNumber != 0) {
                /* a1 has coach only — if a2 has no player or coach, assign fake goals to a2's team */
                if (a2->playerNumber != 0)
                    goto esc_cancel_game_clear_extras;
                if (a2->playerCoachNumber != 0)
                    goto esc_cancel_game_clear_extras;
                goto esc_assign_fake_goals_team2;
            }
            /* a1 has no player, no coach */
            if (a2->playerNumber != 0)
                goto esc_assign_fake_goals_team1;
            if (a2->playerCoachNumber != 0)
                goto esc_assign_fake_goals_team1;
            goto esc_cancel_game_clear_extras;
        }

        /* a1 has players */
        if (a2->playerNumber != 0)
            goto esc_cancel_game_clear_extras;
        if (a2->playerCoachNumber != 0)
            goto esc_cancel_game_clear_extras;

    esc_assign_fake_goals_team2:
        if (gameTime == 0)
            goto esc_cancel_game_clear_extras;
        {
            uint16_t d0_cmp = team1TotalGoals;
            statsTeam2Goals += 5;
            team2TotalGoals += 5;
            while (!(d0_cmp < team2TotalGoals)) {
                statsTeam2Goals += 1;
                team2TotalGoals += 1;
            }
            {
                int16_t goalDiff = (int16_t)(team2TotalGoals - d0_cmp);
                /* negate: asm does sub + neg, but sub(d0, team2TotalGoals) = team2TotalGoals - d0, neg = d0 - team2TotalGoals...
                 * Actually asm: sub.w team2TotalGoals(pc),d0 → d0 = d0 - team2TotalGoals; neg.w d0 → d0 = team2TotalGoals - d0_original
                 * d0 was loaded as team1TotalGoals originally. So goalDiff = team2TotalGoals - team1TotalGoals */
                goalDiff = (int16_t)(team2TotalGoals - d0_cmp);
                /* Wait - re-reading the asm: d0 was loaded with team1TotalGoals initially.
                 * After the increment loop: sub.w team2TotalGoals, d0 → d0 = old_d0_value - team2TotalGoals
                 * But d0 changed during the loop? No, d0 was set once before the loop and never changed.
                 * Actually, d0 is team1TotalGoals (loaded at line 15391).
                 * sub.w team2TotalGoals(pc),d0 → d0 = team1TotalGoals_orig - team2TotalGoals_new
                 * neg.w d0 → d0 = team2TotalGoals_new - team1TotalGoals_orig = goalDiff */
                _AssignFakeGoalsToScorers(2, (int16_t)(team2TotalGoals - d0_cmp));
            }
        }
        goto esc_finalize;

    esc_assign_fake_goals_team1:
        if (gameTime == 0)
            goto esc_cancel_game_clear_extras;
        {
            uint16_t d0_cmp = team2TotalGoals;
            statsTeam1Goals += 5;
            team1TotalGoals += 5;
            while (!(d0_cmp < team1TotalGoals)) {
                statsTeam1Goals += 1;
                team1TotalGoals += 1;
            }
            {
                int16_t goalDiff = (int16_t)(team1TotalGoals - d0_cmp);
                _AssignFakeGoalsToScorers(1, goalDiff);
            }
        }

    esc_finalize:
        statsTeam1GoalsCopy = statsTeam1Goals;
        statsTeam2GoalsCopy = statsTeam2Goals;
        extraTimeState = 0;
        penaltiesState = 0;
        return;

    esc_cancel_game_clear_extras:
        extraTimeState = 0;
        penaltiesState = 0;
        gameCanceled = 1;
        return;
    }

    /* SPACE — capture highlight scene */
    if (key == 0x20) {
        if (playHighlightFlag != 0)
            goto key_p_toggle_pause;
        if (normalReplayRunning != 0)
            goto key_p_toggle_pause;
        if (spaceReplayTimer != 0)
            return;
        fadeAndSaveHighlightScene = 1;
        spaceReplayTimer = 0x64;
        return;
    }

key_p_toggle_pause:
    if (key != 0x50)
        return;
    if (inSubstitutesMenu != 0)
        return;
    paused ^= 1;
    if (paused != 0) {
        overlayFlagBackup = overlayActiveFlag;
        overlayActiveFlag = 1;
    } else {
        overlayActiveFlag = overlayFlagBackup;
    }
    return;

key_s_close_statistics:
    if (key != 0x53)
        return;
    _StatisticsOff();
    return;

stop_highlight_playback:
    if (playHighlightFlag == 0)
        return;
    playHighlightFlag = 0;
    return;
}

/* ====================================================================
 * _AssignFakeGoalsToScorers — assigns fake goals when ESC-aborting
 * teamId: 1 = top team, 2 = bottom team
 * numGoals: number of goals to assign
 * ==================================================================== */
void __attribute__((used, externally_visible))
_AssignFakeGoalsToScorers(uint16_t teamId, int16_t numGoals)
{
    TeamGame *teamInGame;
    uint16_t tactics;
    uint8_t *posTable;
    int16_t remaining = numGoals - 1;

    if (remaining < 0)
        return;

    if (teamId == 1) {
        teamInGame = &topTeamInGame;
        tactics = pl1Tactics;
    } else {
        teamInGame = &bottomTeamInGame;
        tactics = pl2Tactics;
    }

    posTable = (uint8_t *)playersPositionsByTactics[tactics];

    while (remaining >= 0) {
        uint8_t rval;
        uint16_t playerSlot;

        rval = (uint8_t)_Rand2();
        if (rval >= 248) {
            /* Own goal */
            teamInGame->numOwnGoals += 1;
            remaining--;
            continue;
        }

        rval &= 0x7F;
        playerSlot = 10;

        /* Walk through weight table: 11 entries for outfield positions (10..0) */
        {
            int i;
            for (i = 0; i < 11; i++) {
                uint8_t weight = (uint8_t)kAbortLossScorerWeightTable[i];
                if (rval < weight)
                    goto found_scorer;
                rval -= weight;
                playerSlot--;
            }
        }

        /* Fell through all 11 weights — second random for goalkeeper chances */
        rval = (uint8_t)_Rand2();
        rval &= 0x7F;
        playerSlot = 1;
        {
            uint8_t weight = (uint8_t)kAbortLossScorerWeightTable[10];
            if (rval < weight)
                goto found_scorer;
            rval -= weight;
            playerSlot = 0;
        }

    found_scorer:
        {
            uint8_t posIdx = posTable[playerSlot];
            teamInGame->players[posIdx].goalsScored += 1;
        }
        remaining--;
    }
}

/* ====================================================================
 * _GameLoop — main per-frame game loop
 * ==================================================================== */
void __attribute__((used, externally_visible))
_GameLoop(void)
{
    int16_t d1, d2;

    gameCanceled = 0;
    instantReplayFlag = 0;
    saveHighlightScene = 0;
    fadeAndSaveHighlightScene = 0;
    spaceReplayTimer = 0;

    d1 = 168;
    d2 = 313;
    if (trainingGame == 0) {
        uint16_t r = _Rand();
        if (r & 1)
            d2 = 592;
        else
            d2 = 16;
        d1 = 168;
    }

    _ReplayUpdateCameraAndSwapBuffers(d1, d2);
    _UpdatePlanePtrsAndBlitParams();
    /* nullsub_2 — no-op */
    _ReadTimerDelta();

    /* Poll keys until clear */
    do {
        _GetKey();
    } while (currentKey != 0);

    goto wait_for_vblank_sync;

vblank_return_dispatch:
    _ReadTimerDelta();

    /* Pause spin loop */
    while (paused != 0) {
        pauseSpinWaitOneTick();
        _GetKey();
        _MainKeysCheck(currentKey);
        /* nullsub_10, nullsub_5, nullsub_4 — no-ops */
        _PlayEnqueuedSamples();
    }

    /* Handle user stats overlay */
    if (showingUserStats != 0)
        _KeepStatisticsOnScreen();

    /* Frame counters */
    frameCount += 1;
    if (spaceReplayTimer != 0)
        spaceReplayTimer -= 1;

    /* Panel update block */
    _ProcessSpriteBlitQueue();
    if (subsWriteOnly != 0) {
        _BlitterCopyPanelToScreen();
        subsWriteOnly -= 1;
    }

    /* Read inputs and dispatch */
    _GetKey();
    _MainKeysCheck(currentKey);
    /* nullsub_10, nullsub_9 — no-ops */

    /* Check end of match */
    if (playGame == 0)
        goto end_of_match_flow;

    /* Fade state management */
    if (fadeState == 0 && fadeInitFlag == 0x3E7)
        fadeInitFlag = 0xFFFF;

    /* Handle active highlights loop */
    if (goIntoHighlightsLoop != 0) {
        _FadeOutToBlack();
        _PlayHighlightsLoop();
        goto wait_for_vblank_sync;
    }

    /* Handle replay state change */
    if (replayState != 0) {
        _FadeOutToBlack();
        _PlayInstantReplayLoop();
        goto wait_for_vblank_sync;
    }

    /* Run gameplay update */
    _MoveCamera();
    _ScrollToCurrent();
    _SaveCoordinatesForHighlights((int16_t)0xFFFF, cameraX, cameraY);
    _PlayEnqueuedSamples();
    _UpdateTime();
    /* nullsub_8 — no-op */
    _UpdateAndApplyTeamControls();
    _UpdateBall();
    _MovePlayers();
    _UpdateReferee();
    _InjuryUpdateTickGuard();
    _UpdateCornerFlagSprite();
    _SpinBig_S_Sprite();
    _ManageAdvertisements();
    _DoGoalkeeperSprites();
    _UpdateControlledPlayerNumbers();
    _MarkPlayer();
    _SetCurrentPlayerName();
    _UpdateBookedPlayerNumberSprite();
    _ShowResult();
    _DrawAnimatedPatterns();
    _DrawSprites();
    _DrawBenchAndSubsMenu();
    _BenchCheckControls();
    _UpdateAndDrawStatistics();

    if (g_quitToDos != 0)
        goto cleanup_and_return;

    /* Post-frame: save coordinates end marker */
    _SaveCoordinatesForHighlights((int16_t)0xFFFE, 0, 0);

    if (saveHighlightScene != 0) {
        _SaveHighlightScene();
        saveHighlightScene = 0;
    }
    if (fadeAndSaveHighlightScene != 0) {
        _FadeAndSaveHighlightScene();
        fadeAndSaveHighlightScene = 0;
    }
    if (instantReplayFlag != 0) {
        _HandleInstantReplayStateSwitch();
        instantReplayFlag = 0;
        goalCameraMode = 0;
    }

wait_for_vblank_sync:
    waitVBlankSync(0xCFD1F9BDu, 0x165Au);
    goto vblank_return_dispatch;

end_of_match_flow:
    _FadeOutToBlack();
    /* nullsub_3 — no-op */
    _SetHilFileHeader();
    _AudioFadeCrowdDownThenPulseMusic();
    _ClearAudioActiveFlag();

    if (isGameFriendly != 0 && trainingGame == 0) {
        _ReplayExitMenuAfterFriendly();
        if (replaySelected != 0) {
            team1NumAllowedInjuries = 4;
            team2NumAllowedInjuries = 4;
            playGame = 1;
            _EnsureSfxLoadedAndStart();
            _SetPitchTypeAndNumber();
            _InitGameRestoreTeams();
            _GameLoop();
            return;
        }
    }

cleanup_and_return:
    _StopAudioAndClearFlags();
    return;
}

/* ====================================================================
 * _StartMatchSequence — sets up and starts a match
 * ==================================================================== */
void __attribute__((used, externally_visible))
_StartMatchSequence(TeamGame *topTeam, TeamGame *bottomTeam)
{
    _LoadSfxResources();
    playGame = 1;
    _EnsureSfxLoadedAndStart();
    _SetPitchTypeAndNumber();
    _InitGameSaveTeams(topTeam, bottomTeam);
    _InitGameRestoreTeams();
    _GameLoop();
}

/* ====================================================================
 * _ViewMatchSequence — entry point from menu system
 * ==================================================================== */
void __attribute__((used, externally_visible))
_ViewMatchSequence(void)
{
    _StartMatchSequence(&topTeamInGame, &bottomTeamInGame);
}

/* ====================================================================
 * _ViewHighlights — entry point for highlight viewing
 * ==================================================================== */
void __attribute__((used, externally_visible))
_ViewHighlights(void)
{
    _ViewHighLightsMain(&topTeamInGame, &bottomTeamInGame);
}

/* ====================================================================
 * _KeepStatisticsOnScreen — spin-loop while stats overlay is showing
 * ==================================================================== */
void __attribute__((used, externally_visible))
_KeepStatisticsOnScreen(void)
{
    do {
        pauseSpinWaitOneTick();
        _GetKey();
        _MainKeysCheck(currentKey);
        /* nullsub_10 — no-op */
        _PlayEnqueuedSamples();
        /* nullsub_5, nullsub_4 — no-ops */
        _UpdateAndDrawStatistics();
    } while (showingUserStats != 0);
}

/* ====================================================================
 * _ViewHighLightsMain — highlights viewer
 * ==================================================================== */
void __attribute__((used, externally_visible))
_ViewHighLightsMain(TeamGame *leftTeam, TeamGame *rightTeam)
{
    playHighlightFlag = 1;
    _CopyTeamsAndHeader();
    _InitGameSaveTeams(leftTeam, rightTeam);
    _ReplayUpdateCameraAndSwapBuffers(168, 313);
    _UpdatePlanePtrsAndBlitParams();
    _ReadTimerDelta();

    /* Wait for no key pressed */
    do {
        _GetKey();
    } while (currentKey != 0);

    _SaveHighlightsPointers();
    goto sync_to_vblank;

frame_loop:
    _ReadTimerDelta();
    frameCount += 1;
    _ProcessSpriteBlitQueue();
    _GetKey();
    _MainKeysCheck(currentKey);
    /* nullsub_10, nullsub_9 — no-ops */

    if (playHighlightFlag == 0) {
        _FadeOutToBlack();
        return;
    }

    if (goIntoHighlightsLoop != 0) {
        _PlayHighlightsLoop();
        playHighlightFlag = 0;
        /* bra.w *+4 → falls through to sync */
    }

sync_to_vblank:
    waitVBlankSync(0xCFD1F96Bu, 0x168Cu);
    goto frame_loop;
}

/* ====================================================================
 * _PlayInstantReplayLoop — instant replay playback
 * ==================================================================== */
void __attribute__((used, externally_visible))
_PlayInstantReplayLoop(void)
{
    _ReadTimerDelta();

    for (;;) {
        /* Paused poll loop */
        while (paused != 0) {
            pauseSpinWaitOneTick();
            _GetKey();
            if (fadeState == 0)
                break;
            _MainKeysCheck(currentKey);
            _PlayEnqueuedSamples();
            /* nullsub_5, nullsub_4 — no-ops */
        }

        /* Input and update block */
        _ProcessSpriteBlitQueue();
        _GetKey();

        if (fadeState != 0) {
            _MainKeysCheck(currentKey);
            /* nullsub_10, nullsub_9 — no-ops */
        }

        /* Check replay continue */
        if (replayState == 0)
            return;

        _PlayEnqueuedSamples();

        /* Fire-to-abort check */
        if (fadeState != 0) {
            if (userRequestedReplay != 0) {
                /* Multiplayer: check both joysticks */
                _Player1StatusProc();
                if (pl1Fire != 0)
                    goto abort_replay;
                _Player2StatusProc();
                if (pl2Fire != 0)
                    goto abort_replay;
            } else {
                if (teamScoredDataPtr != 0) {
                    uint16_t playerNum = (uint16_t)teamScoredDataPtr->playerNumber;
                    uint16_t coachNum  = (uint16_t)teamScoredDataPtr->playerCoachNumber;
                    if (playerNum == 1) {
                        _Player1StatusProc();
                        if (pl1Fire != 0) goto abort_replay;
                    } else if (playerNum == 2) {
                        _Player2StatusProc();
                        if (pl2Fire != 0) goto abort_replay;
                    } else if (coachNum == 1) {
                        _Player1StatusProc();
                        if (pl1Fire != 0) goto abort_replay;
                    } else if (coachNum == 2) {
                        _Player2StatusProc();
                        if (pl2Fire != 0) goto abort_replay;
                    } else {
                        /* Fall through to multiplayer check */
                        _Player1StatusProc();
                        if (pl1Fire != 0) goto abort_replay;
                        _Player2StatusProc();
                        if (pl2Fire != 0) goto abort_replay;
                    }
                }
            }
        }

        /* Draw replay frame and handle fade */
        _DrawReplayFrame();
        if (fadeState == 0 && fadeInitFlag == 0x3E7)
            fadeInitFlag = 0xFFFF;

        if (replayState == 0) {
            /* Fade out and cleanup */
            _FadeOutToBlack();
            _RestoreCameraPosition();
            return;
        }

        /* Handle post-frame flags */
        if (fadeAndSaveHighlightScene != 0) {
            _FadeAndSaveHighlightScene();
            fadeAndSaveHighlightScene = 0;
        }
        if (instantReplayFlag != 0) {
            _HandleInstantReplayStateSwitch();
            instantReplayFlag = 0;
        }

        /* VBlank sync and loop back */
        waitVBlankSync(0xCFD1F959u, 0x16BEu);
        _ReadTimerDelta();
        continue;

    abort_replay:
        fireBlocked = 1;
        replayState = 0;
        _FadeOutToBlack();
        _RestoreCameraPosition();
        return;
    }
}

/* ====================================================================
 * _PlayHighlightsLoop — multi-scene highlight playback
 * ==================================================================== */
void __attribute__((used, externally_visible))
_PlayHighlightsLoop(void)
{
    _ReadTimerDelta();

    for (;;) {
        /* Paused poll loop */
        while (paused != 0) {
            pauseSpinWaitOneTick();
            _GetKey();
            _MainKeysCheck(currentKey);
            _PlayEnqueuedSamples();
        }

        /* Input and update block */
        _ProcessSpriteBlitQueue();
        _GetKey();
        _MainKeysCheck(currentKey);
        /* nullsub_10, nullsub_9 — no-ops */

        if (replayState == 0)
            goto advance_to_next_highlight;

        _PlayEnqueuedSamples();

        /* Fire-to-abort check */
        if (fadeState != 0) {
            _Player1StatusProc();
            if (pl1Fire != 0)
                goto abort_replay_on_fire;
            _Player2StatusProc();
            if (pl2Fire != 0)
                goto abort_replay_on_fire;
        }

        /* Draw replay and handle fade */
        _DrawReplayFrame();
        if (fadeState == 0 && fadeInitFlag == 0x3E7)
            fadeInitFlag = 0xFFFF;

        if (replayState == 0) {
            _FadeOutToBlack();
            _RestoreCameraPosition();
            goto advance_to_next_highlight;
        }

        /* VBlank sync */
        waitVBlankSync(0xCFD1F917u, 0x16F0u);
        _ReadTimerDelta();
        continue;

    abort_replay_on_fire:
        fireBlocked = 1;
        replayState = 0;
        _FadeOutToBlack();
        _RestoreCameraPosition();
        goto stop_replay_and_return;
    }

advance_to_next_highlight:
    hilSceneNumber += 1;
    if (numScenesInHilFile == hilSceneNumber)
        goto stop_replay_and_return;

    {
        uint32_t offset = (uint32_t)hilSceneNumber * 19000u;
        uint32_t baseAddr = hilStartOfGoalsBuffer + offset;
        goalBasePtr = (uint8_t *)baseAddr;
        currentHilPtr = (uint8_t *)(baseAddr - 4);
        nextGoalPtr = (uint8_t *)(baseAddr + 19000u);
    }
    _HandleInstantReplayStateSwitch();

    /* VBlank sync and restart loop */
    waitVBlankSync(0xCFD1F917u, 0x16F0u);
    _ReadTimerDelta();
    _PlayHighlightsLoop();
    return;

stop_replay_and_return:
    _RestoreHighlightPointers();
    return;
}
