/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "swos_asm.h"

__attribute__((used, externally_visible)) volatile uint16_t *_panicReasonCode = &panicReasonCode;

/* Minimal libc stubs needed when LTO/Ofast optimizes loops into library calls */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memcpy(void *dest, const void *src, unsigned long len) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while (len--)
        *d++ = *s++;
    return dest;
}

__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memset(void *dest, int val, unsigned long len) {
    unsigned char *ptr = (unsigned char *)dest;
    while (len-- > 0)
        *ptr++ = (unsigned char)val;
    return dest;
}

__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memmove(void *dest, const void *src, unsigned long len) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    if (d < s) {
        while (len--)
            *d++ = *s++;
    } else {
        const char *lasts = s + (len - 1);
        char *lastd = d + (len - 1);
        while (len--)
            *lastd-- = *lasts--;
    }
    return dest;
}

int main(void)
{
    _startShim();
    return 0;
}