/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef IMAGE_CMD_H
#define IMAGE_CMD_H

typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed short int16_t;
typedef signed int int32_t;

void _ProcessImageCommandStream(const uint16_t *cmdBuffer, uint8_t *workBuffer);

#endif
