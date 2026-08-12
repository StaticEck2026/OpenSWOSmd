/* simulation_data.h — Simulation-related data tables */
#ifndef SIMULATION_DATA_H
#define SIMULATION_DATA_H

#include "game_types.h"

/* Goal probability thresholds (5 values each) */
extern uint8_t _simGoalProbThreshA[5];
extern uint8_t _simGoalProbThreshB[5];
extern uint8_t _simGoalProbThreshC[5];
extern uint8_t _simGoalProbThreshD[5];
extern uint8_t _simGoalProbThreshE[5];

/* Extra goal probability thresholds (3 values each) */
extern uint8_t _simExtraGoalProbThreshA[3];
extern uint8_t _simExtraGoalProbThreshB[3];
extern uint8_t _simExtraGoalProbThreshC[3];
extern uint8_t _simExtraGoalProbThreshD[3];
extern uint8_t _simExtraGoalProbThreshE[3];

/* Goal scorer distributions (12 values each) */
extern uint8_t _goalScorerDist442[12];
extern uint8_t _goalScorerDist541[12];
extern uint8_t _goalScorerDist451[12];
extern uint8_t _goalScorerDist532[12];
extern uint8_t _goalScorerDist352[12];
extern uint8_t _goalScorerDist433[12];
extern uint8_t _goalScorerDist424[12];
extern uint8_t _goalScorerDist343[12];
extern uint8_t _goalScorerDistSweep[12];
extern uint8_t _goalScorerDist523[12];
extern uint8_t _goalScorerDistAttack[12];
extern uint8_t _goalScorerDistDefend[12];

/* Goalkeeper deflection */
extern uint32_t _goalkeeperDeflectDeltaZ;
extern int16_t _goalkeeperDeflectBallSpeeds[4];

/* Player card chances (16 values each, indexed by aggression level) */
extern uint16_t _playerCardChances3min[16];
extern uint16_t _playerCardChances5min[16];
extern uint16_t _playerCardChances7min[16];
extern uint16_t _playerCardChances10min[16];

/* Simulation helpers */
extern int8_t _kMenuIrqCookieFailTrap[56];
extern uint16_t _replayContTbl[16];
#endif /* SIMULATION_DATA_H */
