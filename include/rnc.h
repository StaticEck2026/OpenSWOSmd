/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef RNC_H
#define RNC_H

/* ---- Types ---- */
typedef signed char int8_t;
typedef signed short int16_t;
typedef int int32_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

/* ---- RNC Decompressor (Chunk 45) ---- */

/* RNC v1 decompressor - Huffman-based.
 * src=compressed data (with RNC header), dst=output buffer.
 * Panics on decompression error.
 * Returns uncompressed size. */
void RncDecompressV1(uint8_t *src, uint8_t *dst);

/* RNC v2 decompressor - bit-stream LZ variant.
 * src=compressed data (with RNC header), dst=output buffer. */
void RncDecompressV2(uint8_t *src, uint8_t *dst);

#endif /* RNC_H */
