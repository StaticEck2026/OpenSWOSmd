/* easing.c — Penner's curves in 16.16 fixed point. See include/easing.h for why
 * there is not a float in the file. */
#include "easing.h"

/* ---- the three transcendentals the curves need -----------------------------
 *
 * Written as tables rather than approximated with polynomials: these are read a
 * few dozen times a frame at most, the tables are 328 bytes together, and a
 * table is the one form whose error you can state exactly (worst case here is
 * under 1/3000 of the range, invisible at 16 px a tile). */

/* sin over a QUARTER turn, 65 entries: sin(i/64 * pi/2) in 16.16. Angles are
 * measured in TURNS everywhere below (EASE_ONE = a full circle), which keeps pi
 * out of the file entirely -- there is no pi in fixed point, only a rounding of
 * it, and the curves that need it all divide it back out again. */
static const int32_t k_sinQuarter[65] = {
         0,   1608,   3216,   4821,   6424,   8022,   9616,  11204,
     12785,  14359,  15924,  17479,  19024,  20557,  22078,  23586,
     25080,  26558,  28020,  29466,  30893,  32303,  33692,  35062,
     36410,  37736,  39040,  40320,  41576,  42806,  44011,  45190,
     46341,  47464,  48559,  49624,  50660,  51665,  52639,  53581,
     54491,  55368,  56212,  57022,  57798,  58538,  59244,  59914,
     60547,  61145,  61705,  62228,  62714,  63162,  63572,  63944,
     64277,  64571,  64827,  65043,  65220,  65358,  65457,  65516,
     65536,
};

/* 2^(i/16), 17 entries in 16.16: the fractional half of exp2. */
static const int32_t k_exp2Frac[17] = {
     65536,  68438,  71468,  74632,  77936,  81386,
     84990,  88752,  92682,  96785, 101070, 105545,
    110218, 115098, 120194, 125515, 131072,
};

#define EASE_BACK_S       111515   /* 1.701580 -- Penner's overshoot: the curve
                                    * pulls back about a tenth of the distance
                                    * before it sets off */
#define EASE_BACK_S2      170060   /* 2.594909 = BACK_S * 1.525, scaled so the
                                    * middle of an in-out move overshoots as far
                                    * as the ends of a one-sided one */
#define EASE_BOUNCE_N     495616   /* 7.562500 */
#define EASE_B_1D          23831   /* 0.363636 = 1/2.75 */
#define EASE_B_2D          47663   /* 0.727273 */
#define EASE_B_25D         59578   /* 0.909091 */
#define EASE_B_15D         35747   /* 0.545455 */
#define EASE_B_225D        53620   /* 0.818182 */
#define EASE_B_2625D       62557   /* 0.954545 */
#define EASE_B_075         49152
#define EASE_B_09375       61440
#define EASE_B_0984375     64512

/* (a*b) >> 16. The 64-bit intermediate is deliberate and it is fine on m68k:
 * the 68020 has a 32x32->64 multiply, so gcc inlines this with no libgcc helper
 * (src/camera.c:61 does the same and its object references none). */
static int32_t fmul(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
}

/* (a << 16) / b, in 32-bit arithmetic only.
 *
 * The obvious `((int64_t)a << 16) / b` does NOT work on the Amiga: a 64-bit
 * divide is a call to __divdi3, and -nostdlib has no libgcc to call (the m68020
 * has a 32x32->64 multiply, which is why fmul above inlines, but its divu.l
 * traps when the quotient will not fit 32 bits, so gcc will not use it either).
 * The link fails outright, which is how this was found.
 *
 * So: the integer part by a plain 32-bit divide, then sixteen fraction bits one
 * at a time. The remainder stays below b, so doubling it cannot overflow. An
 * integer part of 32768 or more overflows the 16.16 result, but Ease() clamps
 * anything at or past 1.0 anyway. */
static int32_t fdiv(int32_t a, int32_t b)
{
    uint32_t ua, ub, q, r, f;
    int32_t i, neg = 0;

    if (b == 0)
        return 0;
    if (a < 0) { a = -a; neg ^= 1; }
    if (b < 0) { b = -b; neg ^= 1; }
    ua = (uint32_t)a;
    ub = (uint32_t)b;

    q = ua / ub;
    r = ua % ub;
    f = 0;
    for (i = 0; i < 16; i++) {
        r <<= 1;
        f <<= 1;
        if (r >= ub) { r -= ub; f |= 1u; }
    }
    q = (q << 16) | f;
    return neg ? -(int32_t)q : (int32_t)q;
}

/* sin of an angle in TURNS: EASE_ONE is a full circle. Reduces to a quadrant,
 * then interpolates the quarter table. */
static int32_t sinTurns(int32_t turns)
{
    int32_t q, idx, frac, a, b, v;
    int32_t neg = 0;

    turns &= (EASE_ONE - 1);              /* mod one turn; negatives wrap right,
                                           * since the mask is a power of two */
    if (turns >= EASE_ONE / 2) {          /* lower half: same shape, negated */
        turns -= EASE_ONE / 2;
        neg = 1;
    }
    if (turns >= EASE_ONE / 4)            /* second quadrant mirrors the first */
        turns = EASE_ONE / 2 - turns;

    q    = turns << 2;                    /* quarter turn -> 0..EASE_ONE */
    idx  = q >> 10;                       /* 64 steps */
    frac = q & 1023;
    if (idx >= 64)
        v = k_sinQuarter[64];
    else {
        a = k_sinQuarter[idx];
        b = k_sinQuarter[idx + 1];
        v = a + (int32_t)(((int64_t)(b - a) * frac) >> 10);
    }
    return neg ? -v : v;
}

static int32_t cosTurns(int32_t turns)
{
    return sinTurns(turns + EASE_ONE / 4);
}

/* 2^x for x in 16.16. Only ever called with x in [-20, 0] here, but it handles
 * a positive exponent too as long as the result fits. */
static int32_t exp2Fix(int32_t x)
{
    int32_t whole = x >> 16;              /* arithmetic shift: floors, which is
                                           * what the fraction below assumes */
    int32_t frac  = x - (whole << 16);    /* 0 .. EASE_ONE-1 */
    int32_t idx   = frac >> 12;           /* 16 steps */
    int32_t rem   = frac & 4095;
    int32_t a     = k_exp2Frac[idx];
    int32_t b     = k_exp2Frac[idx + 1];
    int32_t v     = a + (int32_t)(((int64_t)(b - a) * rem) >> 12);

    if (whole > 0) {
        if (whole >= 15)
            return 0x7FFFFFFF;
        return v << whole;
    }
    if (whole < 0) {
        if (whole <= -31)
            return 0;
        return v >> (-whole);
    }
    return v;
}

/* sqrt of a 16.16 value, bit by bit. x < 0 answers 0 rather than exploding:
 * CIRC's radicand can go a hair below zero on the rounding. */
static int32_t sqrtFix(int32_t x)
{
    uint64_t rem = 0, root = 0, v;
    int32_t i;

    if (x <= 0)
        return 0;
    v = ((uint64_t)(uint32_t)x) << 16;    /* sqrt(x) in 16.16 = isqrt(x << 16) */
    for (i = 0; i < 32; i++) {
        rem  = (rem << 2) | (v >> 62);
        v  <<= 2;
        root <<= 1;
        if (rem > root) {
            rem -= root | 1;
            root += 2;
        }
    }
    return (int32_t)(root >> 1);
}

/* BOUNCE is not a formula but four parabolas end to end -- the drop and three
 * smaller hops -- which is why it alone is written as a ladder. */
static int32_t bounceOut(int32_t t)
{
    if (t < EASE_B_1D)
        return fmul(EASE_BOUNCE_N, fmul(t, t));
    if (t < EASE_B_2D) {
        t -= EASE_B_15D;
        return fmul(EASE_BOUNCE_N, fmul(t, t)) + EASE_B_075;
    }
    if (t < EASE_B_25D) {
        t -= EASE_B_225D;
        return fmul(EASE_BOUNCE_N, fmul(t, t)) + EASE_B_09375;
    }
    t -= EASE_B_2625D;
    return fmul(EASE_BOUNCE_N, fmul(t, t)) + EASE_B_0984375;
}

EaseFix Ease(EaseKind kind, EaseFix t)
{
    int32_t u, t2, u2, s;

    if (t <= 0)
        return 0;
    if (t >= EASE_ONE)
        return EASE_ONE;

    u  = EASE_ONE - t;
    t2 = fmul(t, t);
    u2 = fmul(u, u);

    switch (kind) {
    case EASE_LINEAR:        return t;

    case EASE_IN_QUAD:       return t2;
    case EASE_OUT_QUAD:      return EASE_ONE - u2;
    case EASE_IN_OUT_QUAD:   return (t < EASE_ONE / 2) ? 2 * t2
                                                       : EASE_ONE - 2 * u2;

    case EASE_IN_CUBIC:      return fmul(t2, t);
    case EASE_OUT_CUBIC:     return EASE_ONE - fmul(u2, u);
    case EASE_IN_OUT_CUBIC:  return (t < EASE_ONE / 2) ? 4 * fmul(t2, t)
                                                       : EASE_ONE - 4 * fmul(u2, u);

    case EASE_IN_QUART:      return fmul(t2, t2);
    case EASE_OUT_QUART:     return EASE_ONE - fmul(u2, u2);
    case EASE_IN_OUT_QUART:  return (t < EASE_ONE / 2) ? 8 * fmul(t2, t2)
                                                       : EASE_ONE - 8 * fmul(u2, u2);

    case EASE_IN_QUINT:      return fmul(fmul(t2, t2), t);
    case EASE_OUT_QUINT:     return EASE_ONE - fmul(fmul(u2, u2), u);
    case EASE_IN_OUT_QUINT:  return (t < EASE_ONE / 2) ? 16 * fmul(fmul(t2, t2), t)
                                                       : EASE_ONE - 16 * fmul(fmul(u2, u2), u);

    /* A quarter turn is t/4 in turns, which is where the pi goes. */
    case EASE_IN_SINE:       return EASE_ONE - cosTurns(t >> 2);
    case EASE_OUT_SINE:      return sinTurns(t >> 2);
    case EASE_IN_OUT_SINE:   return -(cosTurns(t >> 1) - EASE_ONE) >> 1;

    case EASE_IN_EXPO:       return exp2Fix(10 * t - 10 * EASE_ONE);
    case EASE_OUT_EXPO:      return EASE_ONE - exp2Fix(-10 * t);
    case EASE_IN_OUT_EXPO:   return (t < EASE_ONE / 2)
                                    ? exp2Fix(20 * t - 10 * EASE_ONE) >> 1
                                    : (2 * EASE_ONE - exp2Fix(-20 * t + 10 * EASE_ONE)) >> 1;

    case EASE_IN_CIRC:       return EASE_ONE - sqrtFix(EASE_ONE - t2);
    case EASE_OUT_CIRC:      return sqrtFix(EASE_ONE - u2);
    case EASE_IN_OUT_CIRC:   return (t < EASE_ONE / 2)
                                    ? (EASE_ONE - sqrtFix(EASE_ONE - 4 * t2)) >> 1
                                    : (sqrtFix(EASE_ONE - 4 * u2) + EASE_ONE) >> 1;

    case EASE_IN_BACK:
        return fmul(t2, fmul(EASE_BACK_S + EASE_ONE, t) - EASE_BACK_S);
    case EASE_OUT_BACK:
        return EASE_ONE + fmul(u2, fmul(EASE_BACK_S + EASE_ONE, -u) + EASE_BACK_S);
    case EASE_IN_OUT_BACK:
        s = fmul(EASE_BACK_S2 + EASE_ONE, 2 * t) - EASE_BACK_S2;
        if (t < EASE_ONE / 2)
            return fmul(4 * t2, s) >> 1;
        s = fmul(EASE_BACK_S2 + EASE_ONE, 2 * u) - EASE_BACK_S2;
        return EASE_ONE - (fmul(4 * u2, s) >> 1);

    /* ELASTIC's period is 2pi/3 of a radian, i.e. a third of a turn -- so the
     * angle is simply (10t - 0.75)/3 turns, and the in-out one is /4.5. */
    case EASE_IN_ELASTIC:      /* 10.75 turns/3, i.e. 704512 in 16.16 */
        return -fmul(exp2Fix(10 * t - 10 * EASE_ONE),
                     sinTurns((10 * t - 704512) / 3));
    case EASE_OUT_ELASTIC:
        return fmul(exp2Fix(-10 * t), sinTurns((10 * t - 49152) / 3)) + EASE_ONE;
    case EASE_IN_OUT_ELASTIC:
        if (t < EASE_ONE / 2)
            return -(fmul(exp2Fix(20 * t - 10 * EASE_ONE),
                          sinTurns((20 * t - 729088) * 2 / 9)) >> 1);
        return (fmul(exp2Fix(-20 * t + 10 * EASE_ONE),
                     sinTurns((20 * t - 729088) * 2 / 9)) >> 1) + EASE_ONE;

    case EASE_IN_BOUNCE:     return EASE_ONE - bounceOut(u);
    case EASE_OUT_BOUNCE:    return bounceOut(t);
    case EASE_IN_OUT_BOUNCE: return (t < EASE_ONE / 2)
                                    ? (EASE_ONE - bounceOut(EASE_ONE - 2 * t)) >> 1
                                    : (EASE_ONE + bounceOut(2 * t - EASE_ONE)) >> 1;

    default:                 return t;
    }
}

EaseFix EasePenner(EaseKind kind, EaseFix t, EaseFix b, EaseFix c, EaseFix d)
{
    if (d <= 0)
        return b + c;
    return b + fmul(c, Ease(kind, fdiv(t, d)));
}

EaseFix EaseBetween(EaseKind kind, EaseFix from, EaseFix to, EaseFix t)
{
    return from + fmul(to - from, Ease(kind, t));
}

/* The names the JSON and any settings screen use. They follow the enum exactly,
 * so a name is found by walking the table rather than being mapped -- there are
 * thirty-one of them and this is read once. */
static const char *const k_easeNames[EASE_COUNT] = {
    "linear",
    "inquad",    "outquad",    "inoutquad",
    "incubic",   "outcubic",   "inoutcubic",
    "inquart",   "outquart",   "inoutquart",
    "inquint",   "outquint",   "inoutquint",
    "insine",    "outsine",    "inoutsine",
    "inexpo",    "outexpo",    "inoutexpo",
    "incirc",    "outcirc",    "inoutcirc",
    "inback",    "outback",    "inoutback",
    "inelastic", "outelastic", "inoutelastic",
    "inbounce",  "outbounce",  "inoutbounce",
};

const char *EaseName(EaseKind kind)
{
    if ((int32_t)kind < 0 || (int32_t)kind >= EASE_COUNT)
        return k_easeNames[EASE_LINEAR];
    return k_easeNames[kind];
}

/* strcasecmp is not the same name on every toolchain we build for, and these
 * are ASCII words, so the comparison is written out. */
static int32_t sameName(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a++, cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
    }
    return *a == 0 && *b == 0;
}

EaseKind EaseFromName(const char *name, EaseKind fallback)
{
    int32_t i;
    if (!name)
        return fallback;
    for (i = 0; i < EASE_COUNT; i++)
        if (sameName(name, k_easeNames[i]))
            return (EaseKind)i;
    return fallback;
}
