/* md_game.h -- the Mega Drive build's game-side API (front end, match, renderer). */
#ifndef MD_GAME_H
#define MD_GAME_H

#include <stdint.h>

struct TeamFileEx;

/* A friendly: two ROM teams (md_teamsData indices) and who plays each: 0 the computer,
 * 1 pad 1, 2 pad 2. */
typedef struct { uint16_t teamA, teamB; uint8_t ctrlA, ctrlB; } MdFriendly;

/* What the OPTIONS screen sets beyond the engine's own option globals (_gameLength,
 * _autoReplays): the pitch type -- 0..6 frozen..hard, or one of these -- and the month a
 * seasonal pitch is played in (0 = January). */
#define MD_PITCH_RANDOM   0xFFFF
#define MD_PITCH_SEASONAL 0xFFFE
typedef struct { uint16_t pitch; uint8_t month; } MdOptions;
extern MdOptions md_options;
extern uint8_t md_demo;         /* the attract mode: any button leaves the match */

/* md_match.c */
void md_engine_boot(void);
int  md_play_friendly(const MdFriendly *f);
const struct TeamFileEx *md_rom_team(uint16_t n);
struct TeamFileEx *md_team_by_id(uint16_t teamId);
void md_stadium_screen(void);
void md_match_enter(void);
void md_match_leave(void);

/* md_render.c */
void md_render_init_match(void);
void md_render_frame(void);       /* main loop: stage the finished frame */
void md_render_commit(void);      /* VBlank: queue the staged frame's VDP updates */
void md_render_shutdown(void);

/* md_isr.c */
extern volatile uint16_t md_lagTicks, md_gameFrames, md_matchTicks;
extern volatile uint8_t md_inMatch;
extern uint8_t md_clock60;
extern uint8_t md_abandoned;          /* set when a pad left the match before its end */
void md_input_poll(void);
void md_match_clock_stop(void);
extern volatile uint16_t md_ticks;

/* md_sound.c */
void md_sound_tick(void);         /* one 50 Hz game tick of the engine's audio */

#endif
