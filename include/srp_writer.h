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
/* A RESUMABLE recording (docs/REPLAY_FORMAT_V2.md §4, "Playing on from a recording"):
 * what the match was set up from, the state it kicked off in, and every frame's inputs --
 * enough to play it again exactly, not only to watch it. */
#define CHUNK_SIMV          FOURCC('S', 'I', 'M', 'V')     /* which builds can re-simulate it */
#define CHUNK_SETP          FOURCC('S', 'E', 'T', 'P')     /* the match setup (netplay_setup.c) */
#define CHUNK_STAT          FOURCC('S', 'T', 'A', 'T')     /* the state at frame 0 (state_wire.c) */
#define CHUNK_INPT          FOURCC('I', 'N', 'P', 'T')     /* both players' input, every frame */
#define CHUNK_HASH          FOURCC('H', 'A', 'S', 'H')     /* each frame's SIM-tier hash, at its start */

#define FRAME_HEAD_BYTES    20      /* count, camera x/y (16.16), goals, crowd, minute, flags */
#define SPRITE_BYTES        6       /* image, world x, world y */
#define AUDIO_EVENT_BYTES   8       /* frame, kind, channel, id */
#define CHAPTER_BYTES       8       /* frame, kind, team, value */
#define SEEK_INTERVAL       50
#define SRP_INPUT_BYTES     4       /* a frame's input: two players x (controls, key) */
#define SRP_HASH_BYTES      4       /* the low half of the frame's 64-bit SIM hash */
#define SRP_SIMV_BYTES      16      /* version, rules, pad, layout hash, sim width, pad */
#define SRP_SIMV_VERSION    1

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

    /* A resumable recording, or all zero. SETP, STAT, INPT, HASH and SIMV are written
     * only when setup, kickoff and inputs are all present. */
    const uint8_t       *setup;         /* NetplaySetup_Encode */
    uint32_t             setupLen;
    const uint8_t       *kickoff;       /* a state_wire packet: the state at frame 0 */
    uint32_t             kickoffLen;
    const uint8_t       *inputs;        /* SRP_INPUT_BYTES a frame, for frames 0..frameCount-1 */
    const uint32_t      *hashes;        /* one a frame, for frames 0..frameCount-1 */
    uint64_t             layout;        /* SimState_LayoutHash of the recording build */
    uint8_t              rules;         /* P2P_RULE_*: what it was played by */
    uint16_t             simWidth;      /* the simulation draw width it was played at */
} SrpContent;

/* The whole file, malloc'd, or NULL when memory ran out. The parts are copied, never
 * taken. */
uint8_t *Srp_Build(const SrpContent *c, uint32_t *outLen);

#ifdef __cplusplus
}
#endif

#endif /* SRP_WRITER_H */
