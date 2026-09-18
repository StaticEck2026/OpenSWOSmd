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
/* An m68k `lsl` on a signed register shifts the BIT PATTERN, and the port is full of
 * registers held as int32_t. `v << n` with v negative is UNDEFINED in C -- and the two
 * netplay platforms are built by different compilers (gcc on Linux and Cygwin, clang on
 * macOS), so "every compiler does the obvious thing" is a bet, not a guarantee, and a
 * simulation that loses that bet desyncs. Shift the unsigned value and convert back,
 * which is defined, is what the 68k did, and compiles to the same instruction.
 * UBSan over the corpus named three sites (ball_physics.c, sprites.c). */
static inline int32_t M68kShiftLeft32(int32_t v, int32_t n)
{
    return (int32_t)((uint32_t)v << n);
}

static inline int16_t M68kShiftLeft16(int16_t v, int32_t n)
{
    return (int16_t)((uint16_t)v << n);
}

uint16_t CalculateDeltaXAndY(int16_t destX, int16_t destY,
                               int16_t srcX, int16_t srcY,
                               int16_t speed,
                               int32_t *outDeltaX, int32_t *outDeltaY);

#endif
