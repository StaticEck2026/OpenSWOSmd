/* tactics_menu.c — Tactics selection menu callback functions.
 *
 * Chunk 80 (partial): 7 exported functions ported from SWOS2.s lines 86791-86831.
 */

#include "tactics_menu.h"
#include "menu_system.h"

/* ---- ASM externs ---- */
extern uint16_t chosenTactics;

/* Tactic ID constants */
#define TACTIC_USER_A  12
#define TACTIC_USER_B  13
#define TACTIC_USER_C  14
#define TACTIC_USER_D  15
#define TACTIC_USER_E  16
#define TACTIC_USER_F  17

/* ================================================================
 * User_A_Tactics  (SWOS2.s line 86793)
 * Sets chosenTactics to USER_A and exits menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void _User_A_Tactics(void)
{
    chosenTactics = TACTIC_USER_A;
    _SetExitMenuFlag();
}

/* ================================================================
 * User_B_Tactics  (SWOS2.s line 86799)
 * ================================================================ */
__attribute__((used, externally_visible))
void _User_B_Tactics(void)
{
    chosenTactics = TACTIC_USER_B;
    _SetExitMenuFlag();
}

/* ================================================================
 * User_C_Tactics  (SWOS2.s line 86805)
 * ================================================================ */
__attribute__((used, externally_visible))
void _User_C_Tactics(void)
{
    chosenTactics = TACTIC_USER_C;
    _SetExitMenuFlag();
}

/* ================================================================
 * User_D_Tactics  (SWOS2.s line 86811)
 * ================================================================ */
__attribute__((used, externally_visible))
void _User_D_Tactics(void)
{
    chosenTactics = TACTIC_USER_D;
    _SetExitMenuFlag();
}

/* ================================================================
 * User_E_Tactics  (SWOS2.s line 86817)
 * ================================================================ */
__attribute__((used, externally_visible))
void _User_E_Tactics(void)
{
    chosenTactics = TACTIC_USER_E;
    _SetExitMenuFlag();
}

/* ================================================================
 * User_F_Tactics  (SWOS2.s line 86823)
 * ================================================================ */
__attribute__((used, externally_visible))
void _User_F_Tactics(void)
{
    chosenTactics = TACTIC_USER_F;
    _SetExitMenuFlag();
}

/* ================================================================
 * ExitChooseTacticsMenu  (SWOS2.s line 86829)
 * Sets chosenTactics to -1 (abort) and exits menu.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ExitChooseTacticsMenu(void)
{
    chosenTactics = 0xFFFF;
    _SetExitMenuFlag();
}
