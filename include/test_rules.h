#pragma once

#include "game_types.h"   /* the fixed-width types, on both targets */

/* ----------------------------------------------------------------------------
 * The testers' rule: no injuries, no cards.
 *
 * With it on, nobody is booked or sent off in a match, nobody is injured or suspended
 * after one, and a player already carrying an injury or a ban can be picked. It exists so
 * a tester can VIEW RESULT through a career season without a substitution or a booking
 * prompt at every match.
 *
 * It is a SETTING -- `[testing] no_injuries_cards = true` in SWOS.ini, OFF by default --
 * and it applies to OFFLINE play only:
 *
 *   - an online match plays the real rules, whatever either player's file says, so there
 *     is nothing for two players to disagree about -- unless the HOST switches CARDS/
 *     INJURIES off in the lobby (netplay_match.h, the host's switches): then both play
 *     that match by this rule (TestRules_Force), and its recording says so;
 *   - the test rigs (the replay corpus, the rollback rig, the netplay tests) always play
 *     the real rules too: their recordings were made that way;
 *   - `--srp-resim` plays a recording by the rule it was RECORDED under (SIMV's rule
 *     bits), which is how matches recorded by the old no-injuries tester builds still
 *     replay exactly.
 *
 * Until 2026-10-11 this was a compile-time flag (SWOS_TEST_NO_INJURIES_CARDS) that made
 * a tester's binary a different game from a release: it took the rule into online
 * matches, and two builds that differed in it could not play each other.
 *
 * SWOS_TEST_RULES is what is left for the compiler: built WITH it (the default, and
 * every tester drop) the setting exists; built without it (`make SWOS_TEST_RULES=0`,
 * `scripts/build_dist.sh --release`) the rule is compiled out and the setting is
 * ignored -- for a public release nobody should be able to switch it on in.
 * It also marks a TESTER build for the few defaults that differ (match reports, the
 * menu navigation log, the version line).
 * ---------------------------------------------------------------------------- */

#ifdef SWOS_TEST_RULES
/* 1 while the rule is in force for what is being played right now. */
int32_t TestRules_NoInjuriesCards(void);
/* SWOS.ini's value. */
void    TestRules_SetSetting(int32_t on);
int32_t TestRules_Setting(void);
/* A recording brings its own rule: 0 or 1 while it is replayed, -1 to let go. */
void    TestRules_Force(int32_t rule);
/* Online (1) from the moment an online menu opens until the main menu is back (0):
 * the rule is off for all of it, not only during the frames of a match. Everything a
 * match is made from is decided in between -- who may be picked, and the host's random
 * numbers at kick-off, which a skipped injury roll (after the match before, say) would
 * have left in a different place. Not a pair around the menu: a live match is played
 * after its menu has returned, and the lobby is shown again after it. */
void    TestRules_Online(int32_t online);
#else
#define TestRules_NoInjuriesCards()  0
#define TestRules_SetSetting(on)     ((void)(on))
#define TestRules_Setting()          0
#define TestRules_Force(rule)        ((void)(rule))
#define TestRules_Online(online)     ((void)(online))
#endif
