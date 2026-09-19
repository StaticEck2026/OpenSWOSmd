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

/* The recording so far, as the parts of a file (include/srp_writer.h): what the match
 * recorder (src/match_record.c) streams, frame by confirmed frame. Pointers into the
 * recorder's own buffers, good until the next frame is recorded. 0 when this match is
 * being recorded, 1 when it is not. */
struct SrpContent;
int32_t ReplayRec_Content(struct SrpContent *out);

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
