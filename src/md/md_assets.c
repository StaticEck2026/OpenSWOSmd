/* md_assets.c -- the engine's asset loading, which on the cartridge is the ROM. */
#include "md_hw.h"
#include "md_game.h"
#include "game_types.h"

extern void md_pitch_select(uint16_t n);

/* LoadPitchMap (src/pitch.c): read grafs/swcpichN.map. The ROM holds all six pitches. */
void LoadPitchMap(uint16_t mapIndex) { md_pitch_select(mapIndex); }
/* FixupAndColorPitchFile (src/pitch.c): unpack the tiles, colour the pitch type. The
 * renderer does both from the ROM's tiles (md_render_init_match). */
void FixupAndColorPitchFile(void) {}
/* ProcessImageCommandStream (src/image_cmd.c) cuts the sprite sheets into sprites: the
 * ROM's sprite table is that result, made by scripts/md/make_md_data.py. */
void ProcessImageCommandStream(const uint16_t *cmdBuffer, uint8_t *workBuffer) { (void)cmdBuffer; (void)workBuffer; }
void JagGfx_NewMatch(void) {}
void JagGfx_PitchApplied(void) {}

/* ReplayExitMenuAfterFriendly (src/replay_menu.c): the "REPLAY / EXIT" menu after a
 * friendly. The cartridge's front end asks its own question after the match. */
extern uint16_t _replaySelected;
void ReplayExitMenuAfterFriendly(void) { _replaySelected = 0; }
