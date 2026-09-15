/*
 * sim_state.h -- the match state, as a registry of named variables.
 *
 * Everything a match needs to be rolled back to an earlier frame: save these
 * regions, let the game run on, load them, and the game continues exactly as it
 * did from the save. Netplay's GGPO save/load callbacks are built on it
 * (docs/NETPLAY.md), and scripts/rollback_test.sh proves the list by rolling real
 * matches back (docs/ROLLBACK.md, src/rollback_rig.c).
 *
 * Two tiers:
 *
 *   SIM_TIER_SIM   what the match reads, and everything that decides anything it
 *                  reads. Closed: no SIM byte depends on a VIEW byte, so SIM alone
 *                  is a complete resync and what a desync checksum covers. That
 *                  puts the draw list and every sprite in it here, HUD included:
 *                  a render pass sets `onScreen` from the list, and the match reads it.
 *   SIM_TIER_VIEW  the highlight ring and its pointers. Only instant-replay playback
 *                  reads it, but a local rollback that skips it leaves a rolled-back
 *                  goal in the replay.
 *
 * Render state that describes what is ALREADY DRAWN (scroll offsets, the
 * frontbuffers, the render stacks) is in neither tier on purpose: rolling it back
 * would make the renderer believe the screen shows an older frame than it does.
 *
 * Pointers. A region that holds pointers says exactly where, in a SimPointerMap.
 * SimState_Save/Load copy them as they are, which is right for a rollback inside
 * this process and nowhere else. SimState_Hash and the portable save/load write
 * each one as its offset from this executable instead (0 and all-ones, the sprites'
 * "no animation table" sentinel, are not addresses and stay as they are), which is the
 * same number in every process running the same build -- so a checksum can be
 * compared between peers and a resync buffer loaded by another machine. Every
 * pointer here points into the executable's own data; the rig proves the maps
 * complete by requiring two runs, loaded at different addresses, to hash alike.
 */
#ifndef SIM_STATE_H
#define SIM_STATE_H

#include "swos_types.h"

/* SimStateRegion.flags */
#define SIM_VIEW         0x0002u   /* SIM_TIER_VIEW; otherwise SIM_TIER_SIM */
#define SIM_PART         0x0004u   /* covers part of its variable, not all of it */

/* Tier masks for Size/Save/Load/Hash. */
#define SIM_TIER_SIM     0x0001u
#define SIM_TIER_VIEW    0x0002u
#define SIM_TIER_ALL     (SIM_TIER_SIM | SIM_TIER_VIEW)

/* Where a region's pointers are: at each of `offsets` in every `stride`-byte element. */
#define SIM_MAX_POINTERS_PER_ELEMENT 10
#define SIM_MAX_POINTER_STRIDE       256
typedef struct {
    uint32_t stride;       /* element size; the region is a whole number of elements */
    uint8_t  width;        /* sizeof(void *), or 4 for a slot holding a pointer's low 32 bits */
    uint8_t  count;
    uint16_t offsets[SIM_MAX_POINTERS_PER_ELEMENT];
} SimPointerMap;

typedef struct {
    const char          *name;
    void                *addr;
    uint32_t             size;
    uint32_t             flags;
    const SimPointerMap *pointers;   /* NULL: holds no pointers */
} SimStateRegion;

extern const SimStateRegion g_simStateRegions[];
extern const int32_t        g_simStateRegionCount;

/* The tier mask a region belongs to. */
#define SIM_REGION_TIER(r) (((r)->flags & SIM_VIEW) ? SIM_TIER_VIEW : SIM_TIER_SIM)

/* Bytes SimState_Save writes for these tiers. */
uint32_t SimState_Size(uint32_t tiers);

/* Copy the regions of these tiers, in table order, into dst (SimState_Size bytes) /
 * back. Load also brings derived render mirrors back in line with what it loaded. */
void     SimState_Save(uint8_t *dst, uint32_t tiers);
void     SimState_Load(const uint8_t *src, uint32_t tiers);

/* FNV-1a over the regions of these tiers, in table order, pointers as offsets from the
 * executable: equal in every process running this build. */
uint64_t SimState_Hash(uint32_t tiers);

/* SimState_Save/Load with pointers as offsets from the executable: a buffer that another
 * process running the same build can load (a resync). Same size as SimState_Save. */
void     SimState_SavePortable(uint8_t *dst, uint32_t tiers);
void     SimState_LoadPortable(const uint8_t *src, uint32_t tiers);

/* A fingerprint of what a saved buffer means: every region's name, size, tier, pointer
 * map and whether it is disabled, the pointer width and byte order -- and, while
 * pointers travel as offsets from the executable, where each region sits in it. Two
 * processes may exchange portable state only if theirs are equal (src/state_wire.c
 * refuses a packet otherwise). */
uint64_t SimState_LayoutHash(void);

/* Whether region `index` takes part in the calls above for these tiers. */
int      SimState_RegionActive(int32_t index, uint32_t tiers);

/* Test hook for the rollback rig: leave a region out of every call above, to try a
 * removal without rebuilding. Returns 0 when no region has that name. */
int      SimState_Disable(const char *name);

#endif /* SIM_STATE_H */
