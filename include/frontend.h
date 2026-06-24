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

void mainC(void);
void FrontEndMainLoop(void);
void LoadOverlayToFrontbuffer(const uint8_t *path);
void FrontEndPreLoopSetup(void);

#endif
