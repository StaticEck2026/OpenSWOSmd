/*
 * srp_writer.h -- a whole .SRP file from its parts (docs/REPLAY_FORMAT_V2.md).
 *
 * Two things write .SRP files: the game (src/replay_v2.c, LATEST.SRP at the end of
 * every match) and the recorder (third_party/matchmaker/Server/Recorder.cpp, from the
 * frames a player streams it). They share this, so a match the server recorded is the
 * same file the player's own game wrote -- the test holds them to it byte for byte.
 *
 * Plain C, no game state: the caller hands over the parts, this lays out the file.
 */
#ifndef SRP_WRITER_H
#define SRP_WRITER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SWRP_VERSION        1
#define SWRP_HEADER_BYTES   32
#define SWRP_DIR_BYTES      16
#define SWRP_FLAG_AUDIO     0x0001u
#define SWRP_FLAG_SEEK      0x0002u
#define SWRP_FLAG_CHAPTERS  0x0004u
#define SWRP_QUIRK_ORIGINAL_FIRE_HOLD_LOCKOUT 0x0001u

#define FOURCC(a, b, c, d)  ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))
#define CHUNK_META          FOURCC('M', 'E', 'T', 'A')
#define CHUNK_FRMS          FOURCC('F', 'R', 'M', 'S')
#define CHUNK_AUDI          FOURCC('A', 'U', 'D', 'I')
#define CHUNK_SEEK          FOURCC('S', 'E', 'E', 'K')
#define CHUNK_CHAP          FOURCC('C', 'H', 'A', 'P')

#define FRAME_HEAD_BYTES    20      /* count, camera x/y (16.16), goals, crowd, minute, flags */
#define SPRITE_BYTES        6       /* image, world x, world y */
#define AUDIO_EVENT_BYTES   8       /* frame, kind, channel, id */
#define CHAPTER_BYTES       8       /* frame, kind, team, value */
#define SEEK_INTERVAL       50

/* Where in META the final score sits: filled in when the match is over. */
#define SRP_META_SCORE_OFF  (2 + 2 + 2 + 2 + 4 + 32)

typedef struct {
    uint32_t frame;
    uint8_t  kind, channel;
    uint16_t id;
} SrpAudioEvent;

typedef struct {
    uint32_t frame;
    uint8_t  kind, team;
    uint16_t value;
} SrpChapter;

/* Everything a file holds. `offsets[i]` is where frame i starts in `frms`. */
typedef struct SrpContent {
    const uint8_t       *meta;
    uint32_t             metaLen;
    const uint8_t       *frms;
    uint32_t             frmsLen;
    const uint32_t      *offsets;
    uint32_t             frameCount;
    const SrpAudioEvent *audio;
    uint32_t             audioCount;
    const SrpChapter    *chapters;
    uint32_t             chapterCount;
    uint16_t             tickRate;      /* frames a second the match was played at */
    uint16_t             quirks;        /* SWRP_QUIRK_* */
} SrpContent;

/* The whole file, malloc'd, or NULL when memory ran out. The parts are copied, never
 * taken. */
uint8_t *Srp_Build(const SrpContent *c, uint32_t *outLen);

#ifdef __cplusplus
}
#endif

#endif /* SRP_WRITER_H */
