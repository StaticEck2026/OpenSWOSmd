/* md_game.h -- the Mega Drive build's game-side API (front end, match, renderer). */
#ifndef MD_GAME_H
#define MD_GAME_H

#include <stdint.h>

struct TeamFileEx;

/* md_match.c */
void md_engine_boot(void);
int  md_play_friendly(uint16_t teamA, uint16_t teamB, int human1, int human2, uint8_t pad1Team);
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
extern volatile uint16_t md_lagTicks, md_gameFrames;
extern uint8_t md_clock60;
void md_input_poll(void);

/* md_sound.c */
void md_sound_tick(void);         /* one 50 Hz game tick of the engine's audio */

#endif
