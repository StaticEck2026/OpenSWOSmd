/* easing.h — how a value gets from where it was to where it is going.
 *
 * Robert Penner's easing equations. A thing that moves at a constant rate looks
 * mechanical; the same move on a curve — quick at first and settling at the end,
 * or overshooting slightly and coming back — reads as something with weight
 * behind it, which is the whole point of animating a change instead of popping
 * to it. docs/PITCH_FORMAT_V2.md is the first consumer.
 *
 * FIXED POINT, NOT FLOAT, and not by preference. The m68k target is built
 * `-nostdlib` and its toolchain has no <math.h> at all — no sinf, no powf, and
 * not even the soft-float helpers (__muldf3, __floatsidf) a float expression
 * would pull in. src/mjson.c hit exactly this and ended up with an integer-only
 * number parser for the same reason. So every curve here is computed in 16.16
 * with a sine table, an exp2 table and an integer square root, which has the
 * side benefit of giving both targets bit-identical results.
 *
 * Everything is normalised: give it t from 0 to EASE_ONE and it answers 0 to
 * EASE_ONE. Most curves stay inside those bounds, but BACK overshoots at both
 * ends and ELASTIC swings well past them, so anything that cannot survive a
 * value outside the range — an index, a colour channel — must clamp for itself.
 */
#ifndef EASING_H
#define EASING_H

/* game_types.h, not <stdint.h>: this header is included from files that build
 * for the m68k target too, and that toolchain is -nostdlib. */
#include "game_types.h"

/* 16.16: EASE_ONE is 1.0. A curve's input and output are both in these units. */
#define EASE_ONE 65536

typedef int32_t EaseFix;

typedef enum {
    EASE_LINEAR = 0,

    EASE_IN_QUAD, EASE_OUT_QUAD, EASE_IN_OUT_QUAD,
    EASE_IN_CUBIC, EASE_OUT_CUBIC, EASE_IN_OUT_CUBIC,
    EASE_IN_QUART, EASE_OUT_QUART, EASE_IN_OUT_QUART,
    EASE_IN_QUINT, EASE_OUT_QUINT, EASE_IN_OUT_QUINT,
    EASE_IN_SINE, EASE_OUT_SINE, EASE_IN_OUT_SINE,
    EASE_IN_EXPO, EASE_OUT_EXPO, EASE_IN_OUT_EXPO,
    EASE_IN_CIRC, EASE_OUT_CIRC, EASE_IN_OUT_CIRC,
    EASE_IN_BACK, EASE_OUT_BACK, EASE_IN_OUT_BACK,
    EASE_IN_ELASTIC, EASE_OUT_ELASTIC, EASE_IN_OUT_ELASTIC,
    EASE_IN_BOUNCE, EASE_OUT_BOUNCE, EASE_IN_OUT_BOUNCE,

    EASE_COUNT
} EaseKind;

/* t outside 0..EASE_ONE is clamped first, so a caller that has run past the end
 * of a transition gets the resting value rather than nonsense. */
EaseFix Ease(EaseKind kind, EaseFix t);

/* Penner's own shape: t elapsed of d, starting at b and changing by c. Reads
 * more clearly than the normalised form when the caller has a real duration. */
EaseFix EasePenner(EaseKind kind, EaseFix t, EaseFix b, EaseFix c, EaseFix d);

/* Between two values on a curve, which is what most callers actually want. */
EaseFix EaseBetween(EaseKind kind, EaseFix from, EaseFix to, EaseFix t);

/* For the JSON and any settings screen: "outback" <-> EASE_OUT_BACK. */
const char *EaseName(EaseKind kind);
EaseKind    EaseFromName(const char *name, EaseKind fallback);

#endif /* EASING_H */
