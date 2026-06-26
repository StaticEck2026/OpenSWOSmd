/*
 * dos_replay_load.c — load DOS / SWOS-2020 .HIL replays into our engine.
 *
 * The DOS/2020 .HIL (and .RPL) format is little-endian with a 3626-byte header
 * (1704-byte teams) and a different sprite numbering than our Amiga-derived
 * engine. Rather than teach the replay engine a second format, we convert a
 * loaded DOS .HIL in place into our big-endian Amiga HIL layout, so the existing
 * (now widescreen-correct) replay path plays it unchanged.
 *
 * Frame stream is structurally identical to ours — per frame: a camera-marker
 * dword (bit31 set), a goals dword, an anim-state dword, then sprite dwords
 * (bit31 clear), with 0xFFFFFFFF ending each scene. The only differences:
 *   - endianness (LE vs our BE)
 *   - camera marker word order: DOS = Y<<16|X, ours = X<<16|Y
 *   - goals word order:         DOS = t2<<16|t1, ours = t1<<16|t2
 *   - sprite index: DOS gameplay index = Amiga index + 114
 *     (see docs/AMIGA_DOS_DIFF.md; matches color_sprites.c::_DosOverlaySprites)
 *
 * SDL-only: the DOS replays are a PC-extras feature.
 */
#include "game_types.h"     /* provides uintN_t on both SDL and Amiga (no <stdint.h>:
                             * the m68k toolchain can't resolve it and swos_platform.h
                             * deliberately keeps it out to avoid typedef conflicts) */
#include "endian_helpers.h"
#include <string.h>

#ifdef SWOS_SDL_BUILD

extern uint8_t  _hilFileBuffer[];
extern uint16_t _hilNumGoals, _hilTeam1Goals, _hilTeam2Goals;
extern uint16_t _hilPitchNumber, _hilMaxSubstitutes;
extern uint8_t  _hilGameName[];
extern uint8_t  _hilGameRound[];

/* ---- DOS header layout (little-endian) ---- */
#define DOS_HDR_SIZE        0x0E2A   /* 3626 */
#define DOS_TEAM1_OFF       8
#define DOS_TEAM2_OFF       1712     /* 8 + 1704 */
#define DOS_TEAM_SIZE       1704
#define DOS_LEAGUE_NAME_OFF 0x0D58
#define DOS_NUMSCENES_OFF   0x0E20
#define DOS_T1GOALS_OFF     0x0E22
#define DOS_T2GOALS_OFF     0x0E24
#define DOS_PITCH_OFF       0x0E26
#define DOS_MAXSUBS_OFF     0x0E28

/* ---- our HIL layout ---- */
#define OUR_TEAM1_OFF       8
#define OUR_TEAM2_OFF       1026     /* 8 + 1018 */
#define OUR_TEAM_SIZE       1018
#define OUR_SCENE_OFF       2254     /* header size; scenes follow */
#define OUR_SCENE_BYTES     19000
#define TEAM_HDR_INT16S     11       /* kit/marker fields, byteswapped */
#define TEAM_NAME_OFF       22       /* team + player name region (byte-sized) */

#define DOS_TO_AMIGA_SPR    114
#define K_HIL_SIG           0x0000000Cu
#define K_HIL_MAXSZ         192254u

/* Detect a DOS/2020 replay: first u32 is the constant 8 (LE), unlike our
 * big-endian sig (0x0C at byte 3). */
int DosReplay_IsDosHil(const uint8_t *buf, uint32_t size)
{
    return size >= DOS_HDR_SIZE &&
           buf[0] == 8 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0;
}

/* Convert one team: DOS LE 11-int16 kit header -> ours (host-native in buffer,
 * stored BE on the way out by SetHilFileHeader's reader path is N/A here — the
 * replay's CopyTeamsAndHeader does BE16_TO_HOST, so we write BE). Name region
 * (team + player names, byte-sized) copies verbatim. */
static void convert_team(const uint8_t *dosTeam, uint8_t *ourTeam)
{
    for (int i = 0; i < TEAM_HDR_INT16S; i++) {
        uint16_t v = (uint16_t)(dosTeam[i*2] | (dosTeam[i*2+1] << 8)); /* LE read */
        ((uint16_t *)ourTeam)[i] = HOST_TO_BE16(v);                    /* BE out  */
    }
    /* byte-sized remainder (team name + per-player fields). Copy what fits our
     * smaller struct; players whose layout differs are not rendered from here. */
    int n = OUR_TEAM_SIZE - TEAM_NAME_OFF;
    if (n > DOS_TEAM_SIZE - TEAM_NAME_OFF) n = DOS_TEAM_SIZE - TEAM_NAME_OFF;
    memcpy(ourTeam + TEAM_NAME_OFF, dosTeam + TEAM_NAME_OFF, (size_t)n);
}

/* Convert a DOS frame stream (one or more scenes) into our format. Reads LE from
 * `src`, writes BE into `dst`. Returns bytes written. The DOS scene region sits
 * above our scene region in the same buffer, so a forward in-place copy keeps
 * the write cursor behind the read cursor (1:1 dwords). */
/* Set by convert_frames: 1 if the recording's sprites span wider than the 336px
 * DOS/Amiga view (a SWOS-2020 widescreen capture), 0 otherwise. The SDL
 * widescreen camera helper (highlight.c ReplayCameraX) reads this to decide
 * whether to centre a 336 recording or use the wide recorded camera verbatim.
 * The recorded camera markers are the same regardless; only the playback
 * mapping differs, so the core replay/render path is untouched. */
int _replayRecordingIsWide = 0;

static uint32_t convert_frames(const uint8_t *src, uint32_t srcLen, uint8_t *dst)
{
    uint32_t si = 0, di = 0;
    int afterMarker = 0;  /* next two dwords after a marker are goals + anim */
    int isWide = 0;       /* recorded sprite span exceeds the 336 DOS/Amiga view */
    while (si + 4 <= srcLen) {
        uint32_t d = (uint32_t)src[si] | ((uint32_t)src[si+1] << 8) |
                     ((uint32_t)src[si+2] << 16) | ((uint32_t)src[si+3] << 24);
        si += 4;
        uint32_t out;
        if (d == 0xFFFFFFFFu) {
            out = 0xFFFFFFFFu;            /* scene terminator */
            afterMarker = 0;
        } else if (afterMarker == 1) {     /* goals: DOS t2<<16|t1 -> ours t1<<16|t2 */
            out = ((d & 0xFFFFu) << 16) | (d >> 16);
            afterMarker = 2;
        } else if (afterMarker == 2) {     /* anim state (duplicated word): value only */
            out = d;
            afterMarker = 0;
        } else if (d & 0x80000000u) {      /* camera marker: DOS Y<<16|X -> ours X<<16|Y */
            uint32_t v = d & 0x7FFFFFFFu;
            out = (((v & 0xFFFFu) << 16) | (v >> 16)) | 0x80000000u;
            afterMarker = 1;
        } else {                           /* sprite: [idx:11][x:10][y:10], idx -= 114 */
            uint32_t idx = d >> 20;
            int32_t  x   = (int32_t)(d << 12) >> 22;
            int32_t  y   = (int32_t)(d << 22) >> 22;
            if (x < -40 || x > 360) isWide = 1;   /* sprite beyond the 336-wide DOS view */
            if (idx >= DOS_TO_AMIGA_SPR) idx -= DOS_TO_AMIGA_SPR;
            out = ((idx & 0x7FF) << 20) |
                  (((uint32_t)x & 0x3FF) << 10) | ((uint32_t)y & 0x3FF);
            out &= ~0x80000000u;
        }
        BE_WRITE_U32(dst + di, out);
        di += 4;
    }
    _replayRecordingIsWide = isWide;
    return di;
}

/* In-place convert the DOS .HIL already loaded in _hilFileBuffer into our HIL
 * format. Reads the DOS header into locals first, then overwrites. Returns 0. */
int DosReplay_ConvertHilInPlace(uint32_t fileSize)
{
    uint8_t *b = _hilFileBuffer;

    /* 1. read DOS header fields (before we overwrite anything) */
    uint16_t numScenes = (uint16_t)(b[DOS_NUMSCENES_OFF] | (b[DOS_NUMSCENES_OFF+1] << 8));
    uint16_t t1g       = (uint16_t)(b[DOS_T1GOALS_OFF]   | (b[DOS_T1GOALS_OFF+1]   << 8));
    uint16_t t2g       = (uint16_t)(b[DOS_T2GOALS_OFF]   | (b[DOS_T2GOALS_OFF+1]   << 8));
    uint16_t pitch     = (uint16_t)(b[DOS_PITCH_OFF]     | (b[DOS_PITCH_OFF+1]     << 8));
    uint16_t maxSubs   = (uint16_t)(b[DOS_MAXSUBS_OFF]   | (b[DOS_MAXSUBS_OFF+1]   << 8));

    static uint8_t team1[DOS_TEAM_SIZE], team2[DOS_TEAM_SIZE];
    memcpy(team1, b + DOS_TEAM1_OFF, DOS_TEAM_SIZE);
    memcpy(team2, b + DOS_TEAM2_OFF, DOS_TEAM_SIZE);

    uint8_t gameName[40];
    memcpy(gameName, b + DOS_LEAGUE_NAME_OFF, 40);

    /* 2. convert the frame stream (DOS scenes at DOS_HDR_SIZE -> ours at
     *    OUR_SCENE_OFF). Forward 1:1, write cursor stays behind read cursor. */
    if (fileSize > DOS_HDR_SIZE)
        convert_frames(b + DOS_HDR_SIZE, fileSize - DOS_HDR_SIZE, b + OUR_SCENE_OFF);

    /* 3. write our header */
    BE_WRITE_U32(b,     K_HIL_SIG);
    BE_WRITE_U32(b + 4, K_HIL_MAXSZ);
    convert_team(team1, b + OUR_TEAM1_OFF);
    convert_team(team2, b + OUR_TEAM2_OFF);
    memcpy(_hilGameName,  gameName, 40);
    memset(_hilGameRound, 0, 40);

    /* 4. scalars host-native (overlaid on the buffer via their symbols) */
    if (numScenes == 0) numScenes = 1;
    _hilNumGoals       = numScenes;
    _hilTeam1Goals     = t1g;
    _hilTeam2Goals     = t2g;
    _hilPitchNumber    = pitch;
    _hilMaxSubstitutes = maxSubs;
    return 0;
}

/* ---- RPL: a whole-match replay in the same DOS format, one ~1 MB scene ----
 * Too big for _hilFileBuffer (192 KB) or the 19000-B scene slots, so load it
 * into a dedicated buffer, convert the frame stream in place, and hand that
 * buffer to the replay engine as a single long scene (game_loop.c picks it up
 * via _rplBufferPtr). The 2020 widescreen RPLs reach ~2.4 MB; size for those. */
#define RPL_BUFFER_SIZE   (2700u * 1024u)
static uint8_t s_rplBuffer[RPL_BUFFER_SIZE];

uint8_t  *_rplBufferPtr   = (uint8_t *)0;   /* non-NULL while an RPL is loaded */
uint32_t  _rplStreamBytes = 0;              /* converted frame-stream length */

extern int16_t CAssetSys_DispatchGetSize(uint16_t opcode, uint16_t param,
                                          void *name, void *dest,
                                          uint32_t *outFileSize);
#define DOS_ASSET_OP_INIT_SOURCE0   2
extern volatile uint16_t _numScenesInHilFile;

/* Load + convert a DOS/2020 .RPL. Returns 0 on success. Bypasses LoadFileEx
 * (which panics past the ~91 KB frontbuffer stage) with the direct asset
 * dispatch — the same path dos_load_pitch uses for the big .BLK banks. */
int DosReplay_LoadRpl(uint8_t *filename)
{
    uint32_t size = 0;
    CAssetSys_DispatchGetSize(DOS_ASSET_OP_INIT_SOURCE0, 0, filename,
                               s_rplBuffer, &size);
    if (size <= DOS_HDR_SIZE || size > RPL_BUFFER_SIZE)
        return 1;
    if (!DosReplay_IsDosHil(s_rplBuffer, size))
        return 1;

    uint8_t *b = s_rplBuffer;

    /* 1. read the DOS header into locals before we overwrite it */
    uint16_t t1g     = (uint16_t)(b[DOS_T1GOALS_OFF] | (b[DOS_T1GOALS_OFF+1] << 8));
    uint16_t t2g     = (uint16_t)(b[DOS_T2GOALS_OFF] | (b[DOS_T2GOALS_OFF+1] << 8));
    uint16_t pitch   = (uint16_t)(b[DOS_PITCH_OFF]   | (b[DOS_PITCH_OFF+1]   << 8));
    uint16_t maxSubs = (uint16_t)(b[DOS_MAXSUBS_OFF] | (b[DOS_MAXSUBS_OFF+1] << 8));
    static uint8_t team1[DOS_TEAM_SIZE], team2[DOS_TEAM_SIZE];
    memcpy(team1, b + DOS_TEAM1_OFF, DOS_TEAM_SIZE);
    memcpy(team2, b + DOS_TEAM2_OFF, DOS_TEAM_SIZE);
    uint8_t gameName[40];
    memcpy(gameName, b + DOS_LEAGUE_NAME_OFF, 40);

    /* 2. convert the frame stream in place: DOS @DOS_HDR_SIZE -> our @0. Forward
     *    1:1, write cursor stays behind the read cursor. */
    _rplStreamBytes = convert_frames(b + DOS_HDR_SIZE, size - DOS_HDR_SIZE, b);

    /* 3. teams + scalars into the _hilFileBuffer header (CopyTeamsAndHeader reads
     *    the teams there; the scalars are the overlaid symbols). */
    BE_WRITE_U32(_hilFileBuffer,     K_HIL_SIG);
    BE_WRITE_U32(_hilFileBuffer + 4, K_HIL_MAXSZ);
    convert_team(team1, _hilFileBuffer + OUR_TEAM1_OFF);
    convert_team(team2, _hilFileBuffer + OUR_TEAM2_OFF);
    memcpy(_hilGameName,  gameName, 40);
    memset(_hilGameRound, 0, 40);
    _hilNumGoals       = 1;            /* one whole-match scene */
    _hilTeam1Goals     = t1g;
    _hilTeam2Goals     = t2g;
    _hilPitchNumber    = pitch;
    _hilMaxSubstitutes = maxSubs;

    /* 4. hand the buffer to the replay engine */
    _rplBufferPtr     = b;
    _numScenesInHilFile = 1;
    return 0;
}

/* Clear the RPL state so a subsequent .HIL load uses the normal scene buffer. */
void DosReplay_ClearRpl(void)
{
    _rplBufferPtr = (uint8_t *)0;
    _rplStreamBytes = 0;
}

#endif /* SWOS_SDL_BUILD */
