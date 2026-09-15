/*
 * sim_state.c -- the match state registry. See include/sim_state.h.
 *
 * Every entry is backed by evidence (docs/ROLLBACK.md), and so is every exclusion:
 *
 *   - the rollback rig's leave-one-out runs over the corpus (--rollback=discover):
 *     leaving the variable out of a rollback changed the game;
 *   - or the variable is written by code reachable from GameLoop that the corpus
 *     never reaches (penalties, cards, injuries, substitutions, instant replays),
 *     and read by the match -- found from the executable, not from the recordings;
 *   - or it is state the random generator's call pattern depends on. The crowd
 *     chants, advertising boards and crowd animation call Rand() in both original
 *     releases, so their state decides what the NEXT gameplay Rand() returns.
 *
 * Deliberately NOT here:
 *
 *   - Per-match constants set before kick-off (ApplyGameStyle, InitGameRestoreTeams:
 *     the gs* timings, ball physics constants, pitch factors, card chance). Equal on
 *     both peers by the match setup, never changed by play.
 *   - Variables no instruction in the executable reads and no data relocation points
 *     at: _ballSpeed, _ballSpeedAfterSave, _cameraSnapToPlayerCount,
 *     _disallowedTurnFlagsCounter, _turnDirCopiedFromCameraCount, _halfNumber,
 *     _timeVar, _strikeDestY/Z, _lastCameraDirection, _lastPlayerTurnFlags,
 *     _goalkeeperDiveAttemptCount, _nearestTeammatePtr, _findClosestBallDist,
 *     _teamNumThatScored, _substitutedPlX/Y, _pendingSampleId3_4 and the dead* /
 *     *WriteOnly* variables of the disassembly.
 *   - Values recomputed from scratch within the frame that reads them (the
 *     prevLastPlayer/prevLastTeamPlayed and freeKickDestX_local statics of
 *     update_players.c).
 *   - Render state describing what is already drawn: scroll offsets and their
 *     Saved0/Saved1 copies, frontbuffers, render stacks, palette, copper list, and
 *     the HUD layer. And audio mixer state. (The draw list is NOT render state: a
 *     render pass sets each listed sprite's onScreen, which the match reads.)
 *   - Raw joystick/keyboard input: netplay supplies it every frame.
 *
 * The tiers: SIM may not depend on VIEW, or a peer resynced with SIM alone would
 * checksum differently. `scripts/rollback_test.sh --restore sim` proves it.
 *
 * _s_pitchFullPtrs (pitch.c) mirrors the fan tile slots of _pitchDatBuffer at full
 * pointer width for the renderer; SimState_Load rebuilds it from the slots.
 */
#include "sim_state.h"
#include "game_types.h"
#include "animated_patterns.h"
#include "sprites.h"

#include <string.h>

/* ---- random generators ---- */
extern uint8_t _seed, _randXorIndex, _randXorKey;
extern uint8_t _seed2, _randXorIndex2, _randXorKey2;

/* ---- clock ---- */
extern volatile uint16_t _currentGameTick;
extern volatile uint16_t _currentTick;
extern uint16_t          _lastGameTick;
extern uint16_t          _lastFrameTicks[1];
extern uint16_t          _frameCount[1], _inGameCounter[1];
extern uint16_t          _gameSeconds[1], _secondsSwitchAccumulator[1];
extern uint8_t           _gameTime[4];
extern uint16_t          _playGame[1];

/* ---- match flow ---- */
extern uint16_t _gameState, _gameStatePl;
extern uint16_t _breakState[1], _breakCameraMode[1];
extern uint16_t _extraTimeState[1], _penaltiesState[1];
extern uint16_t _fireBlocked, _statsTimer, _resultTimer;
extern uint16_t _endGameCounter[1], _goalOut[1], _isGoalAttempt[1];
extern uint16_t _stoppageEventTimer[1], _stoppageTimerActive[1], _stoppageTimerTotal[1];
extern uint16_t _AI_maxStoppageTime[1];
extern uint16_t _teamPlayingUp[1], _teamStarting[1];
extern uint16_t _teamSwitchCounter;
extern uint16_t _playRefereeWhistle[1], _endOfHalfWriteOnly[1];
extern uint16_t _fadeState[1], _fadeInitFlag[1];
extern uint16_t _foulXCoordinate[1], _foulYCoordinate[1], _strikeDestX[1];
extern uint16_t _throwInPassOrKick[1], _waitForPlayerToGoInTimer[1];
extern uint16_t _nobodysBallTimer[1], _nobodysBallLastFrame[1], _ballOutOfGameTimer[1];
extern uint16_t _runSlower[1], _hideBall[1], _goalScored[1];
extern uint16_t _showStats[1];
extern int8_t   _statsNumber[6];
extern uint16_t _statsTeam1Goals, _statsTeam2Goals;
extern uint16_t _statsTeam1GoalsCopy[1], _statsTeam2GoalsCopy[1];
extern uint16_t _statsTeam1GoalsCopy2[1], _statsTeam2GoalsCopy2[1];

/* ---- camera ---- */
extern FixedPoint _cameraX, _cameraY;
extern uint16_t   _cameraXVelocity, _cameraYVelocity;
extern uint16_t   _cameraDirection[1], _cameraCoordinatesValid[1];
extern uint16_t   _cameraLeavingSubsTimer[1], _goalCameraMode[1];

/* ---- ball and player bookkeeping ---- */
extern uint16_t _ballQuadrantIndex[1], _playerXQuadrantOffset[1], _playerYQuadrantOffset[1];
extern uint16_t _ballDefensiveX[1], _ballDefensiveY[1], _ballDefensiveZ[1];
extern uint16_t _ballInGoalkeeperArea[1], _ballInLowerPenaltyArea[1], _ballInUpperPenaltyArea[1];
extern uint16_t _ballNextGroundX[1], _ballNextX[1], _ballNextY[1], _ballNextYGroundY[1];
extern uint16_t _ballNotHighX[1], _ballNotHighY[1], _ballNotHighZ[1];
extern uint16_t _playerHadBall[1];
extern uint8_t  _playerTurnFlags;
extern Sprite          *_lastPlayerPlayed, *_lastPlayerBeforeGoalkeeper, *_lastKeeperPlayed;
extern Sprite          *_lastPlayerScored;
extern TeamGeneralInfo *_lastTeamPlayed, *_lastTeamPlayedBeforeBreak, *_lastTeamScored;
extern TeamGeneralInfo *_teamScoredDataPtr;
extern TeamGame        *_teamScoredGamePtr;
extern uintptr_t        _currentScorer[1], _winningTeamPtr[1];
extern uint16_t         _lastTeamScoredNumber[1];

/* ---- AI ---- */
extern uint16_t _aiCounter[1], _aiResumePlayTimer[1], _aiTurnDirection[1];
extern uint16_t _aiRand[1], _aiAttackHalf[1], _totalSkillsFactorSum[1];

/* ---- teams ---- */
extern TeamGame        _topTeamInGame, _bottomTeamInGame;
extern TeamGeneralInfo _topTeamData, _bottomTeamData;
extern uint16_t        _team1StatsData[7], _team2StatsData[7];
extern uint16_t        _pl1Tactics[1], _pl2Tactics[1];

/* ---- sprites the match moves ---- */
extern Sprite _ballShadowSprite, _ballSprite, _goal1TopSprite, _goal2BottomSprite;
extern Sprite _goalie1Sprite, _goalie2Sprite;
extern Sprite _team1Player2Sprite,  _team1Player3Sprite,  _team1Player4Sprite;
extern Sprite _team1Player5Sprite,  _team1Player6Sprite,  _team1Player7Sprite;
extern Sprite _team1Player8Sprite,  _team1Player9Sprite,  _team1Player10Sprite;
extern Sprite _team1Player11Sprite;
extern Sprite _team2Player2Sprite,  _team2Player3Sprite,  _team2Player4Sprite;
extern Sprite _team2Player5Sprite,  _team2Player6Sprite,  _team2Player7Sprite;
extern Sprite _team2Player8Sprite,  _team2Player9Sprite,  _team2Player10Sprite;
extern Sprite _team2Player11Sprite;
extern Sprite _team1CurPlayerNumSprite, _team2CurPlayerNumSprite;
extern Sprite _playerMarkSprite, _bookedPlayerCardOrNumberSprite;
extern Sprite _refereeSprite, _cornerFlagSprite, _currentPlayerNameSprite;

/* ---- referee and cards ---- */
extern uint16_t _refState[1], _refTimer[1], _whichCard[1];
extern Sprite  *_bookedPlayer;

/* ---- controls: what the game derived from the inputs, including last frame's ---- */
extern uint16_t _pl1Direction[1], _pl2Direction[1];
extern uint8_t  _pl1Fire, _pl2Fire, _pl1ShortFire, _pl2ShortFire;
extern uint8_t  _pl1Up, _pl1Down, _pl1Left, _pl1Right, _pl2Up, _pl2Down, _pl2Left, _pl2Right;
extern uint8_t  _pl1UpRight, _pl1UpLeft, _pl1DownLeft, _pl1DownRight;
extern uint8_t  _pl2UpRight, _pl2UpLeft, _pl2DownLeft, _pl2DownRight;
extern uint8_t  _pl1WasUp, _pl1WasDown, _pl1WasLeft, _pl1WasRight;
extern uint8_t  _pl2WasUp, _pl2WasDown, _pl2WasLeft, _pl2WasRight;
extern uint8_t  _pl1UpPress, _pl1DownPress, _pl1LeftPress, _pl1RightPress;
extern uint8_t  _pl2UpPress, _pl2DownPress, _pl2LeftPress, _pl2RightPress;
extern uint16_t _pl1FireCounter[1], _pl2FireCounter[1];
extern uint8_t  _pl1LastFired, _pl2LastFired, _subsFire;

/* ---- substitutes bench ---- */
extern uint16_t _bench1Called, _bench2Called;
extern uint16_t _subsPl1TapCount, _subsPl2TapCount;
extern uint16_t _subsPrevPl1Direction, _subsPrevPl2Direction;
extern uint16_t _pl1BenchBlockWhileHoldingDirection, _pl2BenchBlockWhileHoldingDirection;
extern uint16_t _subsDirection, _subsState, _goToSubsTimer;
extern uint16_t _subsBlockDirections, _subsFireTimer, _subsBlockFire;
extern uint16_t _subsPrevDirection, _subsMovementDelayTimer;
extern uint16_t _subsTrainingTopTeam, _subsTeamsSwapped, _benchPickTeamCounter;
extern uint16_t _plToEnterGameIndex, _plToBeSubstitutedPos, _plToBeSubstitutedOrd;
extern uint16_t _selectedFormationEntry, _benchPlayerIndex;
extern uint16_t _inSubstitutesMenu[1], _leavingSubsMenu[1], _substituteInProgress[1];
extern uint16_t _substitutedPlDestX[1], _substitutedPlDestY[1];
extern uint16_t _pl1BenchTimeoutCounter[1], _pl2BenchTimeoutCounter[1];
extern uint16_t _benchColors[9], _benchOff[1];
extern uintptr_t _benchTeam[1], _benchTeamGame[1];
extern uint16_t _benchXLimit[1], _benchY[1], _opponentBenchY[1];
extern uint16_t _formationMenuX, _subsMenuX[1];
extern uint16_t _subsMenuBackgroundColor[1], _subsMenuEntryFrameColor[1], _subsMenuHighlightColor[1];
extern uint16_t _plComingX[1], _plComingY[1], _plComingYTmp[1], _plLeavingX[1];
extern uint8_t  _plNamesLengthTableCopy[32], _plOrdinalsTable[32];
extern uintptr_t _plPositions[1], _teamThatSubstitutes[1];
extern uint16_t _plSubstitutedX[1], _plSubstitutedY[1];
extern Sprite  *_substitutedPlSprite;
extern uint16_t _initialSubsState[1], _selectedPlayerInSubs[1], _showingUserStats[1];
extern uint16_t _team1NumSubs[1], _team2NumSubs[1];
extern uint16_t _team1NumAllowedInjuries[1], _team2NumAllowedInjuries[1];
extern uint16_t _team1GoalkeeperReplaced[1], _team2GoalkeeperReplaced[1];
extern uint16_t _penalty[1], _currentKey[1];

/* ---- penalty shoot-out ---- */
extern uint16_t _penaltiesTimer[1], _playingPenalties[1];
extern Sprite  *_penaltyShooterSprite;
extern uint16_t _team1PenaltyAttempts[1], _team1PenaltyGoals[1], _team1PenaltyShooterIndex[1];
extern uint16_t _team2PenaltyAttempts[1], _team2PenaltyGoals[1], _team2PenaltyShooterIndex[1];
extern uint16_t _savedTeam1Goals[1], _savedTeam2Goals[1], _dontShowScorers[1];
extern uint16_t _team1GoalsDigit1[1], _team1GoalsDigit2[1], _team2GoalsDigit1[1], _team2GoalsDigit2[1];

/* ---- instant replay: GameLoop leaves the match loop on these ---- */
extern uint16_t _instantReplayFlag[1], _saveHighlightScene[1], _fadeAndSaveHighlightScene[1];
extern uint16_t _userRequestedReplay, _replayState;
extern uint16_t _spaceReplayTimer[1], _gameCanceled[1], _paused[1], _playHighlightFlag[1];

/* ---- crowd chants (Rand) ---- */
extern uint16_t _nextChantTimer[1], _fadeInChantTimer[1], _fadeOutChantTimer[1];
extern uint16_t _crowdChantVolume[1], _currentCrowdSample[1], _crowdEventFlags[1];
extern uint16_t _sfxQueueCount[1], _sfxQueueWriteIdx[1], _goalCounter[1];
extern uint16_t _lastKickSampleTick[1], _crowdChantsOn[1];

/* ---- advertising boards (Rand) ---- */
extern uint16_t _advertSlideTimer[1], _advertIndex[1], _currentAnimatedAdLine[1];
extern Sprite   _advertInGameSprites[8];
extern uint8_t *_advert1Pointers[8], *_advert2Pointers[8], *_advert3Pointers[8];
extern uint16_t _spinBigS[1], _bigSIndex[1];
extern Sprite   _big_S_Sprite;

/* ---- crowd animation (Rand) ---- */
extern uint16_t _animPatternsIndex[1], _animPatternsState[1];
extern uint8_t  _animPatternsRandomTable[64];
extern uint16_t _animatedPatterns[8];
extern uint16_t _otherTeamScored[1], _patternStatic[1], _patternsGoalCounter[1], _showFansCounter[1];
extern uint8_t  _pitchDatBuffer[];

/* ---- the draw list: the simulation reads what the renderer derives from it ---- */
extern Sprite  *_sortedSprites[78];
extern uint16_t _numSpritesToRender;

/* ---- VIEW: highlights ---- */
extern char      _goalBase[0x4A38];
extern uint32_t *_currentHilPtr, *_currentGoalPtr;
extern uintptr_t _goalBasePtr[1], _nextGoalPtr[1];
extern uint16_t  _numScenesInHilFile[1], _hilSceneNumber[1];

/* ---- result, scorers, clock: sprites in the draw list, and what builds them ---- */
extern ScorerInfo _team1Scorers[8], _team2Scorers[8];
extern uint16_t   _team1TotalGoals[1], _team2TotalGoals[1];
extern Sprite     _team1ScorerSprite[8], _team2ScorerSprite[8];
extern uint16_t   _team1ScorerSpritesWidths[8], _team2ScorerSpritesWidths[8];
extern Sprite     _team1GoalsDigit1Sprite, _team1GoalsDigit2Sprite;
extern Sprite     _team2GoalsDigit1Sprite, _team2GoalsDigit2Sprite;
extern Sprite     _team1TotalGoalsDigit1Sprite, _team1TotalGoalsDigit2Sprite;
extern Sprite     _team2TotalGoalsDigit1Sprite, _team2TotalGoalsDigit2Sprite;
extern Sprite     _team1NameSprite, _team2NameSprite, _dashSprite;
extern Sprite     _gridUpperRowLeftSprite, _gridUpperRowRightSprite, _gridUpperRowExtraSprite;
extern Sprite     _gridMiddleRowLeftSprite, _gridMiddleRowRightSprite, _gridMiddleRowExtraSprite;
extern Sprite     _gridBottomRowLeftSprite, _gridBottomRowRightSprite, _gridBottomRowExtraSprite;
extern Sprite     _resultAfterTheGameSprite, _resultOnHalftimeSprite;
extern Sprite     _currentTimeSprite, _cornerFlagSprite2;
extern uint16_t   _copyTimeSprite[1];
extern uint16_t   _overlayFlagBackup[1], _currentPlayerIndicatorPosition[1];

/* The crowd rows of the pitch tile map: DrawAnimatedPatterns picks fan tiles only
 * from pitch rows 1-3 and 51-53 (sprite indices 21..83 and 1071..1133, 42 columns
 * per row), including the widescreen mirror, which stays within its row. */
#define FAN_ROWS_TOP_SLOT     42
#define FAN_ROWS_BOTTOM_SLOT  2142
#define FAN_ROWS_SLOTS        126

/* Pointer maps. Sprite and TeamGeneralInfo are the only structs here with pointer
 * fields (include/game_types.h); TeamGame, ScorerInfo and the stats have none. */
static const SimPointerMap k_pointer  = { sizeof(void *), sizeof(void *), 1, { 0 } };
static const SimPointerMap k_tileSlot = { 4, 4, 1, { 0 } };
static const SimPointerMap k_sprite   = { sizeof(Sprite), sizeof(void *), 2, {
    offsetof(Sprite, animationTable), offsetof(Sprite, frameIndicesTable) } };
static const SimPointerMap k_teamInfo = { sizeof(TeamGeneralInfo), sizeof(void *), 9, {
    offsetof(TeamGeneralInfo, opponentsTeam), offsetof(TeamGeneralInfo, inGameTeamPtr),
    offsetof(TeamGeneralInfo, teamStatsPtr), offsetof(TeamGeneralInfo, spritesTable),
    offsetof(TeamGeneralInfo, shotChanceTable), offsetof(TeamGeneralInfo, controlledPlayer),
    offsetof(TeamGeneralInfo, passToPlayerPtr), offsetof(TeamGeneralInfo, lastHeadingTacklingPlayer),
    offsetof(TeamGeneralInfo, passingKickingPlayer) } };

#define S  0
#define V  SIM_VIEW
#define REGION(sym, fl)          { #sym, (void *)&sym, (uint32_t)sizeof sym, (fl), NULL }
#define REGION_P(sym, fl, map)   { #sym, (void *)&sym, (uint32_t)sizeof sym, (fl), &map }
#define REGION_PART(sym, name, off, len, fl, map) \
                                 { name, (void *)((uint8_t *)sym + (off)), (uint32_t)(len), (fl) | SIM_PART, map }

const SimStateRegion g_simStateRegions[] = {
    /* random generators */
    REGION(_seed, S), REGION(_randXorIndex, S), REGION(_randXorKey, S),
    REGION(_seed2, S), REGION(_randXorIndex2, S), REGION(_randXorKey2, S),

    /* clock */
    REGION(_currentGameTick, S), REGION(_currentTick, S), REGION(_lastGameTick, S),
    REGION(_lastFrameTicks, S), REGION(_frameCount, S), REGION(_inGameCounter, S),
    REGION(_gameSeconds, S), REGION(_secondsSwitchAccumulator, S), REGION(_gameTime, S),
    REGION(_playGame, S),

    /* match flow */
    REGION(_gameState, S), REGION(_gameStatePl, S),
    REGION(_breakState, S), REGION(_breakCameraMode, S),
    REGION(_extraTimeState, S), REGION(_penaltiesState, S),
    REGION(_fireBlocked, S), REGION(_statsTimer, S), REGION(_resultTimer, S),
    REGION(_endGameCounter, S), REGION(_goalOut, S), REGION(_isGoalAttempt, S),
    REGION(_stoppageEventTimer, S), REGION(_stoppageTimerActive, S), REGION(_stoppageTimerTotal, S),
    REGION(_AI_maxStoppageTime, S),
    REGION(_teamPlayingUp, S), REGION(_teamStarting, S), REGION(_teamSwitchCounter, S),
    REGION(_playRefereeWhistle, S), REGION(_endOfHalfWriteOnly, S),
    REGION(_fadeState, S), REGION(_fadeInitFlag, S),
    REGION(_foulXCoordinate, S), REGION(_foulYCoordinate, S), REGION(_strikeDestX, S),
    REGION(_throwInPassOrKick, S), REGION(_waitForPlayerToGoInTimer, S),
    REGION(_nobodysBallTimer, S), REGION(_nobodysBallLastFrame, S), REGION(_ballOutOfGameTimer, S),
    REGION(_runSlower, S), REGION(_hideBall, S), REGION(_goalScored, S),
    REGION(_showStats, S), REGION(_statsNumber, S),
    REGION(_statsTeam1Goals, S), REGION(_statsTeam2Goals, S),
    REGION(_statsTeam1GoalsCopy, S), REGION(_statsTeam2GoalsCopy, S),
    REGION(_statsTeam1GoalsCopy2, S), REGION(_statsTeam2GoalsCopy2, S),

    /* camera */
    REGION(_cameraX, S), REGION(_cameraY, S), REGION(_cameraXVelocity, S), REGION(_cameraYVelocity, S),
    REGION(_cameraDirection, S), REGION(_cameraCoordinatesValid, S),
    REGION(_cameraLeavingSubsTimer, S), REGION(_goalCameraMode, S),

    /* ball and player bookkeeping */
    REGION(_ballQuadrantIndex, S), REGION(_playerXQuadrantOffset, S), REGION(_playerYQuadrantOffset, S),
    REGION(_ballDefensiveX, S), REGION(_ballDefensiveY, S), REGION(_ballDefensiveZ, S),
    REGION(_ballInGoalkeeperArea, S), REGION(_ballInLowerPenaltyArea, S), REGION(_ballInUpperPenaltyArea, S),
    REGION(_ballNextGroundX, S), REGION(_ballNextX, S), REGION(_ballNextY, S), REGION(_ballNextYGroundY, S),
    REGION(_ballNotHighX, S), REGION(_ballNotHighY, S), REGION(_ballNotHighZ, S),
    REGION(_playerHadBall, S), REGION(_playerTurnFlags, S),
    REGION_P(_lastPlayerPlayed, S, k_pointer), REGION_P(_lastPlayerBeforeGoalkeeper, S, k_pointer), REGION_P(_lastKeeperPlayed, S, k_pointer),
    REGION_P(_lastPlayerScored, S, k_pointer),
    REGION_P(_lastTeamPlayed, S, k_pointer), REGION_P(_lastTeamPlayedBeforeBreak, S, k_pointer), REGION_P(_lastTeamScored, S, k_pointer),
    REGION_P(_teamScoredDataPtr, S, k_pointer), REGION_P(_teamScoredGamePtr, S, k_pointer),
    REGION_P(_currentScorer, S, k_pointer), REGION_P(_winningTeamPtr, S, k_pointer), REGION(_lastTeamScoredNumber, S),

    /* AI */
    REGION(_aiCounter, S), REGION(_aiResumePlayTimer, S), REGION(_aiTurnDirection, S),
    REGION(_aiRand, S), REGION(_aiAttackHalf, S), REGION(_totalSkillsFactorSum, S),

    /* teams */
    REGION(_topTeamInGame, S), REGION(_bottomTeamInGame, S),
    REGION_P(_topTeamData, S, k_teamInfo), REGION_P(_bottomTeamData, S, k_teamInfo),
    REGION(_team1StatsData, S), REGION(_team2StatsData, S),
    REGION(_pl1Tactics, S), REGION(_pl2Tactics, S),

    /* sprites the match moves (Sprite holds animation table pointers) */
    REGION_P(_ballShadowSprite, S, k_sprite), REGION_P(_ballSprite, S, k_sprite),
    REGION_P(_goal1TopSprite, S, k_sprite), REGION_P(_goal2BottomSprite, S, k_sprite),
    REGION_P(_goalie1Sprite, S, k_sprite), REGION_P(_goalie2Sprite, S, k_sprite),
    REGION_P(_team1Player2Sprite, S, k_sprite), REGION_P(_team1Player3Sprite, S, k_sprite), REGION_P(_team1Player4Sprite, S, k_sprite),
    REGION_P(_team1Player5Sprite, S, k_sprite), REGION_P(_team1Player6Sprite, S, k_sprite), REGION_P(_team1Player7Sprite, S, k_sprite),
    REGION_P(_team1Player8Sprite, S, k_sprite), REGION_P(_team1Player9Sprite, S, k_sprite), REGION_P(_team1Player10Sprite, S, k_sprite),
    REGION_P(_team1Player11Sprite, S, k_sprite),
    REGION_P(_team2Player2Sprite, S, k_sprite), REGION_P(_team2Player3Sprite, S, k_sprite), REGION_P(_team2Player4Sprite, S, k_sprite),
    REGION_P(_team2Player5Sprite, S, k_sprite), REGION_P(_team2Player6Sprite, S, k_sprite), REGION_P(_team2Player7Sprite, S, k_sprite),
    REGION_P(_team2Player8Sprite, S, k_sprite), REGION_P(_team2Player9Sprite, S, k_sprite), REGION_P(_team2Player10Sprite, S, k_sprite),
    REGION_P(_team2Player11Sprite, S, k_sprite),
    REGION_P(_team1CurPlayerNumSprite, S, k_sprite), REGION_P(_team2CurPlayerNumSprite, S, k_sprite),
    REGION_P(_playerMarkSprite, S, k_sprite), REGION_P(_bookedPlayerCardOrNumberSprite, S, k_sprite),
    REGION_P(_refereeSprite, S, k_sprite), REGION_P(_cornerFlagSprite, S, k_sprite), REGION_P(_currentPlayerNameSprite, S, k_sprite),

    /* referee and cards */
    REGION(_refState, S), REGION(_refTimer, S), REGION(_whichCard, S), REGION_P(_bookedPlayer, S, k_pointer),

    /* controls */
    REGION(_pl1Direction, S), REGION(_pl2Direction, S),
    REGION(_pl1Fire, S), REGION(_pl2Fire, S), REGION(_pl1ShortFire, S), REGION(_pl2ShortFire, S),
    REGION(_pl1Up, S), REGION(_pl1Down, S), REGION(_pl1Left, S), REGION(_pl1Right, S),
    REGION(_pl2Up, S), REGION(_pl2Down, S), REGION(_pl2Left, S), REGION(_pl2Right, S),
    REGION(_pl1UpRight, S), REGION(_pl1UpLeft, S), REGION(_pl1DownLeft, S), REGION(_pl1DownRight, S),
    REGION(_pl2UpRight, S), REGION(_pl2UpLeft, S), REGION(_pl2DownLeft, S), REGION(_pl2DownRight, S),
    REGION(_pl1WasUp, S), REGION(_pl1WasDown, S), REGION(_pl1WasLeft, S), REGION(_pl1WasRight, S),
    REGION(_pl2WasUp, S), REGION(_pl2WasDown, S), REGION(_pl2WasLeft, S), REGION(_pl2WasRight, S),
    REGION(_pl1UpPress, S), REGION(_pl1DownPress, S), REGION(_pl1LeftPress, S), REGION(_pl1RightPress, S),
    REGION(_pl2UpPress, S), REGION(_pl2DownPress, S), REGION(_pl2LeftPress, S), REGION(_pl2RightPress, S),
    REGION(_pl1FireCounter, S), REGION(_pl2FireCounter, S),
    REGION(_pl1LastFired, S), REGION(_pl2LastFired, S), REGION(_subsFire, S),

    /* substitutes bench */
    REGION(_bench1Called, S), REGION(_bench2Called, S),
    REGION(_subsPl1TapCount, S), REGION(_subsPl2TapCount, S),
    REGION(_subsPrevPl1Direction, S), REGION(_subsPrevPl2Direction, S),
    REGION(_pl1BenchBlockWhileHoldingDirection, S), REGION(_pl2BenchBlockWhileHoldingDirection, S),
    REGION(_subsDirection, S), REGION(_subsState, S), REGION(_goToSubsTimer, S),
    REGION(_subsBlockDirections, S), REGION(_subsFireTimer, S), REGION(_subsBlockFire, S),
    REGION(_subsPrevDirection, S), REGION(_subsMovementDelayTimer, S),
    REGION(_subsTrainingTopTeam, S), REGION(_subsTeamsSwapped, S), REGION(_benchPickTeamCounter, S),
    REGION(_plToEnterGameIndex, S), REGION(_plToBeSubstitutedPos, S), REGION(_plToBeSubstitutedOrd, S),
    REGION(_selectedFormationEntry, S), REGION(_benchPlayerIndex, S),
    REGION(_inSubstitutesMenu, S), REGION(_leavingSubsMenu, S), REGION(_substituteInProgress, S),
    REGION(_substitutedPlDestX, S), REGION(_substitutedPlDestY, S),
    REGION(_pl1BenchTimeoutCounter, S), REGION(_pl2BenchTimeoutCounter, S),
    REGION(_benchColors, S), REGION(_benchOff, S),
    REGION_P(_benchTeam, S, k_pointer), REGION_P(_benchTeamGame, S, k_pointer),
    REGION(_benchXLimit, S), REGION(_benchY, S), REGION(_opponentBenchY, S),
    REGION(_formationMenuX, S), REGION(_subsMenuX, S),
    REGION(_subsMenuBackgroundColor, S), REGION(_subsMenuEntryFrameColor, S),
    REGION(_subsMenuHighlightColor, S),
    REGION(_plComingX, S), REGION(_plComingY, S), REGION(_plComingYTmp, S), REGION(_plLeavingX, S),
    REGION(_plNamesLengthTableCopy, S), REGION(_plOrdinalsTable, S),
    REGION_P(_plPositions, S, k_pointer), REGION_P(_teamThatSubstitutes, S, k_pointer),
    REGION(_plSubstitutedX, S), REGION(_plSubstitutedY, S), REGION_P(_substitutedPlSprite, S, k_pointer),
    REGION(_initialSubsState, S), REGION(_selectedPlayerInSubs, S), REGION(_showingUserStats, S),
    REGION(_team1NumSubs, S), REGION(_team2NumSubs, S),
    REGION(_team1NumAllowedInjuries, S), REGION(_team2NumAllowedInjuries, S),
    REGION(_team1GoalkeeperReplaced, S), REGION(_team2GoalkeeperReplaced, S),
    REGION(_penalty, S), REGION(_currentKey, S),

    /* penalty shoot-out */
    REGION(_penaltiesTimer, S), REGION(_playingPenalties, S), REGION_P(_penaltyShooterSprite, S, k_pointer),
    REGION(_team1PenaltyAttempts, S), REGION(_team1PenaltyGoals, S), REGION(_team1PenaltyShooterIndex, S),
    REGION(_team2PenaltyAttempts, S), REGION(_team2PenaltyGoals, S), REGION(_team2PenaltyShooterIndex, S),
    REGION(_savedTeam1Goals, S), REGION(_savedTeam2Goals, S), REGION(_dontShowScorers, S),
    REGION(_team1GoalsDigit1, S), REGION(_team1GoalsDigit2, S),
    REGION(_team2GoalsDigit1, S), REGION(_team2GoalsDigit2, S),

    /* instant replay */
    REGION(_instantReplayFlag, S), REGION(_saveHighlightScene, S), REGION(_fadeAndSaveHighlightScene, S),
    REGION(_userRequestedReplay, S), REGION(_replayState, S),
    REGION(_spaceReplayTimer, S), REGION(_gameCanceled, S), REGION(_paused, S),
    REGION(_playHighlightFlag, S),

    /* crowd chants */
    REGION(_nextChantTimer, S), REGION(_fadeInChantTimer, S), REGION(_fadeOutChantTimer, S),
    REGION(_crowdChantVolume, S), REGION(_currentCrowdSample, S), REGION(_crowdEventFlags, S),
    REGION(_sfxQueueCount, S), REGION(_sfxQueueWriteIdx, S), REGION(_goalCounter, S),
    REGION(_lastKickSampleTick, S), REGION(_crowdChantsOn, S),

    /* advertising boards */
    REGION(_advertSlideTimer, S), REGION(_advertIndex, S), REGION(_currentAnimatedAdLine, S),
    REGION_P(_advertInGameSprites, S, k_sprite),
    REGION_P(_advert1Pointers, S, k_pointer), REGION_P(_advert2Pointers, S, k_pointer), REGION_P(_advert3Pointers, S, k_pointer),
    REGION(_spinBigS, S), REGION(_bigSIndex, S), REGION_P(_big_S_Sprite, S, k_sprite),

    /* crowd animation */
    REGION(_animPatternsIndex, S), REGION(_animPatternsState, S),
    REGION(_animPatternsRandomTable, S), REGION(_animatedPatterns, S),
    REGION(_otherTeamScored, S), REGION(_patternStatic, S),
    REGION(_patternsGoalCounter, S), REGION(_showFansCounter, S),
    REGION_PART(_pitchDatBuffer, "_pitchDatBuffer:top_fans",
                FAN_ROWS_TOP_SLOT * 4, FAN_ROWS_SLOTS * 4, S, &k_tileSlot),
    REGION_PART(_pitchDatBuffer, "_pitchDatBuffer:bottom_fans",
                FAN_ROWS_BOTTOM_SLOT * 4, FAN_ROWS_SLOTS * 4, S, &k_tileSlot),

    /* The draw list. BuildSpriteBandAndGeneralRenderStacks sets each listed sprite's
     * onScreen, which the match reads (player auto-select, referee, camera, animation
     * gate) -- so which sprites the list holds is simulation state, and the list is not
     * rebuilt every frame. Its order is history too: the depth sort starts from last
     * frame's order, and the highlight recorder writes sprites in that order. A restore
     * of the VIEW tier alone found it: the list stayed stale, an advertising board's
     * onScreen came out different, and the match diverged frames later. */
    REGION_P(_sortedSprites, S, k_pointer), REGION(_numSpritesToRender, S),

    /* Built with the draw list and read by the animation gate one frame later. Empty
     * unless the render is wider than the simulation view -- and always empty in the
     * replay rig, which pins animation to the simulation view, so the corpus cannot
     * exercise it; it is here on the code, like the penalty and card state. */
    REGION_P(g_spriteAnimVisible, S, k_pointer), REGION(g_spriteAnimVisibleCount, S),

    /* VIEW: highlights -- the replay ring and where the next frame goes in it */
    REGION(_goalBase, V), REGION_P(_currentHilPtr, V, k_pointer), REGION_P(_currentGoalPtr, V, k_pointer),
    REGION_P(_goalBasePtr, V, k_pointer), REGION_P(_nextGoalPtr, V, k_pointer),
    REGION(_numScenesInHilFile, V), REGION(_hilSceneNumber, V),

    /* result, scorers, clock. In the VIEW tier at first; a restore of the SIM tier alone
     * showed they are not: every one of these sprites is in _allSpritesArray, so its
     * `visible` and `y` decide the draw list's contents and order, and the scorer data
     * decides which scorer sprites are visible. SIM may not depend on VIEW. */
    REGION(_team1Scorers, S), REGION(_team2Scorers, S),
    REGION(_team1TotalGoals, S), REGION(_team2TotalGoals, S),
    REGION_P(_team1ScorerSprite, S, k_sprite), REGION_P(_team2ScorerSprite, S, k_sprite),
    REGION(_team1ScorerSpritesWidths, S), REGION(_team2ScorerSpritesWidths, S),
    REGION_P(_team1GoalsDigit1Sprite, S, k_sprite), REGION_P(_team1GoalsDigit2Sprite, S, k_sprite),
    REGION_P(_team2GoalsDigit1Sprite, S, k_sprite), REGION_P(_team2GoalsDigit2Sprite, S, k_sprite),
    REGION_P(_team1TotalGoalsDigit1Sprite, S, k_sprite), REGION_P(_team1TotalGoalsDigit2Sprite, S, k_sprite),
    REGION_P(_team2TotalGoalsDigit1Sprite, S, k_sprite), REGION_P(_team2TotalGoalsDigit2Sprite, S, k_sprite),
    REGION_P(_team1NameSprite, S, k_sprite), REGION_P(_team2NameSprite, S, k_sprite), REGION_P(_dashSprite, S, k_sprite),
    REGION_P(_gridUpperRowLeftSprite, S, k_sprite), REGION_P(_gridUpperRowRightSprite, S, k_sprite),
    REGION_P(_gridUpperRowExtraSprite, S, k_sprite),
    REGION_P(_gridMiddleRowLeftSprite, S, k_sprite), REGION_P(_gridMiddleRowRightSprite, S, k_sprite),
    REGION_P(_gridMiddleRowExtraSprite, S, k_sprite),
    REGION_P(_gridBottomRowLeftSprite, S, k_sprite), REGION_P(_gridBottomRowRightSprite, S, k_sprite),
    REGION_P(_gridBottomRowExtraSprite, S, k_sprite),
    REGION_P(_resultAfterTheGameSprite, S, k_sprite), REGION_P(_resultOnHalftimeSprite, S, k_sprite),
    REGION_P(_currentTimeSprite, S, k_sprite), REGION_P(_cornerFlagSprite2, S, k_sprite),
    REGION(_copyTimeSprite, S),
    REGION(_overlayFlagBackup, S), REGION(_currentPlayerIndicatorPosition, S),
};

#undef S
#undef V
#undef REGION
#undef REGION_P
#undef REGION_PART

const int32_t g_simStateRegionCount = (int32_t)(sizeof g_simStateRegions / sizeof g_simStateRegions[0]);

#define MAX_REGIONS 512
static uint8_t s_disabled[MAX_REGIONS];

_Static_assert(sizeof g_simStateRegions / sizeof g_simStateRegions[0] <= MAX_REGIONS,
               "raise MAX_REGIONS");

int SimState_RegionActive(int32_t i, uint32_t tiers)
{
    return !s_disabled[i] && (SIM_REGION_TIER(&g_simStateRegions[i]) & tiers) != 0;
}

uint32_t SimState_Size(uint32_t tiers)
{
    uint32_t total = 0;
    for (int32_t i = 0; i < g_simStateRegionCount; i++)
        if (SimState_RegionActive(i, tiers))
            total += g_simStateRegions[i].size;
    return total;
}

void SimState_Save(uint8_t *dst, uint32_t tiers)
{
    for (int32_t i = 0; i < g_simStateRegionCount; i++) {
        if (!SimState_RegionActive(i, tiers)) continue;
        memcpy(dst, g_simStateRegions[i].addr, g_simStateRegions[i].size);
        dst += g_simStateRegions[i].size;
    }
}

void SimState_Load(const uint8_t *src, uint32_t tiers)
{
    for (int32_t i = 0; i < g_simStateRegionCount; i++) {
        if (!SimState_RegionActive(i, tiers)) continue;
        memcpy(g_simStateRegions[i].addr, src, g_simStateRegions[i].size);
        src += g_simStateRegions[i].size;
    }
    if (tiers & SIM_TIER_SIM) {
        AnimatedPatterns_SyncTileMirror(FAN_ROWS_TOP_SLOT, FAN_ROWS_SLOTS);
        AnimatedPatterns_SyncTileMirror(FAN_ROWS_BOTTOM_SLOT, FAN_ROWS_SLOTS);
    }
}

/* Pointers are written relative to this table: an address inside the executable, which
 * the loader moves together with everything a registered pointer points at. */
#define IMAGE_ANCHOR ((uintptr_t)g_simStateRegions)

/* Rewrite the pointer fields of one element in place: addresses to offsets (encode) or
 * back. Two values are not addresses and stay as they are: 0, and all ones -- the
 * `(AnimTable *)-1` every sprite that is not a player holds (sprite_data.c). */
static void translate_element(const SimPointerMap *m, uint8_t *el, int encode)
{
    for (uint32_t k = 0; k < m->count; k++) {
        uint8_t *f = el + m->offsets[k];
        if (m->width == 4) {
            uint32_t v;
            memcpy(&v, f, 4);
            if (v != 0 && v != UINT32_MAX)
                v = encode ? v - (uint32_t)IMAGE_ANCHOR : v + (uint32_t)IMAGE_ANCHOR;
            memcpy(f, &v, 4);
        } else {
            uintptr_t v;
            memcpy(&v, f, sizeof v);
            if (v != 0 && v != UINTPTR_MAX)
                v = encode ? v - IMAGE_ANCHOR : v + IMAGE_ANCHOR;
            memcpy(f, &v, sizeof v);
        }
    }
}

static void translate_region(const SimStateRegion *r, uint8_t *buf, int encode)
{
    for (uint32_t off = 0; off + r->pointers->stride <= r->size; off += r->pointers->stride)
        translate_element(r->pointers, buf + off, encode);
}

uint64_t SimState_Hash(uint32_t tiers)
{
    uint64_t h = 0xcbf29ce484222325ull;
    uint8_t el[SIM_MAX_POINTER_STRIDE];
    for (int32_t i = 0; i < g_simStateRegionCount; i++) {
        const SimStateRegion *r = &g_simStateRegions[i];
        const uint8_t *p = (const uint8_t *)r->addr;
        if (!SimState_RegionActive(i, tiers)) continue;
        if (!r->pointers) {
            for (uint32_t n = 0; n < r->size; n++)
                h = (h ^ p[n]) * 0x100000001b3ull;
            continue;
        }
        for (uint32_t off = 0; off + r->pointers->stride <= r->size; off += r->pointers->stride) {
            memcpy(el, p + off, r->pointers->stride);
            translate_element(r->pointers, el, 1);
            for (uint32_t n = 0; n < r->pointers->stride; n++)
                h = (h ^ el[n]) * 0x100000001b3ull;
        }
    }
    return h;
}

void SimState_SavePortable(uint8_t *dst, uint32_t tiers)
{
    for (int32_t i = 0; i < g_simStateRegionCount; i++) {
        const SimStateRegion *r = &g_simStateRegions[i];
        if (!SimState_RegionActive(i, tiers)) continue;
        memcpy(dst, r->addr, r->size);
        if (r->pointers)
            translate_region(r, dst, 1);
        dst += r->size;
    }
}

void SimState_LoadPortable(const uint8_t *src, uint32_t tiers)
{
    for (int32_t i = 0; i < g_simStateRegionCount; i++) {
        const SimStateRegion *r = &g_simStateRegions[i];
        if (!SimState_RegionActive(i, tiers)) continue;
        memcpy(r->addr, src, r->size);
        if (r->pointers)
            translate_region(r, (uint8_t *)r->addr, 0);
        src += r->size;
    }
    if (tiers & SIM_TIER_SIM) {
        AnimatedPatterns_SyncTileMirror(FAN_ROWS_TOP_SLOT, FAN_ROWS_SLOTS);
        AnimatedPatterns_SyncTileMirror(FAN_ROWS_BOTTOM_SLOT, FAN_ROWS_SLOTS);
    }
}

static uint64_t fnv_mix(uint64_t h, const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    for (uint32_t i = 0; i < len; i++)
        h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

uint64_t SimState_LayoutHash(void)
{
    const uint32_t byteOrder = 0x01020304u;
    const uint32_t pointerWidth = (uint32_t)sizeof(void *);
    /* Where the code sits relative to the data: moves with any rebuild that changes code
     * size, so two different executables do not pass for one. */
    const uint64_t codeOffset = (uint64_t)((uintptr_t)&SimState_LayoutHash - IMAGE_ANCHOR);
    uint64_t h = 0xcbf29ce484222325ull;

    h = fnv_mix(h, &byteOrder, sizeof byteOrder);
    h = fnv_mix(h, &pointerWidth, sizeof pointerWidth);
    h = fnv_mix(h, &codeOffset, sizeof codeOffset);
    for (int32_t i = 0; i < g_simStateRegionCount; i++) {
        const SimStateRegion *r = &g_simStateRegions[i];
        const uint64_t offset = (uint64_t)((uintptr_t)r->addr - IMAGE_ANCHOR);
        const uint8_t disabled = s_disabled[i];
        h = fnv_mix(h, r->name, (uint32_t)strlen(r->name) + 1);
        h = fnv_mix(h, &r->size, sizeof r->size);
        h = fnv_mix(h, &r->flags, sizeof r->flags);
        h = fnv_mix(h, &offset, sizeof offset);
        h = fnv_mix(h, &disabled, sizeof disabled);
        if (r->pointers) {
            h = fnv_mix(h, &r->pointers->stride, sizeof r->pointers->stride);
            h = fnv_mix(h, &r->pointers->width, sizeof r->pointers->width);
            h = fnv_mix(h, &r->pointers->count, sizeof r->pointers->count);
            h = fnv_mix(h, r->pointers->offsets, r->pointers->count * sizeof r->pointers->offsets[0]);
        }
    }
    return h;
}

int SimState_Disable(const char *name)
{
    int found = 0;
    for (int32_t i = 0; i < g_simStateRegionCount; i++)
        if (!strcmp(g_simStateRegions[i].name, name)) {
            s_disabled[i] = 1;
            found = 1;
        }
    return found;
}
