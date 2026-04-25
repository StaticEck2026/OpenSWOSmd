/* simulation_data.h — Simulation-related data tables */
#ifndef SIMULATION_DATA_H
#define SIMULATION_DATA_H

#include "game_types.h"

/* Goal probability thresholds (5 values each) */
extern uint8_t simGoalProbThreshA[5];
extern uint8_t simGoalProbThreshB[5];
extern uint8_t simGoalProbThreshC[5];
extern uint8_t simGoalProbThreshD[5];
extern uint8_t simGoalProbThreshE[5];

/* Extra goal probability thresholds (3 values each) */
extern uint8_t simExtraGoalProbThreshA[3];
extern uint8_t simExtraGoalProbThreshB[3];
extern uint8_t simExtraGoalProbThreshC[3];
extern uint8_t simExtraGoalProbThreshD[3];
extern uint8_t simExtraGoalProbThreshE[3];

/* Goal scorer distributions (12 values each) */
extern uint8_t goalScorerDist442[12];
extern uint8_t goalScorerDist541[12];
extern uint8_t goalScorerDist451[12];
extern uint8_t goalScorerDist532[12];
extern uint8_t goalScorerDist352[12];
extern uint8_t goalScorerDist433[12];
extern uint8_t goalScorerDist424[12];
extern uint8_t goalScorerDist343[12];
extern uint8_t goalScorerDistSweep[12];
extern uint8_t goalScorerDist523[12];
extern uint8_t goalScorerDistAttack[12];
extern uint8_t goalScorerDistDefend[12];

/* Goalkeeper deflection */
extern uint32_t goalkeeperDeflectDeltaZ;
extern int16_t goalkeeperDeflectBallSpeeds[4];

/* Player card chances (16 values each, indexed by aggression level) */
extern uint16_t playerCardChances3min[16];
extern uint16_t playerCardChances5min[16];
extern uint16_t playerCardChances7min[16];
extern uint16_t playerCardChances10min[16];

/* Simulation helpers */
extern int8_t kMenuIrqCookieFailTrap[56];
extern uint16_t replayContTbl[16];
#endif /* SIMULATION_DATA_H */
