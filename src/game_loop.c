/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* game_loop.c — Game Loop, MainKeysCheck, Highlights Playback, AssignFakeGoalsToScorers
 *
 * Chunk 41: 10 functions (7 exported + 2 static + 1 helper)
 */

#include "game_loop.h"
#include "game_types.h"
#include "display.h"
#include "deterministic.h"

/* ---- Extern globals (data section, exported from asm/Data.c) ---- */
extern uint16_t gameCanceled;
extern uint16_t instantReplayFlag;
extern volatile uint16_t saveHighlightScene;
extern volatile uint16_t fadeAndSaveHighlightScene;
extern uint16_t spaceReplayTimer;
extern uint16_t subsWriteOnly;
extern volatile uint16_t playGame;
extern volatile uint16_t playHighlightFlag;
extern uint16_t normalReplayRunning;
extern uint16_t userRequestedReplay;
extern uint16_t overlayFlagBackup;
extern volatile uint16_t paused;
extern volatile uint16_t panicReasonCode;
extern void PanicHaltLoop(void);
extern uint16_t overlayActiveFlag;
extern volatile uint16_t currentTick;
extern volatile uint16_t currentGameTick;
extern volatile uint16_t currentKey;
extern uint16_t frameCount;
extern volatile uint16_t fadeState;
extern volatile uint16_t fadeInitFlag;
extern volatile uint16_t goIntoHighlightsLoop;
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
extern volatile uint16_t goalCameraMode;
extern uint16_t isGameFriendly;
extern uint16_t replaySelected;
extern uint16_t team1NumAllowedInjuries;
extern uint16_t team2NumAllowedInjuries;
extern uint16_t extraTimeState;
extern uint16_t penaltiesState;
extern uint16_t writeOnly01;
extern uint32_t gameTime;
extern FixedPoint cameraX;
extern FixedPoint cameraY;
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
extern void *playersPositionsByTactics[18];

extern uint16_t hilSceneNumber;
extern volatile uint16_t numScenesInHilFile;
extern uint8_t hilStartOfGoalsBuffer[];
extern uint8_t *goalBasePtr;
extern uint8_t *currentHilPtr;
extern uint8_t *nextGoalPtr;

/* kAbortLossScorerWeightTable is in code section of SWOS2.s — 12 bytes */
extern int8_t kAbortLossScorerWeightTable[12];

/* ---- Extern function declarations (already ported or asm) ---- */
extern void LoadSfxResources(void);
extern void EnsureSfxLoadedAndStart(void);
extern void SetPitchTypeAndNumber(void);
extern void InitGameSaveTeams(TeamGame *topTeam, TeamGame *bottomTeam);
extern void InitGameRestoreTeams(void);
extern uint16_t Rand(void);
extern uint16_t Rand2(void);
extern void ReplayUpdateCameraAndSwapBuffers(int16_t x, int16_t y);
extern void UpdatePlanePtrsAndBlitParams(void);
extern void ReadTimerDelta(void);
extern void GetKey(void);
extern void ProcessSpriteBlitQueue(void);
extern void BlitterCopyPanelToScreen(void);
extern void FadeOutToBlack(void);
extern void FadeOutAndIn(void);
extern void StopAudioAndClearFlags(void);
extern void SaveHighlightsPointers(void);
extern void RestoreHighlightPointers(void);
extern void FadeAndSaveHighlightScene(void);
extern void SaveHighlightScene(void);
extern void DrawReplayFrame(void);
extern void HandleInstantReplayStateSwitch(void);
extern void RestoreCameraPosition(void);
extern void SaveCoordinatesForHighlights(int16_t marker, uint16_t camX, uint16_t camY);
extern void PlayEnqueuedSamples(void);
extern void UpdateTime(void);
extern void MovePlayers(void);
extern void UpdateReferee(void);
extern void UpdateCornerFlagSprite(void);
extern void SpinBig_S_Sprite(void);
extern void ManageAdvertisements(void);
extern void UpdateControlledPlayerNumbers(void);
extern void MarkPlayer(void);
extern void SetCurrentPlayerName(void);
extern void UpdateBookedPlayerNumberSprite(void);
extern void ShowResult(void);
extern void DrawAnimatedPatterns(void);
extern void BenchCheckControls(void);
extern void DrawBenchAndSubsMenu(void);
extern void UpdateAndDrawStatistics(void);
extern void AllowStatistics(void);
extern void StatisticsOff(void);
extern void TurnCrowdChantsOn(void);
extern void TurnCrowdChantsOff(void);
extern void Player1StatusProc(void);
extern void Player2StatusProc(void);
extern void SetHilFileHeader(void);
extern void AudioFadeCrowdDownThenPulseMusic(void);
extern void ClearAudioActiveFlag(void);
extern void ReplayExitMenuAfterFriendly(void);
extern void CopyTeamsAndHeader(void);
extern void ScrollToCurrent(void);

/* Camera (ported to C) */
extern void MoveCamera(void);
/* Asm functions NOT ported — called via bsr.w in original */
extern void UpdateAndApplyTeamControls(void);
extern void UpdateBall(void);
extern void InjuryUpdateTickGuard(void);
extern void DoGoalkeeperSprites(void);
extern void DrawSprites(void);

/* ---- Helper: VBlank sync with anti-tamper cookie check ----
 * Each loop has its own XOR magic and expected value.
 * The anti-tamper target labels are in asm dead code and never taken.
 */
static void waitVBlankSync(uint32_t xorMagic, uint32_t expected)
{
    custom->dmacon = (uint16_t)DMACON_BLT_ON;
    (void)(custom->dmaconr & (1u << 14));
    while (custom->dmaconr & (1u << 14)) {}
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;

    /* Anti-tamper cookie check — always passes in normal execution */
    {
        uint32_t d0 = irqCookieObfuscated ^ xorMagic;
        (void)d0;
        (void)expected;
    }

#ifdef SWOS_SDL_BUILD
    /* The game loop draws the full frame, then enters waitVBlankSync.
     * On Amiga this spun on `waitingInterrupt` until the VBlank ISR
     * cleared it after a buffer-swap. On SDL we run that swap inline
     * (matching `WaitForVSync` in game_utils.c) and hand control to
     * Platform_VBlank, which blits the just-finished frontbuffer to
     * the SDL texture + paces to 50 Hz. Without this, the game-loop
     * branch of `waitVBlankSync` never called Platform_VBlank — the
     * SDL timer thread was the only path that presented frames and it
     * read the frontbuffer mid-draw, tearing sprites every frame. */
    {
        extern void SwapBuffersAndCameraViewState(void);
        extern void SetCameraCoordinates(int16_t x, int16_t y);
        extern int Platform_VBlank(void);
        int16_t cx = cameraX.s.i;
        int16_t cy = cameraY.s.i;
        SwapBuffersAndCameraViewState();
        SetCameraCoordinates(cx, cy);
        Platform_VBlank();
    }
#else
    waitingInterrupt = 1;
    while (waitingInterrupt != 0) {}
#endif
}

/* ---- Pause spin-wait: waits 1 tick (dbf d1 with d1=0 → 1 iteration) ----
 *
 * Original Amiga: busy-spin on `currentTick`. The Amiga's VBlank ISR
 * advances currentTick AND services keyboard input natively, so a
 * busy spin works.
 *
 * On SDL the `currentTick` advance happens on the SDL_Timer thread
 * but keyboard events are dispatched only when something pumps the
 * SDL event queue. The timer thread CAN do a full event pump when
 * the main thread looks idle (>60ms since last render) but on
 * Linux some setups schedule the busy main thread aggressively
 * enough that the timer thread can starve, locking us in the pause
 * loop forever (Linux tester report: "pause hangs, can't unpause").
 *
 * Replace the busy spin with Platform_VBlank() — it does the
 * 50 Hz pacing via SDL_Delay (which yields to the OS), pumps SDL
 * events on the main thread (so keypresses reliably reach
 * keyBuffer), and runs the throttled ISR housekeeping. The
 * Platform_VBlank path also re-renders the same frontbuffer so
 * the paused frame stays on screen. */
#ifdef SWOS_SDL_BUILD
extern int Platform_VBlank(void);
#endif

static void pauseSpinWaitOneTick(void)
{
#ifdef SWOS_SDL_BUILD
    Platform_VBlank();
#else
    uint16_t d0;
    /* dbf d1 with d1=0 means 1 iteration */
    d0 = currentTick;
    while (currentTick == d0) {}
#endif
}

/* ====================================================================
 * MainKeysCheck — key dispatch during game
 * ==================================================================== */
void __attribute__((used, externally_visible))
MainKeysCheck(uint16_t key)
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
        SaveHighlightsPointers();
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
        FadeOutAndIn();
        if (crowdChantsOn != 0)
            TurnCrowdChantsOff();
        else
            TurnCrowdChantsOn();
        return;
    }

    /* 0x10 (Amiga 'Q') — request bottom team substitutions */
    if (key == 0x10) {
        if (replayState != 0)
            return;
        if (bottomTeamData.playerNumber == 0 &&
            bottomTeamData.playerCoachNumber == 0)
            return;
#ifdef SWOS_DEBUG_AUTOSKIP
        SWOS_DBG("[bench] HOTKEY 0x10 (Q) -> bench2Called\n");
#endif
        bench2Called = 1;
        return;
    }

    /* 0x11 (Amiga 'W') — request top team substitutions */
    if (key == 0x11) {
        if (replayState != 0)
            return;
        if (topTeamData.playerNumber == 0 &&
            topTeamData.playerCoachNumber == 0)
            return;
#ifdef SWOS_DEBUG_AUTOSKIP
        SWOS_DBG("[bench] HOTKEY 0x11 (W) -> bench1Called\n");
#endif
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
        AllowStatistics();
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
                AssignFakeGoalsToScorers(2, (int16_t)(team2TotalGoals - d0_cmp));
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
                AssignFakeGoalsToScorers(1, goalDiff);
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
    StatisticsOff();
    return;

stop_highlight_playback:
    if (playHighlightFlag == 0)
        return;
    playHighlightFlag = 0;
    return;
}

/* ====================================================================
 * AssignFakeGoalsToScorers — assigns fake goals when ESC-aborting
 * teamId: 1 = top team, 2 = bottom team
 * numGoals: number of goals to assign
 * ==================================================================== */
void __attribute__((used, externally_visible))
AssignFakeGoalsToScorers(uint16_t teamId, int16_t numGoals)
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

        rval = (uint8_t)Rand2();
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
        rval = (uint8_t)Rand2();
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
 * GameLoop — main per-frame game loop
 * ==================================================================== */
void __attribute__((used, externally_visible))
GameLoop(void)
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
        uint16_t r = Rand();
        if (r & 1)
            d2 = 592;
        else
            d2 = 16;
        d1 = 168;
    }

    ReplayUpdateCameraAndSwapBuffers(d1, d2);
    UpdatePlanePtrsAndBlitParams();
    /* nullsub_2 — no-op */
    ReadTimerDelta();

    /* Poll keys until clear */
    do {
        GetKey();
    } while (currentKey != 0);

    goto wait_for_vblank_sync;

vblank_return_dispatch:
    ReadTimerDelta();

    /* Pause spin loop */
    while (paused != 0) {
        pauseSpinWaitOneTick();
        GetKey();
        MainKeysCheck(currentKey);
        /* nullsub_10, nullsub_5, nullsub_4 — no-ops */
        PlayEnqueuedSamples();
    }

    /* Handle user stats overlay */
    if (showingUserStats != 0)
        KeepStatisticsOnScreen();

    /* Frame counters */
    frameCount += 1;
    if (spaceReplayTimer != 0)
        spaceReplayTimer -= 1;

    /* Panel update block */
    ProcessSpriteBlitQueue();
    if (subsWriteOnly != 0) {
        BlitterCopyPanelToScreen();
        subsWriteOnly -= 1;
    }

    /* Read inputs and dispatch */
    GetKey();
    MainKeysCheck(currentKey);
    /* nullsub_10, nullsub_9 — no-ops */

    /* Check end of match */
    if (playGame == 0)
        goto end_of_match_flow;

    /* Fade state management */
    if (fadeState == 0 && fadeInitFlag == 0x3E7)
        fadeInitFlag = 0xFFFF;

    /* Handle active highlights loop */
    if (goIntoHighlightsLoop != 0) {
        FadeOutToBlack();
        PlayHighlightsLoop();
        goto wait_for_vblank_sync;
    }

    /* Handle replay state change */
    if (replayState != 0) {
        FadeOutToBlack();
        PlayInstantReplayLoop();
        goto wait_for_vblank_sync;
    }

    /* Run gameplay update */
    MoveCamera();
    ScrollToCurrent();
    SaveCoordinatesForHighlights((int16_t)0xFFFF, cameraX.s.i, cameraY.s.i);
    PlayEnqueuedSamples();
    UpdateTime();
    /* nullsub_8 — no-op */
    UpdateAndApplyTeamControls();
    UpdateBall();
    MovePlayers();
    UpdateReferee();
    InjuryUpdateTickGuard();
    UpdateCornerFlagSprite();
    SpinBig_S_Sprite();
    /* Widescreen: the in-match world (ads, animated patterns, players/ball) draws at
     * offset 0 — the pitch fills the whole frame. Menu/HUD centering is re-established
     * by DrawMenu when the frontend draws again. */
    g_menu_draw_x_offset = 0;
    ManageAdvertisements();
    DoGoalkeeperSprites();
    UpdateControlledPlayerNumbers();
    MarkPlayer();
    SetCurrentPlayerName();
    UpdateBookedPlayerNumberSprite();
    ShowResult();
    DrawAnimatedPatterns();
    DrawSprites();
    DrawBenchAndSubsMenu();
    BenchCheckControls();
    UpdateAndDrawStatistics();

    if (g_quitToDos != 0)
        goto cleanup_and_return;

    /* Post-frame: save coordinates end marker */
    SaveCoordinatesForHighlights((int16_t)0xFFFE, 0, 0);

    if (saveHighlightScene != 0) {
        SaveHighlightScene();
        saveHighlightScene = 0;
    }
    if (fadeAndSaveHighlightScene != 0) {
        FadeAndSaveHighlightScene();
        fadeAndSaveHighlightScene = 0;
    }
    if (instantReplayFlag != 0) {
        HandleInstantReplayStateSwitch();
        instantReplayFlag = 0;
        goalCameraMode = 0;
    }

wait_for_vblank_sync:
    waitVBlankSync(0xCFD1F9BDu, 0x165Au);
#ifdef DETERMINISTIC_DUMP_FRAME
    _RngDumpCheck();
#endif
    goto vblank_return_dispatch;

end_of_match_flow:
    FadeOutToBlack();
    /* nullsub_3 — no-op */
    SetHilFileHeader();
    AudioFadeCrowdDownThenPulseMusic();
    ClearAudioActiveFlag();

    if (isGameFriendly != 0 && trainingGame == 0) {
        ReplayExitMenuAfterFriendly();
        if (replaySelected != 0) {
            team1NumAllowedInjuries = 4;
            team2NumAllowedInjuries = 4;
            playGame = 1;
            EnsureSfxLoadedAndStart();
            SetPitchTypeAndNumber();
            InitGameRestoreTeams();
            GameLoop();
            return;
        }
    }

cleanup_and_return:
    StopAudioAndClearFlags();
    return;
}

/* ====================================================================
 * StartMatchSequence — sets up and starts a match
 * ==================================================================== */
void __attribute__((used, externally_visible))
StartMatchSequence(TeamGame *topTeam, TeamGame *bottomTeam)
{
    LoadSfxResources();
    playGame = 1;
    EnsureSfxLoadedAndStart();
    SetPitchTypeAndNumber();
    InitGameSaveTeams(topTeam, bottomTeam);
#ifdef AGA
    _WriteCopperPitchPalette();
#endif
    InitGameRestoreTeams();
    GameLoop();
}

/* ====================================================================
 * ViewMatchSequence — entry point from menu system
 * ==================================================================== */
void __attribute__((used, externally_visible))
ViewMatchSequence(void)
{
    StartMatchSequence(&topTeamInGame, &bottomTeamInGame);
}

/* ====================================================================
 * ViewHighlights — entry point for highlight viewing
 * ==================================================================== */
void __attribute__((used, externally_visible))
ViewHighlights(void)
{
    ViewHighLightsMain(&topTeamInGame, &bottomTeamInGame);
}

/* ====================================================================
 * KeepStatisticsOnScreen — spin-loop while stats overlay is showing
 * ==================================================================== */
void __attribute__((used, externally_visible))
KeepStatisticsOnScreen(void)
{
    do {
        pauseSpinWaitOneTick();
        GetKey();
        MainKeysCheck(currentKey);
        /* nullsub_10 — no-op */
        PlayEnqueuedSamples();
        /* nullsub_5, nullsub_4 — no-ops */
        UpdateAndDrawStatistics();
    } while (showingUserStats != 0);
}

/* ====================================================================
 * ViewHighLightsMain — highlights viewer
 * ==================================================================== */
void __attribute__((used, externally_visible))
ViewHighLightsMain(TeamGame *leftTeam, TeamGame *rightTeam)
{
    playHighlightFlag = 1;
    CopyTeamsAndHeader();
    InitGameSaveTeams(leftTeam, rightTeam);
    ReplayUpdateCameraAndSwapBuffers(168, 313);
    UpdatePlanePtrsAndBlitParams();
    ReadTimerDelta();

    /* Drain the keyBuffer — original Amiga waits here while the user
     * holds the fire button they just pressed (keyboard ISR pushes
     * auto-repeats so currentKey stays non-zero). On Amiga this also
     * gave the stadium "VS" screen its visible duration. On SDL the
     * actual VS-duration wait happens just upstream in
     * InitGameSaveTeams' "wait for fire release" loop (which is
     * properly pumped via WaitForVSync there), so by the time we
     * reach this drain the user has already released and the buffer
     * is empty. Drain anyway to match Amiga semantics. */
    do {
        GetKey();
    } while (currentKey != 0);

    SaveHighlightsPointers();
    goto sync_to_vblank;

frame_loop:
    ReadTimerDelta();
    frameCount += 1;
    ProcessSpriteBlitQueue();
    GetKey();
    MainKeysCheck(currentKey);
    /* nullsub_10, nullsub_9 — no-ops */

    if (playHighlightFlag == 0) {
        FadeOutToBlack();
        return;
    }

    if (goIntoHighlightsLoop != 0) {
        PlayHighlightsLoop();
        playHighlightFlag = 0;
        /* bra.w *+4 → falls through to sync */
    }

sync_to_vblank:
    waitVBlankSync(0xCFD1F96Bu, 0x168Cu);
    goto frame_loop;
}

/* ====================================================================
 * PlayInstantReplayLoop — instant replay playback
 * ==================================================================== */
void __attribute__((used, externally_visible))
PlayInstantReplayLoop(void)
{
    ReadTimerDelta();

    for (;;) {
        /* Paused poll loop */
        while (paused != 0) {
            pauseSpinWaitOneTick();
            GetKey();
            if (fadeState == 0)
                break;
            MainKeysCheck(currentKey);
            PlayEnqueuedSamples();
            /* nullsub_5, nullsub_4 — no-ops */
        }

        /* Input and update block */
        ProcessSpriteBlitQueue();
        GetKey();

        if (fadeState != 0) {
            MainKeysCheck(currentKey);
            /* nullsub_10, nullsub_9 — no-ops */
        }

        /* Check replay continue */
        if (replayState == 0)
            return;

        PlayEnqueuedSamples();

        /* Fire-to-abort check */
        if (fadeState != 0) {
            if (userRequestedReplay != 0) {
                /* Multiplayer: check both joysticks */
                Player1StatusProc();
                if (pl1Fire != 0)
                    goto abort_replay;
                Player2StatusProc();
                if (pl2Fire != 0)
                    goto abort_replay;
            } else {
                if (teamScoredDataPtr != 0) {
                    uint16_t playerNum = (uint16_t)teamScoredDataPtr->playerNumber;
                    uint16_t coachNum  = (uint16_t)teamScoredDataPtr->playerCoachNumber;
                    if (playerNum == 1) {
                        Player1StatusProc();
                        if (pl1Fire != 0) goto abort_replay;
                    } else if (playerNum == 2) {
                        Player2StatusProc();
                        if (pl2Fire != 0) goto abort_replay;
                    } else if (coachNum == 1) {
                        Player1StatusProc();
                        if (pl1Fire != 0) goto abort_replay;
                    } else if (coachNum == 2) {
                        Player2StatusProc();
                        if (pl2Fire != 0) goto abort_replay;
                    } else {
                        /* Fall through to multiplayer check */
                        Player1StatusProc();
                        if (pl1Fire != 0) goto abort_replay;
                        Player2StatusProc();
                        if (pl2Fire != 0) goto abort_replay;
                    }
                }
            }
        }

        /* Draw replay frame and handle fade */
        DrawReplayFrame();
        if (fadeState == 0 && fadeInitFlag == 0x3E7)
            fadeInitFlag = 0xFFFF;

        if (replayState == 0) {
            /* Fade out and cleanup */
            FadeOutToBlack();
            RestoreCameraPosition();
            return;
        }

        /* Handle post-frame flags */
        if (fadeAndSaveHighlightScene != 0) {
            FadeAndSaveHighlightScene();
            fadeAndSaveHighlightScene = 0;
        }
        if (instantReplayFlag != 0) {
            HandleInstantReplayStateSwitch();
            instantReplayFlag = 0;
        }

        /* VBlank sync and loop back */
        waitVBlankSync(0xCFD1F959u, 0x16BEu);
        ReadTimerDelta();
        continue;

    abort_replay:
        fireBlocked = 1;
        replayState = 0;
        FadeOutToBlack();
        RestoreCameraPosition();
        return;
    }
}

/* ====================================================================
 * PlayHighlightsLoop — multi-scene highlight playback
 * ==================================================================== */
void __attribute__((used, externally_visible))
PlayHighlightsLoop(void)
{
    ReadTimerDelta();

    for (;;) {
        /* Paused poll loop */
        while (paused != 0) {
            pauseSpinWaitOneTick();
            GetKey();
            MainKeysCheck(currentKey);
            PlayEnqueuedSamples();
        }

        /* Input and update block */
        ProcessSpriteBlitQueue();
        GetKey();
        MainKeysCheck(currentKey);
        /* nullsub_10, nullsub_9 — no-ops */

        if (replayState == 0)
            goto advance_to_next_highlight;

        PlayEnqueuedSamples();

        /* Fire-to-abort check */
        if (fadeState != 0) {
            Player1StatusProc();
            if (pl1Fire != 0)
                goto abort_replay_on_fire;
            Player2StatusProc();
            if (pl2Fire != 0)
                goto abort_replay_on_fire;
        }

        /* Draw replay and handle fade */
        DrawReplayFrame();
        if (fadeState == 0 && fadeInitFlag == 0x3E7)
            fadeInitFlag = 0xFFFF;

        if (replayState == 0) {
            FadeOutToBlack();
            RestoreCameraPosition();
            goto advance_to_next_highlight;
        }

        /* VBlank sync */
        waitVBlankSync(0xCFD1F917u, 0x16F0u);
        ReadTimerDelta();
        continue;

    abort_replay_on_fire:
        fireBlocked = 1;
        replayState = 0;
        FadeOutToBlack();
        RestoreCameraPosition();
        goto stop_replay_and_return;
    }

advance_to_next_highlight:
    hilSceneNumber += 1;
    if (numScenesInHilFile == hilSceneNumber)
        goto stop_replay_and_return;

    {
        uint32_t offset = (uint32_t)hilSceneNumber * 19000u;
        /* `hilStartOfGoalsBuffer + offset` is a pointer (8 bytes on 64-bit).
         * Storing into uint32_t truncated the upper 32 bits, then the
         * `(uint8_t *)baseAddr` re-cast produced a half-pointer that
         * SIGSEGVed in HandleInstantReplayStateSwitch's `*p & 0x80`.
         * Carry the value as a real pointer instead. */
        uint8_t *baseAddr = hilStartOfGoalsBuffer + offset;
        goalBasePtr = baseAddr;
        currentHilPtr = baseAddr - 4;
        nextGoalPtr = baseAddr + 19000u;
    }
    HandleInstantReplayStateSwitch();

    /* VBlank sync and restart loop */
    waitVBlankSync(0xCFD1F917u, 0x16F0u);
    ReadTimerDelta();
    PlayHighlightsLoop();
    return;

stop_replay_and_return:
    RestoreHighlightPointers();
    return;
}
