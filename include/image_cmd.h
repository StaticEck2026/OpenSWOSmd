#ifndef IMAGE_CMD_H
#define IMAGE_CMD_H

typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef signed short int16_t;
typedef signed long int32_t;

void _ProcessImageCommandStream(const uint16_t *cmdBuffer, uint8_t *workBuffer);

#endif
