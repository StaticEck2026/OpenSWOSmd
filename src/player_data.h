/* player_data.h — Player-related data tables (skills, tactics, positions, formations) */
#ifndef PLAYER_DATA_H
#define PLAYER_DATA_H

#include "game_types.h"

/* Skill bit rotation tables (14 arrays of 7 uint8_t each) */
extern uint8_t skillBitRotEven1[7];
extern uint8_t skillBitRotOdd1[7];
extern uint8_t skillBitRotEven2[7];
extern uint8_t skillBitRotOdd2[7];
extern uint8_t skillBitRotEven3[7];
extern uint8_t skillBitRotOdd3[7];
extern uint8_t skillBitRotEven4[7];
extern uint8_t skillBitRotOdd4[7];
extern uint8_t skillBitRotEven5[7];
extern uint8_t skillBitRotOdd5[7];
extern uint8_t skillBitRotEven6[7];
extern uint8_t skillBitRotOdd6[7];
extern uint8_t skillBitRotEven7[7];
/* skillBitRotOdd7 has 9 bytes in asm (7 + 2 trailing bytes 0x43,0xF9) */
extern uint8_t skillBitRotOdd7[9];

/* Tactic position maps (12 arrays of 11 uint8_t each) */
extern uint8_t tacticPosMap0[11];
extern uint8_t tacticPosMap1[11];
extern uint8_t tacticPosMap2[11];
extern uint8_t tacticPosMap3[11];
extern uint8_t tacticPosMap4[11];
extern uint8_t tacticPosMap5[11];
extern uint8_t tacticPosMap6[11];
extern uint8_t tacticPosMap7[11];
extern uint8_t tacticPosMap8[11];
extern uint8_t tacticPosMap9[11];
extern uint8_t tacticPosMap10[11];
extern uint8_t tacticPosMap11[11];

/* Position search orders (8 arrays of 9 uint8_t each, 0xFF terminated) */
extern uint8_t posSearchOrderGk[9];
extern uint8_t posSearchOrderRb[9];
extern uint8_t posSearchOrderLb[9];
extern uint8_t posSearchOrderDef[9];
extern uint8_t posSearchOrderRight[9];
extern uint8_t posSearchOrderLeft[9];
extern uint8_t posSearchOrderMid[9];
extern uint8_t posSearchOrderAtt[9];

/* Formation position orders (12 arrays of 11 uint8_t each) */
extern uint8_t positionsDefault[11];
extern uint8_t positions541[11];
extern uint8_t positions451[11];
extern uint8_t positions532[11];
extern uint8_t positions352[11];
extern uint8_t positions433[11];
extern uint8_t positions424[11];
extern uint8_t positions343[11];
extern uint8_t positionsSweep[11];
extern uint8_t positions523[11];
extern uint8_t positionsAttack[11];
extern uint8_t positionsDefend[11];

/* Side pitch positions (12 arrays of 33 uint16_t each) */
extern uint16_t sidePitchPositionsDefault[33];
extern uint16_t sidePitchPositions_5_4_1[33];
extern uint16_t sidePitchPositions_4_5_1[33];
extern uint16_t sidePitchPositions_5_3_2[33];
extern uint16_t sidePitchPositions_3_5_2[33];
extern uint16_t sidePitchPositions_4_3_3[33];
extern uint16_t sidePitchPositions_4_2_4[33];
extern uint16_t sidePitchPositions_3_4_3[33];
extern uint16_t sidePitchPositionsSweep[33];
extern uint16_t sidePitchPositions_5_2_3[33];
extern uint16_t sidePitchPositionsAttack[33];
extern uint16_t sidePitchPositionsDefend[33];

/* Position group filters (0xFF terminated) */
extern uint8_t positionGroupGoalkeeper[2];
extern uint8_t positionGroupDefender[4];
extern uint8_t positionGroupMidfielder[4];
extern uint8_t positionGroupAttacker[2];

/* Training position orders (8 arrays of 9 uint8_t, 0xFF terminated) */
extern uint8_t trainingPosOrder0[9];
extern uint8_t trainingPosOrder1[9];
extern uint8_t trainingPosOrder2[9];
extern uint8_t trainingPosOrder3[9];
extern uint8_t trainingPosOrder4[9];
extern uint8_t trainingPosOrder5[9];
extern uint8_t trainingPosOrder6[9];
extern uint8_t trainingPosOrder7[9];

/* Transfer replacement priorities (8 arrays of 9 uint8_t, 0xFF terminated) */
extern uint8_t transferReplacementPriority_role0[9];
extern uint8_t transferReplacementPriority_role1[9];
extern uint8_t transferReplacementPriority_role2[9];
extern uint8_t transferReplacementPriority_role3[9];
extern uint8_t transferReplacementPriority_role4[9];
extern uint8_t transferReplacementPriority_role5[9];
extern uint8_t transferReplacementPriority_role6[9];
extern uint8_t transferReplacementPriority_role7[9];

extern uint8_t *transferReplacementPriorityTable[8];

/* Position filled flags (working buffer, 12 zeros) */
extern uint8_t posFilledFlags[12];

/* Player age to form divisor table (50 entries) */
extern uint8_t playerAgeToFormDivisorTable[50];

#endif /* PLAYER_DATA_H */
