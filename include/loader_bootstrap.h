/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef LOADER_BOOTSTRAP_H
#define LOADER_BOOTSTRAP_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef short int16_t;

void InstallAndInitLoaderBootstrap(void);
void InstallAndRunLoaderBootstrap_Op3(void);

#endif
