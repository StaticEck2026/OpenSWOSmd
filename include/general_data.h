/*
 * general_data.h — System/ISR state, audio state, palette indices,
 * panic strings, and padding/alignment bytes from the original binary.
 */
#ifndef GENERAL_DATA_H
#define GENERAL_DATA_H

#include "game_types.h"

/* ======================================================================
 * System / ISR state
 * ====================================================================== */

extern const char panicFormatStr[];
extern const char aStackOverflow[];

extern uint16_t syncWaitFlag;
extern uint16_t syncReadyFlag;
extern uint16_t int5SerialBusBufferFull;

extern uint32_t *oldStackPtr;
extern uint32_t *tmpStackPtr;

/* ISR trampoline guard buffer — 201 words */
extern uint16_t isrTrampolineGuard[201];

/* ======================================================================
 * Audio state
 * ====================================================================== */

extern uint8_t audioPresetChannelCmdTable[22];
extern uint8_t audioCounter;
extern uint8_t audioFlags[11];

/* ======================================================================
 * Palette indices — maps pitch condition types to palette color indices
 * ====================================================================== */

extern uint8_t paletteIndices[9];

/* ======================================================================
 * Padding / alignment bytes from original binary
 *
 * These exist between ported functions in the original binary layout.
 * They must be preserved to maintain memory layout compatibility until
 * Phase 4 data migration is complete.
 * ====================================================================== */

extern uint8_t pad_102555[9];
extern uint8_t pad_106CFE[2];
extern uint8_t pad_106D03;
extern uint8_t pad_106D08[2];
extern uint8_t pad_10E512[2];
extern uint8_t pad_10E53C[2];
extern uint16_t pad_10E560;
extern uint16_t pad_10E598;
extern uint8_t pad_10E59C[2];
extern uint16_t pad_10E5DE[44];
extern uint8_t pad_10F0D1;
extern uint8_t pad_10F0E9;
extern uint8_t pad_10F101;
extern uint8_t pad_10F110;
extern uint8_t pad_10F14C[2];
extern uint8_t pad_10F1CE[2];
extern uint8_t pad_10F1D8[2];
extern uint8_t pad_10F1DE[6];
extern uint8_t pad_10F1E6[2];
extern uint8_t pad_10F1EE[2];
extern uint8_t pad_10F1F2[2];
extern uint8_t pad_10F441;
extern uint8_t pad_10FBE2[34];
extern uint8_t pad_110F6C[4];
extern uint8_t pad_111673;
extern uint8_t pad_11826C[2];
extern uint16_t pad_11D00A[24];
extern uint8_t pad_120F68[2];
extern uint8_t pad_121558[20];
extern uint32_t pad_12487C;
extern uint8_t pad_12654E[2];
extern uint8_t pad_126A90[4];
extern const char pad_128EDC[];
extern uint8_t pad_133328[2];
extern uint8_t pad_136DFA[70];
extern uint8_t pad_13B35F;
extern uint8_t pad_1448F2[2];
extern uint8_t pad_145360[2];
extern uint8_t pad_145364[38];
extern uint8_t pad_14A1D4[2];

#endif /* GENERAL_DATA_H */
