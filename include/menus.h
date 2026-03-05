#ifndef MENUS_H
#define MENUS_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef short int16_t;

void _PrepareMenuBackground(void);
void _InitMainMenuStuff(void);
void _ZeroOutStars(void);
void _SetDefaultOptions(void);
void _SetDefaultNameAndSurname(void);
void _InitCareerVariables(uint16_t gameTypeParam);
void _InitUserTactics(void);
void _InitializeTacticsPositions(void);
void _SaveOptions(void);
void _RestoreOptions(void);
void _ShowErrorMenu(uint8_t *errorTextStr);
int16_t _DoContinueAbortMenu(uint8_t *header, uint8_t *changing, uint8_t *contText, uint8_t *abortTextStr, uint16_t defaultEntry);
void _ContinueAbortInit(void);
void _ContinueSelected(void);
void _AbortSelected(void);
void _ContinueInit(void);
void _ContinueMenuContinueSelected(void);

/* --- Stadium menu (Chunk 89) --- */
struct TeamGame;
void _ShowStadiumInit(void);
void _StadiumPlayerSpritesBeforeDraw(struct TeamGame *team, uint16_t playerIndex);
void _StadiumBeforeDrawLeftTeam(void *entry);
void _StadiumBeforeDrawRightTeam(void *entry);

/* --- Squad menu callbacks (Chunk 111) --- */
void _SelectTraining(void);
void _SelectGoals(void);
void _SquadOnSelect11(void);
void _SquadOnSelect45(void);
void _Squad45AfterDraw(void);

/* --- Squad operations (Chunk 112) --- */
void _UpdateBigCheckMark(void);
void _SquadRedrawPlayerListAndCheckMark(void);
void _SquadMenuSavePlayerPositions(void);
void _SquadCommitPositionsAndUpdateSkill(void);

/* --- Options menu callbacks (Chunk 113) --- */
#include "menu_system.h"
void _OptionsMenuBeforeDraw(void);
void _ChangePitchType(MenuEntry *entry);
void _ChangeGameLength(MenuEntry *entry);
void _ChangeAutoReplays(MenuEntry *entry);
void _ChangeAllPlTeamsEqual(MenuEntry *entry);
void _ChangeAutoSaveHighlights(MenuEntry *entry);
void _ChangeMenuMusic(MenuEntry *entry);

/* --- Simple menu exit/draw callbacks (Chunk 114) --- */
void _CompetitionTopGoalScorersExit(void);
void _TopGoalScorersExit(void);
void _BuyPlayerExit(void);
void _ViewSquadExitContinue(void);
void _ViewSquad21AfterDraw(void);
void _SelectNationalTeamExit(void);
void _SelectNationalTeam25AfterDraw(void);
void _SelTeamForTacticsExit(void);
void _AbortSaveFile(void);

/* --- CareerOver + ViewSquad + National callbacks (Chunk 115) --- */
void _CareerOverInit(void);
void _ViewSquadOnSelect21(void);
void _SelectNationalTeamOnSelect25(void);

/* --- Menu navigation callbacks (Chunk 116) --- */
void _SWOSMainMenuInit(void);
void _ManagementRecordMenu(void);
void _ViewCompetitionsMenuRetired(void);

/* --- CommonMenuExit (Chunk 118) --- */
void _CommonMenuExit(void);

/* --- Menu callbacks + friendly/national (Chunk 119) --- */
void _FriendlyMenu(void);
void _EditCustomTeamMenu(void);
void _EditTacticsMenu(void);
void _HighlightsMenu(void);
void _OptionsMenuSelected(void);
void _SaveDiskFilingMenu(void);
void _ShowFriendlyMenu(void);
uint16_t _ShowSelectRepresentationMenu(void);
void _SetSelectTwoTeamsText(void);

/* --- Simple menu callbacks (Chunk 121) --- */
void _FriendlyOK(void);
void _ViewSelectedTeams(void);
void _ContestMenuScrollUp(void);
void _ContestMenuScrollDown(void);

/* --- Contest menu helpers (Chunk 122) --- */
void _ScrollUpInGameList(void);
void _ScrollDownInGameList(void);
void _RestoreContestScrollPosition(void);
void _SelectActiveContestAndRound(void);
void _CheckClubBusiness(void);
void _CheckJobOffers(void);
void _ContestMenuOnReturn_SetButtonsAndDefaultSelection(MenuEntry *a5);
void _SeasonLowerButtons(void);
void _CheckTransfers(void);
void _BuildGameListArrayFromTeamSelectionKeyCache(void);
void _PrintNameAndSeason(void);
void _SetNationalSelectorButtons(void);
void _ContestSetButtons(void);
void _ContestMenuInit(void);
void _ContestMenuOnReturn(void);

/* --- Contest menu action handlers (Chunk 123) --- */
void _PlayMatchMenu(MenuEntry *callerEntry);
void _NextMatch(MenuEntry *callerEntry);
void _ViewCompetitionsMenu(void);
void _ClubBusinessMenu(void);
void _JobOffersMenu(void);
void _SeasonFinished(void);
void _TransfersMenu(void);
void _ViewWorldMenu(void);
void _jmp_SetExitMenuFlag(void);

/* --- Choose/select team menu functions (Chunk 125) --- */
void _ChooseCompetitionMenu(void);
void _SelectTeamsForPresetCompetition(void);
void _ChooseSeasonMenu(void);
void _GoGetTeamsForPlay(uint8_t *gameTypeStrParam, uint8_t *playWhatStrParam,
                        uint16_t numTeams, uint16_t showClub,
                        uint16_t showNational, uint16_t showOrigCustom);

/* --- Chunk 201: ChooseTeamsDialog --- */
void _ChooseTeamsDialog(uint16_t showClub, uint16_t showNational, uint16_t showOrigCustom,
                        uint8_t *title, void *onSelect);
void _ChooseTeamsDialogWithNationality(uint16_t nationality, uint16_t showClub,
                                       uint16_t showNational, uint16_t showOrigCustom,
                                       uint8_t *title, void *onSelect);

/* --- Chunk 202: SelectTeamToManage --- */
void _SelectTeamToManage(void);

/* --- Chunk 204: Career_ShowAddInternationalPlayerDialog --- */
void _Career_ShowAddInternationalPlayerDialog(uint16_t homeOrAbroadFlag);

/* --- Chunk 205: Career_AddInternationalPlayerCommon + Home/Abroad --- */
void _Career_AddInternationalPlayerCommon(uint8_t *selectedTeam, uint8_t *title);
void _AddInternationalPlayerHome(uint8_t *selectedTeam);
void _AddInternationalPlayerAbroad(uint8_t *selectedTeam);

/* --- Chunk 206: National Team Selection family --- */
void _SelectNationalTeamInit(void);
void _SelectNationalTeamFinish(void);
void _Career_RebuildInternationalPlayerSelectionMasks(void);
void _NationalTeamAddPlayer(void);
void _NationalTeamRemovePlayers(void);
void _SelectNationalTeamAbort(void);
void _SelectNationalTeamCommon(void *entryPtr);
void _SelectNationalTeamAfterDrawCommon(void *entryPtr);

/* --- Competition menu functions (Chunk 124) --- */
void _DIYCompetitionMenu(void);
void _PresetCompetitionMenu(void);
void _SeasonMenu(void);
void _CareerMenu(void);
void _StartContest(void);
void _SaveContest(void);
void _LoadOldCompetitionMenu(void);

/* Chunk 139 — Team selection type handlers */
void _ClubTeams(void);
void _NationalTeams(void);
void _OriginalCustomTeams(void);
void _EditedCustomTeams(void);
void _ExitChooseTeams(void);
void _SetLeagueNames(void);
void _SetCountryNames(void);

/* ---- Menu Entry Utilities (Chunk 141) ---- */
#include "menu_system.h"
void _SetMenuEntryPlayerFace(MenuEntry *entry, uint8_t *teamData);

/* --- SaveCareer (Chunk 142) --- */
void _SaveCareer(void);

/* --- ExitOptions + SquadMenuStatusSprite (Chunk 143) --- */
void _ExitOptions(void);
void _SetSquadMenuEntryStatusSprite(MenuEntry *entry, uint8_t *player, uint16_t flag);
void _SetSquadMenuEntryStatusSprite_AfterSim(MenuEntry *entry, uint8_t *player);

/* --- Friendly pitch/season menu callbacks (Chunk 147) --- */
void _SetFriendlyPitchOrSeason(void);
void _PitchTypeOrSeasonProc(void);
void _FriendlyPitchTypeProc(MenuEntry *entry);

/* --- Friendly menu cluster + RandomlySwapTeams (Chunk 149) --- */
void _Friendly(void);
void _DrawNumbersOfSubstitutes(void);
void _DisableEntries(void);
void _FriendlyMenuInit(void);
void _FriendlyMenuReinit(void);
void _ChooseTeamsMenu(void);
void _CheckNumberOfTeams(void);
void _MinSubstitutes(void);
void _MaxSubstitutes(void);
void _RandomlySwapTeams(void);

/* --- ShowSquad (Chunk 164) --- */
int16_t _ShowSquad(uint8_t *teamPtr, uint8_t *title, uint32_t maskLo,
                    uint32_t maskHi, uint16_t arg2, uint16_t arg3);

/* --- ViewSquad Init/Common/AfterDraw (Chunk 163) --- */
void _ViewSquadInit(void);
void _ViewSquadCommon(void *entry);
void _ViewSquadAfterDrawCommon(void *entry);

#endif
