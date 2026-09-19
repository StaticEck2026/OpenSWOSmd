/*
 * replay_v2.h -- whole-match replays in the SWRP format (SDL builds).
 * docs/REPLAY_FORMAT_V2.md is the specification.
 *
 * RECORDING taps a live match: every frame's camera, score and crowd state, every world
 * sprite at its WORLD position (so a replay plays at any screen width), and every sound
 * the match starts. When the match ends it is written to LATEST.SRP in the save folder,
 * and with [replays] auto_save also to a dated copy.
 *
 * PLAYBACK hooks into the highlights viewer that already plays .HIL and .RPL: a loaded
 * .SRP is played as one scene, its camera mapped onto this display exactly as a live
 * match maps its own (Render_CameraX), and its recorded sounds fired frame by frame.
 */
#ifndef REPLAY_V2_H
#define REPLAY_V2_H

#ifdef SWOS_SDL_BUILD

#include "swos_types.h"

#define REPLAY_V2_EXT          0x2E535250u      /* '.SRP', as the file menus pack it */
#define REPLAY_V2_LATEST       "LATEST.SRP"

/* ---- recording ------------------------------------------------------------- */

/* From main.c. `path`: record every match to this file instead of LATEST.SRP (a test's
 * --record-replay, which also records under the replay rig); NULL for the default.
 * `autoSave`: also keep a dated copy of each match ([replays] auto_save). */
void    ReplayRec_Configure(const char *path, int32_t autoSave);

/* GameLoop's own hooks, in the order a frame calls them. */
void    ReplayRec_MatchStart(void);     /* before the first frame */
void    ReplayRec_FrameBegin(void);     /* the camera has moved for this frame */
void    ReplayRec_CaptureSprites(void); /* DrawSprites has drawn it */
void    ReplayRec_FrameEnd(void);       /* the frame is complete */
void    ReplayRec_MatchEnd(void);       /* the match is over (or quit): write the file */

/* Netplay: the frame about to be stepped. A re-simulated frame replaces what was
 * recorded for it and every frame after, so the file holds the match as finally played. */
void    ReplayRec_SetFrame(uint32_t frame);

int32_t ReplayRec_Recording(void);
/* Record nothing from now on, for a run that plays a match only to check it (--srp-resim):
 * LATEST.SRP is the last match PLAYED, and a re-simulation is not one. */
void    ReplayRec_Suppress(int32_t on);

/* The recording so far, as the parts of a file (include/srp_writer.h): what the match
 * recorder (src/match_record.c) streams, frame by confirmed frame. Pointers into the
 * recorder's own buffers, good until the next frame is recorded. 0 when this match is
 * being recorded, 1 when it is not. */
struct SrpContent;
int32_t ReplayRec_Content(struct SrpContent *out);

/* A RESUMABLE recording: enough to play the match again exactly, from any frame
 * (include/srp_writer.h, SETP/STAT/INPT/HASH/SIMV). The netplay match gives them; any
 * other match leaves them empty and its .SRP is what it always was.
 *   SetSetup    the match setup (NetplaySetup_Encode), from the lobby, BEFORE MatchStart
 *   SetKickoff  the state at frame 0 (a state_wire packet), and what it needs to be
 *               re-simulated: the registry layout, the rules (P2P_RULE_*), the sim width
 *   SetInput    frame `frame`'s input, SRP_INPUT_BYTES; a later call for the same frame
 *               replaces it (a rollback), so a confirmed frame holds what was played
 *   SetHash     frame `frame`'s SIM-tier hash at its start, the same way */
void    ReplayRec_SetSetup(const uint8_t *setup, uint32_t len);
void    ReplayRec_SetKickoff(const uint8_t *packet, uint32_t len, uint64_t layout, uint8_t rules,
                             uint16_t simWidth);
void    ReplayRec_SetInput(uint32_t frame, const uint8_t *input);
void    ReplayRec_SetHash(uint32_t frame, uint64_t hash);

/* A resumable .SRP's parts, read back: what --srp-resim plays the match again from. */
typedef struct {
    uint8_t  *setup;        /* NetplaySetup_Encode */
    uint32_t  setupLen;
    uint8_t  *kickoff;      /* a state_wire packet: the state at frame 0 */
    uint32_t  kickoffLen;
    uint8_t  *inputs;       /* SRP_INPUT_BYTES a frame */
    uint32_t *hashes;       /* the low half of each frame's SIM hash, at its start */
    uint32_t  frames;
    uint64_t  layout;       /* SimState_LayoutHash of the build that recorded it */
    uint8_t   rules;        /* P2P_RULE_* it was played by */
    uint16_t  simWidth;
} ReplayResume;
/* 0; 1 no such .SRP; 2 recorded only to be watched (no resume parts); 3 broken. A bare
 * name is looked for in the save folder, like LOAD. */
int32_t ReplayV2_LoadResume(const char *name, ReplayResume *out);
void    ReplayV2_FreeResume(ReplayResume *r);

/* ---- playback -------------------------------------------------------------- */

/* Load a .SRP for the highlights viewer. 0 on success; the teams, score and pitch are
 * then in the HIL header the viewer reads, as a .RPL load leaves them. */
int32_t ReplayV2_Load(const uint8_t *name);
void    ReplayV2_Unload(void);
int32_t ReplayV2_Loaded(void);
/* The recording carries its own sounds (so the viewer must not add random crowd chants). */
int32_t ReplayV2_HasAudio(void);

/* PlayHighlightsLoop, starting the one scene a .SRP is. */
void    ReplayV2_StartScene(void);
/* DrawReplayFrame, for a loaded .SRP: one recorded frame, and its sounds. */
void    ReplayV2_DrawFrame(void);

/* ---- test harness ---------------------------------------------------------- */

/* --view-replay=<file>: play a .SRP headless in place of the front end, then exit.
 * SWOS_REPLAY_TRACE=<file> writes a line per frame both when recording and when playing
 * (the sprites actually drawn, and the sounds), so the two can be compared. */
int32_t ReplayV2_ArmViewTest(const char *path);
int32_t ReplayV2_ViewTestArmed(void);
void    ReplayV2_ViewTestRun(void);

/* sprites.c DrawQueuedSprites: a world sprite the live match drew, for the trace. */
void    ReplayTrace_LiveSprite(int16_t ordinal, int16_t x, int16_t y);

#endif /* SWOS_SDL_BUILD */
#endif /* REPLAY_V2_H */
