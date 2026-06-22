/*
 * dos_gameplay.h — DOS-vs-Amiga gameplay tuning, gated on gDosGameplay (dos_mode.h).
 *
 * This tree is reversed from the Amiga (50 FPS) SWOS, so the Amiga values ARE the
 * defaults everywhere. The DOS version ran the match at 70 FPS and pre-scaled a set
 * of physics/timer constants so it FEELS the same despite the higher frame rate.
 * See docs/DOS_GAMEPLAY.md (source of truth: swos-2020 src/options.cpp
 * setGameStyleVariables()).
 *
 * ApplyGameStyle() writes the DOS or Amiga column into the SWOS globals at match
 * init. With gDosGameplay == 0 it writes the Amiga values — which equal the globals'
 * compile-time defaults — so the normal game is completely unaffected. The whole
 * feature is gated: nothing here changes anything unless [dos] gameplay=true.
 */
#ifndef DOS_GAMEPLAY_H
#define DOS_GAMEPLAY_H

/* Match-loop frame rate for the active style: 70 (DOS) or 50 (Amiga). Read by the
 * platform pacing (platform_sdl.c) for the tick interval / throttle / frame budget. */
int DosGameplay_TargetFps(void);

/* Write the per-style physics/timer constants into the SWOS globals. Call once at
 * match init, before the game loop. Idempotent and cheap — safe to call every match.
 * No-op-equivalent when gDosGameplay == 0 (writes the Amiga defaults). */
void ApplyGameStyle(void);

#endif /* DOS_GAMEPLAY_H */
