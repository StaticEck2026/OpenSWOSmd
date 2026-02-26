#ifndef LOADER_BOOTSTRAP_H
#define LOADER_BOOTSTRAP_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef short int16_t;

void _InstallAndInitLoaderBootstrap(void);
void _InstallAndRunLoaderBootstrap_Op3(void);

#endif
