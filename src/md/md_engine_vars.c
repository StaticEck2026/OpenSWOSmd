/* md_engine_vars.c -- shared-engine globals the Mega Drive defines smaller than the Amiga.
 *
 * _selectedTeams is the career's 100-team cache (68 KB) on every other target. A cartridge
 * match reads it only for the "all player teams equal" price average, over the
 * _numSelectedTeams teams in it -- zero here -- so it is one record. Defined without
 * game_types.h, whose extern gives it 100 records, so the two can differ.
 */
#include <stdint.h>

uint16_t _numSelectedTeams;
const uint8_t _selectedTeams[684] __attribute__((aligned(2)));   /* sizeof(TeamFileEx) */

/* SDL settings the shared code tests: the Mega Drive plays the Amiga game. */
int _gDosGameplay;
int _gDosView;
uint16_t _quitToDos;

/* Career state the friendly-match path names but never uses outside a career. The two
 * team buffers are compared against (`team == &_careerTeam`), and so need addresses of
 * their own (in ROM: nothing writes them outside a career); the competition buffers and wage tables are read only in a career, so they
 * are zeros in ROM rather than 7.5 KB of RAM. */
const uint8_t _careerTeam[684] __attribute__((aligned(2)));
const uint8_t _currentTeam[684] __attribute__((aligned(2)));
const uint8_t _primaryCompetitionBuf[3750] __attribute__((aligned(2)));
const uint8_t _competitionContextBuf[3750] __attribute__((aligned(2)));
const uint8_t _playerFormModifiers[32];
const int32_t _weeklyWageByPlayerIndex[16];
int32_t  _weeklyPlayerWages;
uint16_t _wageScalingDivisor = 1;
uint16_t _isNationalSelector;
uint16_t _currentSeasonShowing;
