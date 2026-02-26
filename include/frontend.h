#ifndef FRONTEND_H
#define FRONTEND_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef short int16_t;

void _mainAsm(void);
void _FrontEndMainLoop(void);
void _LoadOverlayToFrontbuffer(const uint8_t *path);
void _FrontEndPreLoopSetup(void);

#endif
