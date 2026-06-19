/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* play_match_menu.c — Play Match Menu UI functions
 *
 * Chunk 98: PlayMatchMenuInit, PlayMatchMenuReinit, SetCurrentSelectedPlayerIndexPointer,
 * OnSwapPlayer, PlayMatchMenuSwapPlayers, HandleSquadSwapSelectionToggle,
 * PlayMatchChangeTactics, ExitPlayMatch, PlayMatchSelected, EditTacticsInPlayMatchMenu,
 * ViewOpponentsSelect, ReturnFromViewOpponent, PlayMatchOnSelect30, PlayMatch30AfterDraw,
 * UpdateTeamCheckmark, SetGameStartButtonText, ShowOrHidePlayMatchButton,
 * SetUpTacticsButtons, SetPlayerSurnamesOnSidePitch, ShowUseMatchTeamIfTraining,
 * SetVariableButtons, DrawPlayerEntries, RedrawPlayerNames, UpdateLittleCheckmark,
 * SavePositionsAndUpdateLittleCheckmark, UpdateCheckMarkSprite, UseMatchTeamSelect,
 * TogglePlayerCoach, GetPlayerPosition.
 */

#include "play_match_menu.h"
#include "endian_helpers.h"
#include "menu_system.h"
#include "game_types.h"

/* Prevent GCC from converting byte loops to memcpy/memset (no libc) */
#pragma GCC optimize("no-tree-loop-distribute-patterns")

/* ---- Assembly externs (globals in SWOS2.s) ---- */
extern MenuEntry menuEntries[];
extern uint16_t fireResetFlag;
extern uint32_t irqCookieObfuscated;
extern uint16_t player1ClearFlag;
extern uint16_t player2ClearFlag;
extern uint16_t exitGameFlag;
extern uint16_t isCareer;
extern uint16_t longFireFlag;
extern uint8_t  pl1Fire;
extern uint8_t  pl2Fire;
extern uint16_t numPlayers;
extern uint16_t careerNumPlayers;
extern uint16_t gameType;
extern uint16_t trainingGame;
extern uint16_t coachOrPlayer;
extern uint16_t currentTeamNumber;
extern uint16_t selTeamNumber;
extern uint16_t panicReasonCode;
extern uint16_t oldPlayerSkill;
extern uint16_t newPlayerSkill;

extern TeamFileEx currentTeam;
extern TeamFileEx careerTeam;
extern uint8_t selTeamsPtr;
extern uint8_t playerPositions[];
extern uint8_t currentMatchPlayers[];
extern uint8_t playerPositionsArray[];
extern uint8_t oldPlayerPositionsArray[];
extern void *teamTacticsTable[];
extern void *positionsTable[];
extern void *countriesTable[];

/* Globals defined in play match menu data section of SWOS2.s */
extern uint16_t showingCpuTeams;
extern uint16_t squadChangesAllowed;
extern TeamFileEx *playMatchTeam1Ptr;
extern TeamFileEx *playMatchTeam2Ptr;
extern uint16_t maxSubstitutesEntries;
extern uint16_t pmMenuNumPlayers;
extern uint16_t numPlayersToShowInMenu;
extern uint16_t viewingOpponentTeam;
extern uint16_t is_A_team;
extern int16_t  selectedPlayerPosition;
extern int16_t  selectedPlayerTacticsPosition;
extern uintptr_t pmSelectedPlayerEntry;
extern uint16_t playMatchControlFlags;
extern uint16_t playerNumThatStarted;
extern int16_t  playMatchExcludeFlags;
extern uint16_t playMatchButtonVisibilityState;
extern uint16_t careerTeamCoachOnly;
extern uint16_t isFirstPlayerChoosingTeam;
extern uintptr_t currentSelectedPlayerIndexPtr;
extern uint16_t currentTeamSelectedPlayerPosition;
extern uint16_t selTeamSelectedPlayerPosition;
extern uint16_t team1SelectedPlayerIndex;
extern uint16_t team2SelectedPlayerIndex;
extern uint32_t allowShorterMenuItemsWithFrames;

/* String constants from SWOS2.s */
extern uint8_t aViewResult[];
extern uint8_t aPlayCoachMatch[];
extern uint8_t aCoachMatch[];
extern uint8_t aViewMatch[];
extern uint8_t aViewOppo[];
extern uint8_t aViewTeam[];
extern uint8_t aViewOtherTeam[];
extern uint8_t aViewATeam[];
extern uint8_t aViewBTeam[];
extern uint8_t aCoach_1[];
extern uint8_t aPlayer_1[];
extern uint8_t fundsBuffer[];
extern void *playerPositionsStringTable[];
extern uint8_t teamAName[];
extern uint8_t teamBName[];
extern void *sidePitchSurnamePositionsTable[];
extern int16_t playMatchRedrawList[];
extern int16_t togglePlayerCoachRedrawItemList[];

/* ---- C-ported function externs ---- */
extern void _CheckIsTeamNational(void);
extern void _SetBenchPlayersNumbers(TeamFileEx *team);
extern void _DrawMenu(void);
extern void _CallDrawMenuItem(MenuEntry *entry);
extern void _SetCurrentEntry(uint16_t index);
extern void _DrawMultipleItems(uint16_t maxItems, MenuEntry *firstEntry);
extern void _CallDrawMultipleItems(uint16_t maxItems, MenuEntry *firstEntry);
extern void _DrawMenuItemsWaitForVSync(const int16_t *list);
extern void _SetExitMenuFlag(void);
extern MenuEntry *_CalcMenuEntryAddress(uint16_t ordinal);
extern uint8_t *_ExtractSurname(uint8_t *name, uint16_t maxLenAndFlags);
extern uint8_t *_StringCopy(const uint8_t *src, uint8_t *dst);
extern uint16_t _IsTeamNational(TeamFileEx *team);
extern void _InitPlayerValueDisplayStruct(uint8_t *playerEntry, uint8_t *dst);
extern uint16_t _IsPlayerInjured(PlayerFile *playerFile, int16_t mode);
extern uint16_t _GetNumberOfSubstitutes(uint8_t *team, uint16_t numPl);
extern uint16_t _GetNumberOfSubstitutesInSelectionList(uint8_t *team, uint16_t extraCount);
extern uint32_t _GetNumberOfAvailablePlayers(uint8_t *team, int16_t excludeCardsFlag, uint16_t numPl);
extern uint32_t _GetNumberOfAvailableSubstitutes(uint8_t *team, int16_t flag);
extern void _RotatePlayersLeft(TeamFileEx *team, uint16_t maxSubs);
extern int16_t _CheckIfTeamIsChanged(void);
extern uint8_t *_GetPlayerInfoBreakPosition(TeamFileEx *teamPtr, uint16_t requestedSlot,
                                             uint16_t *outSlot, uint16_t *outPlayerIndex);
extern uint8_t *_GetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal);
extern uint8_t *_GetPlayer(uint16_t teamNumber, uint16_t playerFileIndex);
extern uint16_t _cGetPlayerAtIndexFileIdx;
extern uint16_t _ComputeOldPlayerSkillForViewResult(TeamFileEx *team, uint16_t goalsCount);
extern void _ShowStandardChooseTacticsMenu(TeamFileEx *team);
extern void _TrainingMatch_RestoreLastMatchState(void);
extern void _TrainingMatch_RestoreSavedTeamSelectionState(void);

extern void _FormTopSkillsString(uint8_t *dst, PlayerFile *playerFile);
extern void _FormatPlayerValueOrStatusText(uint8_t *base, uint8_t *dst);
extern void _SetSquadMenuEntryStatusSprite(MenuEntry *entry, PlayerFile *playerFile, uint16_t flag);
extern void _TrainingMatch_BuildCurrentTeamFromCareerTeam(void);
extern void _TrainingMatch_ApplyUseMatchTeamAndSwapSubs(void);

/* ---- Helper: menu entry from byte offset ---- */

/* Helper: access bg as the high 16 bits (big-endian BACKANDFRAMECOLOR) */

/* ================================================================
 * GetPlayerPosition  (SWOS2.s line 79648)
 *
 * Reverse lookup: given team + player file index, return position slot.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _GetPlayerPosition(TeamFileEx *team, int16_t playerIndex) {
    int16_t total;
    if (team == &careerTeam)
        total = (int16_t)careerNumPlayers;
    else if (team == &currentTeam)
        total = (int16_t)numPlayers;
    else
        total = 16;

    int16_t i;
    for (i = total - 1; i >= 0; i--) {
        uint16_t slot, fIdx;
        _GetPlayerInfoBreakPosition(team, (uint16_t)i, &slot, &fIdx);
        if ((int16_t)fIdx == playerIndex)
            return i;
    }
    panicReasonCode = 0x5996;
    return -1;
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _SetCurrentSelectedPlayerIndexPointer(void) {
    uint16_t *a0 = &team1SelectedPlayerIndex;
    uint16_t *a1 = &team2SelectedPlayerIndex;
    if (!isFirstPlayerChoosingTeam) { uint16_t *t = a0; a0 = a1; a1 = t; }
    if (viewingOpponentTeam)        { uint16_t *t = a0; a0 = a1; a1 = t; }
    currentSelectedPlayerIndexPtr = (uintptr_t)a0;
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _UpdateCheckMarkSprite(void) {
    MenuEntry *e = &menuEntries[0];
    e->isInvisible = 1;
    if (viewingOpponentTeam) return;
    e->stringFlags &= ~(1u << 13);
    if (oldPlayerSkill == newPlayerSkill) return;
    e->fgWord = (oldPlayerSkill > newPlayerSkill) ? SPR_CHECK_BIG : SPR_NOT_CHECK_BIG;
    e->stringFlags |= (1u << 13);
    e->isInvisible = 0;
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _UpdateLittleCheckmark(void) {
    newPlayerSkill = oldPlayerSkill;
    int16_t i; for (i = 0; i < 32; i++) oldPlayerPositionsArray[i] = playerPositionsArray[i];
    _UpdateCheckMarkSprite();
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _SavePositionsAndUpdateLittleCheckmark(void) {
    newPlayerSkill = oldPlayerSkill;
    int16_t i; for (i = 0; i < 32; i++) oldPlayerPositionsArray[i] = playerPositionsArray[i];
    TeamFileEx *team = playMatchTeam1Ptr;
    team->team.playerValuesDirty = TEAM_VALUES_DIRTY;
    _ComputeOldPlayerSkillForViewResult((TeamFileEx *)playMatchTeam1Ptr, maxSubstitutesEntries);
    _UpdateCheckMarkSprite();
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _SetGameStartButtonText(void) {
    MenuEntry *e = &menuEntries[9];
    e->bgColor = (e->bgColor & 0xFFF0) | YELLOW_GREEN_BLACK_14;
    e->fg = (uintptr_t)aViewResult;
    if (!squadChangesAllowed) return;

    TeamFileEx *t1 = playMatchTeam1Ptr;
    TeamFileEx *t2 = playMatchTeam2Ptr;
    if (t1->team.teamControls == TEAM_COMPUTER &&
        t2->team.teamControls == TEAM_COMPUTER) {
        e->fg = (uintptr_t)aViewMatch;
    } else {
        e->fg = (uintptr_t)aPlayCoachMatch;
        if (t1->team.teamControls != TEAM_PLAYER_COACH)
            e->fg = (uintptr_t)aCoachMatch;
    }
    e->bgColor = (e->bgColor & 0xFFF0) | RED_10;
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _ShowUseMatchTeamIfTraining(void) {
    menuEntries[5].isInvisible = 1;
    if (!trainingGame) return;
    if (is_A_team) return;
    if (!_CheckIfTeamIsChanged()) return;
    menuEntries[5].isInvisible = 0;
}

/* ================================================================
 * ShowOrHidePlayMatchButton  (line 81484)
 *
 * Very complex control flow. Translated label-by-label.
 * Labels mapped:
 *   loc_14855C  → notViewingOpponent
 *   loc_148590  → afterTrainingTextSet
 *   loc_1485D8  → careerPath
 *   loc_1485FE  → checkAvailSubs
 *   loc_148606  → checkMaxDisabled
 *   loc_14860E  → checkSubstitutes
 *   loc_148654  → checkAvailPlayersLimit
 *   loc_1486D0  → canPlay
 *   loc_1486D4  → nonCareerSubCheck
 *   loc_14872A  → nonCareerAvailPlayersLimit
 *   loc_14874A  → canPlay (shared)
 *   loc_148748  → cannotPlay
 *   loc_148760  → setVisState1
 *   loc_148768  → showPlayBtn (clr entry $1F8 invisible)
 *   loc_14876E  → hidePlayBtnPath
 *   loc_148780  → hidePlayBtn (set entry $1F8 invisible=1)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ShowOrHidePlayMatchButton(void) {
    menuEntries[8].isInvisible = 1;

    if (viewingOpponentTeam) {
        TeamFileEx *t2 = playMatchTeam2Ptr;
        if (t2->team.teamControls != 1) goto hidePlayBtnPath;
        if (!squadChangesAllowed) goto hidePlayBtnPath;
        menuEntries[8].isInvisible = 0;
        if (!playMatchButtonVisibilityState) goto hidePlayBtnPath;
        goto canPlay;
    }

    /* notViewingOpponent */
    menuEntries[8].isInvisible = 0;
    {
        MenuEntry *vb = &menuEntries[7];
        vb->stringFlags &= ~(1u << 13);
        if (trainingGame) {
            vb->fg = (uintptr_t)aViewATeam;
            if (!is_A_team)
                vb->fg = (uintptr_t)aViewBTeam;
        }
    }

    {
        TeamFileEx *team = playMatchTeam1Ptr;
        uint32_t subsR = _GetNumberOfAvailableSubstitutes((uint8_t *)team, playMatchExcludeFlags);
        uint16_t d2 = (uint16_t)(subsR & 0xFFFF); /* d7→d2: available subs (goalkeepers) */

        uint32_t plR = _GetNumberOfAvailablePlayers((uint8_t *)team, playMatchExcludeFlags, 0);
        /* d7 = available, d6 = disabled (after bridge unpack) */
        /* But C returns packed: low=available, high=disabled */
        uint16_t d7_avail = (uint16_t)(plR & 0xFFFF);
        uint16_t d6_disabled = (uint16_t)(plR >> 16);

        if (gameType == GAME_TYPE_CAREER) {
            /* careerPath */
            if (team == &currentTeam) {
                uint32_t csR = _GetNumberOfAvailableSubstitutes((uint8_t *)&careerTeam, playMatchExcludeFlags);
                uint16_t careerSubs = (uint16_t)(csR & 0xFFFF);
                if (careerSubs >= 2) goto checkMaxDisabled;
            }
            /* checkAvailSubs */
            if (d2 < 1) goto cannotPlay;
        checkMaxDisabled:
            if (d6_disabled > 4) goto cannotPlay;
        } else {
            if (d2 < 1) goto cannotPlay;
            if (d6_disabled > 2) goto cannotPlay;
        }

        /* checkSubstitutes */
        if (gameType == GAME_TYPE_CAREER) {
            uint16_t ns = _GetNumberOfSubstitutes((uint8_t *)team, 1);
            if (ns != 1) goto showPlayBtnPath;

            if (maxSubstitutesEntries > 5) {
                uint16_t nss = _GetNumberOfSubstitutesInSelectionList((uint8_t *)team, 1);
                if (nss != 1) goto showPlayBtnPath;
            }

            /* checkAvailPlayersLimit */
            uint32_t apR = _GetNumberOfAvailablePlayers((uint8_t *)team, playMatchExcludeFlags,
                                                         11 + maxSubstitutesEntries);
            uint16_t disabled2 = (uint16_t)(apR >> 16);
            if (disabled2 != 0) goto showPlayBtnPath;

            if (trainingGame && team == &careerTeam) {
                uint16_t ns2 = _GetNumberOfSubstitutes((uint8_t *)&currentTeam, 1);
                if (ns2 != 1) {
                    /* Set blink on view button */
                    MenuEntry *vb = &menuEntries[7];
                    vb->stringFlags |= (1u << 13);
                    uint32_t sr2 = _GetNumberOfAvailableSubstitutes((uint8_t *)team, playMatchExcludeFlags);
                    if ((uint16_t)(sr2 & 0xFFFF) < 2) goto cannotPlay;
                    goto showPlayBtnPath;
                }
            }
            goto canPlay;
        } else {
            /* nonCareerSubCheck */
            uint16_t ns = _GetNumberOfSubstitutes((uint8_t *)team, 1);
            if (ns != 1) goto showPlayBtnPath;

            if (maxSubstitutesEntries > 5) {
                uint32_t sr3 = _GetNumberOfAvailableSubstitutes((uint8_t *)team, playMatchExcludeFlags);
                if ((uint16_t)(sr3 & 0xFFFF) < 2) goto nonCareerAvailPlayersLimit;
                uint16_t nss2 = _GetNumberOfSubstitutesInSelectionList((uint8_t *)team, 1);
                if (nss2 != 1) goto showPlayBtnPath;
            }

        nonCareerAvailPlayersLimit:
            {
                uint32_t apR2 = _GetNumberOfAvailablePlayers((uint8_t *)team, playMatchExcludeFlags, 11);
                uint16_t disabled3 = (uint16_t)(apR2 >> 16);
                if (disabled3 != 0) goto showPlayBtnPath;
                goto canPlay;
            }
        }
    }

cannotPlay:
    /* loc_148748: bra.s loc_148768 — jump directly to showPlayBtn,
     * bypassing canPlay's visibility-state check. When the player roster
     * is invalid (not enough subs/players), the button is always shown
     * so the user can still VIEW RESULT. */
    goto showPlayBtn;

canPlay:
    /* loc_14874A */
    if (playMatchTeam1Ptr == &currentTeam) {
        if (!playMatchButtonVisibilityState) goto hidePlayBtn;
        /* else fall through to showPlayBtn */
    } else {
        playMatchButtonVisibilityState = 1;
    }
showPlayBtn:
    /* loc_148768 */
    menuEntries[9].isInvisible = 0;
    return;

showPlayBtnPath:
    /* loc_14876E */
hidePlayBtnPath:
    if (playMatchTeam1Ptr != &currentTeam)
        playMatchButtonVisibilityState = 0;
hidePlayBtn:
    /* loc_148780 */
    menuEntries[9].isInvisible = 1;
    return;
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _UpdateTeamCheckmark(void) {
    menuEntries[37].isInvisible = 1;
    if (!squadChangesAllowed) return;

    int16_t d1;
    TeamFileEx *team1 = playMatchTeam1Ptr;

    if (viewingOpponentTeam) {
        uint16_t *ip = (uint16_t *)currentSelectedPlayerIndexPtr;
        d1 = (int16_t)*ip;
        if (d1 < 0) return;
        d1 = _GetPlayerPosition(playMatchTeam2Ptr, d1);
    } else {
        if (team1 == &careerTeam)
            d1 = (int16_t)selTeamSelectedPlayerPosition;
        else if (team1 == &currentTeam)
            d1 = (int16_t)currentTeamSelectedPlayerPosition;
        else {
            uint16_t *ip = (uint16_t *)currentSelectedPlayerIndexPtr;
            d1 = (int16_t)*ip;
        }
        if (d1 < 0) return;
        d1 = _GetPlayerPosition(team1, d1);
        if ((uint16_t)d1 >= pmMenuNumPlayers) {
            if (team1 == &currentTeam) currentTeamSelectedPlayerPosition = 0xFFFF;
            else selTeamSelectedPlayerPosition = 0xFFFF;
            return;
        }
    }

    {
        MenuEntry *ck = &menuEntries[37];
        ck->stringFlags |= (1u << 13);
        MenuEntry *pe = _CalcMenuEntryAddress(13 + d1);
        ck->y = pe->y;
        ck->isInvisible = 0;
    }
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _SetUpTacticsButtons(void) {
    TeamFileEx *team = playMatchTeam1Ptr;
    if (viewingOpponentTeam) team = playMatchTeam2Ptr;
    uint8_t curTact = team->team.tactics;

    MenuEntry *e = &menuEntries[38];
    int16_t i;
    for (i = 0; i < 18; i++, e++) {
        uint16_t c = e->bgColor & 0xFFF0;
        if (viewingOpponentTeam || showingCpuTeams) {
            c |= (i == curTact) ? PINK_TO_BROWN_1 : PINK_TO_BROWN_9;
        } else {
            c |= (i == curTact) ? ORANGE_TO_BROWN_5 : ORANGE_TO_BROWN_4;
        }
        e->bgColor = c;
        e->isDisabled = (showingCpuTeams || viewingOpponentTeam) ? 1 : 0;
    }
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _SetPlayerSurnamesOnSidePitch(void) {
    MenuEntry *entry = &menuEntries[57];
    TeamFileEx *team = playMatchTeam1Ptr;
    if (viewingOpponentTeam) team = playMatchTeam2Ptr;

    int8_t tIdx = (int8_t)team->team.tactics;
    TeamTactics *tac = (TeamTactics *)teamTacticsTable[tIdx];
    int8_t fIdx = (int8_t)tac->ballOutOfPlayTactics;
    int16_t *pos = (int16_t *)sidePitchSurnamePositionsTable[fIdx];

    uint16_t tNum = PACK_BE_U16(team->team.countryNumber, team->team.teamOrdinal);
    int16_t i;
    for (i = 0; i <= 10; i++, entry++) {
        entry->x = *pos++ + 196;
        entry->y = *pos++ + 40;
        entry->stringFlags = *pos++;
        uint8_t *pl = _GetPlayerAtIndex(tNum, (uint16_t)i);
        PlayerFile *plPf = (PlayerFile *)(pl + 76);
        uint8_t *surname;
        if (entry->x == 0x106)
            surname = _ExtractSurname((uint8_t *)plPf->playerName, 0x400F);
        else
            surname = _ExtractSurname((uint8_t *)plPf->playerName, 0x400A);
        _StringCopy(surname, (uint8_t *)entry->fg);
    }
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _SetVariableButtons(void) {
    _SetGameStartButtonText();
    _ShowUseMatchTeamIfTraining();

    MenuEntry *vb = &menuEntries[7];
    vb->fg = (uintptr_t)aViewOppo;
    if (viewingOpponentTeam) vb->fg = (uintptr_t)aViewTeam;
    if (showingCpuTeams) { vb->fg = (uintptr_t)aViewOtherTeam; vb->dwWidth = 93; }

    vb->stringFlags &= ~(1u << 13);

    if (trainingGame) {
        vb->fg = (uintptr_t)aViewATeam;
        if (!is_A_team) {
            vb->fg = (uintptr_t)aViewBTeam;
            uint16_t ns = _GetNumberOfSubstitutes((uint8_t *)&currentTeam, 1);
            if (ns != 1)
                vb->stringFlags |= (1u << 13);
        }
    }

    /* Team country name */
    {
        MenuEntry *top = &menuEntries[2];
        uint8_t *nameOut = (uint8_t *)top->fg;
        if (!(trainingGame && !is_A_team)) {
            TeamFileEx *tm = playMatchTeam1Ptr;
            if (viewingOpponentTeam) tm = playMatchTeam2Ptr;
            uint8_t *cname = (uint8_t *)countriesTable[tm->team.countryNumber];
            cname++;
            *nameOut++ = '(';
            nameOut = _StringCopy(cname, nameOut);
            *nameOut++ = ')';
        }
        *nameOut = 0;
    }

    _UpdateCheckMarkSprite();

    TeamFileEx *curTeam = playMatchTeam1Ptr;

    if (viewingOpponentTeam) {
        /* Viewing opponent */
        menuEntries[4].isInvisible = 0;
        menuEntries[3].isInvisible = 0;
        menuEntries[3].y = 204;
        menuEntries[4].y = 204;
        menuEntries[6].isInvisible = 1;
        menuEntries[1].bgColor = (menuEntries[1].bgColor & 0xFFF0) | ORANGE_TO_BROWN_12;

        curTeam = playMatchTeam2Ptr;
        menuEntries[10].isInvisible = 1;

        TeamFileEx *t2 = playMatchTeam2Ptr;
        if (t2->team.teamControls != TEAM_COMPUTER || !squadChangesAllowed)
            menuEntries[7].isInvisible = 1;

        menuEntries[11].isInvisible = 0;
        menuEntries[29].isInvisible = 1;
        menuEntries[30].isInvisible = 1;
    } else {
        /* Viewing own team */
        menuEntries[4].isInvisible = 0;
        menuEntries[3].isInvisible = 1;
        menuEntries[3].y = 204;
        menuEntries[4].y = 204;
        menuEntries[6].isInvisible = 0;

        MenuEntry *cb = &menuEntries[6];
        TeamFileEx *t1 = playMatchTeam1Ptr;

        if (showingCpuTeams) {
            menuEntries[6].isInvisible = 1;
            menuEntries[3].isInvisible = 0;
        }

        cb->fg = (uintptr_t)aCoach_1;
        if (t1->team.teamControls == TEAM_PLAYER_COACH)
            cb->fg = (uintptr_t)aPlayer_1;

        cb->bgColor = (cb->bgColor & 0xFFF0) | BRIGHT_BLUE_TO_BLUE_13;
        if (t1->team.teamControls == TEAM_PLAYER_COACH)
            cb->bgColor = (cb->bgColor & 0xFFF0) | BLUE_TO_PURPLE_11;

        if (!squadChangesAllowed) {
            menuEntries[3].isInvisible = 0;
            menuEntries[6].isInvisible = 1;
        }

        cb->isDisabled = 0;
        if (is_A_team) menuEntries[3].isInvisible = 1;

        if (trainingGame) {
            if (t1->team.teamControls != TEAM_PLAYER_COACH) {
                TeamFileEx *t2 = playMatchTeam2Ptr;
                if (t2->team.teamControls == TEAM_PLAYER_COACH) {
                    if (t1->team.teamControls != TEAM_PLAYER_COACH)
                        cb->isDisabled = 1;
                }
            }
        }

        /* Edit tactics button */
        MenuEntry *eb = &menuEntries[1];
        eb->bgColor = (eb->bgColor & 0xFFF0) | BLUE_TO_PURPLE_11;
        if (t1->team.teamControls != TEAM_PLAYER_COACH) {
            eb->bgColor = (eb->bgColor & 0xFFF0) | BRIGHT_BLUE_TO_BLUE_13;
            if (t1->team.teamControls != TEAM_COACH)
                eb->bgColor = (eb->bgColor & 0xFFF0) | ORANGE_TO_BROWN_12;
        }
        if (trainingGame && t1->team.teamControls == TEAM_COMPUTER)
            eb->bgColor = (eb->bgColor & 0xFFF0) | BRIGHT_BLUE_TO_BLUE_13;

        menuEntries[10].isInvisible = 0;
        if (showingCpuTeams) menuEntries[10].isInvisible = 1;
        if (is_A_team) menuEntries[10].isInvisible = 1;

        menuEntries[7].isInvisible = 0;
        menuEntries[11].isInvisible = 1;
        menuEntries[29].isInvisible = 1;
        menuEntries[30].isInvisible = 1;

        if (!showingCpuTeams) {
            int16_t diff = (int16_t)numPlayersToShowInMenu - (int16_t)pmMenuNumPlayers;
            if (diff != 0) {
                menuEntries[29].isInvisible = 0;
                if (diff != 1) menuEntries[30].isInvisible = 0;
            }
        }
    }

    /* Set team name and coach name */
    {
        MenuEntry *nm = &menuEntries[1];

        if (trainingGame && !is_A_team) {
            /* B team: show team B name */
            if (playMatchTeam1Ptr == &careerTeam)
                nm->fg = (uintptr_t)teamBName;
            else
                nm->fg = (uintptr_t)teamAName;
        } else {
            TeamFileEx *tm = playMatchTeam1Ptr;
            if (viewingOpponentTeam) tm = playMatchTeam2Ptr;
            nm->fg = (uintptr_t)tm->team.teamName;
            if (trainingGame) {
                nm->fg = (uintptr_t)teamAName;
                if (playMatchTeam1Ptr != &careerTeam)
                    nm->fg = (uintptr_t)teamBName;
            }
        }
    }

    {
        TeamFileEx *tm = playMatchTeam1Ptr;
        if (viewingOpponentTeam) tm = playMatchTeam2Ptr;
        menuEntries[4].fg = (uintptr_t)tm->team.coachName;
    }
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _PlayMatchMenuReinit(void) {
    _UpdateTeamCheckmark();
    pmMenuNumPlayers = 16 - 5 + maxSubstitutesEntries;

    uint16_t np = numPlayers;
    if (playMatchTeam1Ptr != &currentTeam) {
        if (selTeamNumber == 0xFFFF) {
            np = 16;
        } else {
            if (PACK_BE_U16(playMatchTeam1Ptr->team.countryNumber, playMatchTeam1Ptr->team.teamOrdinal) == selTeamNumber)
                np = careerNumPlayers;
            else
                np = 16;
        }
    }
    numPlayersToShowInMenu = np;
    pmSelectedPlayerEntry = 0;
    selectedPlayerPosition = -1;

    _SetVariableButtons();
    _SetUpTacticsButtons();
    _SetPlayerSurnamesOnSidePitch();
    _ShowOrHidePlayMatchButton();

    int16_t extra = 4 - (int16_t)maxSubstitutesEntries;
    if (extra >= 0) {
        MenuEntry *sub = &menuEntries[28];
        int16_t j;
        for (j = extra; j >= 0; j--) {
            sub->isInvisible = 1;
            sub--;
            menuEntries[12].y -= 9;
            menuEntries[30].y -= 9;
            menuEntries[29].y -= 9;
        }
    }

    _SavePositionsAndUpdateLittleCheckmark();
    _UpdateLittleCheckmark();
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _PlayMatchMenuInit(void) {
    if (!allowShorterMenuItemsWithFrames)
        fireResetFlag = 1;

    uint32_t d0 = irqCookieObfuscated ^ 0xCFD1F85F;
    if (d0 != 0x17B8) return;

    player1ClearFlag = 0;
    player2ClearFlag = 0;
    if (!(playMatchControlFlags & 0x01)) player1ClearFlag = 0xFF;
    if (!(playMatchControlFlags & 0x02)) player2ClearFlag = 0xFF;

    showingCpuTeams = 0;
    {
        if (playMatchTeam1Ptr->team.teamControls == TEAM_COMPUTER) {
            if (playMatchTeam2Ptr->team.teamControls == TEAM_COMPUTER)
                showingCpuTeams = 1;
        }
    }

    viewingOpponentTeam = 0;
    _SetCurrentSelectedPlayerIndexPointer();
    _CheckIsTeamNational();
    _SetBenchPlayersNumbers((TeamFileEx *)playMatchTeam1Ptr);
    is_A_team = 0;
    _PlayMatchMenuReinit();

    /* If entry 9 (PLAY/VIEW RESULT button) is invisible, default to entry 7 (VIEW OPPO).
     * Original asm: lea ($2A26).w,a0; tst.w 4(a0) — $2A26 = menuEntries[9] in original binary,
     * offset 4 = isInvisible (coincidentally same as TEAMFILE_TEAMCONTROLS EQU). */
    if (menuEntries[9].isInvisible != 0)
        _SetCurrentEntry(7);
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _OnSwapPlayer(MenuEntry *a5) {
    TeamFileEx *t1 = playMatchTeam1Ptr;

    if (viewingOpponentTeam) {
        t1 = playMatchTeam2Ptr;
        goto cpuPath;
    }
    if (showingCpuTeams) goto cpuPath;

    /* Normal player path */
    {
        int16_t d6 = a5->ordinal - menuEntries[13].ordinal;
        if (d6 == 16) d6 = d6 - 5 + maxSubstitutesEntries;

        uint16_t slot, fIdx;
        _GetPlayerInfoBreakPosition((TeamFileEx *)playMatchTeam1Ptr, (uint16_t)d6, &slot, &fIdx);
        d6 = (int16_t)slot;
        int16_t d7 = (int16_t)fIdx;

        if (selectedPlayerPosition >= 0) {
            if (d7 == selectedPlayerPosition) {
                d7 = -1;
                if (!_HandleSquadSwapSelectionToggle(d6)) return;
            } else {
                TeamFileEx *team = playMatchTeam1Ptr;
                _PlayMatchMenuSwapPlayers(team, selectedPlayerTacticsPosition, d6);
                _SetBenchPlayersNumbers((TeamFileEx *)team);
                team->team.playerValuesDirty = TEAM_VALUES_DIRTY;
                _SavePositionsAndUpdateLittleCheckmark();
                selectedPlayerPosition = -1;
                _SetPlayerSurnamesOnSidePitch();
                _ShowOrHidePlayMatchButton();
                _UpdateTeamCheckmark();
                _ShowUseMatchTeamIfTraining();
                _DrawMenu();
                return;
            }
        } else {
            if (longFireFlag) return;
        }

        selectedPlayerPosition = d7;
        selectedPlayerTacticsPosition = d6;
        {
            MenuEntry *old = (MenuEntry *)pmSelectedPlayerEntry;
            pmSelectedPlayerEntry = (uintptr_t)a5;
            if (old) { _CallDrawMenuItem(old); }
        }
        _CallDrawMenuItem(a5);
        _UpdateLittleCheckmark();
        _RedrawPlayerNames();
        return;
    }

cpuPath:
    if (longFireFlag) return;
    if (!squadChangesAllowed) return;
    {
        int16_t d1 = a5->ordinal - menuEntries[13].ordinal;
        uint16_t slot, fIdx;
        uint8_t *pl = _GetPlayerInfoBreakPosition((TeamFileEx *)t1, (uint16_t)d1, &slot, &fIdx);
        PlayerFile *plPf = (PlayerFile *)(pl + 76);
        if (((plPf->positionAndFace & PF_POSITION_MASK) >> 5) == 0) return;

        uint16_t *ip = (uint16_t *)currentSelectedPlayerIndexPtr;
        if (*ip == fIdx)
            *ip = 0xFFFF;
        else
            *ip = fIdx;
        _UpdateTeamCheckmark();
        _DrawMenu();
    }
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _PlayMatchMenuSwapPlayers(TeamFileEx *team, int16_t d3, int16_t d4) {
    /* Original asm only takes D3 (selected slot) and D4 (dest slot).
     * Primary swap runs when the two slots are in different groups
     * (one starting, one bench) OR either slot >= 16 (extended squad). */
    int skipPrimary = 0;
    if ((uint16_t)d3 < 16 && (uint16_t)d4 < 16) {
        /* Both < 16: skip primary if same group */
        if ((uint16_t)d3 < 11 && (uint16_t)d4 < 11)
            skipPrimary = 1;  /* both starting 11 */
        else if ((uint16_t)d3 >= 11 && (uint16_t)d4 >= 11)
            skipPrimary = 1;  /* both bench */
    }

    if (!skipPrimary) {
        /* Primary swap: playerPositionsArray + shirt numbers.
         * Asm calls GetPlayerAtIndex(teamNum, d3/d4) which translates
         * through playerPositions/currentMatchPlayers/playerNumbers,
         * then uses the translated file indices to index playerPositionsArray. */
        uint16_t tNum = PACK_BE_U16(team->team.countryNumber, team->team.teamOrdinal);
        uint8_t *p1 = _GetPlayerAtIndex(tNum, (uint16_t)d3);
        uint16_t fileIdx1 = _cGetPlayerAtIndexFileIdx;
        uint8_t *p2 = _GetPlayerAtIndex(tNum, (uint16_t)d4);
        uint16_t fileIdx2 = _cGetPlayerAtIndexFileIdx;

        uint8_t tmp = playerPositionsArray[fileIdx2];
        playerPositionsArray[fileIdx2] = playerPositionsArray[fileIdx1];
        playerPositionsArray[fileIdx1] = tmp;

        PlayerFile *pf1 = (PlayerFile *)(p1 + 76);
        PlayerFile *pf2 = (PlayerFile *)(p2 + 76);
        int8_t ts = pf2->shirtNumber;
        pf2->shirtNumber = pf1->shirtNumber;
        pf1->shirtNumber = ts;
    }

    /* Secondary swap: swap in playerPositions/currentMatchPlayers/playerNumbers
     * using d3/d4 (slot positions) */
    {
        uint8_t *pn;
        uint16_t tNum = PACK_BE_U16(team->team.countryNumber, team->team.teamOrdinal);
        if (team == &currentTeam) {
            pn = (uint8_t *)playerPositions;
        } else if (gameType == GAME_TYPE_CAREER && (team == &careerTeam || tNum == selTeamNumber)) {
            /* The player's matchday lineup lives in currentMatchPlayers. In a CUP,
             * playMatchTeam1Ptr is a selTeams COPY (not &careerTeam) whose team
             * number still equals selTeamNumber. The asm's pointer-only test
             * (`==&careerTeam`) only works because the Amiga's playMatchTeam1Ptr IS
             * &careerTeam; on SDL the cup uses a copy, so that test fails and the
             * secondary swap edits team->playerNumbers instead — the lineup never
             * changes and a carded substitution silently fails. Key on
             * selTeamNumber to match the PRIMARY swap's _GetPlayerAtIndex
             * (teams.c:924), so both halves of the swap agree. */
            pn = (uint8_t *)currentMatchPlayers;
        } else {
            pn = team->team.playerNumbers;
        }
        uint8_t t = pn[(uint16_t)d4];
        pn[(uint16_t)d4] = pn[(uint16_t)d3];
        pn[(uint16_t)d3] = t;
    }

    if (trainingGame && !is_A_team) {
        uint16_t lim = 11 + maxSubstitutesEntries;
        if ((d3 < (int16_t)lim && d4 >= (int16_t)lim) ||
            (d3 >= (int16_t)lim && d4 < (int16_t)lim))
            _TrainingMatch_BuildCurrentTeamFromCareerTeam();
    }
}

/* ================================================================ */
__attribute__((used, externally_visible))
int16_t _HandleSquadSwapSelectionToggle(int16_t d6) {
    uint16_t tNum = currentTeamNumber;
    uint16_t *selPos = &currentTeamSelectedPlayerPosition;

    if (playMatchTeam1Ptr == &currentTeam) {
        /* defaults already set */
    } else if (playMatchTeam1Ptr == &careerTeam) {
        tNum = selTeamNumber;
        selPos = &selTeamSelectedPlayerPosition;
    } else {
        tNum = PACK_BE_U16(playMatchTeam1Ptr->team.countryNumber, playMatchTeam1Ptr->team.teamOrdinal);
        selPos = (uint16_t *)currentSelectedPlayerIndexPtr;
    }

    if (!longFireFlag) return 1;
    if (longFireFlag != 1) return 0;
    if (!squadChangesAllowed) return 0;
    if ((uint16_t)d6 >= pmMenuNumPlayers) return 0;

    int16_t curSel = (int16_t)selectedPlayerPosition;
    if (curSel == (int16_t)*selPos) {
        curSel = -1;
    } else {
        /* Asm calls GetPlayer (not GetPlayerAtIndex) — selectedPlayerPosition
         * is already a file index, no translation needed. */
        uint8_t *pl = _GetPlayer(tNum, (uint16_t)selectedPlayerPosition);
        PlayerFile *plPf = (PlayerFile *)(pl + 76);
        if (((plPf->positionAndFace & PF_POSITION_MASK) >> 5) == 0) return 0;
    }

    *selPos = (uint16_t)curSel;
    selectedPlayerPosition = -1;
    _UpdateTeamCheckmark();
    _ShowUseMatchTeamIfTraining();
    _DrawMenu();
    return 0;
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _PlayMatchChangeTactics(MenuEntry *a5) {
    _UpdateLittleCheckmark();
    _RedrawPlayerNames();
    int16_t nt = a5->ordinal - menuEntries[38].ordinal;
    TeamFileEx *t = playMatchTeam1Ptr;
    if ((int8_t)t->team.tactics == (int8_t)nt) return;
    t->team.tactics = (uint8_t)nt;
    t->team.playerValuesDirty = TEAM_VALUES_DIRTY;
    _SavePositionsAndUpdateLittleCheckmark();
    _SetUpTacticsButtons();
    _SetPlayerSurnamesOnSidePitch();
    _ShowUseMatchTeamIfTraining();
    _UpdateTeamCheckmark();
    _DrawMenu();
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _ExitPlayMatch(void) {
    exitGameFlag = 1;
    player1ClearFlag = 0;
    player2ClearFlag = 0;
    isCareer = 0;
    if (playMatchTeam1Ptr == &careerTeam ||
        playMatchTeam2Ptr == &careerTeam)
        isCareer = 1;
    _SetExitMenuFlag();
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _PlayMatchSelected(void) {
    if (careerTeam.team.teamControls == TEAM_COMPUTER)
        careerTeamCoachOnly = 1;
    if (pl1Fire) {
        playerNumThatStarted = 1;
        player1ClearFlag = 0; player2ClearFlag = 0;
        _SetExitMenuFlag();
    } else if (pl2Fire) {
        playerNumThatStarted = 2;
        player1ClearFlag = 0; player2ClearFlag = 0;
        _SetExitMenuFlag();
    }
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _EditTacticsInPlayMatchMenu(void) {
    _UpdateLittleCheckmark();
    _ShowStandardChooseTacticsMenu((TeamFileEx *)playMatchTeam1Ptr);
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _ViewOpponentsSelect(void) {
    if (viewingOpponentTeam) {
        viewingOpponentTeam = 0;
        goto common;
    }
    if (trainingGame) {
        is_A_team ^= 1;
        TeamFileEx *tmp = playMatchTeam1Ptr;
        playMatchTeam1Ptr = playMatchTeam2Ptr;
        playMatchTeam2Ptr = tmp;
        numPlayersToShowInMenu = careerNumPlayers;
        if (playMatchTeam1Ptr != &careerTeam)
            numPlayersToShowInMenu = numPlayers;
        selectedPlayerPosition = -1;
        _SavePositionsAndUpdateLittleCheckmark();
        _UpdateLittleCheckmark();
        _SetVariableButtons();
        _SetUpTacticsButtons();
        _SetPlayerSurnamesOnSidePitch();
        _ShowOrHidePlayMatchButton();
        _UpdateTeamCheckmark();
        _DrawMenu();
        return;
    }
    viewingOpponentTeam = 1;
    {
        if (playMatchTeam2Ptr->team.teamControls != TEAM_COMPUTER || !squadChangesAllowed)
            _SetCurrentEntry(11);
    }
common:
    _SetCurrentSelectedPlayerIndexPointer();
    _UpdateLittleCheckmark();
    _SetVariableButtons();
    _SetUpTacticsButtons();
    _SetPlayerSurnamesOnSidePitch();
    _ShowOrHidePlayMatchButton();
    _UpdateTeamCheckmark();
    _DrawMenu();
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _ReturnFromViewOpponent(void) {
    viewingOpponentTeam = 0;
    _SetCurrentSelectedPlayerIndexPointer();
    _SetVariableButtons();
    _SetCurrentEntry(7);
    _SetUpTacticsButtons();
    _SetPlayerSurnamesOnSidePitch();
    _ShowOrHidePlayMatchButton();
    _UpdateTeamCheckmark();
    _DrawMenu();
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _PlayMatchOnSelect30(void) {
    _RotatePlayersLeft((TeamFileEx *)playMatchTeam1Ptr, maxSubstitutesEntries);
    _DrawMenuItemsWaitForVSync(playMatchRedrawList);
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _PlayMatch30AfterDraw(void) {
    _CallDrawMenuItem(&menuEntries[29]);
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _RedrawPlayerNames(void) {
    _CallDrawMultipleItems(11, &menuEntries[13]);
    _CallDrawMenuItem(&menuEntries[0]);
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _UseMatchTeamSelect(void) {
    _TrainingMatch_RestoreLastMatchState();
    uint8_t saved = currentTeam.team.tactics;
    _TrainingMatch_ApplyUseMatchTeamAndSwapSubs();
    currentTeam.team.tactics = saved;
    _TrainingMatch_RestoreSavedTeamSelectionState();
    _SetBenchPlayersNumbers(&careerTeam);
    _TrainingMatch_BuildCurrentTeamFromCareerTeam();
    selectedPlayerPosition = -1;
    _SavePositionsAndUpdateLittleCheckmark();
    _UpdateLittleCheckmark();
    _SetVariableButtons();
    _SetUpTacticsButtons();
    _SetPlayerSurnamesOnSidePitch();
    _ShowOrHidePlayMatchButton();
    _UpdateTeamCheckmark();
    _SetCurrentEntry(0x1D);
    _DrawMenu();
}

/* ================================================================ */
__attribute__((used, externally_visible))
void _TogglePlayerCoach(void) {
    if (gameType == GAME_TYPE_CAREER) {
        if (is_A_team) {
            uint8_t nc = (currentTeam.team.teamControls == TEAM_PLAYER_COACH)
                         ? TEAM_COACH : TEAM_PLAYER_COACH;
            currentTeam.team.teamControls = nc;
            careerTeam.team.teamControls = 1;
            coachOrPlayer = 0;
        } else {
            coachOrPlayer ^= 1;
            uint8_t nc = coachOrPlayer ? TEAM_PLAYER_COACH : TEAM_COACH;
            careerTeam.team.teamControls = nc;
            selectedTeams->team.teamControls = nc;
            if (trainingGame)
                currentTeam.team.teamControls = 1;
        }
    } else {
        TeamFileEx *t = playMatchTeam1Ptr;
        t->team.teamControls = (t->team.teamControls == TEAM_PLAYER_COACH)
                          ? TEAM_COACH : TEAM_PLAYER_COACH;
    }
    _SetVariableButtons();
    _DrawMenuItemsWaitForVSync(togglePlayerCoachRedrawItemList);
}

/* ================================================================
 * DrawPlayerEntries  (line 81983)
 *
 * Before-draw callback for player entries in play match menu.
 * Sets colors, status sprites, shirt numbers, names, positions,
 * skills, and value/status for each player row.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawPlayerEntries(MenuEntry *a5) {
    int16_t d7 = a5->ordinal - 13;
    int16_t d5 = 0;

    MenuEntry *bgE = &menuEntries[31];  /* entry 31 */
    MenuEntry *numE = &menuEntries[33]; /* entry 33 */

    if (d7 == 16) {
        d5 = 1;
        d7 = d7 - 5 + maxSubstitutesEntries;
        bgE->bgColor = (bgE->bgColor & 0xFFF0) | BLUE_TO_BLACK_8;
        numE->bgColor = (numE->bgColor & 0xFFF0) | BLUE_TO_BLACK_8;
    } else if (d7 < 11) {
        if (!showingCpuTeams && !viewingOpponentTeam) {
            uint16_t c = (d7 == 0) ? YELLOW_GREEN_BLACK_14 : BRIGHT_BLUE_TO_BLUE_13;
            bgE->bgColor = (bgE->bgColor & 0xFFF0) | c;
            numE->bgColor = (numE->bgColor & 0xFFF0) | c;
        } else {
            bgE->bgColor = (bgE->bgColor & 0xFFF0) | RED_10;
            numE->bgColor = (numE->bgColor & 0xFFF0) | RED_10;
        }
    } else {
        /* >= 11: substitute */
        if (!viewingOpponentTeam && !showingCpuTeams) {
            if (d7 == 11 && maxSubstitutesEntries > 5) {
                bgE->bgColor = (bgE->bgColor & 0xFFF0) | YELLOW_GREEN_BLACK_14;
                numE->bgColor = (numE->bgColor & 0xFFF0) | YELLOW_GREEN_BLACK_14;
            } else {
                bgE->bgColor = (bgE->bgColor & 0xFFF0) | BLUE_TO_PURPLE_11;
                numE->bgColor = (numE->bgColor & 0xFFF0) | BLUE_TO_PURPLE_11;
            }
        } else {
            bgE->bgColor = (bgE->bgColor & 0xFFF0) | ORANGE_TO_BROWN_12;
            numE->bgColor = (numE->bgColor & 0xFFF0) | ORANGE_TO_BROWN_12;
        }
    }

    TeamFileEx *team = playMatchTeam1Ptr;
    if (viewingOpponentTeam) team = playMatchTeam2Ptr;

    uint16_t slot, fIdx;
    uint8_t *player = _GetPlayerInfoBreakPosition((TeamFileEx *)team, (uint16_t)d7, &slot, &fIdx);
    int16_t d4 = (int16_t)slot;
    int16_t d1 = (int16_t)fIdx;

    /* Injured check */
    if (!viewingOpponentTeam) {
        if (_IsPlayerInjured((PlayerFile *)player, playMatchExcludeFlags)) {
            bgE->bgColor = (bgE->bgColor & 0xFFF0) | RED_10;
            numE->bgColor = (numE->bgColor & 0xFFF0) | RED_10;
        }
    }

    /* Selected player highlight */
    if (!viewingOpponentTeam && d1 == selectedPlayerPosition) {
        bgE->bgColor = (bgE->bgColor & 0xFFF0) | ORANGE_TO_BROWN_6;
        numE->bgColor = (numE->bgColor & 0xFFF0) | ORANGE_TO_BROWN_6;
    }

    bgE->y = a5->y;

    /* Status sprite */
    _SetSquadMenuEntryStatusSprite(bgE, (PlayerFile *)player, (uint16_t)playMatchExcludeFlags);

    /* Position change arrows */
    if (!viewingOpponentTeam && d4 < 11) {
        uint8_t cur = playerPositionsArray[d1];
        uint8_t old = oldPlayerPositionsArray[d1];
        if (cur < old) bgE->fgWord = SPR_NOT_CHECK_SMALL;
        else if (cur > old) bgE->fgWord = SPR_CHECK_SMALL;
    }

    /* Checkmark entry sync */
    {
        int16_t selIdx;
        if (viewingOpponentTeam) {
            selIdx = (int16_t)*(uint16_t *)currentSelectedPlayerIndexPtr;
        } else if (playMatchTeam1Ptr == &careerTeam) {
            selIdx = (int16_t)selTeamSelectedPlayerPosition;
        } else if (playMatchTeam1Ptr == &currentTeam) {
            selIdx = (int16_t)currentTeamSelectedPlayerPosition;
        } else {
            selIdx = (int16_t)*(uint16_t *)currentSelectedPlayerIndexPtr;
        }
        if (selIdx == d1) {
            MenuEntry *ck = &menuEntries[37];
            ck->y = bgE->y;
            ck->bgColor = bgE->bgColor;
            ck->fgWord = bgE->fgWord;
        }
    }

    /* Shirt number entry (entry 32) */
    MenuEntry *shE = bgE + 1;
    PlayerFile *playerPf = (PlayerFile *)(player + 76);
    shE->isInvisible = 1;
    if (!d5) {
        shE->fgWord = (uint16_t)(uint8_t)playerPf->shirtNumber;
        shE->isInvisible = 0;
    }
    shE->y = a5->y;

    /* Surname entry (entry 33 = numE) */
    {
        uint8_t *surname = _ExtractSurname((uint8_t *)playerPf->playerName, 0x800D);
        numE->fg = (uintptr_t)surname;
        numE->y = a5->y;
    }

    /* Position text (entry 34) */
    MenuEntry *posE = numE + 1;
    {
        uint16_t posField = (playerPf->positionAndFace & PF_POSITION_MASK) >> 3;
        posE->fg = (uintptr_t)playerPositionsStringTable[posField >> 2];
        posE->y = a5->y;
    }

    /* Skills string (entry 35) */
    MenuEntry *skE = posE + 1;
    _FormTopSkillsString((uint8_t *)skE->fg, (PlayerFile *)player);
    skE->y = a5->y;

    /* Value/status (entry 36) */
    MenuEntry *valE = skE + 1;
    valE->isInvisible = 1;
    valE->y = a5->y;

    {
        if (_IsTeamNational((TeamFileEx *)playMatchTeam1Ptr)) {
            _InitPlayerValueDisplayStruct(player, (uint8_t *)valE);
        } else {
            _FormatPlayerValueOrStatusText(player, fundsBuffer);
            valE->fg = (uintptr_t)fundsBuffer;
        }
    }

    {
        uint16_t cb = 0x0D;
        uint8_t sf = (uint8_t)playerPf->form;
        if (sf == 0x9B)      cb = 0x0D;
        else if (sf == 0x9A) cb = 0x01;
        else {
            cb = 0x0F;
            if (!(playerPf->positionAndFace & PF_LOAN)) cb = PF_LOAN;
        }
        valE->stringFlags = (valE->stringFlags & 0xFFF0) | cb;
        valE->isInvisible = 0;
    }

    _DrawMultipleItems(6, &menuEntries[31]);
}

/* ================================================================
 * Chunk 151: FormTopSkillsString
 *
 * Forms the 3-letter skill abbreviation string shown after a player's
 * name in the play match menu (e.g., "TSP", "HCF").
 *
 * in:  a1 = destination buffer (must hold 4 bytes)
 *      a6 = player base (team entry pointer, player data at +0x4C)
 *
 * The algorithm:
 * - Get position group (bits 7-5 of POSITIONANDFACE field)
 * - Select even/odd skill letter and bit-rotation tables based on
 *   XOR of price and first two name bytes (bit 0)
 * - Extract 7 skill values (3-bit each) from PASSING longword via
 *   rotating by amounts from the bit-rotation table
 * - Pick the top 3 skill values and output their corresponding letters
 * ================================================================ */
extern void *topSkillLettersEven[];  /* 7 pointers to letter strings */
extern void *topSkillLettersOdd[];
extern void *topSkillBitRotEven[];   /* 7 pointers to byte arrays */
extern void *topSkillBitRotOdd[];

__attribute__((used, externally_visible))
void _FormTopSkillsString(uint8_t *dst, PlayerFile *playerFile)
{
    /* playerFile = GetPlayer-style pointer (teamBase + fileIdx*38) cast to PlayerFile *.
     * PlayerFile data is at playerFile[2] (2 * 38 = 76 bytes in). */
    PlayerFile *pf = &playerFile[2];

    uint16_t posGroup = (pf->positionAndFace & PF_POSITION_MASK) >> 5;
    if (posGroup == 0) {
        /* Goalkeeper: no skill string */
        *dst = 0;
        return;
    }
    posGroup -= 1;  /* 0-based index into tables (0-6) */

    /* Determine even/odd variant from XOR of price and name bytes */
    uint8_t price = (uint8_t)pf->price;
    uint8_t name0 = (uint8_t)pf->playerName[0];
    uint8_t name1 = (uint8_t)pf->playerName[1];
    uint8_t xorVal = price ^ name0 ^ name1;

    const uint8_t *letters;
    const uint8_t *bitRot;

    if (xorVal & 1) {
        letters = (const uint8_t *)topSkillLettersOdd[posGroup];
        bitRot = (const uint8_t *)topSkillBitRotOdd[posGroup];
    } else {
        letters = (const uint8_t *)topSkillLettersEven[posGroup];
        bitRot = (const uint8_t *)topSkillBitRotEven[posGroup];
    }

    /* Get PASSING longword (4 skills packed: passing, velocityHeading,
     * tacklingControl, speedFinishing) and shift left 4.
     *
     * The Amiga asm did `move.l (a0)+,d7` which on m68k BE reads the
     * bytes as (passing<<24)|(velocityHeading<<16)|(tacklingControl
     * <<8)|speedFinishing. A native `*(uint32_t *)` on our LE host
     * reverses the byte order, so the subsequent rot+mask extracts the
     * wrong skill nibbles — giving wrong top-skill letters ("VHF" in
     * place of "STP" etc.) and breaking the star count the same way.
     * Use BE_READ_U32 so both builds see the same packed layout. */
    uint32_t d7 = BE_READ_U32(&pf->passing);
    d7 <<= 4;

    /* Find top 3 skill values. Each skill is extracted as 3 bits after
     * rotating d7 left by the amount from bitRot[]. */
    int16_t best1 = -1, best2 = -1, best3 = -1;   /* skill values */
    uint8_t let1 = 0, let2 = 0, let3 = 0;          /* corresponding letters */

    for (int16_t i = 0; i < 7; i++) {
        uint8_t rot = bitRot[i];
        uint32_t rotated = (d7 << rot) | (d7 >> (32 - rot));
        uint8_t skillVal = (uint8_t)(rotated & 7);

        if ((int8_t)skillVal > (int8_t)best1) {
            best3 = best2; let3 = let2;
            best2 = best1; let2 = let1;
            best1 = skillVal;
            let1 = letters[i];
        } else if ((int8_t)skillVal > (int8_t)best2) {
            best3 = best2; let3 = let2;
            best2 = skillVal;
            let2 = letters[i];
        } else if ((int8_t)skillVal > (int8_t)best3) {
            best3 = skillVal;
            let3 = letters[i];
        }
    }

    *dst++ = let1;
    *dst++ = let2;
    *dst++ = let3;
    *dst = 0;
}

/* ================================================================
 * Chunk 212: ShowPlayMatchMenu (1 → play_match_menu.c)
 * ================================================================ */

extern uint16_t inEditTactics;
extern uint8_t playMatchMenu[];
extern void _TrainingMatch_AdjustSelectedTeamRosterForTrialists(uint16_t teamNum, uint16_t baseCount, TeamFileEx *teamPtr);

/* ShowPlayMatchMenu (SWOS2.s line 65312)
 * Sets up play match globals from params, optionally adjusts trialists,
 * shows the play match menu, returns playerNumThatStarted. */
__attribute__((used, externally_visible))
int16_t _ShowPlayMatchMenu(int16_t controlFlags, int16_t maxSubstitutes,
                            int16_t excludeFlags, int16_t isFirstPlayer,
                            int16_t squadChanges, TeamFileEx *team1, TeamFileEx *team2)
{
    isFirstPlayerChoosingTeam = (uint16_t)isFirstPlayer;
    squadChangesAllowed = (uint16_t)squadChanges;
    playMatchTeam1Ptr = team1;
    playMatchTeam2Ptr = team2;
    maxSubstitutesEntries = (uint16_t)maxSubstitutes;
    playMatchControlFlags = (uint16_t)controlFlags;
    playMatchExcludeFlags = (int16_t)excludeFlags;
    inEditTactics = (uint16_t)excludeFlags;

    if (gameType == GAME_TYPE_CAREER) {
        _TrainingMatch_AdjustSelectedTeamRosterForTrialists(
            (uint16_t)playMatchExcludeFlags,
            maxSubstitutesEntries,
            (TeamFileEx *)playMatchTeam1Ptr);
    }

    _ShowMenu(playMatchMenu);
    return (int16_t)playerNumThatStarted;
}
