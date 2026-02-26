#ifndef ANIMATED_PATTERNS_H
#define ANIMATED_PATTERNS_H

#include "game_types.h"

/* ---- DrawAnimatedPatterns ----
 * Draws animated crowd fan patterns based on game state.
 * Called once per frame (including during replays).
 * 8-state machine: no_game, ball_out, greatest_cheers, middle_cheers,
 * other_team_scored, cheers_fading, game_hasnt_started, game_in_progress.
 */
void _DrawAnimatedPatterns(void);

/* ---- ResetAnimatedPatternsForBothTeams ----
 * Resets animated pitch tiles (top + bottom fan sections) back to static,
 * and clears the animatedPatterns tracking array to 0xFFFF.
 */
void _ResetAnimatedPatternsForBothTeams(void);

#endif
