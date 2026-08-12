/*
 * sfx_debug.c — runtime helpers for SWOS_DEBUG_SFX.
 *
 * Two responsibilities:
 *   1. A monotonic "ms since first SFX trigger" counter so log
 *      timestamps line up with the user's in-game observations.
 *   2. A best-effort id → SFX name lookup so the user can read the
 *      log without consulting sfx_ids.h.
 *
 * Compiled unconditionally; the header gates the call sites on
 * `SWOS_DEBUG_SFX`. Nothing here references SDL2 types, so we don't
 * pull SDL into translation units that don't already have it.
 */
#ifdef SWOS_DEBUG_SFX

#include <stdint.h>
#include <SDL.h>

#include "sfx_debug.h"
#include "sfx_ids.h"

unsigned swos_sfx_log_now_ms(void)
{
    static uint32_t s_origin_ms = 0;
    uint32_t now = SDL_GetTicks();
    if (s_origin_ms == 0) s_origin_ms = now;
    return (unsigned)(now - s_origin_ms);
}

/* Best-effort id → name table. Not exhaustive — IDs without a
 * sfx_ids.h macro return a literal "?". Multi-meaning IDs prefer
 * the wave-id interpretation since that's what crowd / event SFX
 * use; the few preset-id collisions are obvious from the log
 * context (preset triggers always print a `preset=` line right
 * before the per-channel triggers fire). */
const char *swos_sfx_id_name(unsigned id)
{
    switch (id) {
    case SFX_CROWD_BG_1:                return "SFX_CROWD_BG_1";
    case SFX_CROWD_BG_2:                return "SFX_CROWD_BG_2";
    case SFX_WHISTLE_FOUL_1:            return "SFX_WHISTLE_FOUL_1";
    case SFX_CROWD_GOAL_CHEER:          return "SFX_CROWD_GOAL_CHEER";
    case SFX_CROWD_MATCH_ENGINE_START:  return "SFX_CROWD_MATCH_ENGINE_START";
    case SFX_CROWD_REFEREE_CARD_FADEOUT:return "SFX_CROWD_REFEREE_CARD_FADEOUT";
    case SFX_CROWD_CHEER_FADEOUT:       return "SFX_CROWD_CHEER_FADEOUT";
    case SFX_BALL_KEEPER_SAFE_1:        return "SFX_BALL_KEEPER_SAFE_1";
    case SFX_BALL_BOUNCE_2:             return "SFX_BALL_BOUNCE_2";
    case SFX_BALL_TACKLED_FOUL:         return "SFX_BALL_TACKLED_FOUL";
    case SFX_CROWD_KEEPERSAVES:         return "SFX_CROWD_KEEPERSAVES";
    case SFX_BALL_BOUNCE_1:             return "SFX_BALL_BOUNCE_1";
    case SFX_CROWD_NEARMISS:            return "SFX_CROWD_NEARMISS";
    case SFX_CROWD_REFEREE_CARD:        return "SFX_CROWD_REFEREE_CARD";
    case SFX_WHISTLE_ENDGAME:           return "SFX_WHISTLE_ENDGAME";
    case SFX_BALL_KEEPER_SAFE_2:        return "SFX_BALL_KEEPER_SAFE_2";
    case SFX_BALL_HEADER_STANDING:      return "SFX_BALL_HEADER_STANDING";
    case SFX_BALL_PASSED_OR_TACKLED:    return "SFX_BALL_PASSED_OR_TACKLED";
    case SFX_BALL_SHOT_HEADER_LOW:      return "SFX_BALL_SHOT_HEADER_LOW";
    case SFX_WHISTLE_HALFTIME:          return "SFX_WHISTLE_HALFTIME";
    case SFX_WHISTLE_FOUL_2:            return "SFX_WHISTLE_FOUL_2";
    case SFX_BALL_BOUNCE_3:             return "SFX_BALL_BOUNCE_3";
    case SFX_BALL_BOUNCE_4:             return "SFX_BALL_BOUNCE_4";
    case SFX_BALL_BOUNCE_5:             return "SFX_BALL_BOUNCE_5";
    case SFX_WHISTLE_KICKOFF:           return "SFX_WHISTLE_KICKOFF";
    case SFX_CROWD_CHANT_1:             return "SFX_CROWD_CHANT_1";
    case SFX_CROWD_CHANT_2:             return "SFX_CROWD_CHANT_2";
    case SFX_CROWD_CHANT_3:             return "SFX_CROWD_CHANT_3";
    case SFX_CROWD_CHANT_4:             return "SFX_CROWD_CHANT_4";
    default:                            return "?";
    }
}

#endif /* SWOS_DEBUG_SFX */
