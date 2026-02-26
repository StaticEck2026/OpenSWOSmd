#ifndef INTERRUPTS_H
#define INTERRUPTS_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef short int16_t;

void _InitHardwareAtFrameStart(void);
void _SetupInterrupts(void);
void _Int2KeyboardHandler(void);
void _Int3TimerHandler(void);

#endif
