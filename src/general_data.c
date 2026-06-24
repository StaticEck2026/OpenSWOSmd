/*
 * data/general_data.c — Remaining misc initialized data
 *
 * NOT part of the build yet — lives in data/ for preparation.
 * When ready, move to src/ and add to the Makefile.
 *
 * Contains: system/ISR state, audio state, palette indices,
 * panic strings, and padding/alignment bytes from the original binary.
 */
#include "general_data.h"

#if GENERAL_DATA_C

/* ======================================================================
 * Forward declarations
 * ====================================================================== */



/* ======================================================================
 * System / ISR state
 * ====================================================================== */

const char _panicFormatStr[] = "%a";
const char _aStackOverflow[] = "STACK OVERFLOW";

uint16_t _syncWaitFlag = 1;
uint16_t _syncReadyFlag = 0;
uint16_t _int5SerialBusBufferFull = 0;

uint32_t *_oldStackPtr = 0;
/* tmpStackPtr is set to ReturnFromFunction (asm label) at init by _start.
   Initialize to 0 here; the asm label is not exported. */
uint32_t *_tmpStackPtr = 0;

/* ISR trampoline guard buffer — 201 words */
uint16_t _isrTrampolineGuard[201] = { 0 };

/* ======================================================================
 * Audio state
 * ====================================================================== */

uint8_t _audioPresetChannelCmdTable[22] = { 0 };
uint8_t _audioCounter = 0;
uint8_t _audioFlags[11] = { 0 };

/* ======================================================================
 * Palette indices — maps pitch condition types to palette color indices
 * ====================================================================== */

uint8_t _paletteIndices[9] = { 0, 7, 9, 78, 79, 80, 81, 106, 107 };

/* ======================================================================
 * Padding / alignment bytes from original binary
 *
 * These exist between ported functions in the original binary layout.
 * They must be preserved to maintain memory layout compatibility until
 * Phase 4 data migration is complete.
 * ====================================================================== */

uint8_t _pad_102555[9] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0xC0, 0xFC, 0x0, 0xA };
uint8_t _pad_106CFE[2] = { 0, 0 };
uint8_t _pad_106D03 = 0;
uint8_t _pad_106D08[2] = { 0, 0 };
uint8_t _pad_10E512[2] = { 1, 0 };
uint8_t _pad_10E53C[2] = { 1, 0 };
uint16_t _pad_10E560 = 0;
uint16_t _pad_10E598 = 0x500;
uint8_t _pad_10E59C[2] = { 0x1, 0x80 };
uint16_t _pad_10E5DE[44] = { 0, 6144, 1089, 1024, 1089, 1088, 1216, 1216, 1024, 1089, 1088, 1216, 1216, 1216, 1216, 1216, 1216, 1216, 1216, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 896, 64, 64, 64, 64, 1024 };
uint8_t _pad_10F0D1 = 0;
uint8_t _pad_10F0E9 = 0;
uint8_t _pad_10F101 = 0;
uint8_t _pad_10F110 = 0;
uint8_t _pad_10F14C[2] = { 0, 1 };
uint8_t _pad_10F1CE[2] = { 0x12, 0x34 };
uint8_t _pad_10F1D8[2] = { 0, 0 };
uint8_t _pad_10F1DE[6] = { 0, 0, 0, 0, 0, 0 };
uint8_t _pad_10F1E6[2] = { 0, 0 };
uint8_t _pad_10F1EE[2] = { 0x12, 0x34 };
uint8_t _pad_10F1F2[2] = { 0, 0 };
uint8_t _pad_10F441 = 0;
uint8_t _pad_10FBE2[34] = { 0 };
uint8_t _pad_110F6C[4] = { 0, 0, 0, 0 };
uint8_t _pad_111673 = 0;
uint8_t _pad_11826C[2] = { 0, 0 };
uint16_t _pad_11D00A[24] = { 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0x212, 0x213, 0x214, 0x215, 0x216, 0x217, 0x218, 0x219, 0x21A, 0x21B, 0x21C, 0x21D };
uint8_t _pad_120F68[2] = { 0x53, 0x40 };
uint8_t _pad_121558[20] = { 0 };
uint32_t _pad_12487C = 1191557;
uint8_t _pad_12654E[2] = { 0, 0 };
uint8_t _pad_126A90[4] = { 0, 0, 0, 0 };
const char _pad_128EDC[] = "cd_dZdUdPdKdFdAdcd_dZdUdPdKdFdAd";
uint8_t _pad_133328[2] = { 0, 0 };
uint8_t _pad_136DFA[70] = { 0 };
uint8_t _pad_13B35F = 0;
uint8_t _pad_1448F2[2] = { 0, 0 };
uint8_t _pad_145360[2] = { 0, 255 };
uint8_t _pad_145364[38] = { 0 };
uint8_t _pad_14A1D4[2] = { 0, 0 };

#endif /* GENERAL_DATA_C */
