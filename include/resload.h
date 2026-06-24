/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef RESLOAD_H
#define RESLOAD_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed short int16_t;
typedef signed int int32_t;

/* --- Initialization / cleanup --- */
void SafeStart(void);
void DetachOSDisplay(void);
void LoadDosLibrary(void);
void CloseDosLibrary(void);
void _ExitToDos(void);
uint8_t *_AllocMemAny(uint32_t size);

/* --- Hardware helpers --- */
void FixSnoopBug(void);
void FixMemoryRoutine(void);
uint8_t KBHook(void);

/* --- RobNorthenLoader C implementation (called via asm bridge) --- */
uint32_t RobNorthenLoader_C(uint32_t cmd, uint32_t arg1, uint8_t *name, uint8_t *buf);

#endif /* RESLOAD_H */
