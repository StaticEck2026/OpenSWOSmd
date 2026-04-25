/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef MATH68K_H
#define MATH68K_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;

/* CalculateDeltaXAndY — direction + delta vector from src to dest at given speed.
 *
 * Returns direction 0..255 (or 0xFFFF if no movement).
 * outDeltaX/outDeltaY receive 32-bit fixed-point deltas (16×16 muls result).
 */
uint16_t _CalculateDeltaXAndY(int16_t destX, int16_t destY,
                               int16_t srcX, int16_t srcY,
                               int16_t speed,
                               int32_t *outDeltaX, int32_t *outDeltaY);

#endif
