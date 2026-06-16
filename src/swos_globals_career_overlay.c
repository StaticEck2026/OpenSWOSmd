/* ================================================================
 * swos_globals_career_overlay.c
 *
 * The careerFileBuffer overlay is RETIRED.
 *
 * On the Amiga the 338 labels between `careerFileBuffer:` and
 * `numSelectedTeams:` sat in one contiguous `ds.b` region, and the
 * legacy `.CAR` save was a raw byte dump of that region. The data
 * untangle (roadmap step 3) extracted every one of those labels into
 * its own standalone, typed struct — careerFinances, careerStatusFlags,
 * careerSeasonRecord, careerSettings, the 5 CompetitionSlotBuffers,
 * selected_teams.c, career_team_buffers.c, … — and the save format
 * (roadmap step 4) became typed JSON of NAMED fields
 * (save_format_json.c), with no raw region dump.
 *
 * After all that, the only two values that still physically lived in
 * the old overlay run were:
 *   - careerFileBuffer (uint16): the TMD team-pool count (offset 0).
 *   - numSelectedTeams (uint16): the count of selected teams.
 * Both are saved/loaded as named JSON scalars (tmd_team_count /
 * num_selected_teams), so nothing depends on them being contiguous,
 * on their relative order, or on a careerFileBufferBuf carrier
 * existing. They are now plain standalone globals, and the 256 KB
 * backing buffer + its `.skip` hole sequence are gone.
 *
 * (History: this file used to emit a `careerFileBufferBuf` BSS block
 * with each overlay label as a real label at its computed offset, and
 * 74 `.skip` holes where pilots had been extracted. See git history
 * and docs/COMPETITION_SLOT_ALIAS_UNTANGLE.md §8 for that layout.)
 * ================================================================ */

#include "swos_types.h"     /* uint16_t */

/* TMD team-pool count. Manipulated as a plain count (career_teams.c
 * increments/decrements it); saved as the JSON `tmd_team_count` scalar. */
uint16_t careerFileBuffer = 0;

/* Count of selected teams. Saved as `num_selected_teams` and re-derived
 * on .DIY/.PRE load from primaryCompetitionBuf.tourn.header.numTeams. */
uint16_t numSelectedTeams = 0;
