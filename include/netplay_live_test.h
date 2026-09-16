/*
 * netplay_live_test.h -- two processes play a LIVE match against each other.
 * See src/netplay_live_test.c and docs/NETPLAY.md section 7.
 */
#ifndef NETPLAY_LIVE_TEST_H
#define NETPLAY_LIVE_TEST_H

#ifdef SWOS_SDL_BUILD

#include "swos_types.h"

/* --netplay-live-pair=host|join. 0 if the role is one of those. */
int32_t NetplayLiveTest_Arm(const char *role);
int32_t NetplayLiveTest_Armed(void);

/* Runs the whole thing and exits the process. Called from the bootstrap in
 * place of the front end, once the game data is loaded -- the same hook the
 * replay rig uses, and for the same reason: a match needs its data. */
void    NetplayLiveTest_Run(void);

/* --netplay-state-diff=A,B: name the registry entries two kick-off dumps
 * disagree on (SWOS_LIVE_DUMP=<dir> makes the pair test write them). */
int32_t NetplayLiveTest_StateDiff(const char *spec);

#endif /* SWOS_SDL_BUILD */
#endif /* NETPLAY_LIVE_TEST_H */
