/* simulation_data.c
 * NOT part of the build yet -- lives in data/ for preparation.
 *
 * Simulation-related data tables extracted from SWOS2.s for eventual
 * migration to C.
 */

#include "simulation_data.h"

/*
 * Goal probability thresholds.
 * Cumulative probability thresholds for goal scoring, referenced by tactic index.
 * simGoalProbThreshA: tactic indices 0-1
 * simGoalProbThreshB: tactic indices 2-3
 * simGoalProbThreshC: tactic indices 4-6
 * simGoalProbThreshD: tactic indices 7-9
 * simGoalProbThreshE: tactic indices 10-17
 */
uint8_t simGoalProbThreshA[5] = { 0x76, 0x64, 0x1E, 0x07, 0x01 };
uint8_t simGoalProbThreshB[5] = { 0x76, 0x64, 0x1E, 0x07, 0x01 };
uint8_t simGoalProbThreshC[5] = { 0x78, 0x65, 0x1F, 0x04, 0x00 };
uint8_t simGoalProbThreshD[5] = { 0x89, 0x6D, 0x0A, 0x00, 0x00 };
uint8_t simGoalProbThreshE[5] = { 0x9C, 0x64, 0x00, 0x00, 0x00 };

/*
 * Extra goal probability thresholds.
 * Cumulative probability thresholds for extra goal scoring.
 * simExtraGoalProbThreshA: tactic indices 0-1
 * simExtraGoalProbThreshB: tactic indices 2-3
 * simExtraGoalProbThreshC: tactic indices 4-6  (all zero)
 * simExtraGoalProbThreshD: tactic indices 7-9  (all zero)
 * simExtraGoalProbThreshE: tactic indices 10-15 (all zero)
 */
uint8_t simExtraGoalProbThreshA[3] = { 0xA0, 0x40, 0x20 };
uint8_t simExtraGoalProbThreshB[3] = { 0xA0, 0x60, 0x00 };
uint8_t simExtraGoalProbThreshC[3] = { 0x00, 0x00, 0x00 };
uint8_t simExtraGoalProbThreshD[3] = { 0x00, 0x00, 0x00 };
uint8_t simExtraGoalProbThreshE[3] = { 0x00, 0x00, 0x00 };

/*
 * Goal scorer probability distributions (12 entries each).
 * Each array is indexed by squad position to determine who scores.
 */
uint8_t goalScorerDist442[12] = {
    0x2D, 0x28, 0x08, 0x0F, 0x05, 0x08, 0x02, 0x01, 0x01, 0x02, 0x7C, 0x04
};
uint8_t goalScorerDist541[12] = {
    0x36, 0x0C, 0x1E, 0x0C, 0x0C, 0x02, 0x01, 0x01, 0x01, 0x02, 0x7C, 0x04
};
uint8_t goalScorerDist451[12] = {
    0x36, 0x0A, 0x16, 0x16, 0x0A, 0x03, 0x02, 0x01, 0x01, 0x02, 0x7C, 0x04
};
uint8_t goalScorerDist532[12] = {
    0x2D, 0x28, 0x09, 0x11, 0x09, 0x01, 0x02, 0x02, 0x01, 0x01, 0x7C, 0x04
};
uint8_t goalScorerDist352[12] = {
    0x2F, 0x2A, 0x07, 0x0F, 0x03, 0x03, 0x07, 0x01, 0x01, 0x01, 0x7C, 0x04
};
uint8_t goalScorerDist433[12] = {
    0x2D, 0x1D, 0x1D, 0x05, 0x08, 0x05, 0x02, 0x01, 0x01, 0x02, 0x7C, 0x04
};
uint8_t goalScorerDist424[12] = {
    0x0B, 0x32, 0x29, 0x0B, 0x05, 0x05, 0x01, 0x01, 0x01, 0x01, 0x7C, 0x04
};
uint8_t goalScorerDist343[12] = {
    0x2C, 0x1C, 0x1C, 0x06, 0x0A, 0x02, 0x06, 0x01, 0x01, 0x01, 0x7C, 0x04
};
uint8_t goalScorerDistSweep[12] = {
    0x2D, 0x28, 0x08, 0x0F, 0x05, 0x08, 0x02, 0x01, 0x01, 0x02, 0x7C, 0x04
};
uint8_t goalScorerDist523[12] = {
    0x14, 0x32, 0x14, 0x0F, 0x0F, 0x02, 0x01, 0x01, 0x01, 0x02, 0x7C, 0x04
};
uint8_t goalScorerDistAttack[12] = {
    0x08, 0x2A, 0x25, 0x16, 0x08, 0x05, 0x02, 0x01, 0x01, 0x01, 0x7C, 0x04
};
uint8_t goalScorerDistDefend[12] = {
    0x3C, 0x2D, 0x08, 0x08, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x7C, 0x04
};

/*
 * Goalkeeper deflection data.
 * goalkeeperDeflectDeltaZ: Fixed-point delta Z applied to ball on deflection (0x00020000 = 2.0).
 * goalkeeperDeflectBallSpeeds: Ball speed values for goalkeeper deflections (signed).
 */
uint32_t goalkeeperDeflectDeltaZ = 0x00020000;
int16_t goalkeeperDeflectBallSpeeds[4] = { 512, 128, -128, -640 };

/*
 * Player card chances tables.
 * 16 entries each (indexed by aggression level 0-15).
 * One table per game length setting: 3min, 5min, 7min, 10min.
 */
uint16_t playerCardChances3min[16] = {
    0x0004, 0x0004, 0x0004, 0x0004, 0x0005, 0x0005, 0x0005, 0x0005,
    0x0006, 0x0006, 0x0006, 0x0007, 0x0007, 0x0008, 0x0009, 0x000A
};
uint16_t playerCardChances5min[16] = {
    0x0002, 0x0002, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0005, 0x0005, 0x0006
};
uint16_t playerCardChances7min[16] = {
    0x0001, 0x0001, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0004, 0x0004, 0x0004
};
uint16_t playerCardChances10min[16] = {
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0003, 0x0003
};

/*
 * kMenuIrqCookieFailTrap: 56 signed bytes used for menu IRQ cookie fail handling.
 */
int8_t kMenuIrqCookieFailTrap[56] = {
    -1,  0,  0,  2,  3,  3,  2,  1, -1, -3, -2, -2,  0,  1,  2,  3,
     3,  2,  1,  1,  0, -2, -1,  1,  2,  3,  1,  0, -1, -3, -2, -1,
     0, -2, -3, -1,  0,  2,  1,  2,  2,  1, -1,  0, -2, -1,  0, -1,
    -2,  0,  2,  2,  2,  0, -1, -2
};

/*
 * replayContTbl: 16-entry replay continuation table.
 * Note: entry at index 10 is 0 (not 10), matching the original assembly.
 */
uint16_t replayContTbl[16] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 0, 11, 12, 13, 14, 15
};

/*
 * playerAgeToFormDivisorTable: 50 entries mapping player age offset to
 * a form divisor value. Higher values = more form variation for older players.
 */
uint8_t playerAgeToFormDivisorTable[50] = {
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5,
     5,  5,  6,  6,  7,  7,  7,  7,  7,  7,  7,  7,  7,  8,  8,  8,
     8,  8,  8,  9,  9,  9, 10, 10, 12, 12, 12, 15, 15, 15, 15, 15,
    15, 15
};
