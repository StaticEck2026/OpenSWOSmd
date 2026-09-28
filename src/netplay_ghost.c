/*
 * netplay_ghost.c -- what a netplay peer drew against the match as finally played.
 * include/netplay_ghost.h has why.
 */
#include "netplay_ghost.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_types.h"
#include "sprite_data.h"
#include "dos_gameplay.h"       /* DosGameplay_TargetFps: frames a second, for match times */

extern TeamGeneralInfo _topTeamData, _bottomTeamData;
extern Sprite *_teamSpritesTable[];

enum {
    GHOST_PLAYERS = 22,         /* _teamSpritesTable: team 1's eleven, then team 2's */
    TOUCH_PX      = 5,          /* two players this close are drawn on top of each other */
    APART_PX      = 12,         /* ... and this far apart in the other version, it never happened */
    NEAR_BALL_PX  = 16,         /* only overlaps where the play is */
    CARRIER_PX    = 6,          /* the player nearest the ball, this close, has it at his feet */
    WALK_FRAMES   = 4,          /* on top of the carrier this long without winning it: a walk-through */
    KEPT_FRAMES   = 8,          /* ... and the carrier still has it this long after */
    MAX_PRINTED   = 12,         /* events printed, longest first */
    MAX_MARKS     = 64,
    MARK_WINDOW_S = 3,          /* a mark looks back this many seconds */
};

typedef enum {
    EV_DRAWN_ONLY,              /* drawn touching near the ball, apart in the final game */
    EV_FINAL_ONLY,              /* touching in the final game, drawn apart */
    EV_WALK_THROUGH,            /* on top of the ball carrier in the final game, won nothing */
    EV_KINDS
} GhostKind;

static const char *const k_kindText[EV_KINDS] = {
    "drawn touching, never did",
    "touched, never drawn",
    "walked through the man on the ball, won nothing",
};

typedef struct {
    int16_t x[GHOST_PLAYERS], y[GHOST_PLAYERS];
    int16_t bx, by, bz;
    uint8_t owner;              /* 1 top team has the ball, 2 bottom, 0 nobody */
    uint8_t have;
} GhostFrame;

typedef struct {
    int32_t   frame, frames;    /* first frame and length */
    int32_t   a, b;             /* the two players (indices into the table) */
    int32_t   drawnPx, truePx;  /* their distance drawn, and in the final game, at the first frame */
    GhostKind kind;
} GhostEvent;

static struct {
    int32_t     player;
    GhostFrame *drawn, *final;
    uint32_t    cap;
    uint32_t    corrections, changed;   /* re-simulated plays, and those that moved anything */
    int32_t     marks[MAX_MARKS];
    int32_t     markCount;
    GhostEvent *events;
    uint32_t    eventCount, eventCap;
} s_g;

static int32_t reserve(uint32_t frame)
{
    uint32_t cap;
    GhostFrame *d, *f;
    if (frame < s_g.cap)
        return 0;
    cap = s_g.cap ? s_g.cap : 4096;
    while (cap <= frame)
        cap *= 2;
    d = (GhostFrame *)realloc(s_g.drawn, cap * sizeof *d);
    if (d)
        s_g.drawn = d;
    f = (GhostFrame *)realloc(s_g.final, cap * sizeof *f);
    if (f)
        s_g.final = f;
    if (!d || !f)
        return 1;
    memset(s_g.drawn + s_g.cap, 0, (cap - s_g.cap) * sizeof *d);
    memset(s_g.final + s_g.cap, 0, (cap - s_g.cap) * sizeof *f);
    s_g.cap = cap;
    return 0;
}

void Ghost_Begin(int32_t player)
{
    s_g.player = player;
    s_g.corrections = s_g.changed = 0;
    s_g.markCount = 0;
    s_g.eventCount = 0;
    if (s_g.cap) {
        memset(s_g.drawn, 0, s_g.cap * sizeof *s_g.drawn);
        memset(s_g.final, 0, s_g.cap * sizeof *s_g.final);
    }
}

static void capture(GhostFrame *g)
{
    int32_t i;
    for (i = 0; i < GHOST_PLAYERS; i++) {
        const Sprite *s = _teamSpritesTable[i];
        g->x[i] = s ? s->x.s.i : 0;
        g->y[i] = s ? s->y.s.i : 0;
    }
    g->bx = _ballSprite.x.s.i;
    g->by = _ballSprite.y.s.i;
    g->bz = _ballSprite.z.s.i;
    g->owner = _topTeamData.playerHasBall ? 1 : _bottomTeamData.playerHasBall ? 2 : 0;
    g->have = 1;
}

void Ghost_Played(int32_t frame, int32_t first)
{
    if (frame < 0 || reserve((uint32_t)frame))
        return;
    if (first) {
        capture(&s_g.final[frame]);         /* the last play of a frame is the one kept */
        s_g.drawn[frame] = s_g.final[frame];
    } else {
        GhostFrame before = s_g.final[frame];
        capture(&s_g.final[frame]);
        s_g.corrections++;
        s_g.changed += memcmp(&before, &s_g.final[frame], sizeof before) != 0;
    }
}

/* "0:34": frames into the match as minutes and seconds of play. */
static const char *clock_text(int32_t frame, char *buf, size_t cap)
{
    int32_t fps = DosGameplay_TargetFps(), s;
    if (fps <= 0)
        fps = 50;
    s = frame / fps;
    snprintf(buf, cap, "%d:%02d", (int)(s / 60), (int)(s % 60));
    return buf;
}

void Ghost_Mark(int32_t frame)
{
    char t[16];
    if (s_g.markCount < MAX_MARKS)
        s_g.marks[s_g.markCount++] = frame;
    fprintf(stderr, "[ghost] MARKED by this player at %s (frame %d): the report at the end "
                    "of the match looks at the %d seconds before it\n",
            clock_text(frame, t, sizeof t), (int)frame, MARK_WINDOW_S);
}

static int32_t dist(const GhostFrame *g, int32_t a, int32_t b)
{
    int32_t dx = g->x[a] - g->x[b], dy = g->y[a] - g->y[b];
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    return dx > dy ? dx : dy;               /* the larger axis: sprites are boxes */
}

static int32_t ballDist(const GhostFrame *g, int32_t a)
{
    int32_t dx = g->x[a] - g->bx, dy = g->y[a] - g->by;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    return dx > dy ? dx : dy;
}

static int32_t nearBall(const GhostFrame *g, int32_t a)
{
    return ballDist(g, a) < NEAR_BALL_PX;
}

/* One team-1 player against one team-2 player: touching near the ball in `a`, and
 * apart in `b`. */
static int32_t ghostPair(const GhostFrame *a, const GhostFrame *b, int32_t i, int32_t j)
{
    return dist(a, i, j) <= TOUCH_PX && (nearBall(a, i) || nearBall(a, j)) &&
           dist(b, i, j) >= APART_PX;
}

/* The player with the ball at his feet in `g`, or -1: the nearest to a ball on the
 * ground, and close enough to be playing it. */
static int32_t carrier(const GhostFrame *g)
{
    int32_t i, best = -1, bestD = CARRIER_PX + 1;
    if (!g->have || g->bz > 4)
        return -1;
    for (i = 0; i < GHOST_PLAYERS; i++) {
        int32_t d = ballDist(g, i);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

static void addEvent(const GhostEvent *e)
{
    if (s_g.eventCount == s_g.eventCap) {
        uint32_t cap = s_g.eventCap ? s_g.eventCap * 2 : 256;
        GhostEvent *n = (GhostEvent *)realloc(s_g.events, cap * sizeof *n);
        if (!n)
            return;
        s_g.events = n;
        s_g.eventCap = cap;
    }
    s_g.events[s_g.eventCount++] = *e;
}

static int compareEvents(const void *p, const void *q)
{
    const GhostEvent *a = (const GhostEvent *)p, *b = (const GhostEvent *)q;
    return b->frames != a->frames ? b->frames - a->frames : a->frame - b->frame;
}

static int compareFrames(const void *p, const void *q)
{
    const GhostEvent *a = (const GhostEvent *)p, *b = (const GhostEvent *)q;
    return a->frame - b->frame;
}

static void printEvent(const GhostEvent *e)
{
    char t[16];
    int32_t a = e->a < 11 ? e->a : e->b, b = e->a < 11 ? e->b : e->a;
    fprintf(stderr, "[ghost]   %s (frame %d), %d frame(s): team 1 #%d and team 2 #%d %s -- "
                    "%d px apart drawn, %d px in the final game\n",
            clock_text(e->frame, t, sizeof t), (int)e->frame, (int)e->frames, (int)a + 1,
            (int)b - 10, k_kindText[e->kind], (int)e->drawnPx, (int)e->truePx);
}

static void writeTrace(int32_t lastFrame)
{
    const char *path = getenv("SWOS_GHOST_TRACE");
    FILE *f;
    int32_t fr, i, k;
    if (!path || !path[0] || !(f = fopen(path, "w")))
        return;
    fprintf(f, "frame,version,ball_x,ball_y,ball_z,owner");
    for (i = 0; i < GHOST_PLAYERS; i++)
        fprintf(f, ",t%dp%d_x,t%dp%d_y", i / 11 + 1, i % 11 + 1, i / 11 + 1, i % 11 + 1);
    fputc('\n', f);
    for (fr = 0; fr <= lastFrame && (uint32_t)fr < s_g.cap; fr++)
        for (k = 0; k < 2; k++) {
            const GhostFrame *g = k ? &s_g.final[fr] : &s_g.drawn[fr];
            if (!g->have)
                continue;
            fprintf(f, "%d,%s,%d,%d,%d,%d", fr, k ? "final" : "drawn", g->bx, g->by, g->bz, g->owner);
            for (i = 0; i < GHOST_PLAYERS; i++)
                fprintf(f, ",%d,%d", g->x[i], g->y[i]);
            fputc('\n', f);
        }
    fclose(f);
    fprintf(stderr, "[ghost] every frame, drawn and final, written to %s\n", path);
}

/* Rollback ghosts: a run of frames per team-1/team-2 pair, drawn and final disagreeing. */
static void findRollbackGhosts(int32_t lastFrame)
{
    static GhostEvent cur[2][11][11];
    static int8_t open[2][11][11];
    int32_t fr, i, j, k;

    memset(open, 0, sizeof open);
    for (fr = 0; fr <= lastFrame + 1; fr++) {
        const GhostFrame *d = fr <= lastFrame ? &s_g.drawn[fr] : NULL;
        const GhostFrame *t = fr <= lastFrame ? &s_g.final[fr] : NULL;
        int32_t valid = d && t && d->have && t->have;
        for (k = 0; k < 2; k++)
            for (i = 0; i < 11; i++)
                for (j = 0; j < 11; j++) {
                    int32_t hit = valid && (k == 0 ? ghostPair(d, t, i, 11 + j)
                                                   : ghostPair(t, d, i, 11 + j));
                    GhostEvent *e = &cur[k][i][j];
                    if (hit && !open[k][i][j]) {
                        e->frame = fr;
                        e->frames = 1;
                        e->a = i;
                        e->b = 11 + j;
                        e->drawnPx = dist(d, i, 11 + j);
                        e->truePx = dist(t, i, 11 + j);
                        e->kind = k == 0 ? EV_DRAWN_ONLY : EV_FINAL_ONLY;
                        open[k][i][j] = 1;
                    } else if (hit) {
                        e->frames++;
                    } else if (open[k][i][j]) {
                        addEvent(e);
                        open[k][i][j] = 0;
                    }
                }
    }
}

/* Walk-throughs in the FINAL game: an opponent on top of the man on the ball for
 * WALK_FRAMES and more, and the man still on the ball KEPT_FRAMES later. Plenty of
 * these are ordinary SWOS -- there are no bodies, only the ball -- so the count means
 * most beside a player's mark, or against a match played offline. */
static void findWalkThroughs(int32_t lastFrame)
{
    int32_t fr = 0;
    while (fr <= lastFrame) {
        const GhostFrame *t = &s_g.final[fr];
        int32_t c = carrier(t), o = -1, i, lo, hi, end;
        if (c >= 0) {
            lo = c < 11 ? 11 : 0;
            hi = lo + 11;
            for (i = lo; i < hi; i++)
                if (dist(t, c, i) <= TOUCH_PX && (o < 0 || dist(t, c, i) < dist(t, c, o)))
                    o = i;
        }
        if (o < 0) {
            fr++;
            continue;
        }
        for (end = fr + 1; end <= lastFrame; end++) {
            const GhostFrame *n = &s_g.final[end];
            if (carrier(n) != c || dist(n, c, o) > TOUCH_PX)
                break;
        }
        if (end - fr >= WALK_FRAMES) {
            int32_t kept = 1, k;
            for (k = end; k < end + KEPT_FRAMES && k <= lastFrame; k++)
                if (carrier(&s_g.final[k]) >= 0 && carrier(&s_g.final[k]) != c)
                    kept = 0;
            if (kept) {
                GhostEvent e;
                e.frame = fr;
                e.frames = end - fr;
                e.a = c;
                e.b = o;
                e.drawnPx = s_g.drawn[fr].have ? dist(&s_g.drawn[fr], c, o) : -1;
                e.truePx = dist(t, c, o);
                e.kind = EV_WALK_THROUGH;
                addEvent(&e);
            }
        }
        fr = end;
    }
}

void Ghost_Report(int32_t lastFrame)
{
    uint32_t frames = 0, differed = 0, ownerWrong = 0, count[EV_KINDS] = { 0 }, total[EV_KINDS] = { 0 };
    int32_t fr, i, k, m, maxOff = 0, fps = DosGameplay_TargetFps();
    uint64_t sumOff = 0, offSamples = 0;
    GhostEvent *sorted;

    if (!s_g.cap || lastFrame < 0)
        return;
    if ((uint32_t)lastFrame >= s_g.cap)
        lastFrame = (int32_t)s_g.cap - 1;
    if (fps <= 0)
        fps = 50;

    for (fr = 0; fr <= lastFrame; fr++) {
        const GhostFrame *d = &s_g.drawn[fr], *t = &s_g.final[fr];
        int32_t same = 1;
        if (!d->have || !t->have)
            continue;
        frames++;
        for (i = 0; i < GHOST_PLAYERS; i++) {
            int32_t dx = d->x[i] - t->x[i], dy = d->y[i] - t->y[i], off;
            dx = dx < 0 ? -dx : dx;
            dy = dy < 0 ? -dy : dy;
            off = dx > dy ? dx : dy;
            if (off) {
                same = 0;
                sumOff += (uint32_t)off;
                offSamples++;
                if (off > maxOff)
                    maxOff = off;
            }
        }
        if (d->bx != t->bx || d->by != t->by || d->bz != t->bz)
            same = 0;
        differed += !same;
        ownerWrong += d->owner != t->owner;
    }
    s_g.eventCount = 0;
    findRollbackGhosts(lastFrame);
    findWalkThroughs(lastFrame);
    for (i = 0; i < (int32_t)s_g.eventCount; i++) {
        count[s_g.events[i].kind]++;
        total[s_g.events[i].kind] += (uint32_t)s_g.events[i].frames;
    }

    fprintf(stderr, "[ghost] player %d: %u frames, %u drawn differently from the final game (%.1f%%); "
                    "players drawn up to %d px from where they were (mean %.1f px when off); "
                    "possession drawn wrong on %u frames\n",
            (int)s_g.player, frames, differed, frames ? 100.0 * differed / frames : 0.0, (int)maxOff,
            offSamples ? (double)sumOff / (double)offSamples : 0.0, ownerWrong);
    fprintf(stderr, "[ghost] player %d: %u frames re-simulated, %u of them changed what was drawn\n",
            (int)s_g.player, s_g.corrections, s_g.changed);
    fprintf(stderr, "[ghost] player %d: rollback ghosts: %u drawn touching near the ball and %d+ px "
                    "apart in the final game (%u frames); %u touches the final game had and this "
                    "player never saw (%u frames)\n",
            (int)s_g.player, count[EV_DRAWN_ONLY], APART_PX, total[EV_DRAWN_ONLY],
            count[EV_FINAL_ONLY], total[EV_FINAL_ONLY]);
    fprintf(stderr, "[ghost] player %d: %u walk-throughs in the final game -- an opponent on top of "
                    "the man on the ball for %d+ frames who did not win it (%u frames)\n",
            (int)s_g.player, count[EV_WALK_THROUGH], WALK_FRAMES, total[EV_WALK_THROUGH]);

    sorted = (GhostEvent *)malloc((s_g.eventCount ? s_g.eventCount : 1) * sizeof *sorted);
    if (sorted && s_g.eventCount) {
        memcpy(sorted, s_g.events, s_g.eventCount * sizeof *sorted);
        qsort(sorted, s_g.eventCount, sizeof *sorted, compareEvents);
        for (k = 0; k < (int32_t)s_g.eventCount && k < MAX_PRINTED; k++)
            printEvent(&sorted[k]);
        qsort(sorted, s_g.eventCount, sizeof *sorted, compareFrames);
    }

    /* Each mark: what this player drew against what happened, in the seconds before. */
    for (m = 0; m < s_g.markCount; m++) {
        char t[16];
        int32_t at = s_g.marks[m], from = at - MARK_WINDOW_S * fps, wrong = 0, worst = 0, n = 0;
        if (from < 0)
            from = 0;
        for (fr = from; fr <= at && fr <= lastFrame; fr++) {
            const GhostFrame *d = &s_g.drawn[fr], *tf = &s_g.final[fr];
            int32_t off = 0;
            if (!d->have || !tf->have)
                continue;
            for (i = 0; i < GHOST_PLAYERS; i++) {
                int32_t dx = d->x[i] - tf->x[i], dy = d->y[i] - tf->y[i], o;
                dx = dx < 0 ? -dx : dx;
                dy = dy < 0 ? -dy : dy;
                o = dx > dy ? dx : dy;
                if (o > off)
                    off = o;
            }
            wrong += off > 0;
            if (off > worst)
                worst = off;
        }
        fprintf(stderr, "[ghost] MARK %d at %s (frame %d): in the %d s before it, %d frame(s) drawn "
                        "differently from the final game, players up to %d px off\n",
                (int)m + 1, clock_text(at, t, sizeof t), (int)at, MARK_WINDOW_S, (int)wrong, (int)worst);
        for (k = 0; sorted && k < (int32_t)s_g.eventCount; k++)
            if (sorted[k].frame + sorted[k].frames >= from && sorted[k].frame <= at) {
                printEvent(&sorted[k]);
                n++;
            }
        if (!n)
            fprintf(stderr, "[ghost]   nothing: no rollback ghost and no walk-through near it -- "
                            "what was seen happened in the match as played\n");
    }
    free(sorted);
    writeTrace(lastFrame);
}
