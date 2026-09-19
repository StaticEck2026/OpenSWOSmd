/*
 * srp_writer.c -- lay out a .SRP file. See include/srp_writer.h and
 * docs/REPLAY_FORMAT_V2.md.
 *
 * Moved out of src/replay_v2.c when the recorder needed to write the same files, so
 * a replay the server kept and the one the player's game wrote cannot differ in
 * anything but what they were given.
 */
#include "srp_writer.h"
#include "endian_helpers.h"
#include "miniz_swos.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t fourcc;
    uint8_t *data;              /* owned */
    uint32_t stored, raw;
} Chunk;

/* Raw deflate when it saves anything, else stored as it is. Takes ownership of `raw`
 * (never NULL). */
static void chunk_make(Chunk *c, uint32_t fourcc, uint8_t *raw, uint32_t rawLen, int32_t compress)
{
    c->fourcc = fourcc;
    c->raw = rawLen;
    c->data = raw;
    c->stored = rawLen;
    if (compress && rawLen > 256) {
        size_t cap = (size_t)mz_compressBound(rawLen);
        uint8_t *packed = (uint8_t *)malloc(cap);
        size_t n = packed ? tdefl_compress_mem_to_mem(packed, cap, raw, rawLen,
                                  tdefl_create_comp_flags_from_zip_params(
                                      MZ_BEST_COMPRESSION, -MZ_DEFAULT_WINDOW_BITS,
                                      MZ_DEFAULT_STRATEGY)) : 0;
        if (n > 0 && n < rawLen) {
            free(raw);
            c->data = packed;
            c->stored = (uint32_t)n;
        } else {
            free(packed);
        }
    }
}

static uint8_t *copy_of(const uint8_t *src, uint32_t len)
{
    uint8_t *p = (uint8_t *)malloc(len ? len : 1);
    if (p && len)
        memcpy(p, src, len);
    return p;
}

static uint8_t *build_audio(const SrpContent *c)
{
    uint8_t *a = (uint8_t *)malloc((size_t)c->audioCount * AUDIO_EVENT_BYTES + 1);
    uint32_t i;
    if (!a)
        return NULL;
    for (i = 0; i < c->audioCount; i++) {
        uint8_t *p = a + i * AUDIO_EVENT_BYTES;
        BE_WRITE_U32(p, c->audio[i].frame);
        p[4] = c->audio[i].kind;
        p[5] = c->audio[i].channel;
        BE_WRITE_U16(p + 6, c->audio[i].id);
    }
    return a;
}

static uint8_t *build_seek(const SrpContent *c, uint32_t *len)
{
    uint32_t count = (c->frameCount + SEEK_INTERVAL - 1) / SEEK_INTERVAL, i, ai = 0;
    uint8_t *s = (uint8_t *)malloc(8 + (size_t)count * 12);
    if (!s)
        return NULL;
    BE_WRITE_U16(s, SEEK_INTERVAL);
    BE_WRITE_U16(s + 2, 0);
    BE_WRITE_U32(s + 4, count);
    for (i = 0; i < count; i++) {
        uint32_t frame = i * SEEK_INTERVAL;
        uint8_t *p = s + 8 + i * 12;
        while (ai < c->audioCount && c->audio[ai].frame < frame)
            ai++;
        BE_WRITE_U32(p, frame);
        BE_WRITE_U32(p + 4, c->offsets[frame]);
        BE_WRITE_U32(p + 8, ai);
    }
    *len = 8 + count * 12;
    return s;
}

static uint8_t *build_chapters(const SrpContent *c, uint32_t *len)
{
    uint8_t *out = (uint8_t *)malloc(4 + (size_t)c->chapterCount * CHAPTER_BYTES);
    uint32_t i;
    if (!out)
        return NULL;
    BE_WRITE_U32(out, c->chapterCount);
    for (i = 0; i < c->chapterCount; i++) {
        uint8_t *p = out + 4 + i * CHAPTER_BYTES;
        BE_WRITE_U32(p, c->chapters[i].frame);
        p[4] = c->chapters[i].kind;
        p[5] = c->chapters[i].team;
        BE_WRITE_U16(p + 6, c->chapters[i].value);
    }
    *len = 4 + c->chapterCount * CHAPTER_BYTES;
    return out;
}

uint8_t *Srp_Build(const SrpContent *c, uint32_t *outLen)
{
    Chunk chunks[5];
    uint32_t n = 0, total, off, i, seekLen = 0, chapLen = 0;
    uint8_t *raw[5], *file, *p;
    int32_t missing = 0;

    /* Every chunk's raw bytes first: nothing is laid out unless all of them exist. */
    raw[0] = copy_of(c->meta, c->metaLen);
    raw[1] = copy_of(c->frms, c->frmsLen);
    raw[2] = build_audio(c);
    raw[3] = build_seek(c, &seekLen);
    raw[4] = build_chapters(c, &chapLen);
    memset(chunks, 0, sizeof chunks);
    for (i = 0; i < 5; i++)
        if (!raw[i])
            missing = 1;
    if (missing) {
        for (i = 0; i < 5; i++)
            free(raw[i]);
        return NULL;
    }
    chunk_make(&chunks[n++], CHUNK_META, raw[0], c->metaLen, 0);
    chunk_make(&chunks[n++], CHUNK_FRMS, raw[1], c->frmsLen, 1);
    chunk_make(&chunks[n++], CHUNK_AUDI, raw[2], c->audioCount * AUDIO_EVENT_BYTES, 1);
    chunk_make(&chunks[n++], CHUNK_SEEK, raw[3], seekLen, 0);
    chunk_make(&chunks[n++], CHUNK_CHAP, raw[4], chapLen, 0);

    total = SWRP_HEADER_BYTES + n * SWRP_DIR_BYTES;
    for (i = 0; i < n; i++)
        total += chunks[i].stored;
    file = (uint8_t *)malloc(total);
    if (file) {
        memcpy(file, "SWRP", 4);
        BE_WRITE_U16(file + 4, SWRP_VERSION);
        BE_WRITE_U16(file + 6, SWRP_FLAG_AUDIO | SWRP_FLAG_SEEK | SWRP_FLAG_CHAPTERS);
        BE_WRITE_U16(file + 8, c->tickRate);
        BE_WRITE_U16(file + 10, c->quirks);
        BE_WRITE_U16(file + 12, 0x2A0);
        BE_WRITE_U16(file + 14, 0x370);
        BE_WRITE_U32(file + 16, c->frameCount);
        BE_WRITE_U16(file + 20, (uint16_t)n);
        BE_WRITE_U16(file + 22, 0);
        BE_WRITE_U32(file + 28, 0);
        off = SWRP_HEADER_BYTES + n * SWRP_DIR_BYTES;
        for (i = 0; i < n; i++) {
            p = file + SWRP_HEADER_BYTES + i * SWRP_DIR_BYTES;
            BE_WRITE_U32(p, chunks[i].fourcc);
            BE_WRITE_U32(p + 4, off);
            BE_WRITE_U32(p + 8, chunks[i].stored);
            BE_WRITE_U32(p + 12, chunks[i].raw);
            memcpy(file + off, chunks[i].data, chunks[i].stored);
            off += chunks[i].stored;
        }
        BE_WRITE_U32(file + 24, (uint32_t)mz_crc32(MZ_CRC32_INIT, file + SWRP_HEADER_BYTES,
                                                   total - SWRP_HEADER_BYTES));
        *outLen = total;
    }
    for (i = 0; i < n; i++)
        free(chunks[i].data);
    return file;
}
