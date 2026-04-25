/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef STRING_UTILS_H
#define STRING_UTILS_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef short int16_t;
typedef int int32_t;

/* ---- String utility functions ---- */
uint16_t _GetStringLength(const uint8_t *str);
int16_t _StringEquals(const uint8_t *s1, const uint8_t *s2);
uint8_t *_StringCopy(const uint8_t *src, uint8_t *dst);
uint8_t *_StringCopyUntilChar(const uint8_t *src, uint8_t *dst, uint8_t stopChar);

/* ---- Number formatting (Chunk 100) ---- */
void _Int2Pounds(uint32_t pounds, uint8_t *dst);
void _Int2PoundsSigned(int32_t pounds, uint8_t *dst);
void _WriteAccountBalance(int32_t delta, uint8_t *dst);
void _GetBankBalance(uint8_t *dst);
void _PrepareMenuForFadeIn(void);

#endif
