/* rnc.c — RNC ProPack decompressor (v1 Huffman + v2 LZ bitstream)
 *
 * Chunk 45: Direct translation from SWOS2.s RNC routines.
 * All sub-functions are static (only called internally).
 *
 * V1 register mapping (RncDecodeBlock):
 *   a0 = header read ptr (RncReadU32)  a3 = data read cursor
 *   a5 = output write cursor           a6 = output end
 *   a4 = compressed data end           a2 = table base (sp)
 *   d6 = 32-bit bit buffer (LSB first) d7 = bits available (signed byte)
 *
 * V2 register mapping (RncDecodeV2Body):
 *   a0 = source read ptr   a1 = output write ptr   a2 = back-ref ptr
 *   d0 = match length      d1 = match distance     d2 = 8-bit shift register
 */

#include "rnc.h"

/* ---- Externs ---- */
extern volatile uint16_t panicReasonCode;
extern void _PanicHaltLoop(void);

/* ======================================================================
 *  V1 (Huffman) decompressor
 * ====================================================================== */

#define RNC_SIG_V1 0x524E4301u  /* "RNC\x01" */

/* ---- V1 bitstream state ---- */
typedef struct {
    uint8_t  *src;   /* a3: compressed data read cursor */
    uint32_t  bits;  /* d6: bit accumulator, consumed from LSB */
    int8_t    count; /* d7: bits available (signed, <0 means need refill) */
} V1Bits;

/* RncRefillBits — called when count went negative after subtracting nbits.
 * Loads 2 bytes LE from src, merges into bit accumulator. */
static void v1Refill(V1Bits *bs, uint8_t nbits)
{
    uint8_t oldCount = (uint8_t)((int8_t)(bs->count + nbits));  /* count before subtract */
    uint16_t newWord;
    uint8_t remaining;

    bs->bits >>= oldCount;                       /* shift out consumed bits */
    /* Asm does addq.w #4,a3 then reads -(a3) twice — net effect: advance src
     * past the 2 bytes already in the buffer, then read the next 2 bytes LE.
     * src must be advanced BEFORE reading. */
    bs->src += 2;
    newWord = (uint16_t)bs->src[0] | ((uint16_t)bs->src[1] << 8);
    bs->bits = ((uint32_t)newWord << 16) | (bs->bits & 0xFFFFu);
    remaining = nbits - oldCount;                 /* bits still to consume */
    bs->bits >>= remaining;                       /* asm: lsr.l d1,d6 after refill returns */
    bs->count = (int8_t)(16 - remaining);
}

/* Extract value from bitstream (mask = (1<<nbits)-1 or 0xFFFF for 16-bit) */
static uint16_t v1GetBits(V1Bits *bs, uint16_t mask, uint8_t nbits)
{
    uint16_t val = (uint16_t)bs->bits & mask;
    bs->count -= (int8_t)nbits;
    if (bs->count < 0)
        v1Refill(bs, nbits);
    else
        bs->bits >>= nbits;
    return val;
}

/* Consume bits without returning value */
static void v1SkipBits(V1Bits *bs, uint8_t nbits)
{
    bs->count -= (int8_t)nbits;
    if (bs->count < 0)
        v1Refill(bs, nbits);
    else
        bs->bits >>= nbits;
}

/* RncReadU32 — read 4 bytes big-endian, advance pointer */
static uint32_t v1ReadU32(uint8_t **pp)
{
    uint8_t *p = *pp;
    uint32_t v = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
               | ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
    *pp = p + 4;
    return v;
}

/* RncDecodeSymbol — decode one Huffman symbol from table.
 *
 * Table layout (0x80 bytes):
 *   [0x00..0x3F] up to 16 entries: { uint16_t mask, uint16_t match } (BE)
 *   [0x40..0x7F] up to 16 aux:    { uint8_t codeBits, uint8_t extraCount,
 *                                    uint16_t extraMask } (BE)
 * Entry i is at tab+i*4, aux i is at tab+0x40+i*4.
 *
 * Asm walks a0 forward; when matched, a0 is past the entry, so aux is at
 * a0+0x3C (= tab + (i+1)*4 + 0x3C = tab + i*4 + 0x40). */
static uint16_t v1DecodeSymbol(V1Bits *bs, uint8_t *tab)
{
    uint8_t *p = tab;
    uint16_t d0;
    uint8_t d1, d2;
    uint16_t mask, match;

    /* Search for matching entry */
    for (;;) {
        mask  = ((uint16_t)p[0] << 8) | p[1];
        match = ((uint16_t)p[2] << 8) | p[3];
        p += 4;
        d0 = ((uint16_t)bs->bits & mask) - match;
        if (d0 == 0) break;
    }

    /* Consume code bits (aux at p + 0x3C) */
    d1 = p[0x3C];
    v1SkipBits(bs, d1);

    /* Extra bits */
    d0 = p[0x3D];           /* extra bits count */
    if (d0 < 2) return d0;  /* 0 or 1: literal value */

    d0--;
    d1 = (uint8_t)d0;
    d2 = (uint8_t)d0;

    {
        uint16_t extraMask = ((uint16_t)p[0x3E] << 8) | p[0x3F];
        d0 = (uint16_t)bs->bits & extraMask;
    }

    v1SkipBits(bs, d1);
    d0 |= (uint16_t)(1u << d2);   /* bset d2,d0 */
    return d0;
}

/* RncBuildTable — build Huffman decode table from bitstream.
 * Reads symbol count (5 bits), per-symbol bit lengths (4 bits each),
 * then constructs mask/match/aux entries. */
static void v1BuildTable(V1Bits *bs, uint8_t *outTab)
{
    uint8_t bitLens[32];
    int16_t numSyms, i;
    uint32_t incr;
    uint8_t  curLen;
    uint32_t codeVal;
    uint8_t *wp;

    numSyms = (int16_t)v1GetBits(bs, 0x1F, 5) - 1;
    if (numSyms < 0) return;

    for (i = 0; i <= numSyms; i++)
        bitLens[i] = (uint8_t)v1GetBits(bs, 0x0F, 4);

    incr    = 0x80000000u;   /* ror.l #1 of 1 */
    curLen  = 1;
    codeVal = 0;
    wp      = outTab;

    while (curLen < 17) {
        int16_t si;
        /* Asm scans forward through bitLens[0..numSyms] with d4 counting
         * down from numSyms (dbf) and a1 advancing via (a1)+.
         * extra = forward index (d3 - d4). */
        for (si = 0; si <= numSyms; si++) {
            if (bitLens[si] != curLen)
                continue;

            /* mask = (1 << curLen) - 1 */
            {
                uint16_t m = (uint16_t)((1u << curLen) - 1);
                wp[0] = (uint8_t)(m >> 8);
                wp[1] = (uint8_t)m;
            }

            /* match = bit-reversed high word of codeVal, right-aligned.
             * Asm uses roxl/roxr to reverse the TOP curLen bits of the high
             * word.  Equivalent: right-align them first, then reverse. */
            {
                uint16_t hw = (uint16_t)(codeVal >> 16);
                uint16_t rev = 0;
                uint8_t j;
                hw >>= (16 - curLen);   /* right-align top curLen bits */
                for (j = 0; j < curLen; j++) {
                    rev = (uint16_t)(rev << 1) | (hw & 1);
                    hw >>= 1;
                }
                wp[2] = (uint8_t)(rev >> 8);
                wp[3] = (uint8_t)rev;
            }

            /* aux data at wp + 0x40 */
            {
                uint8_t extra = (uint8_t)si;    /* asm: d3 - d4 = forward index */
                uint16_t emask = (extra > 0) ? (uint16_t)((1u << (extra - 1)) - 1) : 0;
                wp[0x40] = curLen;
                wp[0x41] = extra;
                wp[0x42] = (uint8_t)(emask >> 8);
                wp[0x43] = (uint8_t)emask;
            }

            wp += 4;
            codeVal += incr;
        }
        incr >>= 1;
        curLen++;
    }
}

/* RncDecodeBlock — main V1 decompression.  Returns unpacked size or 0. */
static uint32_t v1DecodeBlock(uint8_t *src, uint8_t *dst)
{
    uint8_t tables[0x1C0];
    uint8_t *hdr = src;
    uint8_t *a3;          /* compressed data start */
    uint8_t *a5 = dst;    /* output cursor */
    uint8_t *a4;          /* end of compressed data */
    uint8_t *a6;          /* end of output */
    uint32_t unpackSize, packSize, sig;
    V1Bits bs;

    /* Overlap save area (asm uses stack words) */
    uint16_t savedCount = 0;
    uint8_t  savedBytes[256];

    sig = v1ReadU32(&hdr);
    if (sig != RNC_SIG_V1)
        return 0;

    unpackSize = v1ReadU32(&hdr);     /* hdr now at src+8 */
    a6 = dst + unpackSize;
    /* asm: lea $A(a0),a3 — a0 is at src+8 after two ReadU32 calls,
     * so a3 = src+8+0x0A = src+0x12 (18 bytes into header) */
    a3 = hdr + 0x0A;                  /* hdr=src+8, so a3=src+18 */
    packSize = v1ReadU32(&hdr);       /* hdr now at src+12, but unused further */
    a4 = a3 + packSize;

    /* ---- Overlap handling ---- */
    /* asm: clr.w -(sp)  → push 0 word (savedCount) */
    if (dst < a4) {
        uint8_t leeway = *(a3 - 2);  /* asm: move.b -2(a3),d0 */
        uint8_t *endWithLeeway = a6 + leeway;

        if (endWithLeeway > a4) {
            /* Must relocate compressed data above output area */
            uint8_t *srcEnd = a4;     /* original end of compressed */
            uint8_t *relDst = endWithLeeway;

            /* Align to even */
            if ((uint32_t)srcEnd & 1) { srcEnd++; relDst++; }
            if ((uint32_t)relDst & 1) { relDst++; }

            /* Save bytes that will be overwritten (between a6 and relDst) */
            {
                uint8_t *p = relDst;
                savedCount = 0;
                while (p != a6) {
                    p--;
                    savedBytes[savedCount++] = *p;
                }
            }

            /* Block-copy compressed data upward (32 bytes at a time) */
            {
                uint8_t *rp = srcEnd;
                uint8_t *wp = relDst;
                while (rp > a3) {
                    uint32_t chunk = (uint32_t)(rp - a3);
                    uint32_t k;
                    if (chunk > 32) chunk = 32;
                    rp -= chunk;
                    wp -= chunk;
                    for (k = 0; k < chunk; k++)
                        wp[k] = rp[k];
                }
                /* Adjust a3: a3_new = a3_old - srcEnd + wp_final
                 * Asm: suba.l a4,a3; adda.l a0,a3 */
                a3 = wp;
            }
        }
    }

    /* ---- Initialize bitstream ---- */
    bs.bits  = (uint32_t)a3[0] | ((uint32_t)a3[1] << 8);  /* 16-bit LE */
    bs.count = 0;
    bs.src   = a3;  /* src pointer for refills AND literal copies */

    /* Skip initial 2 bits */
    v1SkipBits(&bs, 2);

    /* ---- Main decode loop ---- */
    for (;;) {
        int16_t blockCount;

        v1BuildTable(&bs, &tables[0x000]);
        v1BuildTable(&bs, &tables[0x080]);
        v1BuildTable(&bs, &tables[0x100]);

        blockCount = (int16_t)v1GetBits(&bs, 0xFFFF, 16) - 1;

        goto do_literal;

    do_match:
        {
            int32_t dist = -(int32_t)v1DecodeSymbol(&bs, &tables[0x080]);
            uint16_t mlen = v1DecodeSymbol(&bs, &tables[0x100]);
            uint8_t *mp = a5 + dist - 1;
            int16_t cnt = (int16_t)mlen;

            *a5++ = *mp++;
            while (cnt >= 0) {
                *a5++ = *mp++;
                cnt--;
            }
        }

    do_literal:
        {
            uint16_t litLen = v1DecodeSymbol(&bs, &tables[0x000]);
            int16_t cnt = (int16_t)litLen - 1;

            if (cnt >= 0) {
                /* Copy literal bytes from compressed stream */
                while (cnt >= 0) {
                    *a5++ = *bs.src++;
                    cnt--;
                }
                /* Re-sync bit buffer from new src position:
                 * peek 2 bytes LE, shift left by count, merge into low bits */
                {
                    uint32_t peek = (uint32_t)bs.src[0] | ((uint32_t)bs.src[1] << 8);
                    peek <<= (uint8_t)bs.count;
                    if (bs.count > 0) {
                        uint32_t mask = (1u << (uint8_t)bs.count) - 1;
                        bs.bits = (bs.bits & mask) | peek;
                    } else {
                        bs.bits = peek;
                    }
                }
            }
        }

        blockCount--;
        if (blockCount >= 0)
            goto do_match;

        if (a5 >= a6)
            break;
    }

    /* Restore overlap saved bytes */
    if (savedCount > 0) {
        uint16_t i;
        for (i = savedCount; i > 0; i--)
            *a5++ = savedBytes[i - 1];
    }

    return unpackSize;
}

/* RncDecompressV1 — entry point with error check */
__attribute__((used, externally_visible))
void _RncDecompressV1(uint8_t *src, uint8_t *dst)
{
    uint32_t result = v1DecodeBlock(src, dst);
    if (result == 0 || result == 0xFFFFFFFFu || result == 0xFFFFFFFEu) {
        panicReasonCode = 0xE003u;
        _PanicHaltLoop();
    }
}

/* ======================================================================
 *  V2 (LZ bitstream) decompressor
 * ====================================================================== */

/* V2 bit extraction: single-byte shift register with carry chain.
 * add.b d2,d2 shifts MSB into carry; beq refills from src with addx. */
typedef struct {
    uint8_t *src;
    uint8_t  d2;
} V2Bits;

static int v2Bit(V2Bits *bs)
{
    int carry = (bs->d2 >> 7) & 1;
    bs->d2 <<= 1;
    if (bs->d2 == 0) {
        uint8_t byte = *bs->src++;
        uint16_t tmp = (uint16_t)byte * 2 + (uint16_t)carry;
        carry = (tmp >> 8) & 1;
        bs->d2 = (uint8_t)tmp;
    }
    return carry;
}

/* RncDecodeV2Body — V2 decompression engine.
 * Uses goto-based control flow matching the asm branch structure. */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _RncDecompressV2(uint8_t *src, uint8_t *dst)
{
    uint8_t *a1 = dst;
    V2Bits bs;
    uint16_t d0, d1;
    int c;

    bs.src = src + 0x12;

    /* Init: moveq #$80,d2; add.b d2,d2 → d2=0,X=1; move.b (a0)+,d2;
     * addx.b d2,d2; add.b d2,d2; bra main_entry */
    bs.d2 = *bs.src++;
    {
        uint16_t tmp = (uint16_t)bs.d2 * 2 + 1;  /* addx with X=1 */
        bs.d2 = (uint8_t)tmp;
        /* carry from addx is discarded by next add.b */
    }
    /* add.b d2,d2: skip one more bit (carry discarded by bra) */
    bs.d2 <<= 1;
    /* if d2==0 here, it will be caught by first v2Bit call */

    goto main_entry;

    /* ---------- literal copy + entry ---------- */
literal_loop:
    *a1++ = *bs.src++;
main_entry:                          /* loc_106C7E */
    c = v2Bit(&bs);
    if (!c) {                        /* bcs → skip if carry */
        *a1++ = *bs.src++;           /* literal byte */
        c = v2Bit(&bs);
        if (!c) goto literal_loop;   /* bcc → more literals */
    }
    /* beq after bcs: this checks Z from the add.b that preceded the bcs.
     * In our model, v2Bit handles refill transparently, so we just proceed
     * to match decode. */

match_start:                         /* loc_106C8A */
    d0 = 2;
    d1 = 0;
    c = v2Bit(&bs);

    if (!c) goto len_extend;         /* bcc → loc_106C12 */

    /* ---- first carry set ---- */
    c = v2Bit(&bs);
    if (!c) goto copy;               /* bcc → loc_106C4C */

    /* ---- second carry set ---- */
    d0 = 3;
    c = v2Bit(&bs);
    if (!c) goto dist_bit;           /* bcc → loc_106C2C */

    /* ---- third carry set: extended length byte ---- */
    d0 = (uint16_t)*bs.src++;
    if (d0 == 0) goto end_check;     /* beq → loc_106CDC */
    d0 += 8;
    goto dist_bit;

len_extend:                          /* loc_106C12 */
    c = v2Bit(&bs);
    d0 = (uint16_t)(d0 * 2 + c);    /* addx.w d0,d0 */
    c = v2Bit(&bs);
    if (!c) goto dist_bit;           /* bcc → loc_106C2C */

    /* carry set: another length extension */
    d0--;                            /* subq.w #1,d0 */
    c = v2Bit(&bs);
    d0 = (uint16_t)(d0 * 2 + c);    /* addx.w d0,d0 */

    if ((d0 & 0xFF) == 9) {
        /* literal_block: read 4-bit count into d1, copy (d1+3) groups of 4 */
        int16_t k;
        d1 = 0;
        for (k = 3; k >= 0; k--) {
            c = v2Bit(&bs);
            d1 = (uint16_t)(d1 * 2 + c);
        }
        d1 += 2;                     /* addq.w #2,d1 */
        {
            int16_t groups = (int16_t)d1;
            while (groups >= 0) {
                *a1++ = *bs.src++;
                *a1++ = *bs.src++;
                *a1++ = *bs.src++;
                *a1++ = *bs.src++;
                groups--;
            }
        }
        goto main_entry;
    }
    /* fall through to dist_bit */

dist_bit:                            /* loc_106C2C */
    c = v2Bit(&bs);
    if (!c) goto copy;               /* bcc → loc_106C4C */

    /* carry set: read distance high bits */
    c = v2Bit(&bs);
    d1 = (uint16_t)(d1 * 2 + c);    /* addx.w d1,d1 */
    c = v2Bit(&bs);
    if (c) goto dist_high;           /* bcs → loc_106CAC */

    /* carry clear */
    if (d1 != 0) goto dist_combine;  /* tst.w d1; bne → loc_106C4A */
    d1 = 1;                          /* addq.w #1,d1 */

    /* loc_106C44 */
    c = v2Bit(&bs);
    d1 = (uint16_t)(d1 * 2 + c);    /* addx.w d1,d1 */

dist_combine:                        /* loc_106C4A */
    d1 = (uint16_t)((d1 << 8) | (d1 >> 8));  /* rol.w #8,d1 */

copy:                                /* loc_106C4C */
    d1 = (uint16_t)((d1 & 0xFF00) | *bs.src++);  /* move.b (a0)+,d1 */
    {
        uint8_t *a2 = a1 - d1 - 1;
        int16_t cnt;
        uint8_t odd = (uint8_t)(d0 & 1);

        d0 >>= 1;
        if (odd)
            *a1++ = *a2++;

        cnt = (int16_t)(d0 - 1);

        if (d1 == 0) {
            /* RLE: repeat single byte */
            uint8_t val = *a2;
            while (cnt >= 0) {
                *a1++ = val;
                *a1++ = val;
                cnt--;
            }
        } else {
            while (cnt >= 0) {
                *a1++ = *a2++;
                *a1++ = *a2++;
                cnt--;
            }
        }
    }
    goto main_entry;

dist_high:                           /* loc_106CAC */
    c = v2Bit(&bs);
    d1 = (uint16_t)(d1 * 2 + c);    /* addx.w d1,d1 */
    d1 |= 4;                        /* ori.w #4,d1 */
    c = v2Bit(&bs);
    if (c) goto dist_combine;        /* bcs → loc_106C4A */
    /* bra loc_106C44 */
    c = v2Bit(&bs);
    d1 = (uint16_t)(d1 * 2 + c);
    goto dist_combine;

end_check:                           /* loc_106CDC */
    c = v2Bit(&bs);
    if (c) goto main_entry;          /* bcs → more data */
    return;                          /* done */
}
