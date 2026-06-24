/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef FRONTEND_H
#define FRONTEND_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef short int16_t;

void _mainC(void);
void _FrontEndMainLoop(void);
void _LoadOverlayToFrontbuffer(const uint8_t *path);
void _FrontEndPreLoopSetup(void);

#endif
