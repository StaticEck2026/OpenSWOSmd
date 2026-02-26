#ifndef STRING_UTILS_H
#define STRING_UTILS_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef short int16_t;

/* ---- String utility functions ---- */
uint16_t _GetStringLength(const uint8_t *str);
int16_t _StringEquals(const uint8_t *s1, const uint8_t *s2);
uint8_t *_StringCopy(const uint8_t *src, uint8_t *dst);
uint8_t *_StringCopyUntilChar(const uint8_t *src, uint8_t *dst, uint8_t stopChar);

#endif
