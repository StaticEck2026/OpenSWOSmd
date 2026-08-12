/* ======================================================================
 * swos_position_value.c  --  self-contained SWOS squad-screen tick/cross logic
 * ----------------------------------------------------------------------
 * Everything needed to reproduce the green-tick / red-cross indicators SWOS
 * shows when you move a player on the squad / tactics screen: the per-player
 * "position value" calculation and its 6 data tables. Single file, no deps.
 *
 * THE INDICATOR MODEL
 *   Each player has a "value" for the slot he currently occupies, computed by
 *   GetPlayerPrice() from his 7 skills, his slot's 35-ball-position coverage in
 *   the team's formation, distance weighting, and (human teams) crowding +
 *   "nearness". When you move players around, SWOS recomputes each value and
 *   compares it to the snapshot taken before the move:
 *       new value  >  snapshot  ->  GREEN TICK   (player improved his slot)
 *       new value  <  snapshot  ->  RED CROSS    (player got worse)
 *       new value  == snapshot  ->  no mark
 *   That's the SMALL (per-player) mark: SwosSmallMark() over _playerPositionsArray
 *   vs _oldPlayerPositionsArray.
 *
 *   The LARGE (whole-team) mark is the SAME before/after comparison, but on the
 *   team's overall skill total -- NOT a plain average of GetPlayerPrice(). The team
 *   total is ComputeOldPlayerSkillForViewResult(): it
 *   runs every player through UpdatePlayerPositionAndReturnRole, folds the subs in
 *   as a weighted average, sums the 11 outfield values, divides by 15, halves for a
 *   non-GK in goal, and divides by an empty-slot penalty divisor. Snapshot it,
 *   move a player, recompute, diff -> SwosBigMark().
 *
 *   (History: these two globals were once named `_oldPlayerSkill` / `_newPlayerSkill`,
 *   which were back-to-front for this feature -- the live/after total lived in the
 *   "old"-named one. Both the game and this tool now use `_teamSkillCurrent` /
 *   `_teamSkillSnapshot`; see the NAMING NOTE on those.)
 *
 * THE PIECES YOU ASKED FOR
 *   - coordToQuadrant      -> NOT code: it's the lookup table
 *                             _quadrantCoordinatesToBallQuadrant[240] (a
 *                             pitch-coordinate -> ball-quadrant map).
 *   - quadrantDistanceFactor -> the table _quadrantDistanceFactor[1226]
 *                             (a 35x35 distance-weight matrix + a 1-byte head).
 *   - skillsPerQuadrant    -> _skillsPerQuadrant[245] (7 position rows x 35).
 *   - priceSkillPonders    -> _priceSkillPonders[35] (packed nibble weights).
 *   - plPricesIncreaseTable-> _plPricesIncreaseTable[50] (price softening LUT).
 *   - CountNearnessFactor  -> function below.
 *   - GetPlayerPrice       -> function below (the core value calc).
 *   - UpdatePlayerPositionAndReturnRole -> function below (the wrapper that
 *                             picks GK/sub/injured/loan paths and calls
 *                             GetPlayerPrice for outfielders; it does NOT change
 *                             the player's position TYPE, it selects which value
 *                             path to use and caches the result).
 *   - ComputeOldPlayerSkillForViewResult -> function below: the whole-team
 *                             value that drives the LARGE tick/cross.
 *   - SwosSmallMark / SwosBigMark / SwosSnapshotBaseline / SwosMarkSprite ->
 *                             the small (per-player) + large (team) tick/cross
 *                             decisions and the snapshot helper.
 *
 * All tables + function bodies are byte-for-byte from the port's source
 * (src/tactics_data.c, src/swos_data_bulk_imports.c, src/match_sim.c,
 * src/diy_competition.c). All 16-bit intermediate arithmetic is preserved.
 *
 * Build the demo:  cc -O2 -o swos_pv swos_position_value.c -DSWOS_PV_DEMO && ./swos_pv
 * ====================================================================== */

#include <stdint.h>
#include <stdio.h>

/* ---- position type (positionAndFace bits 5-7) + flags ---- */
#define PF_POSITION_MASK   0xE0   /* 0=GK,1=RB,2=LB,3=CD,4=RW,5=LW,6=MF,7=ST */
#define PF_POSITION_SHIFT  5
#define PF_LOAN            0x02   /* bit 1: on loan -> value forced to 1 */
#define PLAYERFILE_SIZE    38
#define TEAM_COMPUTER      1      /* teamControls value for a CPU team */
#define TEAM_VALUES_CLEAN  0      /* playerValuesDirty: cache valid (use cachedMatchValue) */
#define TEAM_VALUES_DIRTY  1      /* playerValuesDirty: squad changed -> recompute */

/* ======================================================================
 * DATA TABLES  (verbatim from the game)
 * ====================================================================== */
uint8_t _skillsPerQuadrant[245] = { 4, 4, 4, 3, 3, 4, 4, 4, 3, 3, 4, 4, 3, 3, 3, 4, 4, 3, 3, 3, 3, 3, 2, 2, 2, 3, 2, 1, 1, 2, 3, 1, 1, 1, 2, 3, 3, 4, 4, 4, 3, 3, 4, 4, 4, 3, 3, 3, 4, 4, 3, 3, 3, 4, 4, 2, 2, 2, 3, 3, 2, 1, 1, 2, 3, 2, 1, 1, 1, 3, 3, 4, 4, 4, 3, 3, 4, 4, 4, 3, 2, 4, 4, 4, 2, 1, 3, 4, 3, 1, 1, 2, 3, 2, 1, 1, 1, 3, 1, 1, 1, 1, 4, 1, 1, 3, 3, 1, 2, 2, 3, 3, 2, 2, 2, 4, 4, 3, 3, 3, 4, 4, 3, 3, 3, 4, 4, 3, 3, 3, 4, 4, 3, 3, 3, 4, 4, 3, 3, 3, 2, 2, 1, 3, 3, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 3, 3, 3, 4, 4, 3, 3, 3, 4, 4, 3, 3, 3, 4, 4, 3, 3, 3, 4, 4, 1, 1, 2, 1, 1, 2, 3, 4, 3, 2, 3, 4, 4, 4, 3, 3, 4, 4, 4, 3, 3, 4, 4, 4, 3, 1, 2, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 3, 2, 1, 2, 3, 4, 3, 2, 3, 4, 4, 4, 3, 3, 4, 4, 4, 3, 3, 4, 4, 4, 3 };
uint8_t _quadrantDistanceFactor[1226] = { 0x20, 0x18, 0x10, 0x8, 0x2, 0x18, 0x18, 0x10, 0x8, 0x2, 0x10, 0x10, 0x10, 0x8, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x18, 0x20, 0x18, 0x10, 0x8, 0x18, 0x18, 0x18, 0x10, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x10, 0x18, 0x20, 0x18, 0x10, 0x10, 0x18, 0x18, 0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x8, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8, 0x10, 0x18, 0x20, 0x18, 0x8, 0x10, 0x18, 0x18, 0x18, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0x8, 0x10, 0x18, 0x20, 0x2, 0x8, 0x10, 0x18, 0x18, 0x2, 0x8, 0x10, 0x10, 0x10, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x18, 0x18, 0x10, 0x8, 0x2, 0x20, 0x18, 0x10, 0x8, 0x2, 0x18, 0x18, 0x10, 0x8, 0x2, 0x10, 0x10, 0x10, 0x8, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1, 0x18, 0x18, 0x18, 0x10, 0x8, 0x18, 0x20, 0x18, 0x10, 0x8, 0x18, 0x18, 0x18, 0x10, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1, 0x10, 0x18, 0x18, 0x18, 0x10, 0x10, 0x18, 0x20, 0x18, 0x10, 0x10, 0x18, 0x18, 0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x8, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1, 0x8, 0x10, 0x18, 0x18, 0x18, 0x8, 0x10, 0x18, 0x20, 0x18, 0x8, 0x10, 0x18, 0x18, 0x18, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x8, 0x10, 0x18, 0x18, 0x2, 0x8, 0x10, 0x18, 0x20, 0x2, 0x8, 0x10, 0x18, 0x18, 0x2, 0x8, 0x10, 0x10, 0x10, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1, 0x10, 0x10, 0x10, 0x8, 0x2, 0x18, 0x18, 0x10, 0x8, 0x2, 0x20, 0x18, 0x10, 0x8, 0x2, 0x18, 0x18, 0x10, 0x8, 0x2, 0x10, 0x10, 0x10, 0x8, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x10, 0x10, 0x10, 0x10, 0x8, 0x18, 0x18, 0x18, 0x10, 0x8, 0x18, 0x20, 0x18, 0x10, 0x8, 0x18, 0x18, 0x18, 0x10, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x18, 0x18, 0x18, 0x10, 0x10, 0x18, 0x20, 0x18, 0x10, 0x10, 0x18, 0x18, 0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x8, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x10, 0x18, 0x18, 0x18, 0x8, 0x10, 0x18, 0x20, 0x18, 0x8, 0x10, 0x18, 0x18, 0x18, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x10, 0x10, 0x10, 0x2, 0x8, 0x10, 0x18, 0x18, 0x2, 0x8, 0x10, 0x18, 0x20, 0x2, 0x8, 0x10, 0x18, 0x18, 0x2, 0x8, 0x10, 0x10, 0x10, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x10, 0x10, 0x10, 0x8, 0x2, 0x18, 0x18, 0x10, 0x8, 0x2, 0x20, 0x18, 0x10, 0x8, 0x2, 0x18, 0x18, 0x10, 0x8, 0x2, 0x10, 0x10, 0x10, 0x8, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x8, 0x8, 0x8, 0x8, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x18, 0x18, 0x18, 0x10, 0x8, 0x18, 0x20, 0x18, 0x10, 0x8, 0x18, 0x18, 0x18, 0x10, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x18, 0x18, 0x18, 0x10, 0x10, 0x18, 0x20, 0x18, 0x10, 0x10, 0x18, 0x18, 0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x10, 0x18, 0x18, 0x18, 0x8, 0x10, 0x18, 0x20, 0x18, 0x8, 0x10, 0x18, 0x18, 0x18, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x8, 0x8, 0x8, 0x8, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x8, 0x10, 0x10, 0x10, 0x2, 0x8, 0x10, 0x18, 0x18, 0x2, 0x8, 0x10, 0x18, 0x20, 0x2, 0x8, 0x10, 0x18, 0x18, 0x2, 0x8, 0x10, 0x10, 0x10, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x10, 0x10, 0x10, 0x8, 0x2, 0x18, 0x18, 0x10, 0x8, 0x2, 0x20, 0x18, 0x10, 0x8, 0x2, 0x18, 0x18, 0x10, 0x8, 0x2, 0x10, 0x10, 0x10, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x18, 0x18, 0x18, 0x10, 0x8, 0x18, 0x20, 0x18, 0x10, 0x8, 0x18, 0x18, 0x18, 0x10, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x8, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x18, 0x18, 0x18, 0x10, 0x10, 0x18, 0x20, 0x18, 0x10, 0x10, 0x18, 0x18, 0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x10, 0x18, 0x18, 0x18, 0x8, 0x10, 0x18, 0x20, 0x18, 0x8, 0x10, 0x18, 0x18, 0x18, 0x8, 0x10, 0x10, 0x10, 0x10, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x8, 0x10, 0x10, 0x10, 0x2, 0x8, 0x10, 0x18, 0x18, 0x2, 0x8, 0x10, 0x18, 0x20, 0x2, 0x8, 0x10, 0x18, 0x18, 0x2, 0x8, 0x10, 0x10, 0x10, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x10, 0x10, 0x10, 0x8, 0x2, 0x18, 0x18, 0x10, 0x8, 0x2, 0x20, 0x18, 0x10, 0x8, 0x2, 0x18, 0x18, 0x10, 0x8, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x18, 0x18, 0x18, 0x10, 0x8, 0x18, 0x20, 0x18, 0x10, 0x8, 0x18, 0x18, 0x18, 0x10, 0x8, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x8, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x18, 0x18, 0x18, 0x10, 0x10, 0x18, 0x20, 0x18, 0x10, 0x10, 0x18, 0x18, 0x18, 0x10, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x10, 0x18, 0x18, 0x18, 0x8, 0x10, 0x18, 0x20, 0x18, 0x8, 0x10, 0x18, 0x18, 0x18, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x8, 0x10, 0x10, 0x10, 0x2, 0x8, 0x10, 0x18, 0x18, 0x2, 0x8, 0x10, 0x18, 0x20, 0x2, 0x8, 0x10, 0x18, 0x18, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x10, 0x10, 0x10, 0x8, 0x2, 0x18, 0x18, 0x10, 0x8, 0x2, 0x20, 0x18, 0x10, 0x8, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x18, 0x18, 0x18, 0x10, 0x8, 0x18, 0x20, 0x18, 0x10, 0x8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x8, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x18, 0x18, 0x18, 0x10, 0x10, 0x18, 0x20, 0x18, 0x10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x10, 0x10, 0x10, 0x10, 0x8, 0x10, 0x18, 0x18, 0x18, 0x8, 0x10, 0x18, 0x20, 0x18, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8, 0x8, 0x8, 0x8, 0x2, 0x8, 0x10, 0x10, 0x10, 0x2, 0x8, 0x10, 0x18, 0x18, 0x2, 0x8, 0x10, 0x18, 0x20, 0x0 };
uint32_t _priceSkillPonders[35] = { 0x5020, 0x24010, 0x34000, 0x24010, 0x5020, 0x1003030, 0x1013020, 0x24010, 0x1013020, 0x1003030, 0x2002030, 0x2013010, 0x1013000, 0x2013010, 0x2002030, 0x2010130, 0x2013000, 0x3011000, 0x2013000, 0x2010130, 0x2001230, 0x3000220, 0x4100200, 0x3000220, 0x2001230, 0x3000240, 0x2210220, 0x3230312, 0x2210220, 0x3000240, 0x3000330, 0x2200221, 0x1220124, 0x2200221, 0x3000330 };
int8_t _quadrantCoordinatesToBallQuadrant[240] = { 0, 0, 0, 5, 5, 10, 10, 15, 15, 20, 20, 25, 25, 30, 30, 30, 0, 0, 0, 5, 5, 10, 10, 15, 15, 20, 20, 25, 25, 30, 30, 30, 0, 0, 0, 5, 5, 10, 10, 15, 15, 20, 20, 25, 25, 30, 30, 30, 1, 1, 1, 6, 6, 11, 11, 16, 16, 21, 21, 26, 26, 31, 31, 31, 1, 1, 1, 6, 6, 11, 11, 16, 16, 21, 21, 26, 26, 31, 31, 31, 1, 1, 1, 6, 6, 11, 11, 16, 16, 21, 21, 26, 26, 31, 31, 31, 2, 2, 2, 7, 7, 12, 12, 17, 17, 22, 22, 27, 27, 32, 32, 32, 2, 2, 2, 7, 7, 12, 12, 17, 17, 22, 22, 27, 27, 32, 32, 32, 2, 2, 2, 7, 7, 12, 12, 17, 17, 22, 22, 27, 27, 32, 32, 32, 3, 3, 3, 8, 8, 13, 13, 18, 18, 23, 23, 28, 28, 33, 33, 33, 3, 3, 3, 8, 8, 13, 13, 18, 18, 23, 23, 28, 28, 33, 33, 33, 3, 3, 3, 8, 8, 13, 13, 18, 18, 23, 23, 28, 28, 33, 33, 33, 4, 4, 4, 9, 9, 14, 14, 19, 19, 24, 24, 29, 29, 34, 34, 34, 4, 4, 4, 9, 9, 14, 14, 19, 19, 24, 24, 29, 29, 34, 34, 34, 4, 4, 4, 9, 9, 14, 14, 19, 19, 24, 24, 29, 29, 34, 34, 34 };
uint8_t _plPricesIncreaseTable[50] = { 0, 1, 2, 3, 4, 4, 5, 5, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
uint8_t _skillValuePonders[8] = { 0x0, 0x1, 0x3, 0x6, 0xA, 0xF, 0x15, 0x1C };

/* ======================================================================
 * STRUCTS  (on-disk SWOS layout -- offsets matter; do not reorder)
 * ====================================================================== */

/* One outfield slot's coverage: 5 ball-zones x 7 quadrants of packed x/y
 * (high nibble = x, low nibble = y). */
typedef struct { uint8_t positions[5][7]; } PlayerPositions;      /* 35 bytes */

/* A formation: name + 10 outfield slots + pairing ids + redirect. */
typedef struct {
    int8_t          name[9];               /* 0  */
    PlayerPositions positions[10];         /* 9  (10 x 35 = 350) */
    uint8_t         pairIds[10];           /* 359 */
    uint8_t         ballOutOfPlayTactics;  /* 369 */
} TeamTactics;                                                    /* 370 bytes */

/* One player record inside a team file (38 bytes). Skills are packed nibbles. */
typedef struct __attribute__((packed, aligned(2))) PlayerFile {
    int8_t   nationality;          /* 0  */
    int8_t   cachedMatchValue;     /* 1  runtime: last computed value (the cache) */
    int8_t   shirtNumber;          /* 2  */
    int8_t   playerName[23];       /* 3  */
    uint8_t  positionAndFace;      /* 26 bits 5-7 = position type, bit 1 = loan */
    uint8_t  cardsInjuries;        /* 27 */
    int8_t   passing;              /* 28 low nibble = passing (hi nibble 0) */
    int8_t   velocityHeading;      /* 29 hi = velocity, lo = heading */
    int8_t   tacklingControl;      /* 30 hi = tackling, lo = control */
    int8_t   speedFinishing;       /* 31 hi = speed,    lo = finishing */
    int8_t   price;                /* 32 base market price id */
    uint8_t  goalStats[4];         /* 33 */
    int8_t   form;                 /* 37 */
} PlayerFile;

/* Team file header (76 bytes) -- only a few fields feed the value calc. */
typedef struct __attribute__((packed, aligned(2))) TeamFile {
    uint8_t  countryNumber;      /* 0  */
    uint8_t  teamOrdinal;        /* 1  */
    int16_t  globalTeamNumber;   /* 2  */
    uint8_t  teamControls;       /* 4  TEAM_COMPUTER(1) skips crowding/nearness/injury */
    int8_t   teamName[17];       /* 5  */
    uint8_t  writesZeroHere;     /* 22 */
    uint8_t  andWith0xFE;        /* 23 */
    uint8_t  tactics;            /* 24 formation index into _teamTacticsTable[] */
    uint8_t  league;             /* 25 */
    uint8_t  pad26[10];          /* 26..35 kit colours (unused here) */
    int8_t   coachName[23];      /* 36 */
    uint8_t  playerValuesDirty;  /* 59 TEAM_VALUES_CLEAN(0) -> use cached value */
    uint8_t  playerNumbers[16];  /* 60 squad-slot -> file-index map */
} TeamFile;

/* Full team entry: 76-byte header + 16 players (684 bytes). */
typedef struct __attribute__((packed, aligned(2))) TeamFileEx {
    TeamFile   team;             /* 0   */
    PlayerFile players[16];      /* 76  */
} TeamFileEx;

/* ======================================================================
 * RUNTIME STATE the functions reference (you populate these).
 *   _teamTacticsTable[i] must point at the 370-byte formation for index i
 *   (the team's `tactics` field selects the row). The other globals are
 *   scratch/accumulators used during one GetPlayerPrice() call.
 * ====================================================================== */
TeamTactics *_teamTacticsTable[19];   /* fill [tacticsIdx] = &yourFormation     */
uintptr_t    _plPositions;            /* scratch: base of current slot coverage */
uint16_t     _totalSkillsFactorSum;   /* accumulator (the running 16-bit total) */
uint16_t     _numPlayersAtQuadrant;   /* scratch: crowding count                */

/* Used only by UpdatePlayerPositionAndReturnRole: */
TeamFileEx   _careerTeam;             /* the human's career team (slot remap)   */
uint8_t      _currentMatchPlayers[16];/* career squad-slot -> file-index map    */
uint8_t      _playerPositionsArray[16];/* output: per-slot value (the live values)*/
uint16_t     _inEditTactics;          /* mode flag passed to IsPlayerInjured     */

/* ---- LARGE (team-level) indicator state ----
 * The big tick/cross compares the whole-team skill total NOW against a snapshot
 * taken BEFORE the move: total rose -> green tick, total fell -> red cross.
 *
 * NAMING NOTE — these were once two generic skill registers the game reused for this
 * feature under back-to-front names: ComputeOldPlayerSkillForViewResult() writes the
 * LIVE (current/after) total, but it landed in a global called `_oldPlayerSkill`, while
 * the BEFORE snapshot lived in `_newPlayerSkill` (only ever `_new = _old`, never
 * recomputed). They have since been renamed in BOTH the game source and this tool to
 * what they actually hold:
 *     _teamSkillCurrent   (was _oldPlayerSkill)  -- live recompute = AFTER
 *     _teamSkillSnapshot  (was _newPlayerSkill)  -- the BEFORE baseline
 * The per-player array pair was always named correctly and is kept verbatim:
 *     _playerPositionsArray    = current per-player values
 *     _oldPlayerPositionsArray = the before snapshot. */
uint16_t _teamSkillCurrent;           /* live team total (just recomputed = AFTER)   */
uint16_t _teamSkillSnapshot;          /* the team total captured BEFORE the move     */
uint8_t  _oldPlayerPositionsArray[16];/* snapshot of _playerPositionsArray (per-player)*/
/* Enable-gate for the big mark. >= 0 hides it; < 0 (0xFFFF, the default the squad
 * screen installs for a human team) shows it. The game derives it per game-type/team
 * in ShowSquadMenuForTeam (SWOS2.s); kept here as a settable flag. */
uint16_t _teamSkillDiffAfterChange = 0xFFFF;
/* Internal accumulator of ComputeOldPlayerSkillForViewResult (empty-non-GK-slot count). */
static uint16_t oldPlayerSkillDivisor;

/* Tick/cross sprite indices (include/play_match_menu.h). BIG = whole-team mark,
 * SMALL = per-player mark; CHECK = green tick (value rose), CROSS = red cross (fell). */
#define SPR_CHECK_BIG     206
#define SPR_CROSS_BIG     207
#define SPR_CHECK_SMALL   208
#define SPR_CROSS_SMALL   209

/* Stub -- replace with your real injury check if you need the injured path.
 * (Returns 0 = not injured, so outfield values aren't forced to 0.) */
static uint16_t IsPlayerInjured(PlayerFile *pf, int16_t mode) { (void)pf; (void)mode; return 0; }

/* ======================================================================
 * CountNearnessFactor  (src/diy_competition.c) -- human teams only.
 * Scales the per-quadrant value by how many "near" quadrants the player still
 * covers at the destination vs the current ball quadrant.
 * NOTE: divides by nearCurCount; the original relies on it being > 0 here.
 * ====================================================================== */
uint16_t CountNearnessFactor(uint16_t curBallQuadrant, uint16_t dstBallQuadrant,
                              uint16_t price, const uint8_t *curQuadDistFactor,
                              const uint8_t *_playerPositions, const uint8_t *coordToQuad)
{
    const uint8_t *dstQuadTable = &_quadrantDistanceFactor[dstBallQuadrant * 35];
    uint16_t nearDstCount = 0;
    uint16_t nearCurCount = 0;
    for (int16_t i = 34; i >= 0; i--) {
        if ((uint16_t)i == curBallQuadrant)   continue;
        if (curQuadDistFactor[i] < 8)          continue;
        nearCurCount++;
        uint8_t playerPos = _playerPositions[i];
        uint8_t quad = coordToQuad[playerPos];
        if (dstQuadTable[quad] < 4)            continue;
        nearDstCount++;
    }
    uint32_t result = (uint32_t)price * nearDstCount;
    result = result / nearCurCount;            /* divu.w */
    return (uint16_t)result;
}

/* ======================================================================
 * GetPlayerPrice  (src/match_sim.c, SWOS2.s line 27392) -- the core value calc.
 *   teamFile     : the team header (provides `tactics` + `teamControls`)
 *   playerFile   : team base + slot*38 (the player skills are read at +76)
 *   playerNumber : 1-based slot number -> selects the formation slot coverage
 *   returns      : the player's value for that slot (price-softened, clamped)
 * Loops 35 ball quadrants, weighting skills by priceSkillPonders/skillValuePonders,
 * the position skill row, distance, crowding, and nearness; / 350 - 8; clamp 0..49;
 * then plPricesIncreaseTable softening once the value reaches the player's price.
 * ====================================================================== */
int16_t GetPlayerPrice(TeamFile *teamFile, PlayerFile *playerFile, uint16_t playerNumber)
{
    PlayerFile *plFile = &playerFile[2];      /* skip the 76-byte team header */
    TeamFile *tf = teamFile;

    uint16_t position = (plFile->positionAndFace & PF_POSITION_MASK) >> 5;
    const uint8_t *skillsPerQ = _skillsPerQuadrant - 35 + position * 35;

    uint32_t d5 = ((uint32_t)(uint8_t)plFile->passing << 24) |
                  ((uint32_t)(uint8_t)plFile->velocityHeading << 16) |
                  ((uint32_t)(uint8_t)plFile->tacklingControl << 8) |
                  (uint8_t)plFile->speedFinishing;
    d5 &= 0x07777777;
    d5 += 0x01111111;                         /* each skill nibble -> 1..8 */

    int16_t tacticsIdx = (int8_t)tf->tactics;
    uint8_t *tacticsData = (uint8_t *)_teamTacticsTable[tacticsIdx] + 9;
    _plPositions = (uintptr_t)tacticsData;

    uint16_t plNum0 = playerNumber - 1;
    uint8_t *playerTactics = tacticsData + plNum0 * 35;
    const uint8_t *distFactor = _quadrantDistanceFactor + 34 * 35;

    _totalSkillsFactorSum = 0;
    for (int16_t d3 = 34; d3 >= 0; d3--) {
        uint16_t quadPrice = 0;
        uint16_t ballQ = playerTactics[d3];
        ballQ = _quadrantCoordinatesToBallQuadrant[ballQ];

        if (tf->teamControls != TEAM_COMPUTER) {
            _numPlayersAtQuadrant = 1;
            if (d3 >= 5 && d3 < 30 && d3 != 7) {
                uint8_t *tacticsBase = (uint8_t *)_plPositions;
                uint16_t count = 0;
                for (int16_t i = 9; i >= 0; i--) {
                    uint16_t q = tacticsBase[d3];
                    q = _quadrantCoordinatesToBallQuadrant[q];
                    if ((uint8_t)q == (uint8_t)ballQ) count++;
                    tacticsBase += 35;
                }
                _numPlayersAtQuadrant = count;
            }
        }

        uint32_t ponderPacked = _priceSkillPonders[ballQ];
        uint32_t skillsCopy = d5;
        for (int16_t k = 6; k >= 0; k--) {
            uint16_t skill = skillsCopy & 0xF;
            uint16_t ponder = ponderPacked & 0xF;
            if (ponder != 0) {
                ponder = _skillValuePonders[ponder];
                quadPrice += skill * ponder;
            }
            skillsCopy >>= 4;
            ponderPacked >>= 4;
        }

        quadPrice >>= 1;
        uint16_t weight = (int8_t)skillsPerQ[ballQ];
        uint32_t temp = (uint32_t)quadPrice * (uint16_t)weight;
        uint16_t tempLow = (uint16_t)temp;     /* mulu.w 32-bit; next mulu uses low 16 */

        uint16_t dist = distFactor[ballQ];
        uint32_t temp2 = (uint32_t)tempLow * dist;
        temp2 >>= 3;

        if (tf->teamControls != TEAM_COMPUTER) {
            uint16_t n = _numPlayersAtQuadrant - 1;
            if (n != 0) temp2 = temp2 / n;     /* divu.w */
        }

        uint16_t d7val = (uint16_t)temp2;
        if (tf->teamControls != TEAM_COMPUTER) {
            d7val = CountNearnessFactor(d3, ballQ, d7val,
                                         distFactor, playerTactics,
                                         (const uint8_t *)_quadrantCoordinatesToBallQuadrant);
        }

        _totalSkillsFactorSum += d7val;
        distFactor -= 35;
    }

    int16_t result = (int16_t)_totalSkillsFactorSum;
    int32_t extended = (int32_t)result;        /* ext.l before divu.w #350 */
    uint16_t priceIdx = (uint16_t)((uint32_t)extended / 350);
    priceIdx -= 8;
    if ((int16_t)priceIdx < 0)      priceIdx = 0;
    else if (priceIdx > 49)         priceIdx = 49;

    uint8_t filePriceByte = (uint8_t)plFile->price;
    uint8_t priceIdxLow   = (uint8_t)priceIdx;
    int8_t  byteDiff      = (int8_t)(priceIdxLow - filePriceByte);
    if (byteDiff < 0)
        return priceIdx;                       /* below the player's price: raw */

    uint16_t diffIdx  = (uint8_t)(priceIdxLow - filePriceByte);
    uint8_t  increase = _plPricesIncreaseTable[diffIdx];   /* softened gain */
    return (int16_t)(uint8_t)(increase + filePriceByte);
}

/* ======================================================================
 * UpdatePlayerPositionAndReturnRole  (src/match_sim.c, SWOS2.s line 26116)
 * Picks the value path (GK / outfield / sub / clean-cache / injured / loan),
 * calls GetPlayerPrice() for outfielders, caches it in cachedMatchValue and
 * _playerPositionsArray[slot], and returns (role << 16) | (value & 0xFFFF).
 * For the squad-screen tick/cross of an OUTFIELD swap this is the entry point;
 * the value it returns is what gets compared before vs after the move.
 * ====================================================================== */
uint32_t UpdatePlayerPositionAndReturnRole(TeamFileEx *team, uint16_t playerIdx)
{
    int16_t slotIdx;
    if (team == &_careerTeam) slotIdx = (int8_t)_currentMatchPlayers[playerIdx];
    else                      slotIdx = (int8_t)team->team.playerNumbers[playerIdx];
    int16_t d6 = slotIdx;

    uint8_t *playerBase = (uint8_t *)team + (uint16_t)((uint16_t)slotIdx * PLAYERFILE_SIZE);
    PlayerFile *pf = (PlayerFile *)(playerBase + sizeof(TeamFile));

    if (team->team.playerValuesDirty == TEAM_VALUES_CLEAN) {
        int16_t  pos  = (int8_t)pf->cachedMatchValue;
        uint16_t role = (pf->positionAndFace & PF_POSITION_MASK) >> 5;
        _playerPositionsArray[d6] = (uint8_t)pos;
        return ((uint32_t)role << 16) | ((uint16_t)pos & 0xFFFF);
    }

    uint16_t posType = (pf->positionAndFace & PF_POSITION_MASK) >> 5;
    int16_t d7val;
    if (playerIdx == 0) {
        if (posType == 0) d7val = (int8_t)pf->price;
        else            { d7val = 0; goto storeResult; }
    } else if (playerIdx < 11) {
        if (posType == 0) { d7val = 0; goto storeResult; }
        else  d7val = GetPlayerPrice(&team->team, (PlayerFile *)playerBase, playerIdx);
    } else {
        d7val = (int8_t)pf->price;
    }

    if (d7val == 0) d7val = 1;

    if (team->team.teamControls != TEAM_COMPUTER) {
        if (pf->positionAndFace & PF_LOAN) d7val = 1;
        if (IsPlayerInjured((PlayerFile *)playerBase, (int16_t)_inEditTactics)) d7val = 0;
    }

storeResult:
    {
        uint16_t posVal = (uint16_t)(int16_t)d7val;
        pf->cachedMatchValue = (int8_t)(uint8_t)posVal;
        _playerPositionsArray[d6] = (uint8_t)posVal;
        uint16_t role = (pf->positionAndFace & PF_POSITION_MASK) >> 5;
        return ((uint32_t)role << 16) | (posVal & 0xFFFF);
    }
}

/* ======================================================================
 * ComputeOldPlayerSkillForViewResult  (src/match_sim.c, SWOS2.s line 47020)
 *                    -- the LARGE / whole-team value.
 *   team       : the team (provides the slot->file map + tactics)
 *   goalsCount : number of substitute slots to fold in (the game passes
 *                _maxSubstitutesEntries, typically 5; 0 skips the subs pass)
 *   returns    : the team's overall skill value, ALSO stored in _teamSkillCurrent.
 *
 * Walks every player through UpdatePlayerPositionAndReturnRole (which refills
 * _playerPositionsArray as a side effect, so the SMALL marks work straight after):
 *   - subs pass (file slots 11..11+goalsCount-1): value*4 for an outfielder, *1 for
 *     a GK, accumulated with a running weight d2, then averaged: (d2/2 + sum)/d2;
 *   - main pass (file slots 0..10): add each raw value; an empty non-GK slot bumps
 *     the divisor (team weakened);
 *   - result = sum / 15; halve it when a non-GK is filling the goalkeeper slot;
 *     then divide by the empty-slot divisor.
 *
 * NOTE on the cache: UpdatePlayerPositionAndReturnRole returns the CACHED value when
 * team.playerValuesDirty == TEAM_VALUES_CLEAN, and this function sets CLEAN at the
 * end. So to force a real recompute after a change, set
 * team->team.playerValuesDirty = TEAM_VALUES_DIRTY before calling again (the game's
 * commit paths do exactly this).
 * (Writes _teamSkillCurrent, the live/after team total -- see the NAMING NOTE.)
 * ====================================================================== */
uint16_t ComputeOldPlayerSkillForViewResult(TeamFileEx *team, uint16_t goalsCount)
{
    oldPlayerSkillDivisor = 1;
    uint16_t d5 = 0;
    uint16_t d6 = goalsCount;

    if (d6 != 0) {
        int16_t  d3 = (int16_t)d6 - 1;
        uint16_t d2 = 0;
        uint16_t d7 = 11;                       /* substitutes start at file slot 11 */
        for (; d3 >= 0; d3--) {
            uint32_t res       = UpdatePlayerPositionAndReturnRole(team, d7);
            uint16_t posValue  = (uint16_t)(res & 0xFFFF);
            uint16_t roleValue = (uint16_t)((res >> 16) & 0xFFFF);
            uint16_t scaled = posValue << 2;    /* lsl.w #2 -> weight x4 */
            d2 += 4;
            if (roleValue == 0) {               /* goalkeeper: undo the x4 weighting */
                scaled = posValue;
                d2 -= 3;
            }
            d5 += scaled;
            d7++;
        }
        uint16_t half = d2 >> 1;
        uint32_t sum  = (uint32_t)(half + d5);
        d5 = (uint16_t)(sum / d2);              /* subs folded into one rounded average */
    }

    for (uint16_t d7 = 0; d7 < 11; d7++) {      /* the 11 outfield slots */
        uint32_t res       = UpdatePlayerPositionAndReturnRole(team, d7);
        uint16_t posValue  = (uint16_t)(res & 0xFFFF);
        uint16_t roleValue = (uint16_t)((res >> 16) & 0xFFFF);
        if (posValue == 0 && roleValue != 0)    /* empty non-GK slot weakens the team */
            oldPlayerSkillDivisor++;
        d5 += posValue;
    }

    uint16_t result = (uint16_t)((uint32_t)d5 / 15);

    /* Goalkeeper-slot penalty: if file slot 0 holds a non-GK, halve the result. */
    uint8_t *gkPlayerNums = (team == &_careerTeam)
                          ? _currentMatchPlayers : team->team.playerNumbers;
    int16_t     gkIdx  = (int8_t)gkPlayerNums[0];
    PlayerFile *gkRec  = &team->players[(uint16_t)gkIdx];
    uint16_t    gkRole = (gkRec->positionAndFace & PF_POSITION_MASK) >> 5;
    if (gkRole != 0) result >>= 1;

    result = (uint16_t)((uint32_t)result / oldPlayerSkillDivisor);
    _teamSkillCurrent = result;
    team->team.playerValuesDirty = TEAM_VALUES_CLEAN;
    return result;
}

/* ======================================================================
 * TICK / CROSS  -- the squad / tactics-screen comparison (small + LARGE).
 *
 * Two indicators, both answering "did this move help?":
 *   SMALL (per-player) : each outfield player's value now vs its before-snapshot.
 *   LARGE (whole-team) : the team total (ComputeOldPlayerSkillForViewResult) now vs its snapshot.
 * In both: value ROSE -> green tick, value FELL -> red cross, equal -> no mark.
 *
 * Typical use:
 *   ComputeOldPlayerSkillForViewResult(team, 5);                       // current values
 *   SwosSnapshotBaseline();                          // freeze them as the "before"
 *   ... move a player / edit the formation ...
 *   team->team.playerValuesDirty = TEAM_VALUES_DIRTY;// force a real recompute
 *   ComputeOldPlayerSkillForViewResult(team, 5);                       // refills _playerPositionsArray
 *   SwosBigMark();                                   // whole-team tick/cross
 *   SwosSmallMark(fileIdx);                          // per-player tick/cross
 * ====================================================================== */
typedef enum { SWOS_NO_MARK = 0, SWOS_GREEN_TICK = 1, SWOS_RED_CROSS = 2 } SwosMark;

SwosMark SwosMarkForChange(int valueBefore, int valueAfter)
{
    if (valueAfter > valueBefore) return SWOS_GREEN_TICK;
    if (valueAfter < valueBefore) return SWOS_RED_CROSS;
    return SWOS_NO_MARK;
}

/* Freeze the current team total + per-player values as the "before" baseline
 * (mirrors SquadMenuSavePlayerPositions / EditTacticsMenuSavePlayerPositions:
 * `_newPlayerSkill = _oldPlayerSkill` + copy the positions array). */
void SwosSnapshotBaseline(void)
{
    _teamSkillSnapshot = _teamSkillCurrent;
    for (int i = 0; i < 16; i++)
        _oldPlayerPositionsArray[i] = _playerPositionsArray[i];
}

/* SMALL mark: one outfield player's value (file index; the game marks only the 11
 * outfield slots) now vs its before-snapshot. */
SwosMark SwosSmallMark(int fileIdx)
{
    if (fileIdx < 0 || fileIdx >= 16) return SWOS_NO_MARK;
    return SwosMarkForChange(_oldPlayerPositionsArray[fileIdx], _playerPositionsArray[fileIdx]);
}

/* LARGE mark: the whole-team total now (_teamSkillCurrent) vs the snapshot
 * (_teamSkillSnapshot). Honours the enable-gate: _teamSkillDiffAfterChange >= 0
 * hides the mark (mirrors UpdateBigCheckMark; the squad screen sets it to 0xFFFF =
 * "show" for a human team, see ShowSquadMenuForTeam). */
SwosMark SwosBigMark(void)
{
    if ((int16_t)_teamSkillDiffAfterChange >= 0) return SWOS_NO_MARK;
    return SwosMarkForChange(_teamSkillSnapshot, _teamSkillCurrent);
}

/* Map a mark to its MENUS.bmp sprite index, big or small (0 = nothing drawn). */
int SwosMarkSprite(SwosMark m, int big)
{
    if (m == SWOS_GREEN_TICK) return big ? SPR_CHECK_BIG : SPR_CHECK_SMALL;
    if (m == SWOS_RED_CROSS)  return big ? SPR_CROSS_BIG : SPR_CROSS_SMALL;
    return 0;
}

#ifdef SWOS_PV_DEMO
/* ----------------------------------------------------------------------
 * Demo: put one defender in formation slot 1 vs slot 2 of the shipped 4-4-2
 * and show the value + the tick/cross. Skill bytes are packed nibbles
 * (see PlayerFile comments). Replace with real team-file bytes to reproduce
 * a specific case.
 * -------------------------------------------------------------------- */
static const uint8_t demo_442[370] = {
    0x34, 0x34, 0x32, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x10, 0x21, 0x31,
    0x31, 0x42, 0x13, 0x22, 0x22, 0x32, 0x42, 0x14, 0x14, 0x24, 0x34, 0x34,
    0x17, 0x25, 0x24, 0x24, 0x35, 0x17, 0x27, 0x26, 0x27, 0x36, 0x19, 0x29,
    0x28, 0x38, 0x37, 0x1A, 0x2A, 0x3A, 0x28, 0x18, 0x50, 0x50, 0x70, 0x70,
    0x70, 0x41, 0x61, 0x60, 0x60, 0x70, 0x61, 0x61, 0x61, 0x62, 0x62, 0x42,
    0x52, 0x62, 0x63, 0x73, 0x44, 0x43, 0x53, 0x63, 0x73, 0x44, 0x54, 0x54,
    0x54, 0x64, 0x57, 0x57, 0x57, 0x57, 0x57, 0x70, 0x70, 0x80, 0x90, 0x90,
    0x70, 0x80, 0x81, 0x81, 0xA1, 0x82, 0x82, 0x82, 0x81, 0x81, 0x73, 0x83,
    0x83, 0x92, 0xA2, 0x73, 0x74, 0x84, 0x94, 0xA4, 0x85, 0x95, 0x95, 0x95,
    0xA5, 0x88, 0x88, 0x88, 0x88, 0x88, 0xA2, 0xB1, 0xB1, 0xC1, 0xD0, 0xA2,
    0xB2, 0xC2, 0xC2, 0xD3, 0xB4, 0xB4, 0xC4, 0xD4, 0xD4, 0xB5, 0xC4, 0xC4,
    0xC5, 0xD7, 0xB6, 0xC7, 0xC6, 0xC7, 0xD7, 0xB7, 0xB8, 0xC8, 0xC9, 0xD9,
    0xD8, 0xC8, 0xBA, 0xCA, 0xDA, 0x33, 0x43, 0x44, 0x44, 0x54, 0x16, 0x24,
    0x25, 0x34, 0x44, 0x18, 0x16, 0x17, 0x27, 0x37, 0x19, 0x09, 0x18, 0x28,
    0x39, 0x1A, 0x3A, 0x1A, 0x2A, 0x2A, 0x1B, 0x1D, 0x2B, 0x3B, 0x3B, 0x1D,
    0x3D, 0x3D, 0x3D, 0x3C, 0x51, 0x71, 0x60, 0x71, 0x91, 0x52, 0x52, 0x71,
    0x92, 0x92, 0x43, 0x53, 0x73, 0x93, 0xA3, 0x45, 0x55, 0x75, 0x95, 0xA5,
    0x47, 0x57, 0x77, 0x97, 0xA7, 0x49, 0x69, 0x79, 0x89, 0xA9, 0x4A, 0x5B,
    0x7B, 0x9B, 0xAA, 0x54, 0x64, 0x73, 0x84, 0x94, 0x54, 0x64, 0x74, 0x84,
    0x94, 0x55, 0x66, 0x76, 0x86, 0x95, 0x58, 0x68, 0x77, 0x88, 0x98, 0x4A,
    0x6A, 0x79, 0x8A, 0xAA, 0x5B, 0x4B, 0x7B, 0xAB, 0x9B, 0x8D, 0x7D, 0x7D,
    0x7D, 0x6D, 0x94, 0xA4, 0xA4, 0xA3, 0xB3, 0xA4, 0xB4, 0xC5, 0xC4, 0xD6,
    0xB7, 0xC7, 0xD7, 0xD6, 0xD8, 0xB9, 0xC8, 0xD8, 0xE9, 0xD9, 0xCA, 0xCA,
    0xDA, 0xBA, 0xDA, 0xBB, 0xBB, 0xCB, 0xDD, 0xDB, 0xBC, 0xBD, 0xBD, 0xBD,
    0xDD, 0x24, 0x44, 0x54, 0x65, 0x85, 0x27, 0x47, 0x57, 0x58, 0x78, 0x29,
    0x39, 0x49, 0x6A, 0x7A, 0x3A, 0x4A, 0x4B, 0x5B, 0x6B, 0x3C, 0x4C, 0x5D,
    0x6D, 0x7D, 0x5E, 0x5E, 0x6E, 0x7E, 0x7E, 0x5F, 0x6F, 0x6F, 0x6F, 0x7F,
    0x65, 0x85, 0x95, 0xA4, 0xC4, 0x78, 0x98, 0x98, 0xA7, 0xC7, 0x7A, 0x8A,
    0xAA, 0xB9, 0xC9, 0x8B, 0x9B, 0xAB, 0xAA, 0xBA, 0x7D, 0x8D, 0x9D, 0xAC,
    0xBC, 0x7E, 0x7E, 0x8E, 0x9E, 0x9E, 0x7F, 0x8F, 0x8F, 0x8F, 0x9F, 0x00,
    0xFF, 0xFF, 0x00, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0x00,
};

static void set_skills(PlayerFile *p, int pass,int vel,int head,int tack,int ctrl,int speed,int fin,
                       int positionType, int price)
{
    p->passing         = (int8_t)(pass & 0xF);
    p->velocityHeading = (int8_t)(((vel & 0xF) << 4) | (head & 0xF));
    p->tacklingControl = (int8_t)(((tack & 0xF) << 4) | (ctrl & 0xF));
    p->speedFinishing  = (int8_t)(((speed & 0xF) << 4) | (fin & 0xF));
    p->positionAndFace = (uint8_t)((positionType & 7) << PF_POSITION_SHIFT);
    p->price           = (int8_t)price;
}

/* value of the player whose skills live at file index `fileIdx`, evaluated for
 * formation slot `slotNumber` (1-based) of the team's tactic. */
static int value_in_slot(TeamFileEx *tfe, int fileIdx, int slotNumber)
{
    PlayerFile *base = (PlayerFile *)((uint8_t *)tfe + fileIdx * PLAYERFILE_SIZE);
    return GetPlayerPrice(&tfe->team, base, (uint16_t)slotNumber);
}

/* Fill a full demo squad: identity slot->file map, a GK in slot 0, ten outfielders
 * in a 4-4-2 shape (files 1..10), five subs (files 11..15). Skills are illustrative
 * 0..7 nibbles. Used by the LARGE (whole-team) tick/cross demo below. */
static void fill_demo_team(TeamFileEx *t)
{
    for (int i = 0; i < 16; i++) t->team.playerNumbers[i] = (uint8_t)i;   /* slot i -> file i */
    set_skills(&t->players[0], 3,3,3,3,4,3,2, /*GK type 0*/0, /*price*/20);
    static const int posType[10] = { 1,3,3,2,  6,6,6,6,  7,7 };           /* RB CD CD LB  MF*4  ST ST */
    for (int i = 1; i <= 10; i++)
        set_skills(&t->players[i], 4,4,4,4,4,4,4, posType[i - 1], /*price*/30);
    for (int i = 11; i < 16; i++)
        set_skills(&t->players[i], 3,3,3,3,3,3,3, /*MF*/6, /*price*/25);
}

int main(void)
{
    const char *mk[] = { "no mark", "GREEN TICK", "RED CROSS" };
    _teamTacticsTable[0] = (TeamTactics *)demo_442;   /* tactic index 0 = 4-4-2 */

    /* ---- SMALL (per-player) mark: one CD in formation slot 1 vs slot 2 ---- */
    static TeamFileEx tfe;          /* zeroed */
    tfe.team.tactics      = 0;      /* use 4-4-2 */
    tfe.team.teamControls = 0;      /* human (not TEAM_COMPUTER) -> full calc */

    /* a central defender at file index 3 (skills are illustrative 0..7 nibbles) */
    set_skills(&tfe.players[3], /*pass*/4,/*vel*/3,/*head*/7,/*tack*/7,/*ctrl*/6,/*speed*/7,/*fin*/3,
               /*positionType CD=3*/3, /*price*/37);

    int v1 = value_in_slot(&tfe, 3, 1);   /* same player, formation slot 1 */
    int v2 = value_in_slot(&tfe, 3, 2);   /* same player, formation slot 2 */
    printf("CD value in slot 1 = %d\n", v1);
    printf("CD value in slot 2 = %d\n", v2);
    printf("moving him from slot 1 to slot 2 -> %s\n", mk[SwosMarkForChange(v1, v2)]);

    /* ---- LARGE (whole-team) mark: boost a midfielder, recompute the team total ---- */
    static TeamFileEx team;         /* zeroed */
    team.team.tactics      = 0;     /* 4-4-2 */
    team.team.teamControls = 0;     /* human */
    fill_demo_team(&team);
    _teamSkillDiffAfterChange = 0xFFFF;   /* enable the big mark (human-team default) */

    team.team.playerValuesDirty = TEAM_VALUES_DIRTY;
    int tBefore = ComputeOldPlayerSkillForViewResult(&team, 5);   /* current team total (fills the value array) */
    SwosSnapshotBaseline();                     /* freeze it as the "before" */

    set_skills(&team.players[6], 7,7,7,7,7,7,7, /*MF*/6, 30);   /* improve a midfielder */
    team.team.playerValuesDirty = TEAM_VALUES_DIRTY;            /* force a real recompute */
    int tAfter = ComputeOldPlayerSkillForViewResult(&team, 5);

    SwosMark big   = SwosBigMark();
    SwosMark small = SwosSmallMark(6);
    printf("\nteam skill before  = %d\n", tBefore);
    printf("team skill after   = %d\n", tAfter);
    printf("BIG team mark      -> %s (sprite %d)\n", mk[big],   SwosMarkSprite(big, 1));
    printf("SMALL mark file 6  -> %s (sprite %d)\n", mk[small], SwosMarkSprite(small, 0));
    return 0;
}
#endif
