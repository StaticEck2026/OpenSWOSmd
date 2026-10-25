/* md_engine_glue.c -- what the shared match engine calls that the Mega Drive does its own way.
 *
 * The engine (the src/*.c files in Makefile.md's ENGINE_SRCS) is the Amiga game's match:
 * it reaches out to the menu system, the music player, the career's team cache and the
 * Amiga display at a handful of points. Those points are here. Each one says what the
 * original did and what the cartridge does instead; none of them changes the match.
 */
#include "md_hw.h"
#include "md_game.h"
#include "game_types.h"

uint16_t _panicReasonCode;
uint16_t _cGetPlayerAtIndexFileIdx;
uint8_t  _stadiumMenu[2];      /* ShowMenu(_stadiumMenu): the pre-match stadium screen */

extern void ViewMatchSequence(void);

/* ---- menus: the cartridge's front end is md_frontend.c, not SWOS's menu system ---- */
void ShowMenu(uint8_t *packedMenu) { (void)packedMenu; md_stadium_screen(); }
void DrawMenu(void) {}
void DrawMenuTextCentered(void) {}

/* highlights_menu.c's ShowMatchSequence saves the menu, plays the match and restores the
 * menu; there is no menu to save here. */
void ShowMatchSequence(void)
{
    md_match_enter();
    ViewMatchSequence();
    md_match_leave();
}

/* ---- music: the menu tune ---- */
void InitMenuMusic(void) {}
void StopMusic(void) {}
void TryPlayMusic(void) {}

/* ---- the team cache: a cartridge match has exactly its two teams ---- */
TeamFileEx *GetTeamPointer(uint16_t teamNumber)
{
    return md_team_by_id(teamNumber);
}

/* VerifyTeamLoaded (src/career_teams.c): the team must be one the match has, or the
 * original panics -- and so does the cartridge. */
TeamFileEx *VerifyTeamLoaded(uint16_t teamNum)
{
    TeamFileEx *t = md_team_by_id(teamNum);
    if (!t) md_panic("VerifyTeamLoaded: team %04X", teamNum);
    return t;
}

/* Int2Ascii (src/menu_system.c): a number in decimal, no leading zeros, terminated. */
void Int2Ascii(uint16_t value, uint8_t *out)
{
    uint16_t d0 = value;
    if (d0 >= 1000) { *out++ = (uint8_t)(d0 / 1000 + '0'); d0 %= 1000; }
    if (value >= 100) { *out++ = (uint8_t)(d0 / 100 + '0'); d0 %= 100; }
    if (value >= 10) { *out++ = (uint8_t)(d0 / 10 + '0'); d0 %= 10; }
    *out++ = (uint8_t)(d0 + '0');
    *out = 0;
}
