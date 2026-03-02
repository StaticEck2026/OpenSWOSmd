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

#endif
