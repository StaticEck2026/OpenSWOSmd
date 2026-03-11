/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "menu_data.h"

extern void _SetExitMenuFlag(void);
extern void _TopGoalScorersMenu_0(void);
extern void _SetupDIYCompetitionStatsMenu(void);

/* ---- Stub functions (trivial wrappers removed from SWOS2.s) ---- */
void _GlobalRetn(void) { }
void _NationalTeamAddPlayerInit(void) { }
void _locret_139AE2(void) { }
void _ExitPlayerMarketMenu(void) { _SetExitMenuFlag(); }
void _DesignDIYTournamentOnSelect91(void) { _SetExitMenuFlag(); }
void _TopGoalScorersMenu(void) { _TopGoalScorersMenu_0(); }
void _DIYCupOnSelect9(void) { _SetupDIYCompetitionStatsMenu(); }
void _j_SetExitMenuFlag(void) { _SetExitMenuFlag(); }
void _j_SetExitMenuFlag2(void) { _SetExitMenuFlag(); }
void _j_SetExitMenuFlag13(void) { _SetExitMenuFlag(); }
void _j_SetExitMenuFlag_10(void) { _SetExitMenuFlag(); }
void _jmpSetExitMenuFlag(void) { _SetExitMenuFlag(); }
void _jmp_SetExitMenuFlag_2(void) { _SetExitMenuFlag(); }
void _jmp_SetExitMenuFlag_3(void) { _SetExitMenuFlag(); }
void _jmp_SetExitMenuFlag_4(void) { _SetExitMenuFlag(); }
void _jmp_SetExitMenuFlag_5(void) { _SetExitMenuFlag(); }
void _jmp_SetExitMenuFlag_6(void) { _SetExitMenuFlag(); }
void _jmp_SetExitMenuFlag_8(void) { _SetExitMenuFlag(); }

/* ---- External function declarations ---- */
extern void _AbortChooseTeamsFriendly(void);
extern void _AbortEditTactics(void);
extern void _AbortSaveFile(void);
extern void _AbortSelected(void);
extern void _AcceptJobOffer(void);
extern void _AcceptOffer(void);
extern void _AskForMore(void);
extern void _BuyOtherDomesticPlayer(void);
extern void _BuyOtherForeignPlayer(void);
extern void _BuyOtherPlayersAfterDrawCommon(void);
extern void _BuyOtherPlayersCommon(void);
extern void _BuyOtherPlayersFinish(void);
extern void _BuyOtherPlayersInit(void);
extern void _BuyPlayerExit(void);
extern void _BuyPlayerInit(void);
extern void _BuyPlayerMenu(void);
extern void _BuyPlayerViewSquad(void);
extern void _CareerGameListCommon(void);
extern void _CareerMenu(void);
extern void _CareerMenuFinish(void);
extern void _CareerMenuInit(void);
extern void _CareerOverFinish(void);
extern void _CareerOverInit(void);
extern void _ChairmanMemosContinue(void);
extern void _ChairmanMemosInit(void);
extern void _ChairmanMemosOnReturn(void);
extern void _ChangeAllPlTeamsEqual(void);
extern void _ChangeAutoReplays(void);
extern void _ChangeAutoSaveHighlights(void);
extern void _ChangeGameLength(void);
extern void _ChangeMenuMusic(void);
extern void _ChangeNationality(void);
extern void _ChangePitchType(void);
extern void _ChangeSex(void);
extern void _ChangeTeams(void);
extern void _ChooseTacticsInit(void);
extern void _ChooseTeamAndStartCareer(void);
extern void _ChooseTeamsFriendlyCommon(void);
extern void _ChooseTeamsFriendlyCommon2(void);
extern void _ChooseTeamsFriendlyInit(void);
extern void _ChooseTeamsFriendlyInitFinish(void);
extern void _ChooseTeamsFriendlyOnSelect76(void);
extern void _ChooseTeamsInit(void);
extern void _ChooseTeamsMenu(void);
extern void _ChooseTeamsPlayGame(void);
extern void _ChooseTeamsPlayMatchSelected(void);
extern void _ClubBusinessInit(void);
extern void _ClubBusinessMenu(void);
extern void _ClubTeams(void);
extern void _CoachOrPlayerCoach(void);
extern void _CompetitionInfo(void);
extern void _CompetitionTopGoalScorersExit(void);
extern void _CompetitionTopGoalScorersInit(void);
extern void _ConsiderJobOffer(void);
extern void _ConsiderOffer(void);
extern void _ContestMenuInit(void);
extern void _ContestMenuOnReturn(void);
extern void _ContestMenuScrollDown(void);
extern void _ContestMenuScrollUp(void);
extern void _ContinueAbortInit(void);
extern void _ContinueCareer(void);
extern void _ContinueInit(void);
extern void _ContinueMenuContinueSelected(void);
extern void _ContinueSelected(void);
extern void _CopyTactics(void);
extern void _DIYCompetitionMenu(void);
extern void _DIYCupFinish(void);
extern void _DIYCupInit(void);
extern void _DIYCupOnSelect1(void);
extern void _DIYCupOnSelect10(void);
extern void _DIYCupOnSelect16(void);
extern void _DIYCupOnSelect17(void);
extern void _DIYCupOnSelect3(void);
extern void _DIYCupOnSelect4(void);
extern void _DIYCupOnSelect5(void);
extern void _DIYCupOnSelect6(void);
extern void _DIYCupOnSelect7(void);
extern void _DIYCupOnSelect8(void);
extern void _DIYLeagueChangePitchTypeOrSeason(void);
extern void _DIYLeagueTournament16BeforeDraw(void);
extern void _DIYLeagueTournamentExit(void);
extern void _DIYLeagueTournamentInit(void);
extern void _DIYLeagueTournamentMatch(void);
extern void _DIYLeagueTournamentNextMatch(void);
extern void _DIYLeagueTournamentOnRestore(void);
extern void _DIYLeagueTournamentOnSelect4(void);
extern void _DIYLeagueTournamentResult(void);
extern void _DIYLeagueTournamentScrollDown(void);
extern void _DIYLeagueTournamentScrollUp(void);
extern void _DIYLeagueTournamentStats(void);
extern void _DIYLeagueTournamentViewHighlights(void);
extern void _DIYLeagueTournamentViewMatch(void);
extern void _DIYLeagueTournamentViewResult(void);
extern void _DesignDIYCupInit(void);
extern void _DesignDIYCupOK(void);
extern void _DesignDIYCupOnDraw(void);
extern void _DesignDIYCupOnSelect11(void);
extern void _DesignDIYCupOnSelect13(void);
extern void _DesignDIYCupOnSelect15(void);
extern void _DesignDIYCupOnSelect2(void);
extern void _DesignDIYCupOnSelect20(void);
extern void _DesignDIYCupOnSelect21(void);
extern void _DesignDIYCupOnSelect22(void);
extern void _DesignDIYCupOnSelect3(void);
extern void _DesignDIYCupOnSelect4(void);
extern void _DesignDIYCupOnSelect48(void);
extern void _DesignDIYCupOnSelect5(void);
extern void _DesignDIYCupOnSelect7(void);
extern void _DesignDIYCupOnSelect9(void);
extern void _DesignDIYCupReinit(void);
extern void _DesignDIYLeagueChangeEndMonth(void);
extern void _DesignDIYLeagueChangeMaxSubstitutes(void);
extern void _DesignDIYLeagueChangeMinSubstitutes(void);
extern void _DesignDIYLeagueChangeNumPointsForWin(void);
extern void _DesignDIYLeagueChangeNumberOfTeams(void);
extern void _DesignDIYLeagueChangePitchType(void);
extern void _DesignDIYLeagueChangePlayEachTeam(void);
extern void _DesignDIYLeagueChangeStartMonth(void);
extern void _DesignDIYLeagueChooseTeams(void);
extern void _DesignDIYLeagueFinish(void);
extern void _DesignDIYLeagueInit(void);
extern void _DesignDIYLeagueOK(void);
extern void _DesignDIYLeagueSetName(void);
extern void _DesignDIYTournamentInit(void);
extern void _DesignDIYTournamentOK(void);
extern void _DesignDIYTournamentOnDraw(void);
extern void _DesignDIYTournamentOnSelect11(void);
extern void _DesignDIYTournamentOnSelect13(void);
extern void _DesignDIYTournamentOnSelect2(void);
extern void _DesignDIYTournamentOnSelect20(void);
extern void _DesignDIYTournamentOnSelect21(void);
extern void _DesignDIYTournamentOnSelect23(void);
extern void _DesignDIYTournamentOnSelect24(void);
extern void _DesignDIYTournamentOnSelect25(void);
extern void _DesignDIYTournamentOnSelect26(void);
extern void _DesignDIYTournamentOnSelect28(void);
extern void _DesignDIYTournamentOnSelect3(void);
extern void _DesignDIYTournamentOnSelect4(void);
extern void _DesignDIYTournamentOnSelect5(void);
extern void _DesignDIYTournamentOnSelect7(void);
extern void _DesignDIYTournamentOnSelect89(void);
extern void _DesignDIYTournamentOnSelect9(void);
extern void _DesignDIYTournamentReinit(void);
extern void _DesignDIYTournament_AdjustStageQualifiers(void);
extern void _DrawPlayerEntries(void);
extern void _EditCustomTeamMenu(void);
extern void _EditTacicsMovePlayersOrBall(void);
extern void _EditTactics28AfterDraw(void);
extern void _EditTactics2AfterDraw(void);
extern void _EditTacticsAfterDrawPlayerFace(void);
extern void _EditTacticsAfterDrawPlayerNames(void);
extern void _EditTacticsCommon(void);
extern void _EditTacticsInPlayMatchMenu(void);
extern void _EditTacticsInit(void);
extern void _EditTacticsMenu(void);
extern void _EditTacticsOnRestore(void);
extern void _EditTacticsOnSelect28(void);
extern void _EditTacticsPlayerNamesCommon(void);
extern void _EditTeamsChangePlayerFace(void);
extern void _EditTeamsChangePlayerName(void);
extern void _EditTeamsChangeShirtsPrimary(void);
extern void _EditTeamsChangeShirtsSecondary(void);
extern void _EditTeamsChangeShortsPrimary(void);
extern void _EditTeamsChangeShortsSecondary(void);
extern void _EditTeamsChangeSocksPrimary(void);
extern void _EditTeamsChangeSocksSecondary(void);
extern void _EditTeamsFinish(void);
extern void _EditTeamsInit(void);
extern void _EditTeamsOK(void);
extern void _EditTeamsPlayersBeforeDraw(void);
extern void _EditTeamsSelectCoachName(void);
extern void _EditTeamsSelectPrimaryShirtType(void);
extern void _EditTeamsSelectSecondaryShirtType(void);
extern void _EditTeamsSelectTeamName(void);
extern void _EditTeamsUndo(void);
extern void _EditedCustomTeams(void);
extern void _ExitCareerMenu(void);
extern void _ExitChooseTacticsMenu(void);
extern void _ExitChooseTeams(void);
extern void _ExitChooseTeams2(void);
extern void _ExitHighlights(void);
extern void _ExitImportTactics(void);
extern void _ExitOptions(void);
extern void _ExitPlayMatch(void);
extern void _ExitSelectTeams(void);
extern void _FillManagementRecordInfo(void);
extern void _FlipOnOff(void);
extern void _FriendlyMenu(void);
extern void _FriendlyMenuInit(void);
extern void _FriendlyMenuReinit(void);
extern void _FriendlyOK(void);
extern void _FriendlyPitchTypeProc(void);
extern void _GiveFreeTransfer(void);
extern void _GotJobOfferFinish(void);
extern void _GotJobOfferInit(void);
extern void _HighlightsMenu(void);
extern void _HighlightsMenuInit(void);
extern void _HighlightsMenuOnReturn(void);
extern void _ImportTactics(void);
extern void _ImportTacticsInit(void);
extern void _ImportTacticsSelectTactics(void);
extern void _ImportTeamsMenu(void);
extern void _JmpShowHighlights(void);
extern void _JobOffer(void);
extern void _JobOffersAfterDrawCommon(void);
extern void _JobOffersCommon(void);
extern void _JobOffersExit(void);
extern void _JobOffersFinish(void);
extern void _JobOffersInit(void);
extern void _JobOffersMenu(void);
extern void _JobOffersMenuRetired(void);
extern void _JobOffers_DrawOldOfferLine(void);
extern void _LoadHighlightsMenu(void);
extern void _LoadLiverpool(void);
extern void _LoadOldCompetitionMenu(void);
extern void _LoadTactics(void);
extern void _ManagementArrowDownSelect(void);
extern void _ManagementArrowUpSelect(void);
extern void _ManagementRecordInit(void);
extern void _ManagementRecordMenu(void);
extern void _MaxSubstitutes(void);
extern void _MinSubstitutes(void);
extern void _NationalTeamAddPlayer(void);
extern void _NationalTeamAddPlayerAbroadOnSelect(void);
extern void _NationalTeamAddPlayerExit(void);
extern void _NationalTeamAddPlayerHomeOnSelect(void);
extern void _NationalTeamRemovePlayers(void);
extern void _NationalTeams(void);
extern void _NextMatch(void);
extern void _OffersAfterDrawCommon(void);
extern void _OffersCommon(void);
extern void _OffersInit(void);
extern void _OldJobOffersCommon(void);
extern void _OnSwapPlayer(void);
extern void _OpenDiyFromFile(void);
extern void _OpenDiyPreset1(void);
extern void _OpenDiyPreset2(void);
extern void _OpenDiyPreset3(void);
extern void _OptionsMenuBeforeDraw(void);
extern void _OptionsMenuSelected(void);
extern void _OriginalCustomTeams(void);
extern void _PitchTypeOrSeasonProc(void);
extern void _PlayMatch30AfterDraw(void);
extern void _PlayMatchChangeTactics(void);
extern void _PlayMatchMenu(void);
extern void _PlayMatchMenuInit(void);
extern void _PlayMatchMenuReinit(void);
extern void _PlayMatchOnSelect30(void);
extern void _PlayMatchSelected(void);
extern void _PlayerMarketFilterAgeGroupOnSelect(void);
extern void _PlayerMarketFilterPositionOnSelect(void);
extern void _PlayerMarketItemBeforeDraw(void);
extern void _PlayerMarketItemOnSelect(void);
extern void _PlayerMarketScrollOnSelect(void);
extern void _PlayerMarketViewSquadOnSelect(void);
extern void _PlayerOffersAdjustOfferAmount(void);
extern void _PlayerOffersAfterDrawCommon(void);
extern void _PlayerOffersInit(void);
extern void _PlayerOffersOnSelect10(void);
extern void _PlayerOffersOnSelect11(void);
extern void _PlayerOffersOnSelect12(void);
extern void _PlayerOffersOnSelect13(void);
extern void _PlayerOffersOnSelect8(void);
extern void _PresetCompetitionMenu(void);
extern void _PutPlayerOnMarketMenu(void);
extern void _RejectJobOffer(void);
extern void _RejectOffer(void);
extern void _ReturnFromViewOpponent(void);
extern void _SWOSMainMenuInit(void);
extern void _SaveAndExitEditTactics(void);
extern void _SaveCareer(void);
extern void _SaveContest(void);
extern void _SaveDiskFilingMenu(void);
extern void _SaveHighlightsMenu(void);
extern void _SeasonMenu(void);
extern void _SelFilesDrawItemDescription(void);
extern void _SelFilesOnSelectCommon(void);
extern void _SelTeamForTacticsCommon(void);
extern void _SelTeamForTacticsExit(void);
extern void _SelTeamForTacticsInit(void);
extern void _SelectCopyTactics(void);
extern void _SelectFilenameToSaveAndExit(void);
extern void _SelectFilesInit(void);
extern void _SelectFilesInputSaveFilenameOnSelect(void);
extern void _SelectFilesOnReturn(void);
extern void _SelectGoals(void);
extern void _SelectLeaguesCommon(void);
extern void _SelectNationalTeam25AfterDraw(void);
extern void _SelectNationalTeamAbort(void);
extern void _SelectNationalTeamAfterDrawCommon(void);
extern void _SelectNationalTeamCommon(void);
extern void _SelectNationalTeamExit(void);
extern void _SelectNationalTeamFinish(void);
extern void _SelectNationalTeamInit(void);
extern void _SelectNationalTeamOnSelect25(void);
extern void _SelectTeamsCommon(void);
extern void _SelectTeamsInit(void);
extern void _SelectTeamsReinit(void);
extern void _SelectTraining(void);
extern void _SetName(void);
extern void _SetSurname(void);
extern void _ShowPlayerMarket(void);
extern void _ShowPlayerMarketInit(void);
extern void _ShowStadiumInit(void);
extern void _Squad45AfterDraw(void);
extern void _SquadAfterDrawCommon(void);
extern void _SquadCommon(void);
extern void _SquadFinish(void);
extern void _SquadInit(void);
extern void _SquadMenu(void);
extern void _SquadOnSelect11(void);
extern void _SquadOnSelect45(void);
extern void _StadiumBeforeDrawLeftTeam(void);
extern void _StadiumBeforeDrawRightTeam(void);
extern void _StartContest(void);
extern void _StatsExit(void);
extern void _StatsHighestScorerList(void);
extern void _StatsViewSquads(void);
extern void _TakeOffMarketMenu(void);
extern void _TeamTactics(void);
extern void _ThirdPartyOfferMenuAfterDrawCommon(void);
extern void _ThirdPartyOfferMenuExit(void);
extern void _ThirdPartyOfferMenuInit(void);
extern void _ThirdPartyOfferViewSquad(void);
extern void _TogglePlayerCoach(void);
extern void _TopGoalScorersBeforeDrawCommon(void);
extern void _TopGoalScorersExit(void);
extern void _TopGoalScorersInit(void);
extern void _TransferMarketAfterDrawCommon(void);
extern void _TransferMarketCommon(void);
extern void _TransferMarketFinish(void);
extern void _TransferMarketInit(void);
extern void _TransferPlayerAfterDrawCommon(void);
extern void _TransferPlayerInit(void);
extern void _TransferPlayerOnSelect10(void);
extern void _TransferPlayerOnSelect11(void);
extern void _TransferPlayerOnSelect12(void);
extern void _TransfersMenu(void);
extern void _UndoTactics(void);
extern void _UseMatchTeamSelect(void);
extern void _User_A_Tactics(void);
extern void _User_B_Tactics(void);
extern void _User_C_Tactics(void);
extern void _User_D_Tactics(void);
extern void _User_E_Tactics(void);
extern void _User_F_Tactics(void);
extern void _ViewCompetitionsInit(void);
extern void _ViewCompetitionsMenu(void);
extern void _ViewCompetitionsMenuRetired(void);
extern void _ViewCompetitionsOnReturn(void);
extern void _ViewDomesticPlayerMarket(void);
extern void _ViewForeignPlayerMarket(void);
extern void _ViewFutureTeam(void);
extern void _ViewFutureTeamLeague(void);
extern void _ViewOffersMenu(void);
extern void _ViewOpponentsSelect(void);
extern void _ViewSelected(void);
extern void _ViewSelectedTeams(void);
extern void _ViewSquad21AfterDraw(void);
extern void _ViewSquadAfterDrawCommon(void);
extern void _ViewSquadCommon(void);
extern void _ViewSquadExitContinue(void);
extern void _ViewSquadInit(void);
extern void _ViewSquadOnSelect21(void);
extern void _ViewWorldMenu(void);
extern void _j_SetExitMenuFlag(void);
extern void _j_SetExitMenuFlag_11(void);
extern void _j_SetExitMenuFlag_14(void);
extern void _jmp_SetExitMenuFlag(void);
extern void _jmp_SetExitMenuFlag_7(void);

/* ---- External string constants ---- */
extern char a343[];
extern char a352[];
extern char a424[];
extern char a433[];
extern char a442[];
extern char a451[];
extern char a523[];
extern char a532[];
extern char a541[];
extern char aA_0[];
extern char aAbort[];
extern char aAccept[];
extern char aAcceptOffer[];
extern char aAddInternation[];
extern char aAddInternation_0[];
extern char aAddPlayer[];
extern char aAdditionalInve_0[];
extern char aAllMatches[];
extern char aAllPlayerTeams[];
extern char aAskForMore[];
extern char aAttack[];
extern char aAutoReplays[];
extern char aAutoSaveHighli[];
extern char aAwayGoals[];
extern char aBankStatement[];
extern char aBestRegards[];
extern char aBuyOtherDomest[];
extern char aBuyOtherForeig[];
extern char aBuyPlayer[];
extern char aCareerTotal[];
extern char aCareer_1[];
extern char aChooseTeam[];
extern char aChooseTeams[];
extern char aClubBusiness[];
extern char aClubTeams[];
extern char aCoach[];
extern char aCoach_3[];
extern char aCoach_4[];
extern char aCompetitionBon[];
extern char aCompetitionInf[];
extern char aComputer[];
extern char aConfirmOffer[];
extern char aConsider[];
extern char aContinue[];
extern char aContinueCareer[];
extern char aCopy[];
extern char aCriteria[];
extern char aCup1[];
extern char aCup2[];
extern char aCup_3[];
extern char aCups[];
extern char aD[];
extern char aDearSirMadam[];
extern char aDefend[];
extern char aDescription[];
extern char aDesignDiyCup[];
extern char aDesignDiyLeagu[];
extern char aDesignDiyTourn[];
extern char aDiyCompetition_0[];
extern char aEditCustomTeam[];
extern char aEditTactics[];
extern char aEditedCustomTe_0[];
extern char aExPlayerGoals[];
extern char aExit[];
extern char aF[];
extern char aFilename[];
extern char aFirstKit[];
extern char aFixPitchType[];
extern char aForm[];
extern char aFriendlyDiyGam[];
extern char aFriendly_1[];
extern char aFrom[];
extern char aGateReceipts[];
extern char aGeneralRunning[];
extern char aGiveFreeTransf[];
extern char aGoals[];
extern char aHighestScorerL[];
extern char aHighlights[];
extern char aHighlights_0[];
extern char aHyphen[];
extern char aImport[];
extern char aImportTactics[];
extern char aJobOffer[];
extern char aJobOffers[];
extern char aJobOffersBeing[];
extern char aL[];
extern char aLeadingCompeti[];
extern char aLge[];
extern char aLge_0[];
extern char aList[];
extern char aLoad[];
extern char aLoadHighlights[];
extern char aLoadLiverpool[];
extern char aLoadOldCompeti[];
extern char aMakeImprovedOf[];
extern char aMakeOffer[];
extern char aManagementReco_0[];
extern char aMatch[];
extern char aMemoFromTheClu[];
extern char aMenuMusic[];
extern char aName[];
extern char aNat[];
extern char aNationalTeams[];
extern char aNationality[];
extern char aNewBalance[];
extern char aNextMatch[];
extern char aNumberOfTeams[];
extern char aOffers[];
extern char aOk[];
extern char aOldBalance[];
extern char aOptions[];
extern char aOriginalCustom[];
extern char aOwnGoals[];
extern char aPl[];
extern char aPlayEachTeam[];
extern char aPlayMatch[];
extern char aPlayerCoach[];
extern char aPlayerPurchase[];
extern char aPlayerSales[];
extern char aPlayerWagesBil[];
extern char aPlayersCurrent[];
extern char aPlus[];
extern char aPointsForAWin[];
extern char aPresetCompetit_0[];
extern char aPts[];
extern char aPutPlayerOnTra_0[];
extern char aReject[];
extern char aRejectOffer[];
extern char aRemovePlayers[];
extern char aResponseToOffe[];
extern char aResult[];
extern char aRounds[];
extern char aSacked[];
extern char aSave[];
extern char aSaveCareer[];
extern char aSaveDiskFiling[];
extern char aSaveExit[];
extern char aSaveHighlights[];
extern char aSeason_1[];
extern char aSecondKit[];
extern char aSeeded[];
extern char aSkills[];
extern char aSquad[];
extern char aStar[];
extern char aStartCareerAs[];
extern char aStatistics[];
extern char aStats[];
extern char aSubstitutes[];
extern char aSweep[];
extern char aSwosVersion152[];
extern char aTakeOffMarket[];
extern char aTeams[];
extern char aThirdPartyOffe[];
extern char aToReplace[];
extern char aTopGoalScorers[];
extern char aTot[];
extern char aTot_0[];
extern char aTotalProfit[];
extern char aTraining[];
extern char aTransferMarket[];
extern char aTransfers[];
extern char aType[];
extern char aUndo[];
extern char aUseMatchTeam[];
extern char aV[];
extern char aViewCompetitio[];
extern char aViewDomesticPl[];
extern char aViewForeignPla[];
extern char aViewHighlights[];
extern char aViewHighlights_0[];
extern char aViewLeague[];
extern char aViewMatch[];
extern char aViewOffers[];
extern char aViewOppo[];
extern char aViewResult[];
extern char aViewSelectedTe[];
extern char aViewSquad[];
extern char aViewSquads[];
extern char aViewTeam[];
extern char aViewWorld[];
extern char aW[];
extern char aYourCareerIsOv[];
extern char aYourChairman[];
extern char asc_131FF2[];
extern char asc_135D96[];

/* ---- External string tables ---- */
extern char* allPlTeamsEqualStrTable[];
extern char* autoReplaysStrTable[];
extern char* autoSaveHlStrTable[];
extern char* diyAwayGoalsStrTab[];
extern char* diyCupAwayGoalsStringTable[];
extern char* diyCupEndMonthStringTable[];
extern char* diyCupExtraTimeStringTable0[];
extern char* diyCupExtraTimeStringTable1[];
extern char* diyCupExtraTimeStringTable2[];
extern char* diyCupExtraTimeStringTable3[];
extern char* diyCupExtraTimeStringTable4[];
extern char* diyCupExtraTimeStringTable5[];
extern char* diyCupLegsStringTable0[];
extern char* diyCupLegsStringTable1[];
extern char* diyCupLegsStringTable2[];
extern char* diyCupLegsStringTable3[];
extern char* diyCupLegsStringTable4[];
extern char* diyCupLegsStringTable5[];
extern char* diyCupPenaltiesStringTable0[];
extern char* diyCupPenaltiesStringTable1[];
extern char* diyCupPenaltiesStringTable2[];
extern char* diyCupPenaltiesStringTable3[];
extern char* diyCupPenaltiesStringTable4[];
extern char* diyCupPenaltiesStringTable5[];
extern char* diyCupPitchTypeOrSeasonStringTable[];
extern char* diyCupPitchTypeStringTable[];
extern char* diyCupStartMonthStringTable[];
extern char* diyEndMonthStrTab[];
extern char* diyLeagueEndingMonthStrTable[];
extern char* diyLeagueMonthsStrTable[];
extern char* diyLeaguePitchTypeStrTable[];
extern char* diyPitchSeasonStrTab[];
extern char* diyPitchTypeOrSeasonStrTable[];
extern char* diyPitchTypeStringTable[];
extern char* diyStage1ExtraTimeStrTab[];
extern char* diyStage1LegsStrTab[];
extern char* diyStage1PenaltiesStrTab[];
extern char* diyStage1PlayCountStrTab[];
extern char* diyStage1PtsForWinStrTab[];
extern char* diyStage2ExtraTimeStrTab[];
extern char* diyStage2LegsStrTab[];
extern char* diyStage2PenaltiesStrTab[];
extern char* diyStage2PlayCountStrTab[];
extern char* diyStage2PtsForWinStrTab[];
extern char* diyStage3ExtraTimeStrTab[];
extern char* diyStage3LegsStrTab[];
extern char* diyStage3PenaltiesStrTab[];
extern char* diyStage3PlayCountStrTab[];
extern char* diyStage3PtsForWinStrTab[];
extern char* diyStage4ExtraTimeStrTab[];
extern char* diyStage4LegsStrTab[];
extern char* diyStage4PenaltiesStrTab[];
extern char* diyStage4PlayCountStrTab[];
extern char* diyStage4PtsForWinStrTab[];
extern char* diyStage5ExtraTimeStrTab[];
extern char* diyStage5LegsStrTab[];
extern char* diyStage5PenaltiesStrTab[];
extern char* diyStage5PlayCountStrTab[];
extern char* diyStage5PtsForWinStrTab[];
extern char* diyStage6ExtraTimeStrTab[];
extern char* diyStage6LegsStrTab[];
extern char* diyStage6PenaltiesStrTab[];
extern char* diyStage6PlayCountStrTab[];
extern char* diyStage6PtsForWinStrTab[];
extern char* diyStartMonthStrTab[];
extern char* flipStrTable[];
extern char* friendlyPitchTypeTable[];
extern char* gameLengthStrTable[];
extern char* menuMusicStrTable[];
extern char* pitchTypeOrSeasonTable[];
extern char* pitchTypeStrTable[];
extern char* positionFilterStringTable[];
extern char* skillFilterStringTable[];

/* ---- External data (char type for pointer compatibility) ---- */
extern char managerNameBuffer[];
extern char managerSexBuffer[];
extern char managerSurnameBuffer[];
extern char offsetCoachOrPlayer[];
extern char saveFileTitle[];
extern char stadiumMenuLittlePlayer[];
extern char teamTacticsUSERA[];
extern char teamTacticsUSERB[];
extern char teamTacticsUSERC[];
extern char teamTacticsUSERD[];
extern char teamTacticsUSERE[];
extern char teamTacticsUSERF[];

/* ---- External data (uint8_t type) ---- */
extern uint8_t editTeamsPrimaryShirts[];
extern uint8_t editTeamsSecondaryShirt[];
extern uint8_t editTeamsSecondaryShorts[];
extern uint8_t editTeamsSecondarySocks[];
extern uint8_t editTeamsShorts[];
extern uint8_t editTeamsSocks[];

Menu_ChooseTeamsFriendly chooseTeamsFriendlyMenu = {
    { _ChooseTeamsFriendlyInitFinish, _ChooseTeamsFriendlyInitFinish, 0, 23 },
    { 2, 0, 300, 15 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { {
          { 20, 35, 19, 9 },
          { MENU_CMD_COLORS, 0xA },
          MENU_CMD_END,
      },
        {
            { 120, 35, 19, 9 },
            { MENU_CMD_COLORS, 0xB },
            MENU_CMD_END,
        },
        {
            { -36, 35, 19, 9 },
            { MENU_CMD_COLORS, 0xD },
            MENU_CMD_END,
        } },
    { {
          { 45, 35, 0, 11 },
          { MENU_CMD_TEXT_PTR, 0x8000, aComputer },
          MENU_CMD_END,
      },
        {
            { -111, 35, 0, 11 },
            { MENU_CMD_TEXT_PTR, 0x8000, aPlayerCoach },
            MENU_CMD_END,
        },
        {
            { -11, 35, 0, 11 },
            { MENU_CMD_TEXT_PTR, 0x8000, aCoach },
            MENU_CMD_END,
        } },
    { MENU_MARK_START_MENU, 0, 16 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x6 },
    { MENU_CMD_TEXT_PTR, 0x0, 0 },
    { MENU_CMD_ONSELECT_FIRE, _ChooseTeamsFriendlyCommon2 },
    MENU_CMD_END,
    {
        { 1, 21, 100, 9 },
        { MENU_CMD_NAV, -1, 23, -1, 8 },
        { MENU_CMD_LEFT_SKIP, 38, 2 },
        MENU_CMD_END,
    },
    { {
          { 1, 31, 100, 9 },
          { MENU_CMD_NAV, -1, 24, 7, 9 },
          { MENU_CMD_UP_SKIP, 22, 2 },
          MENU_CMD_END,
      },
        {
            { 1, 41, 100, 9 },
            { MENU_CMD_NAV, -1, 25, 8, 10 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 51, 100, 9 },
            { MENU_CMD_NAV, -1, 26, 9, 11 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 61, 100, 9 },
            { MENU_CMD_NAV, -1, 27, 10, 12 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 71, 100, 9 },
            { MENU_CMD_NAV, -1, 28, 11, 13 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 81, 100, 9 },
            { MENU_CMD_NAV, -1, 29, 12, 14 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 91, 100, 9 },
            { MENU_CMD_NAV, -1, 30, 13, 15 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 101, 100, 9 },
            { MENU_CMD_NAV, -1, 31, 14, 16 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 111, 100, 9 },
            { MENU_CMD_NAV, -1, 32, 15, 17 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 121, 100, 9 },
            { MENU_CMD_NAV, -1, 33, 16, 18 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -125, 100, 9 },
            { MENU_CMD_NAV, -1, 34, 17, 19 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -115, 100, 9 },
            { MENU_CMD_NAV, -1, 35, 18, 20 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -105, 100, 9 },
            { MENU_CMD_NAV, -1, 36, 19, 21 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -95, 100, 9 },
            { MENU_CMD_NAV, -1, 37, 20, 22 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -85, 100, 9 },
            { MENU_CMD_NAV, -1, 38, 21, 55 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        } },
    { {
          { 102, 21, 100, 9 },
          { MENU_CMD_NAV, 7, 39, -1, 24 },
          MENU_CMD_END,
      },
        {
            { 102, 31, 100, 9 },
            { MENU_CMD_NAV, 8, 40, 23, 25 },
            MENU_CMD_END,
        },
        {
            { 102, 41, 100, 9 },
            { MENU_CMD_NAV, 9, 41, 24, 26 },
            MENU_CMD_END,
        },
        {
            { 102, 51, 100, 9 },
            { MENU_CMD_NAV, 10, 42, 25, 27 },
            MENU_CMD_END,
        },
        {
            { 102, 61, 100, 9 },
            { MENU_CMD_NAV, 11, 43, 26, 28 },
            MENU_CMD_END,
        },
        {
            { 102, 71, 100, 9 },
            { MENU_CMD_NAV, 12, 44, 27, 29 },
            MENU_CMD_END,
        },
        {
            { 102, 81, 100, 9 },
            { MENU_CMD_NAV, 13, 45, 28, 30 },
            MENU_CMD_END,
        },
        {
            { 102, 91, 100, 9 },
            { MENU_CMD_NAV, 14, 46, 29, 31 },
            MENU_CMD_END,
        },
        {
            { 102, 101, 100, 9 },
            { MENU_CMD_NAV, 15, 47, 30, 32 },
            MENU_CMD_END,
        },
        {
            { 102, 111, 100, 9 },
            { MENU_CMD_NAV, 16, 48, 31, 33 },
            MENU_CMD_END,
        },
        {
            { 102, 121, 100, 9 },
            { MENU_CMD_NAV, 17, 49, 32, 34 },
            MENU_CMD_END,
        },
        {
            { 102, -125, 100, 9 },
            { MENU_CMD_NAV, 18, 50, 33, 35 },
            MENU_CMD_END,
        },
        {
            { 102, -115, 100, 9 },
            { MENU_CMD_NAV, 19, 51, 34, 36 },
            MENU_CMD_END,
        },
        {
            { 102, -105, 100, 9 },
            { MENU_CMD_NAV, 20, 52, 35, 37 },
            MENU_CMD_END,
        },
        {
            { 102, -95, 100, 9 },
            { MENU_CMD_NAV, 21, 53, 36, 38 },
            MENU_CMD_END,
        },
        {
            { 102, -85, 100, 9 },
            { MENU_CMD_NAV, 22, 54, 37, 56 },
            MENU_CMD_END,
        } },
    {
        { -53, 21, 100, 9 },
        { MENU_CMD_NAV, 23, -1, -1, 40 },
        { MENU_CMD_LEFT_SKIP, 38, 2 },
        MENU_CMD_END,
    },
    { {
          { -53, 31, 100, 9 },
          { MENU_CMD_NAV, 24, -1, 39, 41 },
          { MENU_CMD_DOWN_SKIP, 54, 2 },
          MENU_CMD_END,
      },
        {
            { -53, 41, 100, 9 },
            { MENU_CMD_NAV, 25, -1, 40, 42 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 51, 100, 9 },
            { MENU_CMD_NAV, 26, -1, 41, 43 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 61, 100, 9 },
            { MENU_CMD_NAV, 27, -1, 42, 44 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 71, 100, 9 },
            { MENU_CMD_NAV, 28, -1, 43, 45 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 81, 100, 9 },
            { MENU_CMD_NAV, 29, -1, 44, 46 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 91, 100, 9 },
            { MENU_CMD_NAV, 30, -1, 45, 47 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 101, 100, 9 },
            { MENU_CMD_NAV, 31, -1, 46, 48 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 111, 100, 9 },
            { MENU_CMD_NAV, 32, -1, 47, 49 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 121, 100, 9 },
            { MENU_CMD_NAV, 33, -1, 48, 50 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -125, 100, 9 },
            { MENU_CMD_NAV, 34, -1, 49, 51 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -115, 100, 9 },
            { MENU_CMD_NAV, 35, -1, 50, 52 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -105, 100, 9 },
            { MENU_CMD_NAV, 36, -1, 51, 53 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -95, 100, 9 },
            { MENU_CMD_NAV, 37, -1, 52, 54 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -85, 100, 9 },
            { MENU_CMD_NAV, 38, -1, 53, 57 },
            { MENU_CMD_DOWN_SKIP, 54, 2 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 0 },
    { 0, -71, 120, 15 },
    { MENU_CMD_NAV, -1, 56, 22, -1 },
    { { MENU_CMD_UP_SKIP, 55, 2 }, { MENU_CMD_RIGHT_SKIP, 55, 1 } },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aViewSelectedTe },
    { MENU_CMD_ONSELECT_FIRE, _ViewSelected },
    MENU_CMD_END,
    {
        { 121, -71, 62, 15 },
        { MENU_CMD_NAV, 55, 57, 38, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _ExitChooseTeams2 },
        MENU_CMD_END,
    },
    { -72, -71, 120, 15 },
    { MENU_CMD_NAV, 56, -1, 54, -1 },
    { { MENU_CMD_DOWN_SKIP, 57, 2 }, { MENU_CMD_RIGHT_SKIP, 57, 0 } },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _ChooseTeamsPlayMatchSelected },
    MENU_CMD_END,
    { -72, -68, 120, 9 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_ChooseTeamsFriendly2 chooseTeamsFriendlyMenu2 = {
    { _ChooseTeamsFriendlyInit, 0, 0, 0x1E },
    { 2, 0, 300, 15 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { 0, 0, 20, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_NUMBER, 0x0, 0 },
    MENU_CMD_END,
    { {
          { 20, 35, 19, 9 },
          { MENU_CMD_COLORS, 0xA },
          MENU_CMD_END,
      },
        {
            { 120, 35, 19, 9 },
            { MENU_CMD_COLORS, 0xB },
            MENU_CMD_END,
        },
        {
            { -36, 35, 19, 9 },
            { MENU_CMD_COLORS, 0xD },
            MENU_CMD_END,
        } },
    { {
          { 45, 35, 0, 11 },
          { MENU_CMD_TEXT_PTR, 0x8000, aComputer },
          MENU_CMD_END,
      },
        {
            { -111, 35, 0, 11 },
            { MENU_CMD_TEXT_PTR, 0x8000, aPlayerCoach },
            MENU_CMD_END,
        },
        {
            { -11, 35, 0, 11 },
            { MENU_CMD_TEXT_PTR, 0x8000, aCoach },
            MENU_CMD_END,
        } },
    { MENU_MARK_START_MENU, 0, -1 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x6 },
    { MENU_CMD_TEXT_PTR, 0x0, 0 },
    { MENU_CMD_ONSELECT_FIRE, _ChooseTeamsFriendlyCommon },
    MENU_CMD_END,
    {
        { 1, 21, 100, 8 },
        { MENU_CMD_NAV, -1, 30, -1, 9 },
        { MENU_CMD_LEFT_SKIP, 51, 2 },
        MENU_CMD_END,
    },
    { {
          { 1, 30, 100, 8 },
          { MENU_CMD_NAV, -1, 31, 8, 10 },
          { MENU_CMD_UP_SKIP, 29, 2 },
          MENU_CMD_END,
      },
        {
            { 1, 39, 100, 8 },
            { MENU_CMD_NAV, -1, 32, 9, 11 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 48, 100, 8 },
            { MENU_CMD_NAV, -1, 33, 10, 12 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 57, 100, 8 },
            { MENU_CMD_NAV, -1, 34, 11, 13 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 66, 100, 8 },
            { MENU_CMD_NAV, -1, 35, 12, 14 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 75, 100, 8 },
            { MENU_CMD_NAV, -1, 36, 13, 15 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 84, 100, 8 },
            { MENU_CMD_NAV, -1, 37, 14, 16 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 93, 100, 8 },
            { MENU_CMD_NAV, -1, 38, 15, 17 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 102, 100, 8 },
            { MENU_CMD_NAV, -1, 39, 16, 18 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 111, 100, 8 },
            { MENU_CMD_NAV, -1, 40, 17, 19 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 120, 100, 8 },
            { MENU_CMD_NAV, -1, 41, 18, 20 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -127, 100, 8 },
            { MENU_CMD_NAV, -1, 42, 19, 21 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -118, 100, 8 },
            { MENU_CMD_NAV, -1, 43, 20, 22 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -109, 100, 8 },
            { MENU_CMD_NAV, -1, 44, 21, 23 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -100, 100, 8 },
            { MENU_CMD_NAV, -1, 45, 22, 24 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -91, 100, 8 },
            { MENU_CMD_NAV, -1, 46, 23, 25 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -82, 100, 8 },
            { MENU_CMD_NAV, -1, 47, 24, 26 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -73, 100, 8 },
            { MENU_CMD_NAV, -1, 48, 25, 27 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -64, 100, 8 },
            { MENU_CMD_NAV, -1, 49, 26, 28 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -55, 100, 8 },
            { MENU_CMD_NAV, -1, 50, 27, 29 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -46, 100, 8 },
            { MENU_CMD_NAV, -1, 51, 28, 74 },
            { MENU_CMD_UP_SKIP, 29, 2 },
            MENU_CMD_END,
        } },
    { {
          { 102, 21, 100, 8 },
          { MENU_CMD_NAV, 8, 52, -1, 31 },
          MENU_CMD_END,
      },
        {
            { 102, 30, 100, 8 },
            { MENU_CMD_NAV, 9, 53, 30, 32 },
            MENU_CMD_END,
        },
        {
            { 102, 39, 100, 8 },
            { MENU_CMD_NAV, 10, 54, 31, 33 },
            MENU_CMD_END,
        },
        {
            { 102, 48, 100, 8 },
            { MENU_CMD_NAV, 11, 55, 32, 34 },
            MENU_CMD_END,
        },
        {
            { 102, 57, 100, 8 },
            { MENU_CMD_NAV, 12, 56, 33, 35 },
            MENU_CMD_END,
        },
        {
            { 102, 66, 100, 8 },
            { MENU_CMD_NAV, 13, 57, 34, 36 },
            MENU_CMD_END,
        },
        {
            { 102, 75, 100, 8 },
            { MENU_CMD_NAV, 14, 58, 35, 37 },
            MENU_CMD_END,
        },
        {
            { 102, 84, 100, 8 },
            { MENU_CMD_NAV, 15, 59, 36, 38 },
            MENU_CMD_END,
        },
        {
            { 102, 93, 100, 8 },
            { MENU_CMD_NAV, 16, 60, 37, 39 },
            MENU_CMD_END,
        },
        {
            { 102, 102, 100, 8 },
            { MENU_CMD_NAV, 17, 61, 38, 40 },
            MENU_CMD_END,
        },
        {
            { 102, 111, 100, 8 },
            { MENU_CMD_NAV, 18, 62, 39, 41 },
            MENU_CMD_END,
        },
        {
            { 102, 120, 100, 8 },
            { MENU_CMD_NAV, 19, 63, 40, 42 },
            MENU_CMD_END,
        },
        {
            { 102, -127, 100, 8 },
            { MENU_CMD_NAV, 20, 64, 41, 43 },
            MENU_CMD_END,
        },
        {
            { 102, -118, 100, 8 },
            { MENU_CMD_NAV, 21, 65, 42, 44 },
            MENU_CMD_END,
        },
        {
            { 102, -109, 100, 8 },
            { MENU_CMD_NAV, 22, 66, 43, 45 },
            MENU_CMD_END,
        },
        {
            { 102, -100, 100, 8 },
            { MENU_CMD_NAV, 23, 67, 44, 46 },
            MENU_CMD_END,
        },
        {
            { 102, -91, 100, 8 },
            { MENU_CMD_NAV, 24, 68, 45, 47 },
            MENU_CMD_END,
        },
        {
            { 102, -82, 100, 8 },
            { MENU_CMD_NAV, 25, 69, 46, 48 },
            MENU_CMD_END,
        },
        {
            { 102, -73, 100, 8 },
            { MENU_CMD_NAV, 26, 70, 47, 49 },
            MENU_CMD_END,
        },
        {
            { 102, -64, 100, 8 },
            { MENU_CMD_NAV, 27, 71, 48, 50 },
            MENU_CMD_END,
        },
        {
            { 102, -55, 100, 8 },
            { MENU_CMD_NAV, 28, 72, 49, 51 },
            MENU_CMD_END,
        },
        {
            { 102, -46, 100, 8 },
            { MENU_CMD_NAV, 29, 73, 50, 75 },
            MENU_CMD_END,
        } },
    {
        { -53, 21, 100, 8 },
        { MENU_CMD_NAV, 30, -1, -1, 53 },
        { MENU_CMD_LEFT_SKIP, 51, 2 },
        MENU_CMD_END,
    },
    { {
          { -53, 30, 100, 8 },
          { MENU_CMD_NAV, 31, -1, 52, 54 },
          { MENU_CMD_DOWN_SKIP, 73, 2 },
          MENU_CMD_END,
      },
        {
            { -53, 39, 100, 8 },
            { MENU_CMD_NAV, 32, -1, 53, 55 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 48, 100, 8 },
            { MENU_CMD_NAV, 33, -1, 54, 56 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 57, 100, 8 },
            { MENU_CMD_NAV, 34, -1, 55, 57 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 66, 100, 8 },
            { MENU_CMD_NAV, 35, -1, 56, 58 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 75, 100, 8 },
            { MENU_CMD_NAV, 36, -1, 57, 59 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 84, 100, 8 },
            { MENU_CMD_NAV, 37, -1, 58, 60 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 93, 100, 8 },
            { MENU_CMD_NAV, 38, -1, 59, 61 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 102, 100, 8 },
            { MENU_CMD_NAV, 39, -1, 60, 62 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 111, 100, 8 },
            { MENU_CMD_NAV, 40, -1, 61, 63 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 120, 100, 8 },
            { MENU_CMD_NAV, 41, -1, 62, 64 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -127, 100, 8 },
            { MENU_CMD_NAV, 42, -1, 63, 65 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -118, 100, 8 },
            { MENU_CMD_NAV, 43, -1, 64, 66 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -109, 100, 8 },
            { MENU_CMD_NAV, 44, -1, 65, 67 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -100, 100, 8 },
            { MENU_CMD_NAV, 45, -1, 66, 68 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -91, 100, 8 },
            { MENU_CMD_NAV, 46, -1, 67, 69 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -82, 100, 8 },
            { MENU_CMD_NAV, 47, -1, 68, 70 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -73, 100, 8 },
            { MENU_CMD_NAV, 48, -1, 69, 71 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -64, 100, 8 },
            { MENU_CMD_NAV, 49, -1, 70, 72 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -55, 100, 8 },
            { MENU_CMD_NAV, 50, -1, 71, 73 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -46, 100, 8 },
            { MENU_CMD_NAV, 51, -1, 72, 76 },
            { MENU_CMD_DOWN_SKIP, 73, 2 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 0 },
    { 0, -71, 120, 15 },
    { MENU_CMD_NAV, -1, 75, 29, -1 },
    { { MENU_CMD_UP_SKIP, 74, 2 }, { MENU_CMD_RIGHT_SKIP, 74, 1 } },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, 0 },
    { MENU_CMD_ONSELECT_FIRE, _ChangeTeams },
    MENU_CMD_END,
    {
        { 121, -71, 62, 15 },
        { MENU_CMD_NAV, 74, 76, 51, -1 },
        { MENU_CMD_COLORS, 0xA },
        { MENU_CMD_TEXT_PTR, 0x0, aAbort },
        { MENU_CMD_ONSELECT_FIRE, _AbortChooseTeamsFriendly },
        MENU_CMD_END,
    },
    { -72, -71, 120, 15 },
    { MENU_CMD_NAV, 75, -1, 73, -1 },
    { { MENU_CMD_DOWN_SKIP, 76, 2 }, { MENU_CMD_RIGHT_SKIP, 76, 0 } },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _ChooseTeamsFriendlyOnSelect76 },
    MENU_CMD_END,
    { -72, -68, 120, 9 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_DesignDIYLeague designDiyLeagueMenu = {
    { _DesignDIYLeagueInit, _DesignDIYLeagueFinish, 0, 13 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aDesignDiyLeagu },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { 0, 0, 304, 15 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_PTR, 0x0, (int8_t*)0x2DEF4 },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 0, 8 },
    MENU_MARK_BYTE_COORDS,
    {
        { 31, 20, -15, 15 },
        { MENU_CMD_NAV, -1, -1, -1, 9 },
        { MENU_CMD_COLORS, 0x1D },
        { MENU_CMD_TEXT_PTR, 0x0, (int8_t*)0x2DEF4 },
        { MENU_CMD_ONSELECT_FIRE, _DesignDIYLeagueSetName },
        MENU_CMD_END,
    },
    { {
          { 31, 60, 120, 15 },
          { MENU_CMD_COLORS, 0x9 },
          { MENU_CMD_TEXT_PTR, 0x0, aNumberOfTeams },
          MENU_CMD_END,
      },
        {
            { 31, 80, 120, 15 },
            { MENU_CMD_COLORS, 0x9 },
            { MENU_CMD_TEXT_PTR, 0x0, aPlayEachTeam },
            MENU_CMD_END,
        },
        {
            { 31, 100, 120, 15 },
            { MENU_CMD_COLORS, 0x9 },
            { MENU_CMD_TEXT_PTR, 0x0, aPointsForAWin },
            MENU_CMD_END,
        },
        {
            { 31, 120, 120, 15 },
            { MENU_CMD_COLORS, 0x9 },
            { MENU_CMD_TEXT_PTR, 0x0, aSubstitutes },
            MENU_CMD_END,
        },
        {
            { -63, 120, 38, 15 },
            { MENU_CMD_COLORS, 0x9 },
            { MENU_CMD_TEXT_PTR, 0x0, aFrom },
            MENU_CMD_END,
        } },
    { {
          { 31, 40, 120, 15 },
          { MENU_CMD_NAV, -1, 9, 2, 13 },
          { MENU_CMD_COLORS, 0xD },
          { MENU_CMD_STRING_TABLE, 0x0, &diyPitchTypeOrSeasonStrTable },
          { MENU_CMD_ONSELECT_FIRE, _DIYLeagueChangePitchTypeOrSeason },
          MENU_CMD_END,
      },
        {
            { -104, 40, 120, 15 },
            { MENU_CMD_NAV, 8, 10, 2, 10 },
            { MENU_CMD_COLORS, 0xD },
            { MENU_CMD_STRING_TABLE, 0x0, &diyLeaguePitchTypeStrTable },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYLeagueChangePitchType },
            MENU_CMD_END,
        },
        {
            { -104, 40, 50, 15 },
            { MENU_CMD_NAV, 9, 12, 9, 13 },
            { MENU_CMD_COLORS, 0xD },
            { MENU_CMD_STRING_TABLE, 0x0, &diyLeagueMonthsStrTable },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYLeagueChangeStartMonth },
            MENU_CMD_END,
        } },
    {
        { -53, 40, 18, 15 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, asc_131FF2 },
        MENU_CMD_END,
    },
    {
        { -34, 40, 50, 15 },
        { MENU_CMD_NAV, 10, -1, 9, 13 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &diyLeagueEndingMonthStrTable },
        { MENU_CMD_ONSELECT_FIRE, _DesignDIYLeagueChangeEndMonth },
        MENU_CMD_END,
    },
    { -104, 60, 120, 15 },
    { MENU_CMD_NAV, -1, -1, 10, 14 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_NUMBER, 0x0, 0 },
    { MENU_CMD_ONSELECT_FIRE, _DesignDIYLeagueChangeNumberOfTeams },
    MENU_CMD_END,
    { -104, 80, 120, 15 },
    { MENU_CMD_NAV, -1, -1, 13, 15 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _DesignDIYLeagueChangePlayEachTeam },
    MENU_CMD_END,
    { -104, 100, 120, 15 },
    { MENU_CMD_NAV, -1, -1, 14, 16 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_NUMBER, 0x0, 0 },
    { MENU_CMD_ONSELECT_FIRE, _DesignDIYLeagueChangeNumPointsForWin },
    MENU_CMD_END,
    { -104, 120, 40, 15 },
    { MENU_CMD_NAV, -1, 17, 15, 21 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _DesignDIYLeagueChangeMinSubstitutes },
    MENU_CMD_END,
    { -24, 120, 40, 15 },
    { MENU_CMD_NAV, 16, -1, 15, 21 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _DesignDIYLeagueChangeMaxSubstitutes },
    MENU_CMD_END,
    { 31, -116, -15, 12 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    { 31, -101, -15, 12 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 0, 0 },
    { {
          { 0, -71, 120, 15 },
          { MENU_CMD_NAV, -1, 22, 16, -1 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aChooseTeams },
          { MENU_CMD_ONSELECT_FIRE, _DesignDIYLeagueChooseTeams },
          MENU_CMD_END,
      },
        {
            { 121, -91, 62, 15 },
            { MENU_CMD_NAV, -1, -1, 16, 22 },
            { MENU_CMD_COLORS, 0xE },
            { MENU_CMD_TEXT_PTR, 0x0, aOk },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYLeagueOK },
            MENU_CMD_END,
        },
        {
            { 121, -71, 62, 15 },
            { MENU_CMD_NAV, 20, -1, 21, -1 },
            { MENU_CMD_COLORS, 0xC },
            { MENU_CMD_TEXT_PTR, 0x0, aExit },
            { MENU_CMD_ONSELECT_FIRE, _jmp_SetExitMenuFlag_6 },
            MENU_CMD_END,
        } },
    { -72, -68, 120, 9 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_DIYLeagueTournament diyLeagueTournamentMenu = {
    { _DIYLeagueTournamentInit, _DIYLeagueTournamentOnRestore, 0, 1 },
    { 0, 0, 304, 14 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { 0, -68, -54, 12 },
    { MENU_CMD_NAV, -1, 2, 41, -1 },
    { MENU_CMD_RIGHT_SKIP, 1, 1 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, 0 },
    { MENU_CMD_ONSELECT_FIRE, _DIYLeagueTournamentNextMatch },
    MENU_CMD_END,
    { 0, -68, -54, 12 },
    { MENU_CMD_NAV, 1, 3, 41, -1 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aPlayMatch },
    MENU_CMD_END,
    {
        { 0, -68, 100, 12 },
        { MENU_CMD_NAV, 2, 4, 41, -1 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aMatch },
        { MENU_CMD_ONSELECT_FIRE, _DIYLeagueTournamentMatch },
        MENU_CMD_END,
    },
    { 77, -68, 80, 12 },
    { MENU_CMD_NAV, 3, 5, 41, -1 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aViewHighlights_0 },
    { MENU_CMD_ONSELECT_FIRE, _DIYLeagueTournamentOnSelect4 },
    MENU_CMD_END,
    { {
          { 101, -68, 101, 12 },
          { MENU_CMD_NAV, 4, 6, 41, -1 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aResult },
          { MENU_CMD_ONSELECT_FIRE, _DIYLeagueTournamentResult },
          MENU_CMD_END,
      },
        {
            { 0, -68, 100, 12 },
            { MENU_CMD_NAV, 5, 7, 41, -1 },
            { MENU_CMD_COLORS, 0xE },
            { MENU_CMD_TEXT_PTR, 0x0, aViewMatch },
            { MENU_CMD_ONSELECT_FIRE, _DIYLeagueTournamentViewMatch },
            MENU_CMD_END,
        } },
    { 61, -68, 90, 12 },
    { MENU_CMD_NAV, 6, 8, 41, -1 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aViewHighlights_0 },
    { MENU_CMD_ONSELECT_FIRE, _DIYLeagueTournamentViewHighlights },
    MENU_CMD_END,
    { {
          { 101, -68, 101, 12 },
          { MENU_CMD_NAV, 7, 9, 41, -1 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aViewResult },
          { MENU_CMD_ONSELECT_FIRE, _DIYLeagueTournamentViewResult },
          MENU_CMD_END,
      },
        {
            { -53, -68, 50, 12 },
            { MENU_CMD_NAV, 8, 10, 41, -1 },
            { MENU_CMD_COLORS, 0xE },
            { MENU_CMD_TEXT_PTR, 0x0, aStats },
            { MENU_CMD_ONSELECT_FIRE, _DIYLeagueTournamentStats },
            MENU_CMD_END,
        },
        {
            { -2, -68, 50, 12 },
            { MENU_CMD_NAV, 9, -1, 41, -1 },
            { MENU_CMD_COLORS, 0xC },
            { MENU_CMD_TEXT_PTR, 0x0, aExit },
            { MENU_CMD_ONSELECT_FIRE, _DIYLeagueTournamentExit },
            MENU_CMD_END,
        } },
    { {
          { -107, -77, 6, 8 },
          { MENU_CMD_COLORS, 0x0 },
          { MENU_CMD_TEXT_PTR, 0x10, 0 },
          MENU_CMD_END,
      },
        {
            { 0, -77, -127, 8 },
            { MENU_CMD_COLORS, 0x0 },
            { MENU_CMD_TEXT_PTR, 0x4010, 0 },
            MENU_CMD_END,
        },
        {
            { -83, -77, -127, 8 },
            { MENU_CMD_COLORS, 0x0 },
            { MENU_CMD_TEXT_PTR, 0x8010, 0 },
            MENU_CMD_END,
        } },
    { {
          { -127, -77, 18, 8 },
          { MENU_CMD_COLORS, 0x0 },
          { MENU_CMD_NUMBER, 0x4010, 0 },
          MENU_CMD_END,
      },
        {
            { -101, -77, 18, 8 },
            { MENU_CMD_COLORS, 0x0 },
            { MENU_CMD_NUMBER, 0x8010, 0 },
            MENU_CMD_END,
        } },
    { {
          { 25, 0, 100, 8 },
          { MENU_CMD_COLORS, 0x5 },
          { MENU_CMD_TEXT_PTR, 0x0, 0 },
          { MENU_CMD_BEFORE_DRAW, _DIYLeagueTournament16BeforeDraw },
          MENU_CMD_END,
      },
        {
            { 25, 0, 100, 8 },
            { MENU_CMD_COLORS, 0x5 },
            { MENU_CMD_TEXT_PTR, 0x0, 0 },
            { MENU_CMD_BEFORE_DRAW, _DIYLeagueTournament16BeforeDraw },
            MENU_CMD_END,
        },
        {
            { 25, 0, 100, 8 },
            { MENU_CMD_COLORS, 0x5 },
            { MENU_CMD_TEXT_PTR, 0x0, 0 },
            { MENU_CMD_BEFORE_DRAW, _DIYLeagueTournament16BeforeDraw },
            MENU_CMD_END,
        },
        {
            { 25, 0, 100, 8 },
            { MENU_CMD_COLORS, 0x5 },
            { MENU_CMD_TEXT_PTR, 0x0, 0 },
            { MENU_CMD_BEFORE_DRAW, _DIYLeagueTournament16BeforeDraw },
            MENU_CMD_END,
        },
        {
            { 25, 0, 100, 8 },
            { MENU_CMD_COLORS, 0x5 },
            { MENU_CMD_TEXT_PTR, 0x0, 0 },
            { MENU_CMD_BEFORE_DRAW, _DIYLeagueTournament16BeforeDraw },
            MENU_CMD_END,
        },
        {
            { 25, 0, 100, 8 },
            { MENU_CMD_COLORS, 0x5 },
            { MENU_CMD_TEXT_PTR, 0x0, 0 },
            { MENU_CMD_BEFORE_DRAW, _DIYLeagueTournament16BeforeDraw },
            MENU_CMD_END,
        },
        {
            { 25, 0, 100, 8 },
            { MENU_CMD_COLORS, 0x5 },
            { MENU_CMD_TEXT_PTR, 0x0, 0 },
            { MENU_CMD_BEFORE_DRAW, _DIYLeagueTournament16BeforeDraw },
            MENU_CMD_END,
        },
        {
            { 25, 0, 100, 8 },
            { MENU_CMD_COLORS, 0x5 },
            { MENU_CMD_TEXT_PTR, 0x0, 0 },
            { MENU_CMD_BEFORE_DRAW, _DIYLeagueTournament16BeforeDraw },
            MENU_CMD_END,
        } },
    { {
          { 126, 10, 24, 8 },
          { MENU_CMD_INVISIBLE },
          { MENU_CMD_COLORS, 0x5 },
          { MENU_CMD_TEXT_PTR, 0x0, aPl },
          MENU_CMD_END,
      },
        {
            { -105, 10, 24, 8 },
            { MENU_CMD_INVISIBLE },
            { MENU_CMD_COLORS, 0x5 },
            { MENU_CMD_TEXT_PTR, 0x0, aW },
            MENU_CMD_END,
        },
        {
            { -80, 10, 24, 8 },
            { MENU_CMD_INVISIBLE },
            { MENU_CMD_COLORS, 0x5 },
            { MENU_CMD_TEXT_PTR, 0x0, aD },
            MENU_CMD_END,
        },
        {
            { -55, 10, 24, 8 },
            { MENU_CMD_INVISIBLE },
            { MENU_CMD_COLORS, 0x5 },
            { MENU_CMD_TEXT_PTR, 0x0, aL },
            MENU_CMD_END,
        },
        {
            { -30, 10, 24, 8 },
            { MENU_CMD_INVISIBLE },
            { MENU_CMD_COLORS, 0x5 },
            { MENU_CMD_TEXT_PTR, 0x0, aF },
            MENU_CMD_END,
        },
        {
            { -5, 10, 24, 8 },
            { MENU_CMD_INVISIBLE },
            { MENU_CMD_COLORS, 0x5 },
            { MENU_CMD_TEXT_PTR, 0x0, aA_0 },
            MENU_CMD_END,
        } },
    MENU_MARK_WIDE_COORDS,
    { 276, 10, 24, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0x5 },
    { MENU_CMD_TEXT_PTR, 0x0, aPts },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    {
        { 6, 0, 22, 6 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 25, 0, 100, 6 },
        { MENU_CMD_COLORS, 0xA },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    { {
          { 126, 0, 24, 6 },
          { MENU_CMD_COLORS, 0x7 },
          { MENU_CMD_NUMBER, 0x0, 0 },
          MENU_CMD_END,
      },
        {
            { -105, 0, 24, 6 },
            { MENU_CMD_COLORS, 0x7 },
            { MENU_CMD_NUMBER, 0x0, 0 },
            MENU_CMD_END,
        },
        {
            { -80, 0, 24, 6 },
            { MENU_CMD_COLORS, 0x7 },
            { MENU_CMD_NUMBER, 0x0, 0 },
            MENU_CMD_END,
        },
        {
            { -55, 0, 24, 6 },
            { MENU_CMD_COLORS, 0x7 },
            { MENU_CMD_NUMBER, 0x0, 0 },
            MENU_CMD_END,
        },
        {
            { -30, 0, 24, 6 },
            { MENU_CMD_COLORS, 0x7 },
            { MENU_CMD_NUMBER, 0x0, 0 },
            MENU_CMD_END,
        },
        {
            { -5, 0, 24, 6 },
            { MENU_CMD_COLORS, 0x7 },
            { MENU_CMD_NUMBER, 0x0, 0 },
            MENU_CMD_END,
        } },
    MENU_MARK_WIDE_COORDS,
    { 276, 0, 24, 6 },
    { MENU_CMD_COLORS, 0x7 },
    { MENU_CMD_NUMBER, 0x0, 0 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { 0, 30, 8, 14 },
    { MENU_CMD_NAV, -1, 1, -1, 41 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_SPRITE_FG, 183 },
    { MENU_CMD_ONSELECT_FIRE, _DIYLeagueTournamentScrollUp },
    MENU_CMD_END,
    { 0, -98, 8, 14 },
    { MENU_CMD_NAV, -1, 1, 40, 1 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_SPRITE_FG, 184 },
    { MENU_CMD_ONSELECT_FIRE, _DIYLeagueTournamentScrollDown },
    MENU_CMD_END,
    { 0, 0, 0, 0 },
    { MENU_CMD_INPUT_FUNC, 0xFFFF, _jmp_SetExitMenuFlag_7 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_DesignDIYCup designDiyCupMenu = {
    { _DesignDIYCupInit, _DesignDIYCupReinit, _DesignDIYCupOnDraw, 11 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aDesignDiyCup },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { 0, 0, 304, 15 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_PTR, 0x0, (int8_t*)0x2DEF4 },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 0, 15 },
    { 0, 5, 304, 15 },
    { MENU_CMD_NAV, -1, -1, -1, 3 },
    { MENU_CMD_COLORS, 0x1D },
    { MENU_CMD_TEXT_PTR, 0x0, (int8_t*)0x2DEF4 },
    { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect2 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { {
          { 0, 25, 75, 15 },
          { MENU_CMD_NAV, -1, 4, 2, 11 },
          { MENU_CMD_COLORS, 0xD },
          { MENU_CMD_STRING_TABLE, 0x0, &diyCupPitchTypeOrSeasonStringTable },
          { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect3 },
          MENU_CMD_END,
      },
        {
            { 76, 25, 56, 15 },
            { MENU_CMD_NAV, 3, 5, 2, 11 },
            { MENU_CMD_COLORS, 0xD },
            { MENU_CMD_STRING_TABLE, 0x0, &diyCupPitchTypeStringTable },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect4 },
            MENU_CMD_END,
        },
        {
            { 76, 25, 22, 15 },
            { MENU_CMD_NAV, 4, 7, 4, 11 },
            { MENU_CMD_COLORS, 0xD },
            { MENU_CMD_STRING_TABLE, 0x0, &diyCupStartMonthStringTable },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect5 },
            MENU_CMD_END,
        } },
    {
        { 99, 25, 10, 15 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aHyphen },
        MENU_CMD_END,
    },
    {
        { 110, 25, 22, 15 },
        { MENU_CMD_NAV, 5, 9, 4, 11 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &diyCupEndMonthStringTable },
        { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect7 },
        MENU_CMD_END,
    },
    {
        { -123, 25, 70, 15 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aAwayGoals },
        MENU_CMD_END,
    },
    {
        { -52, 25, 100, 15 },
        { MENU_CMD_NAV, 7, -1, 2, 13 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &diyCupAwayGoalsStringTable },
        { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect9 },
        MENU_CMD_END,
    },
    {
        { 0, 45, 64, 15 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aRounds },
        MENU_CMD_END,
    },
    { 65, 45, 44, 15 },
    { MENU_CMD_NAV, -1, 13, 5, 20 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_NUMBER, 0x0, 0 },
    { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect11 },
    MENU_CMD_END,
    {
        { 113, 45, 70, 15 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aSubstitutes },
        MENU_CMD_END,
    },
    { -72, 45, 40, 15 },
    { MENU_CMD_NAV, 11, 15, 9, 21 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect13 },
    MENU_CMD_END,
    {
        { -31, 45, 38, 15 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aFrom },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { 264, 45, 40, 15 },
    { MENU_CMD_NAV, 13, -1, 9, 22 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect15 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { {
          { 58, 66, 80, 9 },
          { MENU_CMD_TEXT_PTR, 0x0, aTeams },
          MENU_CMD_END,
      },
        {
            { -127, 66, 120, 9 },
            { MENU_CMD_TEXT_PTR, 0x0, aDescription },
            MENU_CMD_END,
        } },
    { { {
            { 0, 78, 80, 9 },
            { MENU_CMD_COLORS, 0x9 },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            MENU_CMD_END,
        },
          {
              { 88, 78, 18, 9 },
              { MENU_CMD_COLORS, 0x9 },
              { MENU_CMD_NUMBER, 0x0, 0 },
              MENU_CMD_END,
          },
          {
              { 113, 78, 38, 9 },
              { MENU_CMD_NAV, -1, 21, 11, 25 },
              { MENU_CMD_COLORS, 0xD },
              { MENU_CMD_STRING_TABLE, 0x0, &diyCupLegsStringTable0 },
              { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect20 },
              MENU_CMD_END,
          },
          {
              { -104, 78, 72, 9 },
              { MENU_CMD_NAV, 20, 22, 13, 26 },
              { MENU_CMD_COLORS, 0xD },
              { MENU_CMD_STRING_TABLE, 0x0, &diyCupExtraTimeStringTable0 },
              { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect21 },
              MENU_CMD_END,
          },
          {
              { -31, 78, 79, 9 },
              { MENU_CMD_NAV, 21, -1, 15, 27 },
              { MENU_CMD_COLORS, 0xD },
              { MENU_CMD_STRING_TABLE, 0x0, &diyCupPenaltiesStringTable0 },
              { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect22 },
              MENU_CMD_END,
          } },
        { {
              { 0, 88, 80, 9 },
              { MENU_CMD_COLORS, 0x9 },
              { MENU_CMD_TEXT_PTR, 0x8000, 0 },
              MENU_CMD_END,
          },
            {
                { 88, 88, 18, 9 },
                { MENU_CMD_COLORS, 0x9 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { 113, 88, 38, 9 },
                { MENU_CMD_NAV, -1, 26, 20, 30 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupLegsStringTable1 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect20 },
                MENU_CMD_END,
            },
            {
                { -104, 88, 72, 9 },
                { MENU_CMD_NAV, 25, 27, 21, 31 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupExtraTimeStringTable1 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect21 },
                MENU_CMD_END,
            },
            {
                { -31, 88, 79, 9 },
                { MENU_CMD_NAV, 26, -1, 22, 32 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupPenaltiesStringTable1 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect22 },
                MENU_CMD_END,
            } },
        { {
              { 0, 98, 80, 9 },
              { MENU_CMD_COLORS, 0x9 },
              { MENU_CMD_TEXT_PTR, 0x8000, 0 },
              MENU_CMD_END,
          },
            {
                { 88, 98, 18, 9 },
                { MENU_CMD_COLORS, 0x9 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { 113, 98, 38, 9 },
                { MENU_CMD_NAV, -1, 31, 25, 35 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupLegsStringTable2 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect20 },
                MENU_CMD_END,
            },
            {
                { -104, 98, 72, 9 },
                { MENU_CMD_NAV, 30, 32, 26, 36 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupExtraTimeStringTable2 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect21 },
                MENU_CMD_END,
            },
            {
                { -31, 98, 79, 9 },
                { MENU_CMD_NAV, 31, -1, 27, 37 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupPenaltiesStringTable2 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect22 },
                MENU_CMD_END,
            } },
        { {
              { 0, 108, 80, 9 },
              { MENU_CMD_COLORS, 0x9 },
              { MENU_CMD_TEXT_PTR, 0x8000, 0 },
              MENU_CMD_END,
          },
            {
                { 88, 108, 18, 9 },
                { MENU_CMD_COLORS, 0x9 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { 113, 108, 38, 9 },
                { MENU_CMD_NAV, -1, 36, 30, 40 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupLegsStringTable3 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect20 },
                MENU_CMD_END,
            },
            {
                { -104, 108, 72, 9 },
                { MENU_CMD_NAV, 35, 37, 31, 41 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupExtraTimeStringTable3 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect21 },
                MENU_CMD_END,
            },
            {
                { -31, 108, 79, 9 },
                { MENU_CMD_NAV, 36, -1, 32, 42 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupPenaltiesStringTable3 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect22 },
                MENU_CMD_END,
            } },
        { {
              { 0, 118, 80, 9 },
              { MENU_CMD_COLORS, 0x9 },
              { MENU_CMD_TEXT_PTR, 0x8000, 0 },
              MENU_CMD_END,
          },
            {
                { 88, 118, 18, 9 },
                { MENU_CMD_COLORS, 0x9 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { 113, 118, 38, 9 },
                { MENU_CMD_NAV, -1, 41, 35, 45 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupLegsStringTable4 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect20 },
                MENU_CMD_END,
            },
            {
                { -104, 118, 72, 9 },
                { MENU_CMD_NAV, 40, 42, 36, 46 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupExtraTimeStringTable4 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect21 },
                MENU_CMD_END,
            },
            {
                { -31, 118, 79, 9 },
                { MENU_CMD_NAV, 41, -1, 37, 47 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupPenaltiesStringTable4 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect22 },
                MENU_CMD_END,
            } },
        { {
              { 0, -128, 80, 9 },
              { MENU_CMD_COLORS, 0x9 },
              { MENU_CMD_TEXT_PTR, 0x8000, 0 },
              MENU_CMD_END,
          },
            {
                { 88, -128, 18, 9 },
                { MENU_CMD_COLORS, 0x9 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { 113, -128, 38, 9 },
                { MENU_CMD_NAV, -1, 46, 40, 49 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupLegsStringTable5 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect20 },
                MENU_CMD_END,
            },
            {
                { -104, -128, 72, 9 },
                { MENU_CMD_NAV, 45, 47, 41, 49 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupExtraTimeStringTable5 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect21 },
                MENU_CMD_END,
            },
            {
                { -31, -128, 79, 9 },
                { MENU_CMD_NAV, 46, -1, 42, 49 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyCupPenaltiesStringTable5 },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect22 },
                MENU_CMD_END,
            } } },
    { MENU_MARK_START_MENU, 0, 0 },
    { {
          { 0, -71, 120, 15 },
          { MENU_CMD_NAV, -1, 50, 45, -1 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aChooseTeams },
          { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOnSelect48 },
          MENU_CMD_END,
      },
        {
            { 121, -91, 62, 15 },
            { MENU_CMD_NAV, -1, -1, 45, 50 },
            { MENU_CMD_COLORS, 0xE },
            { MENU_CMD_TEXT_PTR, 0x0, aOk },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYCupOK },
            MENU_CMD_END,
        },
        {
            { 121, -71, 62, 15 },
            { MENU_CMD_NAV, 48, -1, 49, -1 },
            { MENU_CMD_COLORS, 0xC },
            { MENU_CMD_TEXT_PTR, 0x0, aExit },
            { MENU_CMD_ONSELECT_FIRE, _jmp_SetExitMenuFlag_4 },
            MENU_CMD_END,
        } },
    { -72, -68, 120, 9 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_DIYCup diyCupMenu = {
    { _DIYCupInit, _DIYCupFinish, 0, 0x1 },
    { 0, 0, 304, 14 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { 0, -68, -54, 12 },
    { MENU_CMD_NAV, -1, 2, 17, -1 },
    { MENU_CMD_RIGHT_SKIP, 1, 1 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, 0 },
    { MENU_CMD_ONSELECT_FIRE, _DIYCupOnSelect1 },
    MENU_CMD_END,
    { 0, -68, -54, 12 },
    { MENU_CMD_NAV, 1, 3, 17, -1 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aPlayMatch },
    MENU_CMD_END,
    {
        { 0, -68, 100, 12 },
        { MENU_CMD_NAV, 2, 4, 17, -1 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aMatch },
        { MENU_CMD_ONSELECT_FIRE, _DIYCupOnSelect3 },
        MENU_CMD_END,
    },
    { 77, -68, 80, 12 },
    { MENU_CMD_NAV, 3, 5, 17, -1 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aViewHighlights_0 },
    { MENU_CMD_ONSELECT_FIRE, _DIYCupOnSelect4 },
    MENU_CMD_END,
    { {
          { 101, -68, 101, 12 },
          { MENU_CMD_NAV, 4, 6, 17, -1 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aResult },
          { MENU_CMD_ONSELECT_FIRE, _DIYCupOnSelect5 },
          MENU_CMD_END,
      },
        {
            { 0, -68, 100, 12 },
            { MENU_CMD_NAV, 5, 7, 17, -1 },
            { MENU_CMD_COLORS, 0xE },
            { MENU_CMD_TEXT_PTR, 0x0, aViewMatch },
            { MENU_CMD_ONSELECT_FIRE, _DIYCupOnSelect6 },
            MENU_CMD_END,
        } },
    { 61, -68, 90, 12 },
    { MENU_CMD_NAV, 6, 8, 17, -1 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aViewHighlights_0 },
    { MENU_CMD_ONSELECT_FIRE, _DIYCupOnSelect7 },
    MENU_CMD_END,
    { {
          { 101, -68, 101, 12 },
          { MENU_CMD_NAV, 7, 9, 17, -1 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aViewResult },
          { MENU_CMD_ONSELECT_FIRE, _DIYCupOnSelect8 },
          MENU_CMD_END,
      },
        {
            { -53, -68, 50, 12 },
            { MENU_CMD_NAV, 8, 10, 17, -1 },
            { MENU_CMD_COLORS, 0xE },
            { MENU_CMD_TEXT_PTR, 0x0, aStats },
            { MENU_CMD_ONSELECT_FIRE, _DIYCupOnSelect9 },
            MENU_CMD_END,
        },
        {
            { -2, -68, 50, 12 },
            { MENU_CMD_NAV, 9, -1, 17, -1 },
            { MENU_CMD_COLORS, 0xC },
            { MENU_CMD_TEXT_PTR, 0x0, aExit },
            { MENU_CMD_ONSELECT_FIRE, _DIYCupOnSelect10 },
            MENU_CMD_END,
        } },
    { {
          { -107, -76, 6, 8 },
          { MENU_CMD_COLORS, 0x0 },
          { MENU_CMD_TEXT_PTR, 0x10, 0 },
          MENU_CMD_END,
      },
        {
            { 0, -76, -127, 8 },
            { MENU_CMD_COLORS, 0x0 },
            { MENU_CMD_TEXT_PTR, 0x4010, 0 },
            MENU_CMD_END,
        },
        {
            { -83, -76, -127, 8 },
            { MENU_CMD_COLORS, 0x0 },
            { MENU_CMD_TEXT_PTR, 0x8010, 0 },
            MENU_CMD_END,
        } },
    { {
          { -127, -76, 18, 8 },
          { MENU_CMD_COLORS, 0x0 },
          { MENU_CMD_NUMBER, 0x4010, 0 },
          MENU_CMD_END,
      },
        {
            { -101, -76, 18, 8 },
            { MENU_CMD_COLORS, 0x0 },
            { MENU_CMD_NUMBER, 0x8010, 0 },
            MENU_CMD_END,
        } },
    { 0, 30, 8, 14 },
    { MENU_CMD_NAV, -1, 1, -1, 17 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_SPRITE_FG, 183 },
    { MENU_CMD_ONSELECT_FIRE, _DIYCupOnSelect16 },
    MENU_CMD_END,
    { 0, -91, 8, 14 },
    { MENU_CMD_NAV, -1, 1, 16, 1 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_SPRITE_FG, 184 },
    { MENU_CMD_ONSELECT_FIRE, _DIYCupOnSelect17 },
    MENU_CMD_END,
    { { { MENU_MARK_START_MENU, 0, 25 },
          {
              { -104, 0, 0, 0 },
              { MENU_CMD_TEXT_PTR, 0x0, 0 },
              MENU_CMD_END,
          },
          { {
                { -121, 0, 0, 0 },
                { MENU_CMD_TEXT_NO_PTR, 0x4000 },
                MENU_CMD_END,
            },
              {
                  { -121, 10, 0, 0 },
                  { MENU_CMD_TEXT_NO_PTR, 0x4000 },
                  MENU_CMD_END,
              },
              {
                  { -87, 0, 0, 0 },
                  { MENU_CMD_TEXT_NO_PTR, 0x8000 },
                  MENU_CMD_END,
              },
              {
                  { -87, 10, 0, 0 },
                  { MENU_CMD_TEXT_NO_PTR, 0x8000 },
                  MENU_CMD_END,
              } },
          {
              { -113, 0, 0, 0 },
              { MENU_CMD_NUMBER, 0x0, 0 },
              MENU_CMD_END,
          },
          {
              { -95, 0, 0, 0 },
              { MENU_CMD_NUMBER, 0x0, 0 },
              MENU_CMD_END,
          },
          {
              { -104, 10, 0, 0 },
              { MENU_CMD_TEXT_NO_PTR, 0xE },
              MENU_CMD_END,
          } },
        { { MENU_MARK_START_MENU, 0, 52 },
            {
                { -104, 0, 0, 0 },
                { MENU_CMD_TEXT_PTR, 0x0, 0 },
                MENU_CMD_END,
            },
            { {
                  { -121, 0, 0, 0 },
                  { MENU_CMD_TEXT_NO_PTR, 0x4000 },
                  MENU_CMD_END,
              },
                {
                    { -121, 10, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x4000 },
                    MENU_CMD_END,
                },
                {
                    { -87, 0, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x8000 },
                    MENU_CMD_END,
                },
                {
                    { -87, 10, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x8000 },
                    MENU_CMD_END,
                } },
            {
                { -113, 0, 0, 0 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { -95, 0, 0, 0 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { -104, 10, 0, 0 },
                { MENU_CMD_TEXT_NO_PTR, 0xE },
                MENU_CMD_END,
            } },
        { { MENU_MARK_START_MENU, 0, 79 },
            {
                { -104, 0, 0, 0 },
                { MENU_CMD_TEXT_PTR, 0x0, 0 },
                MENU_CMD_END,
            },
            { {
                  { -121, 0, 0, 0 },
                  { MENU_CMD_TEXT_NO_PTR, 0x4000 },
                  MENU_CMD_END,
              },
                {
                    { -121, 10, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x4000 },
                    MENU_CMD_END,
                },
                {
                    { -87, 0, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x8000 },
                    MENU_CMD_END,
                },
                {
                    { -87, 10, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x8000 },
                    MENU_CMD_END,
                } },
            {
                { -113, 0, 0, 0 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { -95, 0, 0, 0 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { -104, 10, 0, 0 },
                { MENU_CMD_TEXT_NO_PTR, 0xE },
                MENU_CMD_END,
            } },
        { { MENU_MARK_START_MENU, 0, 106 },
            {
                { -104, 0, 0, 0 },
                { MENU_CMD_TEXT_PTR, 0x0, 0 },
                MENU_CMD_END,
            },
            { {
                  { -121, 0, 0, 0 },
                  { MENU_CMD_TEXT_NO_PTR, 0x4000 },
                  MENU_CMD_END,
              },
                {
                    { -121, 10, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x4000 },
                    MENU_CMD_END,
                },
                {
                    { -87, 0, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x8000 },
                    MENU_CMD_END,
                },
                {
                    { -87, 10, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x8000 },
                    MENU_CMD_END,
                } },
            {
                { -113, 0, 0, 0 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { -95, 0, 0, 0 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { -104, 10, 0, 0 },
                { MENU_CMD_TEXT_NO_PTR, 0xE },
                MENU_CMD_END,
            } },
        { { MENU_MARK_START_MENU, 0, 133 },
            {
                { -104, 0, 0, 0 },
                { MENU_CMD_TEXT_PTR, 0x0, 0 },
                MENU_CMD_END,
            },
            { {
                  { -121, 0, 0, 0 },
                  { MENU_CMD_TEXT_NO_PTR, 0x4000 },
                  MENU_CMD_END,
              },
                {
                    { -121, 10, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x4000 },
                    MENU_CMD_END,
                },
                {
                    { -87, 0, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x8000 },
                    MENU_CMD_END,
                },
                {
                    { -87, 10, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x8000 },
                    MENU_CMD_END,
                } },
            {
                { -113, 0, 0, 0 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { -95, 0, 0, 0 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { -104, 10, 0, 0 },
                { MENU_CMD_TEXT_NO_PTR, 0xE },
                MENU_CMD_END,
            } },
        { { MENU_MARK_START_MENU, 0, 160 },
            {
                { -104, 0, 0, 0 },
                { MENU_CMD_TEXT_PTR, 0x0, 0 },
                MENU_CMD_END,
            },
            { {
                  { -121, 0, 0, 0 },
                  { MENU_CMD_TEXT_NO_PTR, 0x4000 },
                  MENU_CMD_END,
              },
                {
                    { -121, 10, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x4000 },
                    MENU_CMD_END,
                },
                {
                    { -87, 0, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x8000 },
                    MENU_CMD_END,
                },
                {
                    { -87, 10, 0, 0 },
                    { MENU_CMD_TEXT_NO_PTR, 0x8000 },
                    MENU_CMD_END,
                } },
            {
                { -113, 0, 0, 0 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { -95, 0, 0, 0 },
                { MENU_CMD_NUMBER, 0x0, 0 },
                MENU_CMD_END,
            },
            {
                { -104, 10, 0, 0 },
                { MENU_CMD_TEXT_NO_PTR, 0xE },
                MENU_CMD_END,
            } } },
    { 0, 0, 0, 0 },
    { MENU_CMD_INPUT_FUNC, 0xFFFF, _jmp_SetExitMenuFlag_5 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_Options optionsMenu = {
    { _OptionsMenuBeforeDraw, 0, 0, 0xD },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aOptions },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 0 },
    {
        { 10, 64, -116, 11 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aFriendlyDiyGam },
        MENU_CMD_END,
    },
    {
        { -102, 64, -116, 11 },
        { MENU_CMD_NAV, -1, -1, -1, 4 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &gameLengthStrTable },
        { MENU_CMD_ONSELECT_FIRE, _ChangeGameLength },
        MENU_CMD_END,
    },
    {
        { 10, 76, -116, 11 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aFixPitchType },
        MENU_CMD_END,
    },
    {
        { -102, 76, -116, 11 },
        { MENU_CMD_NAV, -1, -1, 2, 6 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &pitchTypeStrTable },
        { MENU_CMD_ONSELECT_FIRE, _ChangePitchType },
        MENU_CMD_END,
    },
    {
        { 10, 88, -116, 11 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aAutoReplays },
        MENU_CMD_END,
    },
    {
        { -102, 88, -116, 11 },
        { MENU_CMD_NAV, -1, -1, 4, 8 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &autoReplaysStrTable },
        { MENU_CMD_ONSELECT_FIRE, _ChangeAutoReplays },
        MENU_CMD_END,
    },
    {
        { 10, 100, -116, 11 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aAllPlayerTeams },
        MENU_CMD_END,
    },
    {
        { -102, 100, -116, 11 },
        { MENU_CMD_NAV, -1, -1, 6, 10 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &allPlTeamsEqualStrTable },
        { MENU_CMD_ONSELECT_FIRE, _ChangeAllPlTeamsEqual },
        MENU_CMD_END,
    },
    {
        { 10, 112, -116, 11 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aAutoSaveHighli },
        MENU_CMD_END,
    },
    {
        { -102, 112, -116, 11 },
        { MENU_CMD_NAV, -1, -1, 8, 12 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &autoSaveHlStrTable },
        { MENU_CMD_ONSELECT_FIRE, _ChangeAutoSaveHighlights },
        MENU_CMD_END,
    },
    {
        { 10, 124, -116, 11 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aMenuMusic },
        MENU_CMD_END,
    },
    {
        { -102, 124, -116, 11 },
        { MENU_CMD_NAV, -1, -1, 10, 13 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &menuMusicStrTable },
        { MENU_CMD_ONSELECT_FIRE, _ChangeMenuMusic },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 0 },
    { 102, -71, 100, 15 },
    { MENU_CMD_NAV, -1, -1, 12, -1 },
    { MENU_CMD_COLORS, 0xC },
    { MENU_CMD_TEXT_PTR, 0x0, aExit },
    { MENU_CMD_INPUT_FUNC, 0x30, _ExitOptions },
    MENU_CMD_END,
    { -104, -48, 0, 6 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_TEXT_PTR, 0x0, aSwosVersion152 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_DesignDiyTournament designDiyTournamentMenu = {
    { _DesignDIYTournamentInit, _DesignDIYTournamentReinit,
        _DesignDIYTournamentOnDraw, 19 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aDesignDiyTourn },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { 0, 0, 304, 15 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_PTR, 0x0, (int8_t*)0x2DAD4 },
    MENU_CMD_END,
    { 0, 17, 304, 15 },
    { MENU_CMD_NAV, -1, -1, -1, 4 },
    { MENU_CMD_COLORS, 0x1D },
    { MENU_CMD_TEXT_PTR, 0x0, (int8_t*)0x2DAD4 },
    { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect2 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { {
          { 0, 34, 76, 12 },
          { MENU_CMD_NAV, -1, 4, 2, 19 },
          { MENU_CMD_COLORS, 0xD },
          { MENU_CMD_STRING_TABLE, 0x0, &diyPitchSeasonStrTab },
          { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect3 },
          MENU_CMD_END,
      },
        {
            { 77, 34, 56, 12 },
            { MENU_CMD_NAV, 3, 5, 2, 5 },
            { MENU_CMD_COLORS, 0xD },
            { MENU_CMD_STRING_TABLE, 0x0, &diyPitchTypeStringTable },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect4 },
            MENU_CMD_END,
        },
        {
            { 77, 34, 22, 12 },
            { MENU_CMD_NAV, 4, 7, 4, 20 },
            { MENU_CMD_COLORS, 0xD },
            { MENU_CMD_STRING_TABLE, 0x0, &diyStartMonthStrTab },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect5 },
            MENU_CMD_END,
        } },
    {
        { 100, 34, 10, 12 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, asc_135D96 },
        MENU_CMD_END,
    },
    {
        { 111, 34, 22, 12 },
        { MENU_CMD_NAV, 5, 9, 4, 11 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &diyEndMonthStrTab },
        { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect7 },
        MENU_CMD_END,
    },
    {
        { -118, 34, 64, 12 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aAwayGoals },
        MENU_CMD_END,
    },
    {
        { -54, 34, 102, 12 },
        { MENU_CMD_NAV, 7, -1, 2, 13 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &diyAwayGoalsStrTab },
        { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect9 },
        MENU_CMD_END,
    },
    {
        { 53, 47, 70, 12 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aSubstitutes },
        MENU_CMD_END,
    },
    { 124, 47, 40, 12 },
    { MENU_CMD_NAV, -1, 13, 7, 23 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect11 },
    MENU_CMD_END,
    {
        { -91, 47, 38, 12 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aFrom },
        MENU_CMD_END,
    },
    { -52, 47, 40, 12 },
    { MENU_CMD_NAV, 11, -1, 9, 25 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect13 },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 0, 33 },
    { {
          { 1, 29, 34, 9 },
          { MENU_CMD_TEXT_PTR, 0x8000, aTeams },
          MENU_CMD_END,
      },
        {
            { -126, 29, 120, 9 },
            { MENU_CMD_TEXT_PTR, 0x0, aDescription },
            MENU_CMD_END,
        },
        {
            { 83, 29, 50, 9 },
            { MENU_CMD_TEXT_PTR, 0x0, aSeeded },
            MENU_CMD_END,
        } },
    { { MENU_MARK_START_MENU, 0, 71 },
        {
            { 0, 0, 6, 9 },
            { MENU_CMD_NUMBER, 0x0, 1 },
            MENU_CMD_END,
        },
        {
            { 8, 10, 16, 8 },
            { MENU_CMD_SPRITE_FG, 185 },
            MENU_CMD_END,
        },
        { 8, 0, 16, 9 }, { MENU_CMD_NAV, -1, 20, 3, 31 },
        { MENU_CMD_COLORS, 0xB }, { MENU_CMD_NUMBER, 0x0, 0 },
        { MENU_CMD_INPUT_FUNC, 0x3C1,
            _DesignDIYTournament_AdjustStageQualifiers },
        MENU_CMD_END, { 25, 0, 78, 9 }, { MENU_CMD_NAV, 19, 21, 5, 32 },
        { MENU_CMD_COLORS, 0xD }, { MENU_CMD_TEXT_NO_PTR, 0x0 },
        { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect20 },
        MENU_CMD_END,
        {
            { 104, 0, 8, 9 },
            { MENU_CMD_NAV, 20, 23, 11, 33 },
            { MENU_CMD_COLORS, 0xD },
            { MENU_CMD_TEXT_PTR, 0x0, aStar },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect21 },
            MENU_CMD_END,
        },
        MENU_MARK_WIDE_COORDS, { 25, 10, 279, 9 }, { MENU_CMD_COLORS, 0x0 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 }, MENU_CMD_END, MENU_MARK_BYTE_COORDS,
        { {
              { 113, 0, 38, 9 },
              { MENU_CMD_NAV, 21, 24, 11, 26 },
              { MENU_CMD_COLORS, 0xD },
              { MENU_CMD_STRING_TABLE, 0x0, &diyStage1LegsStrTab },
              { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect23 },
              MENU_CMD_END,
          },
            {
                { -104, 0, 72, 9 },
                { MENU_CMD_NAV, 23, 25, 11, 27 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyStage1ExtraTimeStrTab },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect24 },
                MENU_CMD_END,
            },
            {
                { -31, 0, 79, 9 },
                { MENU_CMD_NAV, 24, 26, 13, 28 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyStage1PenaltiesStrTab },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect25 },
                MENU_CMD_END,
            },
            {
                { 113, 0, 84, 9 },
                { MENU_CMD_NAV, 25, 27, 23, 35 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyStage1PtsForWinStrTab },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect26 },
                MENU_CMD_END,
            } },
        { 0, 0, 0, 0 }, { MENU_CMD_NAV, 26, 28, 24, 36 },
        { MENU_CMD_INVISIBLE }, MENU_CMD_END,
        {
            { -58, 0, 106, 9 },
            { MENU_CMD_NAV, 26, -1, 25, 37 },
            { MENU_CMD_COLORS, 0xD },
            { MENU_CMD_STRING_TABLE, 0x0, &diyStage1PlayCountStrTab },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect28 },
            MENU_CMD_END,
        } },
    { { MENU_MARK_START_MENU, 0, 90 },
        {
            { 0, 0, 6, 9 },
            { MENU_CMD_NUMBER, 0x0, 2 },
            MENU_CMD_END,
        },
        {
            { 8, 10, 16, 8 },
            { MENU_CMD_SPRITE_FG, 185 },
            MENU_CMD_END,
        },
        { 8, 0, 16, 9 }, { MENU_CMD_NAV, -1, 32, 19, 43 },
        { MENU_CMD_COLORS, 0xB }, { MENU_CMD_NUMBER, 0x0, 0 },
        { MENU_CMD_INPUT_FUNC, 0x3C1,
            _DesignDIYTournament_AdjustStageQualifiers },
        MENU_CMD_END, { 25, 0, 78, 9 }, { MENU_CMD_NAV, 31, 33, 20, 44 },
        { MENU_CMD_COLORS, 0xD }, { MENU_CMD_TEXT_NO_PTR, 0x0 },
        { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect20 },
        MENU_CMD_END,
        {
            { 104, 0, 8, 9 },
            { MENU_CMD_NAV, 32, 35, 21, 45 },
            { MENU_CMD_COLORS, 0xD },
            { MENU_CMD_TEXT_PTR, 0x0, aStar },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect21 },
            MENU_CMD_END,
        },
        MENU_MARK_WIDE_COORDS, { 25, 10, 279, 9 }, { MENU_CMD_COLORS, 0x0 },
        { MENU_CMD_TEXT_PTR, 0x0, (char*)0x4 }, MENU_CMD_END,
        MENU_MARK_BYTE_COORDS,
        { {
              { 113, 0, 38, 9 },
              { MENU_CMD_NAV, 33, 36, 26, 38 },
              { MENU_CMD_COLORS, 0xD },
              { MENU_CMD_STRING_TABLE, 0x0, &diyStage2LegsStrTab },
              { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect23 },
              MENU_CMD_END,
          },
            {
                { -104, 0, 72, 9 },
                { MENU_CMD_NAV, 35, 37, 27, 39 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyStage2ExtraTimeStrTab },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect24 },
                MENU_CMD_END,
            },
            {
                { -31, 0, 79, 9 },
                { MENU_CMD_NAV, 36, 38, 28, 40 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyStage2PenaltiesStrTab },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect25 },
                MENU_CMD_END,
            },
            {
                { 113, 0, 84, 9 },
                { MENU_CMD_NAV, 37, 39, 35, 47 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyStage2PtsForWinStrTab },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect26 },
                MENU_CMD_END,
            } },
        { 0, 0, 0, 0 }, { MENU_CMD_NAV, 38, 40, 36, 48 },
        { MENU_CMD_INVISIBLE }, MENU_CMD_END,
        {
            { -58, 0, 106, 9 },
            { MENU_CMD_NAV, 38, -1, 37, 49 },
            { MENU_CMD_COLORS, 0xD },
            { MENU_CMD_STRING_TABLE, 0x0, &diyStage2PlayCountStrTab },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect28 },
            MENU_CMD_END,
        } },
    { { { MENU_MARK_START_MENU, 0, 109 },
          {
              { 0, 0, 6, 9 },
              { MENU_CMD_NUMBER, 0x0, 3 },
              MENU_CMD_END,
          },
          {
              { 8, 10, 16, 8 },
              { MENU_CMD_SPRITE_FG, 185 },
              MENU_CMD_END,
          },
          { 8, 0, 16, 9 }, { MENU_CMD_NAV, -1, 44, 31, 55 },
          { MENU_CMD_COLORS, 0xB }, { MENU_CMD_NUMBER, 0x0, 0 },
          { MENU_CMD_INPUT_FUNC, 0x3C1,
              _DesignDIYTournament_AdjustStageQualifiers },
          MENU_CMD_END, { 25, 0, 78, 9 }, { MENU_CMD_NAV, 43, 45, 32, 56 },
          { MENU_CMD_COLORS, 0xD }, { MENU_CMD_TEXT_NO_PTR, 0x0 },
          { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect20 },
          MENU_CMD_END,
          {
              { 104, 0, 8, 9 },
              { MENU_CMD_NAV, 44, 47, 33, 57 },
              { MENU_CMD_COLORS, 0xD },
              { MENU_CMD_TEXT_PTR, 0x0, aStar },
              { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect21 },
              MENU_CMD_END,
          },
          MENU_MARK_WIDE_COORDS, { 25, 10, 279, 9 }, { MENU_CMD_COLORS, 0x0 },
          { MENU_CMD_TEXT_NO_PTR, 0x0 }, MENU_CMD_END, MENU_MARK_BYTE_COORDS,
          { {
                { 113, 0, 38, 9 },
                { MENU_CMD_NAV, 45, 48, 38, 50 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyStage3LegsStrTab },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect23 },
                MENU_CMD_END,
            },
              {
                  { -104, 0, 72, 9 },
                  { MENU_CMD_NAV, 47, 49, 39, 51 },
                  { MENU_CMD_COLORS, 0xD },
                  { MENU_CMD_STRING_TABLE, 0x0, &diyStage3ExtraTimeStrTab },
                  { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect24 },
                  MENU_CMD_END,
              },
              {
                  { -31, 0, 79, 9 },
                  { MENU_CMD_NAV, 48, 50, 40, 52 },
                  { MENU_CMD_COLORS, 0xD },
                  { MENU_CMD_STRING_TABLE, 0x0, &diyStage3PenaltiesStrTab },
                  { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect25 },
                  MENU_CMD_END,
              },
              {
                  { 113, 0, 84, 9 },
                  { MENU_CMD_NAV, 49, 51, 47, 59 },
                  { MENU_CMD_COLORS, 0xD },
                  { MENU_CMD_STRING_TABLE, 0x0, &diyStage3PtsForWinStrTab },
                  { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect26 },
                  MENU_CMD_END,
              } },
          { 0, 0, 0, 0 }, { MENU_CMD_NAV, 50, 52, 48, 60 },
          { MENU_CMD_INVISIBLE }, MENU_CMD_END,
          {
              { -58, 0, 106, 9 },
              { MENU_CMD_NAV, 50, -1, 49, 61 },
              { MENU_CMD_COLORS, 0xD },
              { MENU_CMD_STRING_TABLE, 0x0, &diyStage3PlayCountStrTab },
              { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect28 },
              MENU_CMD_END,
          } },
        { { MENU_MARK_START_MENU, 0, 128 },
            {
                { 0, 0, 6, 9 },
                { MENU_CMD_NUMBER, 0x0, 4 },
                MENU_CMD_END,
            },
            {
                { 8, 10, 16, 8 },
                { MENU_CMD_SPRITE_FG, 185 },
                MENU_CMD_END,
            },
            { 8, 0, 16, 9 }, { MENU_CMD_NAV, -1, 56, 43, 67 },
            { MENU_CMD_COLORS, 0xB }, { MENU_CMD_NUMBER, 0x0, 0 },
            { MENU_CMD_INPUT_FUNC, 0x3C1,
                _DesignDIYTournament_AdjustStageQualifiers },
            MENU_CMD_END, { 25, 0, 78, 9 }, { MENU_CMD_NAV, 55, 57, 44, 68 },
            { MENU_CMD_COLORS, 0xD }, { MENU_CMD_TEXT_NO_PTR, 0x0 },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect20 },
            MENU_CMD_END,
            {
                { 104, 0, 8, 9 },
                { MENU_CMD_NAV, 56, 59, 45, 69 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_TEXT_PTR, 0x0, aStar },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect21 },
                MENU_CMD_END,
            },
            MENU_MARK_WIDE_COORDS, { 25, 10, 279, 9 }, { MENU_CMD_COLORS, 0x0 },
            { MENU_CMD_TEXT_NO_PTR, 0x0 }, MENU_CMD_END, MENU_MARK_BYTE_COORDS,
            { {
                  { 113, 0, 38, 9 },
                  { MENU_CMD_NAV, 57, 60, 50, 62 },
                  { MENU_CMD_COLORS, 0xD },
                  { MENU_CMD_STRING_TABLE, 0x0, &diyStage4LegsStrTab },
                  { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect23 },
                  MENU_CMD_END,
              },
                {
                    { -104, 0, 72, 9 },
                    { MENU_CMD_NAV, 59, 61, 51, 63 },
                    { MENU_CMD_COLORS, 0xD },
                    { MENU_CMD_STRING_TABLE, 0x0, &diyStage4ExtraTimeStrTab },
                    { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect24 },
                    MENU_CMD_END,
                },
                {
                    { -31, 0, 79, 9 },
                    { MENU_CMD_NAV, 60, 62, 52, 64 },
                    { MENU_CMD_COLORS, 0xD },
                    { MENU_CMD_STRING_TABLE, 0x0, &diyStage4PenaltiesStrTab },
                    { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect25 },
                    MENU_CMD_END,
                },
                {
                    { 113, 0, 84, 9 },
                    { MENU_CMD_NAV, 61, 63, 59, 71 },
                    { MENU_CMD_COLORS, 0xD },
                    { MENU_CMD_STRING_TABLE, 0x0, &diyStage4PtsForWinStrTab },
                    { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect26 },
                    MENU_CMD_END,
                } },
            { 0, 0, 0, 0 }, { MENU_CMD_NAV, 62, 64, 60, 72 },
            { MENU_CMD_INVISIBLE }, MENU_CMD_END,
            {
                { -58, 0, 106, 9 },
                { MENU_CMD_NAV, 62, -1, 61, 73 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyStage4PlayCountStrTab },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect28 },
                MENU_CMD_END,
            } },
        { { MENU_MARK_START_MENU, 0, 147 },
            {
                { 0, 0, 6, 9 },
                { MENU_CMD_NUMBER, 0x0, 5 },
                MENU_CMD_END,
            },
            {
                { 8, 10, 16, 8 },
                { MENU_CMD_SPRITE_FG, 185 },
                MENU_CMD_END,
            },
            { 8, 0, 16, 9 }, { MENU_CMD_NAV, -1, 68, 55, 79 },
            { MENU_CMD_COLORS, 0xB }, { MENU_CMD_NUMBER, 0x0, 0 },
            { MENU_CMD_INPUT_FUNC, 0x3C1,
                _DesignDIYTournament_AdjustStageQualifiers },
            MENU_CMD_END, { 25, 0, 78, 9 }, { MENU_CMD_NAV, 67, 69, 56, 80 },
            { MENU_CMD_COLORS, 0xD }, { MENU_CMD_TEXT_NO_PTR, 0x0 },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect20 },
            MENU_CMD_END,
            {
                { 104, 0, 8, 9 },
                { MENU_CMD_NAV, 68, 71, 57, 81 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_TEXT_PTR, 0x0, aStar },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect21 },
                MENU_CMD_END,
            },
            MENU_MARK_WIDE_COORDS, { 25, 10, 279, 9 }, { MENU_CMD_COLORS, 0x0 },
            { MENU_CMD_TEXT_NO_PTR, 0x0 }, MENU_CMD_END, MENU_MARK_BYTE_COORDS,
            { {
                  { 113, 0, 38, 9 },
                  { MENU_CMD_NAV, 69, 72, 62, 74 },
                  { MENU_CMD_COLORS, 0xD },
                  { MENU_CMD_STRING_TABLE, 0x0, &diyStage5LegsStrTab },
                  { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect23 },
                  MENU_CMD_END,
              },
                {
                    { -104, 0, 72, 9 },
                    { MENU_CMD_NAV, 71, 73, 63, 75 },
                    { MENU_CMD_COLORS, 0xD },
                    { MENU_CMD_STRING_TABLE, 0x0, &diyStage5ExtraTimeStrTab },
                    { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect24 },
                    MENU_CMD_END,
                },
                {
                    { -31, 0, 79, 9 },
                    { MENU_CMD_NAV, 72, 74, 64, 76 },
                    { MENU_CMD_COLORS, 0xD },
                    { MENU_CMD_STRING_TABLE, 0x0, &diyStage5PenaltiesStrTab },
                    { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect25 },
                    MENU_CMD_END,
                },
                {
                    { 113, 0, 84, 9 },
                    { MENU_CMD_NAV, 73, 75, 71, 83 },
                    { MENU_CMD_COLORS, 0xD },
                    { MENU_CMD_STRING_TABLE, 0x0, &diyStage5PtsForWinStrTab },
                    { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect26 },
                    MENU_CMD_END,
                } },
            { 0, 0, 0, 0 }, { MENU_CMD_NAV, 74, 76, 72, 84 },
            { MENU_CMD_INVISIBLE }, MENU_CMD_END,
            {
                { -58, 0, 106, 9 },
                { MENU_CMD_NAV, 74, -1, 73, 85 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyStage5PlayCountStrTab },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect28 },
                MENU_CMD_END,
            } },
        { { MENU_MARK_START_MENU, 0, 166 },
            {
                { 0, 0, 6, 9 },
                { MENU_CMD_NUMBER, 0x0, 6 },
                MENU_CMD_END,
            },
            {
                { 8, 10, 16, 8 },
                { MENU_CMD_SPRITE_FG, 185 },
                MENU_CMD_END,
            },
            { 8, 0, 16, 9 }, { MENU_CMD_NAV, -1, 80, 67, 90 },
            { MENU_CMD_COLORS, 0xB }, { MENU_CMD_NUMBER, 0x0, 0 },
            { MENU_CMD_INPUT_FUNC, 0x3C1,
                _DesignDIYTournament_AdjustStageQualifiers },
            MENU_CMD_END, { 25, 0, 78, 9 }, { MENU_CMD_NAV, 79, 81, 68, 90 },
            { MENU_CMD_COLORS, 0xD }, { MENU_CMD_TEXT_NO_PTR, 0x0 },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect20 },
            MENU_CMD_END,
            {
                { 104, 0, 8, 9 },
                { MENU_CMD_NAV, 80, 83, 69, 90 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_TEXT_PTR, 0x0, aStar },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect21 },
                MENU_CMD_END,
            },
            MENU_MARK_WIDE_COORDS, { 25, 10, 279, 9 }, { MENU_CMD_COLORS, 0x0 },
            { MENU_CMD_TEXT_NO_PTR, 0x0 }, MENU_CMD_END, MENU_MARK_BYTE_COORDS,
            { {
                  { 113, 0, 38, 9 },
                  { MENU_CMD_NAV, 81, 84, 74, 86 },
                  { MENU_CMD_COLORS, 0xD },
                  { MENU_CMD_STRING_TABLE, 0x0, &diyStage6LegsStrTab },
                  { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect23 },
                  MENU_CMD_END,
              },
                {
                    { -104, 0, 72, 9 },
                    { MENU_CMD_NAV, 83, 85, 75, 87 },
                    { MENU_CMD_COLORS, 0xD },
                    { MENU_CMD_STRING_TABLE, 0x0, &diyStage6ExtraTimeStrTab },
                    { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect24 },
                    MENU_CMD_END,
                },
                {
                    { -31, 0, 79, 9 },
                    { MENU_CMD_NAV, 84, 86, 76, 88 },
                    { MENU_CMD_COLORS, 0xD },
                    { MENU_CMD_STRING_TABLE, 0x0, &diyStage6PenaltiesStrTab },
                    { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect25 },
                    MENU_CMD_END,
                },
                {
                    { 113, 0, 84, 9 },
                    { MENU_CMD_NAV, 85, 87, 83, 90 },
                    { MENU_CMD_COLORS, 0xD },
                    { MENU_CMD_STRING_TABLE, 0x0, &diyStage6PtsForWinStrTab },
                    { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect26 },
                    MENU_CMD_END,
                } },
            { 0, 0, 0, 0 }, { MENU_CMD_NAV, 86, 88, 84, 91 },
            { MENU_CMD_INVISIBLE }, MENU_CMD_END,
            {
                { -58, 0, 106, 9 },
                { MENU_CMD_NAV, 86, -1, 85, 91 },
                { MENU_CMD_COLORS, 0xD },
                { MENU_CMD_STRING_TABLE, 0x0, &diyStage6PlayCountStrTab },
                { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect28 },
                MENU_CMD_END,
            } } },
    { MENU_MARK_START_MENU, 0, 0 },
    { {
          { 0, -71, 120, 15 },
          { MENU_CMD_NAV, -1, 90, 79, -1 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aChooseTeams },
          { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect89 },
          MENU_CMD_END,
      },
        {
            { 89, -71, 62, 15 },
            { MENU_CMD_NAV, 89, 91, 86, -1 },
            { MENU_CMD_COLORS, 0xE },
            { MENU_CMD_TEXT_PTR, 0x0, aOk },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOK },
            MENU_CMD_END,
        },
        {
            { 121, -71, 62, 15 },
            { MENU_CMD_NAV, 90, -1, 87, -1 },
            { MENU_CMD_COLORS, 0xC },
            { MENU_CMD_TEXT_PTR, 0x0, aExit },
            { MENU_CMD_ONSELECT_FIRE, _DesignDIYTournamentOnSelect91 },
            MENU_CMD_END,
        } },
    { -72, -68, 120, 9 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_FriendlyGame friendlyGameMenu = {
    { _FriendlyMenuInit, _FriendlyMenuReinit, 0, 8 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aFriendly_1 },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 10 },
    {
        { 31, 70, 120, 15 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aSubstitutes },
        MENU_CMD_END,
    },
    {
        { -63, 70, 38, 15 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aFrom },
        MENU_CMD_END,
    },
    {
        { 31, 50, 120, 15 },
        { MENU_CMD_NAV, -1, 4, -1, 5 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &pitchTypeOrSeasonTable },
        { MENU_CMD_ONSELECT_FIRE, _PitchTypeOrSeasonProc },
        MENU_CMD_END,
    },
    {
        { -104, 50, 120, 15 },
        { MENU_CMD_NAV, 3, -1, -1, 5 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &friendlyPitchTypeTable },
        { MENU_CMD_ONSELECT_FIRE, _FriendlyPitchTypeProc },
        MENU_CMD_END,
    },
    { -104, 70, 40, 15 },
    { MENU_CMD_NAV, -1, 6, 4, 8 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _MinSubstitutes },
    MENU_CMD_END,
    { -24, 70, 40, 15 },
    { MENU_CMD_NAV, 5, -1, 4, 8 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _MaxSubstitutes },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 0, 0 },
    {
        { 0, -71, 120, 15 },
        { MENU_CMD_NAV, -1, 9, 5, -1 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aChooseTeams },
        { MENU_CMD_ONSELECT_FIRE, _ChooseTeamsMenu },
        MENU_CMD_END,
    },
    {
        { 121, -91, 62, 15 },
        { MENU_CMD_NAV, -1, -1, 5, 9 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aOk },
        { MENU_CMD_ONSELECT_FIRE, _FriendlyOK },
        MENU_CMD_END,
    },
    {
        { 121, -71, 62, 15 },
        { MENU_CMD_NAV, 7, -1, 8, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _jmpSetExitMenuFlag },
        MENU_CMD_END,
    },
    { -72, -68, 120, 9 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_EditTeams editTeamsMenu = {
    { _EditTeamsInit, _EditTeamsFinish, 0, 0 },
    MENU_MARK_BYTE_COORDS,
    {
        { 0, 0, -63, 12 },
        { MENU_CMD_NAV, -1, 5, -1, 39 },
        { MENU_CMD_COLORS, 0xA },
        { MENU_CMD_TEXT_PTR, 0x10, 0 },
        { MENU_CMD_ONSELECT_FIRE, _EditTeamsSelectTeamName },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { 274, 0, 30, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_NUMBER, 0x0, 0 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    {
        { 0, -74, 55, 10 },
        { MENU_CMD_TEXT_PTR, 0x8000, aCoach_3 },
        MENU_CMD_END,
    },
    {
        { 0, -66, -114, 10 },
        { MENU_CMD_NAV, -1, 20, 54, -1 },
        { MENU_CMD_COLORS, 0xA },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        { MENU_CMD_ONSELECT_FIRE, _EditTeamsSelectCoachName },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 211, 21 },
    {
        { 23, 0, 0, 6 },
        { MENU_CMD_TEXT_PTR, 0x0, aFirstKit },
        MENU_CMD_END,
    },
    { 0, 7, 46, 33 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_COPY, editTeamsPrimaryShirts },
    { MENU_CMD_NAV, 41, 13, 0, 6 },
    { MENU_CMD_INPUT_FUNC, 0x3C1, _EditTeamsChangeShirtsPrimary },
    MENU_CMD_END,
    { 0, 40, 46, 19 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_COPY, editTeamsShorts },
    { MENU_CMD_NAV, 43, 14, 5, 7 },
    { MENU_CMD_INPUT_FUNC, 0x3C1, _EditTeamsChangeShortsPrimary },
    MENU_CMD_END,
    { 0, 59, 46, 19 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_COPY, editTeamsSocks },
    { MENU_CMD_NAV, 45, 15, 6, 9 },
    { MENU_CMD_INPUT_FUNC, 0x3C1, _EditTeamsChangeSocksPrimary },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 211, 101 },
    {
        { 23, 0, 0, 6 },
        { MENU_CMD_TEXT_PTR, 0x0, aSecondKit },
        MENU_CMD_END,
    },
    { 0, 7, 46, 33 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_COPY, editTeamsSecondaryShirt },
    { MENU_CMD_NAV, 49, 17, 7, 10 },
    { MENU_CMD_INPUT_FUNC, 0x3C1, _EditTeamsChangeShirtsSecondary },
    MENU_CMD_END,
    { 0, 40, 46, 19 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_COPY, editTeamsSecondaryShorts },
    { MENU_CMD_NAV, 51, 18, 9, 11 },
    { MENU_CMD_INPUT_FUNC, 0x3C1, _EditTeamsChangeShortsSecondary },
    MENU_CMD_END,
    { 0, 59, 46, 19 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_COPY, editTeamsSecondarySocks },
    { MENU_CMD_NAV, 53, 19, 10, 22 },
    { MENU_CMD_INPUT_FUNC, 0x3C1, _EditTeamsChangeSocksSecondary },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 272, 24 },
    { 0, 0, 24, 18 },
    { MENU_CMD_NAV, 5, -1, 0, 13 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_FG, 199 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsSelectPrimaryShirtType },
    MENU_CMD_END,
    { 0, 19, 24, 18 },
    { MENU_CMD_NAV, 5, -1, 12, 14 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_FG, 200 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsSelectPrimaryShirtType },
    MENU_CMD_END,
    { 0, 38, 24, 18 },
    { MENU_CMD_NAV, 6, -1, 13, 15 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_FG, 201 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsSelectPrimaryShirtType },
    MENU_CMD_END,
    { 0, 57, 24, 18 },
    { MENU_CMD_NAV, 7, -1, 14, 16 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_FG, 202 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsSelectPrimaryShirtType },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 272, 104 },
    { 0, 0, 24, 18 },
    { MENU_CMD_NAV, 9, -1, 15, 17 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_FG, 199 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsSelectSecondaryShirtType },
    MENU_CMD_END,
    { 0, 19, 24, 18 },
    { MENU_CMD_NAV, 9, -1, 16, 18 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_FG, 200 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsSelectSecondaryShirtType },
    MENU_CMD_END,
    { 0, 38, 24, 18 },
    { MENU_CMD_NAV, 10, -1, 17, 19 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_FG, 201 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsSelectSecondaryShirtType },
    MENU_CMD_END,
    { 0, 57, 24, 18 },
    { MENU_CMD_NAV, 11, -1, 18, 21 },
    { MENU_CMD_COLORS, 0x9 },
    { MENU_CMD_SPRITE_FG, 202 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsSelectSecondaryShirtType },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 0, 0 },
    {
        { -92, -71, 40, 15 },
        { MENU_CMD_NAV, 3, 22, 54, -1 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aImport },
        { MENU_CMD_ONSELECT_FIRE, _ImportTeamsMenu },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { 264, 185, 40, 15 },
    { MENU_CMD_NAV, 22, -1, 19, -1 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aOk },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsOK },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    {
        { -42, -71, 40, 15 },
        { MENU_CMD_NAV, 20, 21, 11, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aUndo },
        { MENU_CMD_ONSELECT_FIRE, _EditTeamsUndo },
        MENU_CMD_END,
    },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_BEFORE_DRAW, _EditTeamsPlayersBeforeDraw },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 0, 22 },
    { 0, 0, 10, 8 },
    { MENU_CMD_NAV, -1, 39, 0, 24 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, 10, 10, 8 },
    { MENU_CMD_NAV, -1, 40, 23, 25 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, 20, 10, 8 },
    { MENU_CMD_NAV, -1, 41, 24, 26 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, 30, 10, 8 },
    { MENU_CMD_NAV, -1, 42, 25, 27 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, 40, 10, 8 },
    { MENU_CMD_NAV, -1, 43, 26, 28 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, 50, 10, 8 },
    { MENU_CMD_NAV, -1, 44, 27, 29 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, 60, 10, 8 },
    { MENU_CMD_NAV, -1, 45, 28, 30 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, 70, 10, 8 },
    { MENU_CMD_NAV, -1, 46, 29, 31 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, 80, 10, 8 },
    { MENU_CMD_NAV, -1, 47, 30, 32 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, 90, 10, 8 },
    { MENU_CMD_NAV, -1, 48, 31, 33 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, 100, 10, 8 },
    { MENU_CMD_NAV, -1, 49, 32, 34 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, 110, 10, 8 },
    { MENU_CMD_NAV, -1, 50, 33, 35 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, 120, 10, 8 },
    { MENU_CMD_NAV, -1, 51, 34, 36 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, -126, 10, 8 },
    { MENU_CMD_NAV, -1, 52, 35, 37 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, -116, 10, 8 },
    { MENU_CMD_NAV, -1, 53, 36, 38 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    { 0, -106, 10, 8 },
    { MENU_CMD_NAV, -1, 54, 37, 3 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerFace },
    MENU_CMD_END,
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 22 },
    { {
          { 22, 0, -105, 8 },
          { MENU_CMD_NAV, 23, 5, 0, 40 },
          { MENU_CMD_COLORS, 0xA },
          { MENU_CMD_TEXT_PTR, 0x8000, 0 },
          { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
          MENU_CMD_END,
      },
        {
            { 22, 10, -105, 8 },
            { MENU_CMD_NAV, 24, 5, 39, 41 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, 20, -105, 8 },
            { MENU_CMD_NAV, 25, 5, 40, 42 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, 30, -105, 8 },
            { MENU_CMD_NAV, 26, 5, 41, 43 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, 40, -105, 8 },
            { MENU_CMD_NAV, 27, 6, 42, 44 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, 50, -105, 8 },
            { MENU_CMD_NAV, 28, 6, 43, 45 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, 60, -105, 8 },
            { MENU_CMD_NAV, 29, 7, 44, 46 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, 70, -105, 8 },
            { MENU_CMD_NAV, 30, 7, 45, 47 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, 80, -105, 8 },
            { MENU_CMD_NAV, 31, 9, 46, 48 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, 90, -105, 8 },
            { MENU_CMD_NAV, 32, 9, 47, 49 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, 100, -105, 8 },
            { MENU_CMD_NAV, 33, 9, 48, 50 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, 110, -105, 8 },
            { MENU_CMD_NAV, 34, 9, 49, 51 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, 120, -105, 8 },
            { MENU_CMD_NAV, 35, 10, 50, 52 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, -126, -105, 8 },
            { MENU_CMD_NAV, 36, 10, 51, 53 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, -116, -105, 8 },
            { MENU_CMD_NAV, 37, 11, 52, 54 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        },
        {
            { 22, -106, -105, 8 },
            { MENU_CMD_NAV, 38, 11, 53, 3 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            { MENU_CMD_ONSELECT_FIRE, _EditTeamsChangePlayerName },
            MENU_CMD_END,
        } },
    {
        { 11, 0, 10, 8 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -92, 0, 9, 8 },
        { MENU_CMD_COLORS, 0xA },
        MENU_CMD_END,
    },
    {
        { -82, 0, 14, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    { {
          { -68, 0, 7, 7 },
          { MENU_CMD_SPRITE_FG, 181 },
          MENU_CMD_END,
      },
        {
            { -62, 0, 7, 7 },
            { MENU_CMD_SPRITE_FG, 182 },
            MENU_CMD_END,
        },
        {
            { -91, 0, 9, 8 },
            { MENU_CMD_SPRITE_FG, 189 },
            MENU_CMD_END,
        } },
    MENU_MARK_END_MENU,
};
Menu_CompetitionTopGoalScorers competitionTopGoalScorersMenu = {
    { _CompetitionTopGoalScorersInit, 0, 0, 2 },
    { 0, 0, 304, 15 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_PTR, 0x0, 0 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    {
        { 52, 16, -56, 13 },
        { MENU_CMD_COLORS, 0x79 },
        { MENU_CMD_TEXT_PTR, 0x0, aLeadingCompeti },
        MENU_CMD_END,
    },
    { 102, -71, 100, 15 },
    { MENU_CMD_COLORS, 0xC },
    { MENU_CMD_TEXT_PTR, 0x0, aExit },
    { MENU_CMD_ONSELECT_FIRE, _CompetitionTopGoalScorersExit },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 14, 32 },
    {
        { 0, 0, 12, 8 },
        { MENU_CMD_NUMBER, 0x0, 1 },
        MENU_CMD_END,
    },
    { {
          { 20, 0, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, 0, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, 0, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, 10, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 2 },
            MENU_CMD_END,
        } },
    { {
          { 20, 10, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, 10, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, 10, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, 20, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 3 },
            MENU_CMD_END,
        } },
    { {
          { 20, 20, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, 20, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, 20, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, 30, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 4 },
            MENU_CMD_END,
        } },
    { {
          { 20, 30, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, 30, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, 30, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, 40, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 5 },
            MENU_CMD_END,
        } },
    { {
          { 20, 40, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, 40, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, 40, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, 50, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 6 },
            MENU_CMD_END,
        } },
    { {
          { 20, 50, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, 50, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, 50, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, 60, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 7 },
            MENU_CMD_END,
        } },
    { {
          { 20, 60, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, 60, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, 60, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, 70, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 8 },
            MENU_CMD_END,
        } },
    { {
          { 20, 70, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, 70, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, 70, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, 80, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 9 },
            MENU_CMD_END,
        } },
    { {
          { 20, 80, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, 80, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, 80, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, 90, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 10 },
            MENU_CMD_END,
        } },
    { {
          { 20, 90, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, 90, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, 90, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, 100, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 11 },
            MENU_CMD_END,
        } },
    { {
          { 20, 100, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, 100, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, 100, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, 110, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 12 },
            MENU_CMD_END,
        } },
    { {
          { 20, 110, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, 110, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, 110, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, 120, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 13 },
            MENU_CMD_END,
        } },
    { {
          { 20, 120, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, 120, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, 120, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, -126, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 14 },
            MENU_CMD_END,
        } },
    { {
          { 20, -126, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, -126, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { {
          { -1, -126, 30, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, -116, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 15 },
            MENU_CMD_END,
        } },
    { {
          { 20, -116, 90, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { -101, -116, 90, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    {
        { -1, -116, 30, 8 },
        { MENU_CMD_NUMBER, 0x0, 1 },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_TopGoalScorers topGoalScorersMenu = {
    { _TopGoalScorersInit, 0, 0, 5 },
    MENU_MARK_BYTE_COORDS,
    {
        { 52, 0, -56, 13 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aTopGoalScorers },
        MENU_CMD_END,
    },
    { -9, 18, 12, 24 },
    {
        MENU_CMD_MULTILINE,
        { 0x0 },
        aLge_0,
    },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    { {
          { 262, 18, 12, 24 },
          {
              MENU_CMD_MULTILINE,
              { 0x0 },
              aCup1,
          },
          MENU_CMD_END,
      },
        {
            { 277, 18, 12, 24 },
            {
                MENU_CMD_MULTILINE,
                { 0x0 },
                aCup2,
            },
            MENU_CMD_END,
        },
        {
            { 292, 18, 12, 24 },
            {
                MENU_CMD_MULTILINE,
                { 0x0 },
                aTot_0,
            },
            MENU_CMD_END,
        } },
    MENU_MARK_BYTE_COORDS,
    { 102, -71, 100, 15 },
    { MENU_CMD_COLORS, 0xC },
    { MENU_CMD_TEXT_PTR, 0x0, aExit },
    { MENU_CMD_ONSELECT_FIRE, _TopGoalScorersExit },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 0, 45 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_BEFORE_DRAW, _TopGoalScorersBeforeDrawCommon },
    MENU_CMD_END,
    { {
          { 0, 0, 8, 8 },
          { MENU_CMD_NUMBER, 0x0, 1 },
          MENU_CMD_END,
      },
        {
            { 0, 9, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 2 },
            MENU_CMD_END,
        },
        {
            { 0, 18, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 3 },
            MENU_CMD_END,
        },
        {
            { 0, 27, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 4 },
            MENU_CMD_END,
        },
        {
            { 0, 36, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 5 },
            MENU_CMD_END,
        },
        {
            { 0, 45, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 6 },
            MENU_CMD_END,
        },
        {
            { 0, 54, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 7 },
            MENU_CMD_END,
        },
        {
            { 0, 63, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 8 },
            MENU_CMD_END,
        },
        {
            { 0, 72, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 9 },
            MENU_CMD_END,
        },
        {
            { 0, 81, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 10 },
            MENU_CMD_END,
        },
        {
            { 0, 90, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 11 },
            MENU_CMD_END,
        },
        {
            { 0, 99, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 12 },
            MENU_CMD_END,
        },
        {
            { 0, 108, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 13 },
            MENU_CMD_END,
        },
        {
            { 0, 117, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 14 },
            MENU_CMD_END,
        },
        {
            { 0, 126, 8, 8 },
            { MENU_CMD_NUMBER, 0x0, 15 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    {
        { 12, 0, 96, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { -117, 0, 100, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x8000 },
        MENU_CMD_END,
    },
    {
        { -9, 0, 12, 8 },
        { MENU_CMD_NUMBER, 0x0, 22 },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { {
          { 262, 0, 12, 8 },
          { MENU_CMD_NUMBER, 0x0, 33 },
          MENU_CMD_END,
      },
        {
            { 277, 0, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 44 },
            MENU_CMD_END,
        },
        {
            { 292, 0, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 77 },
            MENU_CMD_END,
        } },
    MENU_MARK_END_MENU,
};
Menu_Squad squadMenu = {
    { _SquadInit, _SquadFinish, 0, 9 },
    MENU_MARK_BYTE_COORDS,
    { 0, 0, 20, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_NUMBER, 0x0, 0 },
    MENU_CMD_END,
    { 3, 6, 16, 16 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_SPRITE_FG, 0 },
    MENU_CMD_END,
    {
        { 22, 0, -121, 11 },
        { MENU_CMD_COLORS, 0x0 },
        { MENU_CMD_TEXT_PTR, 0x10, 0 },
        MENU_CMD_END,
    },
    {
        { 89, 14, 0, 6 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    { {
          { 0, -64, 55, 8 },
          { MENU_CMD_TEXT_PTR, 0x8000, aCoach_4 },
          MENU_CMD_END,
      },
        {
            { 44, -64, -114, 8 },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            MENU_CMD_END,
        } },
    { -12, -71, 60, 15 },
    { MENU_CMD_NAV, 8, -1, 45, 9 },
    { MENU_CMD_DOWN_SKIP, 6, 3 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aTraining },
    { MENU_CMD_ONSELECT_FIRE, _SelectTraining },
    MENU_CMD_END,
    { 0, 0, 0, 0 },
    { MENU_CMD_NAV, -1, -1, 6, -1 },
    { MENU_CMD_INVISIBLE },
    MENU_CMD_END,
    { -74, -71, 60, 15 },
    { MENU_CMD_NAV, 44, 6, 45, 9 },
    { MENU_CMD_LEFT_SKIP, 7, 2 },
    { MENU_CMD_RIGHT_SKIP, 8, 1 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aGoals },
    { MENU_CMD_ONSELECT_FIRE, _SelectGoals },
    MENU_CMD_END,
    {
        { -74, -55, 122, 15 },
        { MENU_CMD_NAV, 44, -1, 8, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _j_SetExitMenuFlag },
        MENU_CMD_END,
    },
    { -43, -71, 60, 15 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xC },
    { MENU_CMD_TEXT_PTR, 0x0, aExit },
    { MENU_CMD_ONSELECT_FIRE, _j_SetExitMenuFlag },
    MENU_CMD_END,
    { 0, 0, 0, 0 },
    { MENU_CMD_ONSELECT_FIRE, _SquadOnSelect11 },
    MENU_CMD_END,
    {
        { -19, 2, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x10 },
        MENU_CMD_END,
    },
    { {
          { -41, 14, 60, 6 },
          { MENU_CMD_TEXT_PTR, 0x0, aGoals },
          MENU_CMD_END,
      },
        {
            { -83, 14, 60, 6 },
            { MENU_CMD_TEXT_PTR, 0x0, aNat },
            MENU_CMD_END,
        },
        {
            { -38, 14, 48, 6 },
            { MENU_CMD_TEXT_PTR, 0x0, aForm },
            MENU_CMD_END,
        } },
    {
        { -84, 3, -126, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x10 },
        MENU_CMD_END,
    },
    { {
          { -22, 14, 25, 6 },
          { MENU_CMD_TEXT_PTR, 0x0, aCups },
          MENU_CMD_END,
      },
        {
            { -51, 14, 20, 6 },
            { MENU_CMD_TEXT_PTR, 0x0, aLge },
            MENU_CMD_END,
        },
        {
            { -31, 14, 20, 6 },
            { MENU_CMD_TEXT_PTR, 0x0, aCup_3 },
            MENU_CMD_END,
        } },
    { -11, 14, 20, 6 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_TEXT_PTR, 0x0, 0 },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    { {
          { 265, 14, 20, 6 },
          { MENU_CMD_TEXT_PTR, 0x0, 0 },
          MENU_CMD_END,
      },
        {
            { 285, 14, 18, 6 },
            { MENU_CMD_TEXT_PTR, 0x0, aTot },
            MENU_CMD_END,
        } },
    {
        { 0, 172, 304, 9 },
        { MENU_CMD_COLORS, 0x0 },
        MENU_CMD_END,
    },
    {
        { 274, 183, 0, 8 },
        { MENU_CMD_TEXT_PTR, 0x4000, aOwnGoals },
        MENU_CMD_END,
    },
    {
        { 289, 183, 12, 8 },
        { MENU_CMD_NUMBER, 0x0, 222 },
        MENU_CMD_END,
    },
    {
        { 274, 192, 0, 8 },
        { MENU_CMD_TEXT_PTR, 0x4000, aExPlayerGoals },
        MENU_CMD_END,
    },
    {
        { 289, 192, 12, 8 },
        { MENU_CMD_NUMBER, 0x0, 333 },
        MENU_CMD_END,
    },
    MENU_MARK_START_TEMPLATE,
    MENU_MARK_BYTE_COORDS,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_AFTER_DRAW, _SquadAfterDrawCommon },
    { MENU_CMD_ONSELECT_FIRE, _SquadCommon },
    MENU_CMD_END,
    { {
          { 23, 24, -122, 8 },
          { MENU_CMD_NAV, -1, 8, -1, 29 },
          MENU_CMD_END,
      },
        {
            { 23, 33, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 28, 30 },
            MENU_CMD_END,
        },
        {
            { 23, 42, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 29, 31 },
            MENU_CMD_END,
        },
        {
            { 23, 51, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 30, 32 },
            MENU_CMD_END,
        },
        {
            { 23, 60, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 31, 33 },
            MENU_CMD_END,
        },
        {
            { 23, 69, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 32, 34 },
            MENU_CMD_END,
        },
        {
            { 23, 78, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 33, 35 },
            MENU_CMD_END,
        },
        {
            { 23, 87, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 34, 36 },
            MENU_CMD_END,
        },
        {
            { 23, 96, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 35, 37 },
            MENU_CMD_END,
        },
        {
            { 23, 105, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 36, 38 },
            MENU_CMD_END,
        },
        {
            { 23, 114, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 37, 39 },
            MENU_CMD_END,
        },
        {
            { 23, 125, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 38, 40 },
            MENU_CMD_END,
        },
        {
            { 23, -122, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 39, 41 },
            MENU_CMD_END,
        },
        {
            { 23, -113, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 40, 42 },
            MENU_CMD_END,
        },
        {
            { 23, -104, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 41, 43 },
            MENU_CMD_END,
        },
        {
            { 23, -95, -122, 8 },
            { MENU_CMD_NAV, -1, 8, 42, 44 },
            MENU_CMD_END,
        } },
    { 23, -84, -122, 8 },
    { MENU_CMD_NAV, 45, 8, 43, 8 },
    { MENU_CMD_UP_SKIP, 44, 2 },
    { MENU_CMD_INVISIBLE },
    MENU_CMD_END,
    MENU_MARK_INIT_ENTRY,
    { 0, -84, 12, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_NAV, -1, 44, 43, 8 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _SquadOnSelect45 },
    { MENU_CMD_AFTER_DRAW, _Squad45AfterDraw },
    MENU_CMD_END,
    { 0, 0, 12, 8 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    MENU_CMD_END,
    {
        { 13, 0, 10, 8 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 23, 0, -122, 8 },
        { MENU_CMD_COLORS, 0xA },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { -98, 0, 14, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -75, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0xF },
        MENU_CMD_END,
    },
    { {
          { -84, 0, 7, 7 },
          { MENU_CMD_SPRITE_FG, 181 },
          MENU_CMD_END,
      },
        {
            { -77, 0, 7, 7 },
            { MENU_CMD_SPRITE_FG, 182 },
            MENU_CMD_END,
        },
        {
            { -107, 0, 8, 8 },
            { MENU_CMD_SPRITE_FG, 189 },
            MENU_CMD_END,
        } },
    {
        { -68, 0, 30, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    {
        { -26, 0, 30, 8 },
        { MENU_CMD_NUMBER, 0x0, 333 },
        MENU_CMD_END,
    },
    { -38, 0, 30, 8 },
    { MENU_CMD_FG_FUNC, &_locret_139AE2 },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    {
        { 272, 0, 32, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    MENU_MARK_BYTE_COORDS,
    {
        { -61, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x8000 },
        MENU_CMD_END,
    },
    { {
          { -47, 0, 12, 8 },
          { MENU_CMD_NUMBER, 0x0, 22 },
          MENU_CMD_END,
      },
        {
            { -27, 0, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 33 },
            MENU_CMD_END,
        },
        {
            { -7, 0, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 44 },
            MENU_CMD_END,
        } },
    MENU_MARK_WIDE_COORDS,
    { {
          { 269, 0, 12, 8 },
          { MENU_CMD_NUMBER, 0x0, 55 },
          MENU_CMD_END,
      },
        {
            { 289, 0, 12, 8 },
            { MENU_CMD_NUMBER, 0x0, 77 },
            MENU_CMD_END,
        } },
    MENU_MARK_END_MENU,
};
Menu_CompetitionStats competitionStatsMenu = {
    { 0, 0, 0, 0x1 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aStatistics },
        MENU_CMD_END,
    },
    {
        { 92, 40, 120, 15 },
        { MENU_CMD_NAV, -1, -1, -1, 2 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aHighestScorerL },
        { MENU_CMD_ONSELECT_FIRE, _StatsHighestScorerList },
        MENU_CMD_END,
    },
    {
        { 92, 65, 120, 15 },
        { MENU_CMD_NAV, -1, -1, 1, 3 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aCompetitionInf },
        { MENU_CMD_ONSELECT_FIRE, _CompetitionInfo },
        MENU_CMD_END,
    },
    {
        { 92, 90, 120, 15 },
        { MENU_CMD_NAV, -1, -1, 2, 4 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aViewSquads },
        { MENU_CMD_ONSELECT_FIRE, _StatsViewSquads },
        MENU_CMD_END,
    },
    {
        { 102, -71, 100, 15 },
        { MENU_CMD_NAV, -1, -1, 3, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _StatsExit },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_CareerOver careerOverMenu = {
    { _CareerOverInit, _CareerOverFinish, 0, 4 },
    MENU_MARK_BYTE_COORDS,
    { 0, 0, 90, 14 },
    { MENU_CMD_COLORS, 0x79 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    { 91, 0, 122, 14 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    { -42, 0, 90, 14 },
    { MENU_CMD_COLORS, 0x79 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    { 82, 25, -116, 15 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_TEXT_PTR, 0x0, aYourCareerIsOv },
    MENU_CMD_END,
    {
        { 92, 50, 120, 15 },
        { MENU_CMD_NAV, -1, -1, -1, 5 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aViewCompetitio },
        { MENU_CMD_ONSELECT_FIRE, _ViewCompetitionsMenuRetired },
        MENU_CMD_END,
    },
    {
        { 92, 70, 120, 15 },
        { MENU_CMD_NAV, -1, -1, 4, 6 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aJobOffers },
        { MENU_CMD_ONSELECT_FIRE, _JobOffersMenuRetired },
        MENU_CMD_END,
    },
    {
        { 82, 118, -116, 15 },
        { MENU_CMD_NAV, -1, -1, 5, 7 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aContinueCareer },
        { MENU_CMD_ONSELECT_FIRE, _ContinueCareer },
        MENU_CMD_END,
    },
    {
        { 102, -91, 100, 15 },
        { MENU_CMD_NAV, -1, -1, 6, 8 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aSaveCareer },
        { MENU_CMD_ONSELECT_FIRE, _SaveCareer },
        MENU_CMD_END,
    },
    {
        { 102, -71, 100, 15 },
        { MENU_CMD_NAV, -1, -1, 7, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _ExitCareerMenu },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_Career careerMenu = {
    { _CareerMenuInit, _CareerMenuFinish, 0, 3 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aCareer_1 },
        MENU_CMD_END,
    },
    { 92, 20, 120, 10 },
    { MENU_CMD_NAV, -1, -1, -1, 4 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0x5 },
    { MENU_CMD_TEXT_PTR, 0x0, aLoadLiverpool },
    { MENU_CMD_ONSELECT_FIRE, _LoadLiverpool },
    MENU_CMD_END,
    {
        { 30, 65, 60, 11 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aName },
        MENU_CMD_END,
    },
    {
        { 94, 65, 20, 11 },
        { MENU_CMD_NAV, -1, 4, 1, 7 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_TEXT_PTR, 0x0, (char*)managerSexBuffer },
        { MENU_CMD_ONSELECT_FIRE, _ChangeSex },
        MENU_CMD_END,
    },
    {
        { 115, 65, 58, 11 },
        { MENU_CMD_NAV, 3, 5, 1, 7 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_TEXT_PTR, 0x0, (char*)managerNameBuffer },
        { MENU_CMD_ONSELECT_FIRE, _SetName },
        MENU_CMD_END,
    },
    {
        { -82, 65, 82, 11 },
        { MENU_CMD_NAV, 4, -1, 1, 7 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_TEXT_PTR, 0x0, (char*)managerSurnameBuffer },
        { MENU_CMD_ONSELECT_FIRE, _SetSurname },
        MENU_CMD_END,
    },
    {
        { 30, 85, 80, 11 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aNationality },
        MENU_CMD_END,
    },
    { 114, 85, 40, 11 },
    { MENU_CMD_NAV, -1, -1, 4, 9 },
    { MENU_CMD_COLORS, 0xB },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_INPUT_FUNC, 0x3C1, _ChangeNationality },
    MENU_CMD_END,
    {
        { 30, 105, 120, 11 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aStartCareerAs },
        MENU_CMD_END,
    },
    {
        { -102, 105, 120, 11 },
        { MENU_CMD_NAV, -1, -1, 7, 10 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, (char*)offsetCoachOrPlayer },
        { MENU_CMD_ONSELECT_FIRE, _CoachOrPlayerCoach },
        MENU_CMD_END,
    },
    {
        { 102, -91, 100, 15 },
        { MENU_CMD_NAV, -1, -1, 9, 11 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aChooseTeam },
        { MENU_CMD_ONSELECT_FIRE, _ChooseTeamAndStartCareer },
        MENU_CMD_END,
    },
    {
        { 102, -71, 100, 15 },
        { MENU_CMD_NAV, -1, -1, 10, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _j_SetExitMenuFlag2 },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_ViewCompetitions viewCompetitionsMenu = {
    { _ViewCompetitionsInit, _ViewCompetitionsOnReturn, 0, 1 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aViewCompetitio },
        MENU_CMD_END,
    },
    {
        { 47, 30, -46, 15 },
        { MENU_CMD_NAV, -1, -1, -1, 2 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        { MENU_CMD_ONSELECT_FIRE, _OpenDiyFromFile },
        MENU_CMD_END,
    },
    {
        { 47, 55, -46, 15 },
        { MENU_CMD_NAV, -1, -1, 1, 3 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        { MENU_CMD_ONSELECT_FIRE, _OpenDiyPreset1 },
        MENU_CMD_END,
    },
    {
        { 47, 80, -46, 15 },
        { MENU_CMD_NAV, -1, -1, 2, 4 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        { MENU_CMD_ONSELECT_FIRE, _OpenDiyPreset2 },
        MENU_CMD_END,
    },
    {
        { 47, 105, -46, 15 },
        { MENU_CMD_NAV, -1, -1, 3, 5 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        { MENU_CMD_ONSELECT_FIRE, _OpenDiyPreset3 },
        MENU_CMD_END,
    },
    {
        { 47, -126, -46, 15 },
        { MENU_CMD_NAV, -1, -1, 4, 6 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aTopGoalScorers },
        { MENU_CMD_ONSELECT_FIRE, _TopGoalScorersMenu },
        MENU_CMD_END,
    },
    {
        { 47, -101, -46, 15 },
        { MENU_CMD_NAV, -1, -1, 5, 7 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aManagementReco_0 },
        { MENU_CMD_ONSELECT_FIRE, _ManagementRecordMenu },
        MENU_CMD_END,
    },
    {
        { 102, -71, 100, 15 },
        { MENU_CMD_NAV, -1, -1, 6, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _jmp_SetExitMenuFlag_2 },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_Contest contestMenu = {
    { _ContestMenuInit, _ContestMenuOnReturn, 0, 7 },
    MENU_MARK_BYTE_COORDS,
    { 0, 0, 90, 14 },
    { MENU_CMD_COLORS, 0x79 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    { 91, 0, 122, 14 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    { -42, 0, 90, 14 },
    { MENU_CMD_COLORS, 0x79 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    { 0, 0, 304, 15 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_PTR, 0x0, aSeason_1 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { 0, -94, 100, 12 },
    { MENU_CMD_NAV, -1, 7, 17, 5 },
    { MENU_CMD_UP_SKIP, 4, 2 },
    { MENU_CMD_COLORS, 0x8 },
    { MENU_CMD_TEXT_PTR, 0x0, aTransfers },
    { MENU_CMD_ONSELECT_FIRE, _TransfersMenu },
    MENU_CMD_END,
    { 0, -81, 100, 12 },
    { MENU_CMD_NAV, -1, 8, 4, 6 },
    { MENU_CMD_UP_SKIP, 5, 2 },
    { MENU_CMD_RIGHT_SKIP, 6, 1 },
    { MENU_CMD_COLORS, 0xB },
    { MENU_CMD_TEXT_PTR, 0x0, aJobOffers },
    { MENU_CMD_ONSELECT_FIRE, _JobOffersMenu },
    MENU_CMD_END,
    { 0, -68, 100, 12 },
    { MENU_CMD_NAV, -1, 9, 5, -1 },
    { MENU_CMD_RIGHT_SKIP, 6, 1 },
    { MENU_CMD_UP_SKIP, 6, 2 },
    { MENU_CMD_COLORS, 0x8 },
    { MENU_CMD_TEXT_PTR, 0x0, aClubBusiness },
    { MENU_CMD_ONSELECT_FIRE, _ClubBusinessMenu },
    MENU_CMD_END,
    { 101, -100, 100, 18 },
    { MENU_CMD_NAV, 4, 10, 17, 8 },
    { MENU_CMD_UP_SKIP, 7, 3 },
    { MENU_CMD_DOWN_SKIP, 7, 3 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aPlayMatch },
    { MENU_CMD_ONSELECT_FIRE, _PlayMatchMenu },
    MENU_CMD_END,
    { 101, -81, 100, 12 },
    { MENU_CMD_NAV, 5, 11, 7, 13 },
    { MENU_CMD_LEFT_SKIP, 8, 0 },
    { MENU_CMD_COLORS, 0xC },
    { MENU_CMD_TEXT_PTR, 0x0, aSquad },
    { MENU_CMD_ONSELECT_FIRE, _SquadMenu },
    MENU_CMD_END,
    {
        { 101, -68, 100, 12 },
        { MENU_CMD_NAV, 6, 12, 8, -1 },
        { MENU_CMD_COLORS, 0xA },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _jmp_SetExitMenuFlag },
        MENU_CMD_END,
    },
    { -54, -94, 100, 12 },
    { MENU_CMD_NAV, 7, -1, 17, 11 },
    { MENU_CMD_DOWN_SKIP, 10, 3 },
    { MENU_CMD_COLORS, 0x4 },
    { MENU_CMD_TEXT_PTR, 0x0, aNextMatch },
    { MENU_CMD_ONSELECT_FIRE, _NextMatch },
    MENU_CMD_END,
    {
        { -54, -81, 100, 12 },
        { MENU_CMD_NAV, 8, -1, 10, 12 },
        { MENU_CMD_COLORS, 0x5 },
        { MENU_CMD_TEXT_PTR, 0x0, aViewCompetitio },
        { MENU_CMD_ONSELECT_FIRE, _ViewCompetitionsMenu },
        MENU_CMD_END,
    },
    { -54, -68, 100, 12 },
    { MENU_CMD_NAV, 9, -1, 11, -1 },
    { MENU_CMD_RIGHT_SKIP, 9, 3 },
    { MENU_CMD_COLORS, 0x4 },
    { MENU_CMD_TEXT_PTR, 0x0, aViewWorld },
    { MENU_CMD_ONSELECT_FIRE, _ViewWorldMenu },
    MENU_CMD_END,
    { 101, -68, 100, 11 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_NAV, 17, -1, 8, 14 },
    { MENU_CMD_COLORS, 0x4 },
    { MENU_CMD_TEXT_PTR, 0x0, aNextMatch },
    { MENU_CMD_ONSELECT_FIRE, _NextMatch },
    MENU_CMD_END,
    { 101, -55, 100, 11 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_NAV, 17, -1, 13, 15 },
    { MENU_CMD_COLORS, 0x5 },
    { MENU_CMD_TEXT_PTR, 0x0, aViewCompetitio },
    { MENU_CMD_ONSELECT_FIRE, _ViewCompetitionsMenu },
    MENU_CMD_END,
    { 101, -42, 100, 11 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_NAV, 17, -1, 14, 9 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_TEXT_PTR, 0x0, aExit },
    { MENU_CMD_ONSELECT_FIRE, _jmp_SetExitMenuFlag },
    MENU_CMD_END,
    { 0, 16, 8, 14 },
    { MENU_CMD_NAV, -1, 7, -1, 17 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_SPRITE_FG, 183 },
    { MENU_CMD_ONSELECT_FIRE, _ContestMenuScrollUp },
    MENU_CMD_END,
    { 0, -109, 8, 14 },
    { MENU_CMD_NAV, -1, 7, 16, 4 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_SPRITE_FG, 184 },
    { MENU_CMD_ONSELECT_FIRE, _ContestMenuScrollDown },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    {
        { 0, 31, 304, 116 },
        { MENU_CMD_COLORS, 0x0 },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 113 },
    MENU_MARK_START_TEMPLATE,
    MENU_MARK_BYTE_COORDS,
    { 0, 0, 0, 0 },
    { MENU_CMD_BEFORE_DRAW, _CareerGameListCommon },
    MENU_CMD_END,
    { {
          { 0, 0, 0, 0 },
          MENU_CMD_END,
      },
        {
            { 0, 0, 0, 0 },
            MENU_CMD_END,
        },
        {
            { 0, 0, 0, 0 },
            MENU_CMD_END,
        },
        {
            { 0, 0, 0, 0 },
            MENU_CMD_END,
        },
        {
            { 0, 0, 0, 0 },
            MENU_CMD_END,
        },
        {
            { 0, 0, 0, 0 },
            MENU_CMD_END,
        },
        {
            { 0, 0, 0, 0 },
            MENU_CMD_END,
        },
        {
            { 0, 0, 0, 0 },
            MENU_CMD_END,
        },
        {
            { 0, 0, 0, 0 },
            MENU_CMD_END,
        },
        {
            { 0, 0, 0, 0 },
            MENU_CMD_END,
        },
        {
            { 0, 0, 0, 0 },
            MENU_CMD_END,
        },
        {
            { 0, 0, 0, 0 },
            MENU_CMD_END,
        },
        {
            { 0, 0, 0, 0 },
            MENU_CMD_END,
        },
        {
            { 0, 0, 0, 0 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    MENU_MARK_WIDE_COORDS,
    {
        { 0, 0, 304, 8 },
        { MENU_CMD_COLORS, 0xA },
        MENU_CMD_END,
    },
    MENU_MARK_BYTE_COORDS,
    {
        { 0, 0, 62, 6 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { 0, 8, 62, 6 },
        { MENU_CMD_TEXT_NO_PTR, 0x8000 },
        MENU_CMD_END,
    },
    {
        { 70, 0, 12, 6 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 84, 0, -37, 8 },
        { MENU_CMD_COLORS, 0xB },
        MENU_CMD_END,
    },
    {
        { 84, 0, 96, 6 },
        { MENU_CMD_TEXT_PTR, 0x4000, 0 },
        MENU_CMD_END,
    },
    {
        { -76, 0, 12, 6 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -65, 0, 6, 6 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -60, 0, 12, 6 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -48, 0, 96, 6 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { 84, 8, -36, 6 },
        { MENU_CMD_TEXT_NO_PTR, 0x1 },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_ManagementRecord managementRecordMenu = {
    { _ManagementRecordInit, 0, 0, 0x3 },
    { 27, -25, 250, 15 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_BEFORE_DRAW, _FillManagementRecordInfo },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { 0, 0, 8, 14 },
    { MENU_CMD_NAV, -1, 3, -1, 2 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_SPRITE_FG, 183 },
    { MENU_CMD_ONSELECT_FIRE, _ManagementArrowUpSelect },
    MENU_CMD_END,
    { 0, -70, 8, 14 },
    { MENU_CMD_NAV, -1, 3, 1, 3 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_SPRITE_FG, 184 },
    { MENU_CMD_ONSELECT_FIRE, _ManagementArrowDownSelect },
    MENU_CMD_END,
    {
        { 102, -46, 100, 15 },
        { MENU_CMD_NAV, 2, -1, 2, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _jmp_SetExitMenuFlag_3 },
        MENU_CMD_END,
    },
    { {
          { -104, 0, 0, 6 },
          { MENU_CMD_TEXT_NO_PTR, 0x10 },
          MENU_CMD_END,
      },
        {
            { -104, 0, 0, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x7 },
            MENU_CMD_END,
        },
        {
            { -104, 0, 0, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x11 },
            MENU_CMD_END,
        },
        {
            { 0, 0, -112, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x4001 },
            MENU_CMD_END,
        },
        {
            { -96, 0, -112, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    MENU_MARK_WIDE_COORDS,
    { 0, 0, 304, 6 },
    { MENU_CMD_TEXT_PTR, 0xA, aSacked },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    {
        { -104, 0, 0, 6 },
        { MENU_CMD_TEXT_PTR, 0x7, aAllMatches },
        MENU_CMD_END,
    },
    {
        { -104, 0, 0, 6 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { 0, 0, 304, 6 },
    { MENU_CMD_TEXT_PTR, 0x7, 0 },
    MENU_CMD_END,
    { {
          { 0, 0, 304, 6 },
          { MENU_CMD_TEXT_NO_PTR, 0x1 },
          MENU_CMD_END,
      },
        {
            { 0, 0, 304, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x1 },
            MENU_CMD_END,
        } },
    MENU_MARK_BYTE_COORDS,
    {
        { -104, -68, 0, 6 },
        { MENU_CMD_TEXT_PTR, 0x1D, aCareerTotal },
        MENU_CMD_END,
    },
    {
        { -104, -58, 0, 6 },
        { MENU_CMD_TEXT_NO_PTR, 0xD },
        MENU_CMD_END,
    },
    { {
          { -104, -122, 20, 12 },
          { MENU_CMD_SPRITE_FG, 0 },
          MENU_CMD_END,
      },
        {
            { -104, -122, 20, 12 },
            { MENU_CMD_SPRITE_FG, 0 },
            MENU_CMD_END,
        },
        {
            { -104, -122, 20, 12 },
            { MENU_CMD_SPRITE_FG, 0 },
            MENU_CMD_END,
        },
        {
            { -104, -122, 20, 12 },
            { MENU_CMD_SPRITE_FG, 0 },
            MENU_CMD_END,
        } },
    MENU_MARK_END_MENU,
};
Menu_SelectTeamsForTactics selectTeamsForTacticsMenu = {
    { _SelTeamForTacticsInit, _SelTeamForTacticsInit, 0, 23 },
    MENU_MARK_BYTE_COORDS,
    {
        { 77, 0, -106, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, -1 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x5 },
    { MENU_CMD_TEXT_PTR, 0x0, 0 },
    { MENU_CMD_ONSELECT_FIRE, _SelTeamForTacticsCommon },
    MENU_CMD_END,
    { {
          { 1, 21, 100, 8 },
          { MENU_CMD_NAV, -1, 23, -1, 2 },
          { MENU_CMD_LEFT_SKIP, 44, 2 },
          MENU_CMD_END,
      },
        {
            { 1, 30, 100, 8 },
            { MENU_CMD_NAV, -1, 24, 1, 3 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 39, 100, 8 },
            { MENU_CMD_NAV, -1, 25, 2, 4 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 48, 100, 8 },
            { MENU_CMD_NAV, -1, 26, 3, 5 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 57, 100, 8 },
            { MENU_CMD_NAV, -1, 27, 4, 6 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 66, 100, 8 },
            { MENU_CMD_NAV, -1, 28, 5, 7 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 75, 100, 8 },
            { MENU_CMD_NAV, -1, 29, 6, 8 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 84, 100, 8 },
            { MENU_CMD_NAV, -1, 30, 7, 9 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 93, 100, 8 },
            { MENU_CMD_NAV, -1, 31, 8, 10 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 102, 100, 8 },
            { MENU_CMD_NAV, -1, 32, 9, 11 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 111, 100, 8 },
            { MENU_CMD_NAV, -1, 33, 10, 12 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, 120, 100, 8 },
            { MENU_CMD_NAV, -1, 34, 11, 13 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -127, 100, 8 },
            { MENU_CMD_NAV, -1, 35, 12, 14 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -118, 100, 8 },
            { MENU_CMD_NAV, -1, 36, 13, 15 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -109, 100, 8 },
            { MENU_CMD_NAV, -1, 37, 14, 16 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -100, 100, 8 },
            { MENU_CMD_NAV, -1, 38, 15, 17 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -91, 100, 8 },
            { MENU_CMD_NAV, -1, 39, 16, 18 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -82, 100, 8 },
            { MENU_CMD_NAV, -1, 40, 17, 19 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -73, 100, 8 },
            { MENU_CMD_NAV, -1, 41, 18, 20 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -64, 100, 8 },
            { MENU_CMD_NAV, -1, 42, 19, 21 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -55, 100, 8 },
            { MENU_CMD_NAV, -1, 43, 20, 22 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        },
        {
            { 1, -46, 100, 8 },
            { MENU_CMD_NAV, -1, 44, 21, 67 },
            { MENU_CMD_UP_SKIP, 22, 2 },
            MENU_CMD_END,
        } },
    { {
          { 102, 21, 100, 8 },
          { MENU_CMD_NAV, 1, 45, -1, 24 },
          MENU_CMD_END,
      },
        {
            { 102, 30, 100, 8 },
            { MENU_CMD_NAV, 2, 46, 23, 25 },
            MENU_CMD_END,
        },
        {
            { 102, 39, 100, 8 },
            { MENU_CMD_NAV, 3, 47, 24, 26 },
            MENU_CMD_END,
        },
        {
            { 102, 48, 100, 8 },
            { MENU_CMD_NAV, 4, 48, 25, 27 },
            MENU_CMD_END,
        },
        {
            { 102, 57, 100, 8 },
            { MENU_CMD_NAV, 5, 49, 26, 28 },
            MENU_CMD_END,
        },
        {
            { 102, 66, 100, 8 },
            { MENU_CMD_NAV, 6, 50, 27, 29 },
            MENU_CMD_END,
        },
        {
            { 102, 75, 100, 8 },
            { MENU_CMD_NAV, 7, 51, 28, 30 },
            MENU_CMD_END,
        },
        {
            { 102, 84, 100, 8 },
            { MENU_CMD_NAV, 8, 52, 29, 31 },
            MENU_CMD_END,
        },
        {
            { 102, 93, 100, 8 },
            { MENU_CMD_NAV, 9, 53, 30, 32 },
            MENU_CMD_END,
        },
        {
            { 102, 102, 100, 8 },
            { MENU_CMD_NAV, 10, 54, 31, 33 },
            MENU_CMD_END,
        },
        {
            { 102, 111, 100, 8 },
            { MENU_CMD_NAV, 11, 55, 32, 34 },
            MENU_CMD_END,
        },
        {
            { 102, 120, 100, 8 },
            { MENU_CMD_NAV, 12, 56, 33, 35 },
            MENU_CMD_END,
        },
        {
            { 102, -127, 100, 8 },
            { MENU_CMD_NAV, 13, 57, 34, 36 },
            MENU_CMD_END,
        },
        {
            { 102, -118, 100, 8 },
            { MENU_CMD_NAV, 14, 58, 35, 37 },
            MENU_CMD_END,
        },
        {
            { 102, -109, 100, 8 },
            { MENU_CMD_NAV, 15, 59, 36, 38 },
            MENU_CMD_END,
        },
        {
            { 102, -100, 100, 8 },
            { MENU_CMD_NAV, 16, 60, 37, 39 },
            MENU_CMD_END,
        },
        {
            { 102, -91, 100, 8 },
            { MENU_CMD_NAV, 17, 61, 38, 40 },
            MENU_CMD_END,
        },
        {
            { 102, -82, 100, 8 },
            { MENU_CMD_NAV, 18, 62, 39, 41 },
            MENU_CMD_END,
        },
        {
            { 102, -73, 100, 8 },
            { MENU_CMD_NAV, 19, 63, 40, 42 },
            MENU_CMD_END,
        },
        {
            { 102, -64, 100, 8 },
            { MENU_CMD_NAV, 20, 64, 41, 43 },
            MENU_CMD_END,
        },
        {
            { 102, -55, 100, 8 },
            { MENU_CMD_NAV, 21, 65, 42, 44 },
            MENU_CMD_END,
        },
        {
            { 102, -46, 100, 8 },
            { MENU_CMD_NAV, 22, 66, 43, 67 },
            MENU_CMD_END,
        } },
    { {
          { -53, 21, 100, 8 },
          { MENU_CMD_NAV, 23, -1, -1, 46 },
          { MENU_CMD_LEFT_SKIP, 44, 2 },
          MENU_CMD_END,
      },
        {
            { -53, 30, 100, 8 },
            { MENU_CMD_NAV, 24, -1, 45, 47 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 39, 100, 8 },
            { MENU_CMD_NAV, 25, -1, 46, 48 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 48, 100, 8 },
            { MENU_CMD_NAV, 26, -1, 47, 49 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 57, 100, 8 },
            { MENU_CMD_NAV, 27, -1, 48, 50 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 66, 100, 8 },
            { MENU_CMD_NAV, 28, -1, 49, 51 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 75, 100, 8 },
            { MENU_CMD_NAV, 29, -1, 50, 52 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 84, 100, 8 },
            { MENU_CMD_NAV, 30, -1, 51, 53 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 93, 100, 8 },
            { MENU_CMD_NAV, 31, -1, 52, 54 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 102, 100, 8 },
            { MENU_CMD_NAV, 32, -1, 53, 55 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 111, 100, 8 },
            { MENU_CMD_NAV, 33, -1, 54, 56 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, 120, 100, 8 },
            { MENU_CMD_NAV, 34, -1, 55, 57 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -127, 100, 8 },
            { MENU_CMD_NAV, 35, -1, 56, 58 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -118, 100, 8 },
            { MENU_CMD_NAV, 36, -1, 57, 59 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -109, 100, 8 },
            { MENU_CMD_NAV, 37, -1, 58, 60 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -100, 100, 8 },
            { MENU_CMD_NAV, 38, -1, 59, 61 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -91, 100, 8 },
            { MENU_CMD_NAV, 39, -1, 60, 62 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -82, 100, 8 },
            { MENU_CMD_NAV, 40, -1, 61, 63 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -73, 100, 8 },
            { MENU_CMD_NAV, 41, -1, 62, 64 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -64, 100, 8 },
            { MENU_CMD_NAV, 42, -1, 63, 65 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -55, 100, 8 },
            { MENU_CMD_NAV, 43, -1, 64, 66 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        },
        {
            { -53, -46, 100, 8 },
            { MENU_CMD_NAV, 44, -1, 65, 67 },
            { MENU_CMD_DOWN_SKIP, 66, 2 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 0 },
    {
        { 121, -71, 62, 15 },
        { MENU_CMD_NAV, 22, 66, 44, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _SelTeamForTacticsExit },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_SelectTeams selectTeamsMenu = {
    { _SelectTeamsInit, _SelectTeamsReinit, 0, 0x2B },
    { 2, 0, 300, 15 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_START_TEMPLATE,
    MENU_MARK_BYTE_COORDS,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, 0 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_ONSELECT_FIRE, _SelectLeaguesCommon },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 0, 30 },
    { {
          { 1, 0, -106, 8 },
          { MENU_CMD_NAV, -1, 2, 20, 3 },
          MENU_CMD_END,
      },
        {
            { -104, 0, -105, 8 },
            { MENU_CMD_NAV, 1, -1, 20, 4 },
            MENU_CMD_END,
        },
        {
            { 1, 9, -106, 8 },
            { MENU_CMD_NAV, -1, 4, 1, 5 },
            MENU_CMD_END,
        },
        {
            { -104, 9, -105, 8 },
            { MENU_CMD_NAV, 3, -1, 2, 6 },
            MENU_CMD_END,
        },
        {
            { 1, 18, -106, 8 },
            { MENU_CMD_NAV, -1, 6, 3, 7 },
            MENU_CMD_END,
        },
        {
            { -104, 18, -105, 8 },
            { MENU_CMD_NAV, 5, -1, 4, 8 },
            MENU_CMD_END,
        },
        {
            { 1, 27, -106, 8 },
            { MENU_CMD_NAV, -1, 8, 5, 9 },
            MENU_CMD_END,
        },
        {
            { -104, 27, -105, 8 },
            { MENU_CMD_NAV, 7, -1, 6, 10 },
            MENU_CMD_END,
        },
        {
            { 1, 36, -106, 8 },
            { MENU_CMD_NAV, -1, 10, 7, 21 },
            MENU_CMD_END,
        },
        {
            { -104, 36, -105, 8 },
            { MENU_CMD_NAV, 9, -1, 8, 65 },
            MENU_CMD_END,
        } },
    { MENU_MARK_START_MENU, 0, 30 },
    { {
          { 67, 0, -86, 8 },
          { MENU_CMD_NAV, -1, -1, -1, 12 },
          MENU_CMD_END,
      },
        {
            { 67, 9, -86, 8 },
            { MENU_CMD_NAV, -1, -1, 11, 13 },
            MENU_CMD_END,
        },
        {
            { 67, 18, -86, 8 },
            { MENU_CMD_NAV, -1, -1, 12, 14 },
            MENU_CMD_END,
        },
        {
            { 67, 27, -86, 8 },
            { MENU_CMD_NAV, -1, -1, 13, 15 },
            MENU_CMD_END,
        },
        {
            { 67, 36, -86, 8 },
            { MENU_CMD_NAV, -1, -1, 14, 16 },
            MENU_CMD_END,
        },
        {
            { 67, 45, -86, 8 },
            { MENU_CMD_NAV, -1, -1, 15, 17 },
            MENU_CMD_END,
        },
        {
            { 67, 54, -86, 8 },
            { MENU_CMD_NAV, -1, -1, 16, 18 },
            MENU_CMD_END,
        },
        {
            { 67, 63, -86, 8 },
            { MENU_CMD_NAV, -1, -1, 17, 19 },
            MENU_CMD_END,
        },
        {
            { 67, 72, -86, 8 },
            { MENU_CMD_NAV, -1, -1, 18, 20 },
            MENU_CMD_END,
        },
        {
            { 67, 81, -86, 8 },
            { MENU_CMD_NAV, -1, -1, 19, 43 },
            MENU_CMD_END,
        } },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x4 },
    { MENU_CMD_TEXT_PTR, 0x0, 0 },
    { MENU_CMD_ONSELECT_FIRE, _SelectTeamsCommon },
    MENU_CMD_END,
    { 1, 21, 100, 8 },
    { MENU_CMD_NAV, -1, 43, 9, 22 },
    { MENU_CMD_LEFT_SKIP, 64, 2 },
    { MENU_CMD_RIGHT_SKIP, 43, 3 },
    MENU_CMD_END,
    { 1, 30, 100, 8 },
    { MENU_CMD_NAV, -1, 44, 21, 23 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, 39, 100, 8 },
    { MENU_CMD_NAV, -1, 45, 22, 24 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, 48, 100, 8 },
    { MENU_CMD_NAV, -1, 46, 23, 25 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, 57, 100, 8 },
    { MENU_CMD_NAV, -1, 47, 24, 26 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, 66, 100, 8 },
    { MENU_CMD_NAV, -1, 48, 25, 27 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, 75, 100, 8 },
    { MENU_CMD_NAV, -1, 49, 26, 28 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, 84, 100, 8 },
    { MENU_CMD_NAV, -1, 50, 27, 29 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, 93, 100, 8 },
    { MENU_CMD_NAV, -1, 51, 28, 30 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, 102, 100, 8 },
    { MENU_CMD_NAV, -1, 52, 29, 31 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, 111, 100, 8 },
    { MENU_CMD_NAV, -1, 53, 30, 32 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, 120, 100, 8 },
    { MENU_CMD_NAV, -1, 54, 31, 33 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, -127, 100, 8 },
    { MENU_CMD_NAV, -1, 55, 32, 34 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, -118, 100, 8 },
    { MENU_CMD_NAV, -1, 56, 33, 35 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, -109, 100, 8 },
    { MENU_CMD_NAV, -1, 57, 34, 36 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, -100, 100, 8 },
    { MENU_CMD_NAV, -1, 58, 35, 37 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, -91, 100, 8 },
    { MENU_CMD_NAV, -1, 59, 36, 38 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, -82, 100, 8 },
    { MENU_CMD_NAV, -1, 60, 37, 39 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, -73, 100, 8 },
    { MENU_CMD_NAV, -1, 61, 38, 40 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, -64, 100, 8 },
    { MENU_CMD_NAV, -1, 62, 39, 41 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, -55, 100, 8 },
    { MENU_CMD_NAV, -1, 63, 40, 42 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { 1, -46, 100, 8 },
    { MENU_CMD_NAV, -1, 64, 41, 87 },
    { MENU_CMD_UP_SKIP, 42, 2 },
    MENU_CMD_END,
    { {
          { 102, 21, 100, 8 },
          { MENU_CMD_NAV, 21, 65, 9, 44 },
          MENU_CMD_END,
      },
        {
            { 102, 30, 100, 8 },
            { MENU_CMD_NAV, 22, 66, 43, 45 },
            MENU_CMD_END,
        },
        {
            { 102, 39, 100, 8 },
            { MENU_CMD_NAV, 23, 67, 44, 46 },
            MENU_CMD_END,
        },
        {
            { 102, 48, 100, 8 },
            { MENU_CMD_NAV, 24, 68, 45, 47 },
            MENU_CMD_END,
        },
        {
            { 102, 57, 100, 8 },
            { MENU_CMD_NAV, 25, 69, 46, 48 },
            MENU_CMD_END,
        },
        {
            { 102, 66, 100, 8 },
            { MENU_CMD_NAV, 26, 70, 47, 49 },
            MENU_CMD_END,
        },
        {
            { 102, 75, 100, 8 },
            { MENU_CMD_NAV, 27, 71, 48, 50 },
            MENU_CMD_END,
        },
        {
            { 102, 84, 100, 8 },
            { MENU_CMD_NAV, 28, 72, 49, 51 },
            MENU_CMD_END,
        },
        {
            { 102, 93, 100, 8 },
            { MENU_CMD_NAV, 29, 73, 50, 52 },
            MENU_CMD_END,
        },
        {
            { 102, 102, 100, 8 },
            { MENU_CMD_NAV, 30, 74, 51, 53 },
            MENU_CMD_END,
        },
        {
            { 102, 111, 100, 8 },
            { MENU_CMD_NAV, 31, 75, 52, 54 },
            MENU_CMD_END,
        },
        {
            { 102, 120, 100, 8 },
            { MENU_CMD_NAV, 32, 76, 53, 55 },
            MENU_CMD_END,
        },
        {
            { 102, -127, 100, 8 },
            { MENU_CMD_NAV, 33, 77, 54, 56 },
            MENU_CMD_END,
        },
        {
            { 102, -118, 100, 8 },
            { MENU_CMD_NAV, 34, 78, 55, 57 },
            MENU_CMD_END,
        },
        {
            { 102, -109, 100, 8 },
            { MENU_CMD_NAV, 35, 79, 56, 58 },
            MENU_CMD_END,
        },
        {
            { 102, -100, 100, 8 },
            { MENU_CMD_NAV, 36, 80, 57, 59 },
            MENU_CMD_END,
        },
        {
            { 102, -91, 100, 8 },
            { MENU_CMD_NAV, 37, 81, 58, 60 },
            MENU_CMD_END,
        },
        {
            { 102, -82, 100, 8 },
            { MENU_CMD_NAV, 38, 82, 59, 61 },
            MENU_CMD_END,
        },
        {
            { 102, -73, 100, 8 },
            { MENU_CMD_NAV, 39, 83, 60, 62 },
            MENU_CMD_END,
        },
        {
            { 102, -64, 100, 8 },
            { MENU_CMD_NAV, 40, 84, 61, 63 },
            MENU_CMD_END,
        },
        {
            { 102, -55, 100, 8 },
            { MENU_CMD_NAV, 41, 85, 62, 64 },
            MENU_CMD_END,
        },
        {
            { 102, -46, 100, 8 },
            { MENU_CMD_NAV, 42, 86, 63, 88 },
            MENU_CMD_END,
        } },
    { -53, 21, 100, 8 },
    { MENU_CMD_NAV, 43, -1, 10, 66 },
    { MENU_CMD_LEFT_SKIP, 64, 2 },
    { MENU_CMD_RIGHT_SKIP, 43, 3 },
    MENU_CMD_END,
    { -53, 30, 100, 8 },
    { MENU_CMD_NAV, 44, -1, 65, 67 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, 39, 100, 8 },
    { MENU_CMD_NAV, 45, -1, 66, 68 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, 48, 100, 8 },
    { MENU_CMD_NAV, 46, -1, 67, 69 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, 57, 100, 8 },
    { MENU_CMD_NAV, 47, -1, 68, 70 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, 66, 100, 8 },
    { MENU_CMD_NAV, 48, -1, 69, 71 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, 75, 100, 8 },
    { MENU_CMD_NAV, 49, -1, 70, 72 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, 84, 100, 8 },
    { MENU_CMD_NAV, 50, -1, 71, 73 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, 93, 100, 8 },
    { MENU_CMD_NAV, 51, -1, 72, 74 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, 102, 100, 8 },
    { MENU_CMD_NAV, 52, -1, 73, 75 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, 111, 100, 8 },
    { MENU_CMD_NAV, 53, -1, 74, 76 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, 120, 100, 8 },
    { MENU_CMD_NAV, 54, -1, 75, 77 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, -127, 100, 8 },
    { MENU_CMD_NAV, 55, -1, 76, 78 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, -118, 100, 8 },
    { MENU_CMD_NAV, 56, -1, 77, 79 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, -109, 100, 8 },
    { MENU_CMD_NAV, 57, -1, 78, 80 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, -100, 100, 8 },
    { MENU_CMD_NAV, 58, -1, 79, 81 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, -91, 100, 8 },
    { MENU_CMD_NAV, 59, -1, 80, 82 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, -82, 100, 8 },
    { MENU_CMD_NAV, 60, -1, 81, 83 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, -73, 100, 8 },
    { MENU_CMD_NAV, 61, -1, 82, 84 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, -64, 100, 8 },
    { MENU_CMD_NAV, 62, -1, 83, 85 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, -55, 100, 8 },
    { MENU_CMD_NAV, 63, -1, 84, 86 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    { -53, -46, 100, 8 },
    { MENU_CMD_NAV, 64, -1, 85, 89 },
    { MENU_CMD_DOWN_SKIP, 86, 2 },
    MENU_CMD_END,
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 0 },
    { 0, -71, 120, 15 },
    { MENU_CMD_NAV, -1, 88, 42, -1 },
    { MENU_CMD_UP_SKIP, 87, 2 },
    { MENU_CMD_RIGHT_SKIP, 87, 1 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aViewSelectedTe },
    { MENU_CMD_ONSELECT_FIRE, _ViewSelectedTeams },
    MENU_CMD_END,
    {
        { 121, -71, 62, 15 },
        { MENU_CMD_NAV, 87, 89, 64, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _ExitSelectTeams },
        MENU_CMD_END,
    },
    { -72, -71, 120, 15 },
    { MENU_CMD_NAV, 88, -1, 86, -1 },
    { MENU_CMD_DOWN_SKIP, 89, 2 },
    { MENU_CMD_RIGHT_SKIP, 89, 0 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _ChooseTeamsPlayGame },
    MENU_CMD_END,
    { -72, -68, 120, 9 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_ChooseTeams chooseTeamsMenu = {
    { _ChooseTeamsInit, _ChooseTeamsInit, 0, 0 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 35, 120, 25 },
        { MENU_CMD_NAV, -1, -1, -1, 1 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aClubTeams },
        { MENU_CMD_ONSELECT_FIRE, _ClubTeams },
        MENU_CMD_END,
    },
    {
        { 92, 70, 120, 25 },
        { MENU_CMD_NAV, -1, -1, 0, 2 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aNationalTeams },
        { MENU_CMD_ONSELECT_FIRE, _NationalTeams },
        MENU_CMD_END,
    },
    {
        { 92, 105, 120, 25 },
        { MENU_CMD_NAV, -1, -1, 1, 3 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aOriginalCustom },
        { MENU_CMD_ONSELECT_FIRE, _OriginalCustomTeams },
        MENU_CMD_END,
    },
    {
        { 92, -116, 120, 25 },
        { MENU_CMD_NAV, -1, -1, 2, 4 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aEditedCustomTe_0 },
        { MENU_CMD_ONSELECT_FIRE, _EditedCustomTeams },
        MENU_CMD_END,
    },
    {
        { 102, -71, 100, 15 },
        { MENU_CMD_NAV, -1, -1, 3, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _ExitChooseTeams },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { 0, 0, 304, 15 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_SelectFiles selectFilesMenu = {
    { _SelectFilesInit, _SelectFilesOnReturn, 0, 0x10 },
    { 2, 0, 300, 15 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_PTR, 0x0, 0 },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 0, -1 },
    MENU_MARK_START_TEMPLATE,
    MENU_MARK_BYTE_COORDS,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x6 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _SelFilesOnSelectCommon },
    { MENU_CMD_BEFORE_DRAW, _SelFilesDrawItemDescription },
    MENU_CMD_END,
    {
        { 0, 21, 56, 8 },
        { MENU_CMD_NAV, -1, 16, -1, 2 },
        MENU_CMD_END,
    },
    { 0, 30, 56, 8 },
    { MENU_CMD_NAV, -1, 17, 1, 3 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { 0, 39, 56, 8 },
    { MENU_CMD_NAV, -1, 18, 2, 4 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { 0, 48, 56, 8 },
    { MENU_CMD_NAV, -1, 19, 3, 5 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { 0, 57, 56, 8 },
    { MENU_CMD_NAV, -1, 20, 4, 6 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { 0, 66, 56, 8 },
    { MENU_CMD_NAV, -1, 21, 5, 7 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { 0, 75, 56, 8 },
    { MENU_CMD_NAV, -1, 22, 6, 8 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { 0, 84, 56, 8 },
    { MENU_CMD_NAV, -1, 23, 7, 9 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { 0, 93, 56, 8 },
    { MENU_CMD_NAV, -1, 24, 8, 10 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { 0, 102, 56, 8 },
    { MENU_CMD_NAV, -1, 25, 9, 11 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { 0, 111, 56, 8 },
    { MENU_CMD_NAV, -1, 26, 10, 12 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { 0, 120, 56, 8 },
    { MENU_CMD_NAV, -1, 27, 11, 13 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { 0, -127, 56, 8 },
    { MENU_CMD_NAV, -1, 28, 12, 14 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { 0, -118, 56, 8 },
    { MENU_CMD_NAV, -1, 29, 13, 15 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { 0, -109, 56, 8 },
    { MENU_CMD_NAV, -1, 30, 14, 48 },
    { MENU_CMD_UP_SKIP, 15, 2 },
    MENU_CMD_END,
    { {
          { 104, 21, 56, 8 },
          { MENU_CMD_NAV, 1, 31, -1, 17 },
          MENU_CMD_END,
      },
        {
            { 104, 30, 56, 8 },
            { MENU_CMD_NAV, 2, 32, 16, 18 },
            MENU_CMD_END,
        },
        {
            { 104, 39, 56, 8 },
            { MENU_CMD_NAV, 3, 33, 17, 19 },
            MENU_CMD_END,
        },
        {
            { 104, 48, 56, 8 },
            { MENU_CMD_NAV, 4, 34, 18, 20 },
            MENU_CMD_END,
        },
        {
            { 104, 57, 56, 8 },
            { MENU_CMD_NAV, 5, 35, 19, 21 },
            MENU_CMD_END,
        },
        {
            { 104, 66, 56, 8 },
            { MENU_CMD_NAV, 6, 36, 20, 22 },
            MENU_CMD_END,
        },
        {
            { 104, 75, 56, 8 },
            { MENU_CMD_NAV, 7, 37, 21, 23 },
            MENU_CMD_END,
        },
        {
            { 104, 84, 56, 8 },
            { MENU_CMD_NAV, 8, 38, 22, 24 },
            MENU_CMD_END,
        },
        {
            { 104, 93, 56, 8 },
            { MENU_CMD_NAV, 9, 39, 23, 25 },
            MENU_CMD_END,
        },
        {
            { 104, 102, 56, 8 },
            { MENU_CMD_NAV, 10, 40, 24, 26 },
            MENU_CMD_END,
        },
        {
            { 104, 111, 56, 8 },
            { MENU_CMD_NAV, 11, 41, 25, 27 },
            MENU_CMD_END,
        },
        {
            { 104, 120, 56, 8 },
            { MENU_CMD_NAV, 12, 42, 26, 28 },
            MENU_CMD_END,
        },
        {
            { 104, -127, 56, 8 },
            { MENU_CMD_NAV, 13, 43, 27, 29 },
            MENU_CMD_END,
        },
        {
            { 104, -118, 56, 8 },
            { MENU_CMD_NAV, 14, 44, 28, 30 },
            MENU_CMD_END,
        },
        {
            { 104, -109, 56, 8 },
            { MENU_CMD_NAV, 15, 45, 29, 48 },
            MENU_CMD_END,
        },
        {
            { -48, 21, 56, 8 },
            { MENU_CMD_NAV, 16, -1, -1, 32 },
            MENU_CMD_END,
        } },
    { -48, 30, 56, 8 },
    { MENU_CMD_NAV, 17, -1, 31, 33 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    { -48, 39, 56, 8 },
    { MENU_CMD_NAV, 18, -1, 32, 34 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    { -48, 48, 56, 8 },
    { MENU_CMD_NAV, 19, -1, 33, 35 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    { -48, 57, 56, 8 },
    { MENU_CMD_NAV, 20, -1, 34, 36 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    { -48, 66, 56, 8 },
    { MENU_CMD_NAV, 21, -1, 35, 37 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    { -48, 75, 56, 8 },
    { MENU_CMD_NAV, 22, -1, 36, 38 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    { -48, 84, 56, 8 },
    { MENU_CMD_NAV, 23, -1, 37, 39 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    { -48, 93, 56, 8 },
    { MENU_CMD_NAV, 24, -1, 38, 40 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    { -48, 102, 56, 8 },
    { MENU_CMD_NAV, 25, -1, 39, 41 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    { -48, 111, 56, 8 },
    { MENU_CMD_NAV, 26, -1, 40, 42 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    { -48, 120, 56, 8 },
    { MENU_CMD_NAV, 27, -1, 41, 43 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    { -48, -127, 56, 8 },
    { MENU_CMD_NAV, 28, -1, 42, 44 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    { -48, -118, 56, 8 },
    { MENU_CMD_NAV, 29, -1, 43, 45 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    { -48, -109, 56, 8 },
    { MENU_CMD_NAV, 30, -1, 44, 48 },
    { MENU_CMD_DOWN_SKIP, 45, 2 },
    MENU_CMD_END,
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 0 },
    { 0, 0, 39, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0x5 },
    { MENU_CMD_TEXT_PTR, 0x0, 0 },
    MENU_CMD_END,
    { 111, -104, 0, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_TEXT_PTR, 0x4000, aFilename },
    MENU_CMD_END,
    { 112, -104, 80, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_NAV, -1, -1, 30, 49 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _SelectFilesInputSaveFilenameOnSelect },
    MENU_CMD_END,
    { 121, -91, 62, 15 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_NAV, 15, 45, 48, 50 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aSave },
    { MENU_CMD_ONSELECT_FIRE, _SelectFilenameToSaveAndExit },
    MENU_CMD_END,
    {
        { 121, -71, 62, 15 },
        { MENU_CMD_NAV, 15, 45, 49, -1 },
        { MENU_CMD_COLORS, 0xA },
        { MENU_CMD_TEXT_PTR, 0x0, aAbort },
        { MENU_CMD_ONSELECT_FIRE, _AbortSaveFile },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_Continue continueMenu = {
    { _ContinueInit, 0, 0, 0x1 },
    { 2, 40, 300, 40 },
    { MENU_CMD_COLORS, 0xC },
    { MENU_CMD_TEXT_PTR, 0x10, 0 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { 102, -71, 100, 15 },
    { MENU_CMD_COLORS, 0xC },
    { MENU_CMD_TEXT_PTR, 0x0, aContinue },
    { MENU_CMD_ONSELECT_FIRE, _ContinueMenuContinueSelected },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_ContinueAbort continueAbortMenu = {
    { _ContinueAbortInit, 0, 0, 2 },
    MENU_MARK_BYTE_COORDS,
    {
        { -104, 0, 0, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    { 52, 50, -56, 40 },
    { MENU_CMD_COLORS, 0x4 },
    {
        MENU_CMD_MULTILINE,
        { 0x0 },
        0,
    },
    MENU_CMD_END,
    {
        { 102, -91, 100, 15 },
        { MENU_CMD_NAV, -1, -1, -1, 3 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        { MENU_CMD_ONSELECT_FIRE, _ContinueSelected },
        MENU_CMD_END,
    },
    {
        { 102, -71, 100, 15 },
        { MENU_CMD_NAV, -1, -1, 2, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        { MENU_CMD_ONSELECT_FIRE, _AbortSelected },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_SWOSMainMenu swosMainMenu = {
    { _SWOSMainMenuInit, _GlobalRetn, 0, 0x6 },
    { 88, -24, 128, 76 },
    { MENU_CMD_SPRITE_FG, 224 },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 0, 1 },
    MENU_MARK_BYTE_COORDS,
    {
        { 0, 64, -106, 16 },
        { MENU_CMD_NAV, -1, 6, -1, 2 },
        { MENU_CMD_COLORS, 0x3 },
        { MENU_CMD_TEXT_PTR, 0x10, aEditTactics },
        { MENU_CMD_ONSELECT_FIRE, _EditTacticsMenu },
        MENU_CMD_END,
    },
    {
        { 0, 84, -106, 16 },
        { MENU_CMD_NAV, -1, 7, 1, 3 },
        { MENU_CMD_COLORS, 0x8 },
        { MENU_CMD_TEXT_PTR, 0x10, aEditCustomTeam },
        { MENU_CMD_ONSELECT_FIRE, _EditCustomTeamMenu },
        MENU_CMD_END,
    },
    {
        { 0, 104, -106, 16 },
        { MENU_CMD_NAV, -1, 8, 2, 4 },
        { MENU_CMD_COLORS, 0xB },
        { MENU_CMD_TEXT_PTR, 0x10, aHighlights_0 },
        { MENU_CMD_ONSELECT_FIRE, _HighlightsMenu },
        MENU_CMD_END,
    },
    {
        { 0, 124, -106, 16 },
        { MENU_CMD_NAV, -1, 9, 3, 5 },
        { MENU_CMD_COLORS, 0x8 },
        { MENU_CMD_TEXT_PTR, 0x10, aOptions },
        { MENU_CMD_ONSELECT_FIRE, _OptionsMenuSelected },
        MENU_CMD_END,
    },
    {
        { 0, -112, -106, 16 },
        { MENU_CMD_NAV, -1, 10, 4, 11 },
        { MENU_CMD_COLORS, 0x3 },
        { MENU_CMD_TEXT_PTR, 0x10, aSaveDiskFiling },
        { MENU_CMD_ONSELECT_FIRE, _SaveDiskFilingMenu },
        MENU_CMD_END,
    },
    {
        { -102, 64, -106, 16 },
        { MENU_CMD_NAV, 1, -1, -1, 7 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x10, aFriendly_1 },
        { MENU_CMD_ONSELECT_FIRE, _FriendlyMenu },
        MENU_CMD_END,
    },
    {
        { -102, 84, -106, 16 },
        { MENU_CMD_NAV, 2, -1, 6, 8 },
        { MENU_CMD_COLORS, 0x4 },
        { MENU_CMD_TEXT_PTR, 0x10, aDiyCompetition_0 },
        { MENU_CMD_ONSELECT_FIRE, _DIYCompetitionMenu },
        MENU_CMD_END,
    },
    {
        { -102, 104, -106, 16 },
        { MENU_CMD_NAV, 3, -1, 7, 9 },
        { MENU_CMD_COLORS, 0x5 },
        { MENU_CMD_TEXT_PTR, 0x10, aPresetCompetit_0 },
        { MENU_CMD_ONSELECT_FIRE, _PresetCompetitionMenu },
        MENU_CMD_END,
    },
    {
        { -102, 124, -106, 16 },
        { MENU_CMD_NAV, 4, -1, 8, 10 },
        { MENU_CMD_COLORS, 0x4 },
        { MENU_CMD_TEXT_PTR, 0x10, aSeason_1 },
        { MENU_CMD_ONSELECT_FIRE, _SeasonMenu },
        MENU_CMD_END,
    },
    {
        { -102, -112, -106, 16 },
        { MENU_CMD_NAV, 5, -1, 9, 11 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x10, aCareer_1 },
        { MENU_CMD_ONSELECT_FIRE, _CareerMenu },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 0 },
    { 27, -88, -6, 14 },
    { MENU_CMD_NAV, -1, -1, 10, 12 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _StartContest },
    MENU_CMD_END,
    {
        { 27, -67, -6, 14 },
        { MENU_CMD_NAV, -1, -1, 11, 13 },
        { MENU_CMD_COLORS, 0xA },
        { MENU_CMD_TEXT_PTR, 0x0, (char*)saveFileTitle },
        { MENU_CMD_ONSELECT_FIRE, _SaveContest },
        MENU_CMD_END,
    },
    {
        { 27, -50, -6, 14 },
        { MENU_CMD_NAV, -1, -1, 12, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aLoadOldCompeti },
        { MENU_CMD_ONSELECT_FIRE, _LoadOldCompetitionMenu },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 0 },
    MENU_MARK_END_MENU,
};
Menu_Offers offersMenu = {
    { _OffersInit, _OffersInit, 0, 2 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aOffers },
        MENU_CMD_END,
    },
    {
        { 122, -71, 60, 15 },
        { MENU_CMD_NAV, -1, -1, 6, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _j_SetExitMenuFlag_11 },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 30 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _OffersCommon },
    { MENU_CMD_AFTER_DRAW, _OffersAfterDrawCommon },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 0, 304, 21 },
          { MENU_CMD_NAV, -1, -1, -1, 3 },
          MENU_CMD_END,
      },
        {
            { 0, 30, 304, 21 },
            { MENU_CMD_NAV, -1, -1, 2, 4 },
            MENU_CMD_END,
        },
        {
            { 0, 60, 304, 21 },
            { MENU_CMD_NAV, -1, -1, 3, 5 },
            MENU_CMD_END,
        },
        {
            { 0, 90, 304, 21 },
            { MENU_CMD_NAV, -1, -1, 4, 6 },
            MENU_CMD_END,
        },
        {
            { 0, 120, 304, 21 },
            { MENU_CMD_NAV, -1, -1, 5, 1 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 0 },
    { 0, 0, 304, 21 },
    { MENU_CMD_COLORS, 0x0 },
    MENU_CMD_END,
    { {
          { 15, 0, 289, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { 15, 10, 289, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    MENU_MARK_BYTE_COORDS,
    {
        { 0, 0, 15, 8 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_PlayerTransfer playerTransferMenu = {
    { _TransferPlayerInit, _TransferPlayerInit, 0, 0xB },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aResponseToOffe },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 32, 304, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { 0, 42, 304, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        },
        {
            { 0, 89, 304, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { 0, 156, 304, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_TEXT_NO_PTR, 0x8000 },
    MENU_CMD_END,
    { 0, 166, 304, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_TEXT_NO_PTR, 0x8000 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { {
          { -104, 65, 0, 8 },
          { MENU_CMD_TEXT_PTR, 0x0, aPlus },
          MENU_CMD_END,
      },
        {
            { -104, 110, 0, 8 },
            { MENU_CMD_TEXT_PTR, 0x0, aPlus },
            MENU_CMD_END,
        },
        {
            { -104, -126, 0, 8 },
            { MENU_CMD_TEXT_PTR, 0x0, aPlus },
            MENU_CMD_END,
        } },
    {
        { 112, 100, 80, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    { {
          { 31, -71, 120, 15 },
          { MENU_CMD_NAV, -1, 11, -1, -1 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aMakeImprovedOf },
          { MENU_CMD_ONSELECT_FIRE, _TransferPlayerOnSelect10 },
          MENU_CMD_END,
      },
        {
            { -104, -71, 120, 15 },
            { MENU_CMD_NAV, 10, -1, -1, -1 },
            { MENU_CMD_COLORS, 0xC },
            { MENU_CMD_TEXT_PTR, 0x0, aExit },
            { MENU_CMD_ONSELECT_FIRE, _TransferPlayerOnSelect11 },
            MENU_CMD_END,
        } },
    { 92, -71, 120, 15 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xC },
    { MENU_CMD_TEXT_PTR, 0x0, aContinue },
    { MENU_CMD_ONSELECT_FIRE, _TransferPlayerOnSelect12 },
    MENU_CMD_END,
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_AFTER_DRAW, _TransferPlayerAfterDrawCommon },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 55, 304, 8 },
          MENU_CMD_END,
      },
        {
            { 0, 75, 304, 8 },
            MENU_CMD_END,
        },
        {
            { 0, 120, 304, 8 },
            MENU_CMD_END,
        },
        {
            { 0, 140, 304, 8 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    {
        { 0, 0, 304, 8 },
        { MENU_CMD_COLORS, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_BYTE_COORDS,
    {
        { 2, 0, 10, 8 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 13, 0, 10, 8 },
        { MENU_CMD_SPRITE_FG, 0 },
        MENU_CMD_END,
    },
    {
        { 24, 0, 0, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { -47, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    {
        { -45, 0, 14, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -21, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0xF },
        MENU_CMD_END,
    },
    { {
          { -24, 0, 7, 7 },
          { MENU_CMD_SPRITE_FG, 181 },
          MENU_CMD_END,
      },
        {
            { -17, 0, 7, 7 },
            { MENU_CMD_SPRITE_FG, 182 },
            MENU_CMD_END,
        },
        {
            { -45, 0, 9, 8 },
            { MENU_CMD_SPRITE_FG, 189 },
            MENU_CMD_END,
        } },
    {
        { -13, 0, 30, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    {
        { 272, 0, 32, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_ThirdPartyOffer thirdPartyOfferMenu = {
    { _ThirdPartyOfferMenuInit, _ThirdPartyOfferMenuInit, 0, 13 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aThirdPartyOffe },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 32, 304, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { 0, 42, 304, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { 0, 42, 304, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_TEXT_NO_PTR, 0x8000 },
    MENU_CMD_END,
    {
        { 0, 89, 304, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x8000 },
        MENU_CMD_END,
    },
    MENU_MARK_BYTE_COORDS,
    { {
          { -104, 65, 0, 8 },
          { MENU_CMD_TEXT_PTR, 0x0, aPlus },
          MENU_CMD_END,
      },
        {
            { -104, 110, 0, 8 },
            { MENU_CMD_TEXT_PTR, 0x0, aPlus },
            MENU_CMD_END,
        },
        {
            { -104, -126, 0, 8 },
            { MENU_CMD_TEXT_PTR, 0x0, aPlus },
            MENU_CMD_END,
        } },
    {
        { 112, 100, 80, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    { {
          { 102, -103, 100, 15 },
          { MENU_CMD_NAV, 10, 12, -1, 11 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aViewSquad },
          { MENU_CMD_ONSELECT_FIRE, _ThirdPartyOfferViewSquad },
          MENU_CMD_END,
      },
        {
            { 0, -87, 101, 15 },
            { MENU_CMD_NAV, -1, 11, 9, 13 },
            { MENU_CMD_COLORS, 0xE },
            { MENU_CMD_TEXT_PTR, 0x0, aAcceptOffer },
            { MENU_CMD_ONSELECT_FIRE, _AcceptOffer },
            MENU_CMD_END,
        } },
    { 102, -87, 100, 15 },
    { MENU_CMD_NAV, 10, 12, 9, 13 },
    { MENU_CMD_RIGHT_SKIP, 11, 0 },
    { MENU_CMD_LEFT_SKIP, 11, 0 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aAskForMore },
    { MENU_CMD_ONSELECT_FIRE, _AskForMore },
    MENU_CMD_END,
    { {
          { -53, -87, 101, 15 },
          { MENU_CMD_NAV, 11, -1, 9, 13 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aRejectOffer },
          { MENU_CMD_ONSELECT_FIRE, _RejectOffer },
          MENU_CMD_END,
      },
        {
            { 102, -71, 100, 15 },
            { MENU_CMD_NAV, 10, 12, 11, -1 },
            { MENU_CMD_COLORS, 0x5 },
            { MENU_CMD_TEXT_PTR, 0x0, aConsider },
            { MENU_CMD_ONSELECT_FIRE, _ConsiderOffer },
            MENU_CMD_END,
        } },
    { 122, -71, 60, 15 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xC },
    { MENU_CMD_TEXT_PTR, 0x0, aExit },
    { MENU_CMD_ONSELECT_FIRE, _ThirdPartyOfferMenuExit },
    MENU_CMD_END,
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_AFTER_DRAW, _ThirdPartyOfferMenuAfterDrawCommon },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 55, 304, 8 },
          MENU_CMD_END,
      },
        {
            { 0, 75, 304, 8 },
            MENU_CMD_END,
        },
        {
            { 0, 120, 304, 8 },
            MENU_CMD_END,
        },
        {
            { 0, 140, 304, 8 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    {
        { 0, 0, 304, 8 },
        { MENU_CMD_COLORS, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_BYTE_COORDS,
    {
        { 2, 0, 10, 8 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 13, 0, 10, 8 },
        { MENU_CMD_SPRITE_FG, 0 },
        MENU_CMD_END,
    },
    {
        { 24, 0, 0, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { -47, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    {
        { -45, 0, 14, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -21, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0xF },
        MENU_CMD_END,
    },
    { {
          { -24, 0, 7, 7 },
          { MENU_CMD_SPRITE_FG, 181 },
          MENU_CMD_END,
      },
        {
            { -17, 0, 7, 7 },
            { MENU_CMD_SPRITE_FG, 182 },
            MENU_CMD_END,
        },
        {
            { -45, 0, 9, 8 },
            { MENU_CMD_SPRITE_FG, 189 },
            MENU_CMD_END,
        } },
    {
        { -13, 0, 30, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    {
        { 272, 0, 32, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_PlayerOffers playerOffersMenu = {
    { _PlayerOffersInit, _PlayerOffersInit, 0, 9 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aMakeOffer },
        MENU_CMD_END,
    },
    {
        { 92, 20, 120, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x10 },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    {
        { 0, 32, 304, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, aDearSirMadam },
        MENU_CMD_END,
    },
    { {
          { 0, 42, 304, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { 0, 89, 304, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    MENU_MARK_BYTE_COORDS,
    { {
          { -104, 65, 0, 8 },
          { MENU_CMD_TEXT_PTR, 0x0, aPlus },
          MENU_CMD_END,
      },
        {
            { -104, 110, 0, 8 },
            { MENU_CMD_TEXT_PTR, 0x0, aPlus },
            MENU_CMD_END,
        },
        {
            { -104, -126, 0, 8 },
            { MENU_CMD_TEXT_PTR, 0x0, aPlus },
            MENU_CMD_END,
        } },
    {
        { 0, 65, 14, 8 },
        { MENU_CMD_NAV, -1, 9, -1, 9 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aPlus },
        { MENU_CMD_ONSELECT_FIRE, _PlayerOffersOnSelect8 },
        MENU_CMD_END,
    },
    { 112, 100, 80, 8 },
    { MENU_CMD_NAV, 8, 10, 8, 11 },
    { MENU_CMD_COLORS, 0xB },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    { MENU_CMD_INPUT_FUNC, 0x3C1, _PlayerOffersAdjustOfferAmount },
    MENU_CMD_END,
    { {
          { -54, 100, 14, 8 },
          { MENU_CMD_NAV, 9, -1, 8, 11 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aPlus },
          { MENU_CMD_ONSELECT_FIRE, _PlayerOffersOnSelect10 },
          MENU_CMD_END,
      },
        {
            { 0, -126, 14, 8 },
            { MENU_CMD_NAV, -1, -1, 9, 12 },
            { MENU_CMD_COLORS, 0xE },
            { MENU_CMD_TEXT_PTR, 0x0, aPlus },
            { MENU_CMD_ONSELECT_FIRE, _PlayerOffersOnSelect11 },
            MENU_CMD_END,
        },
        {
            { 102, -91, 100, 15 },
            { MENU_CMD_NAV, -1, -1, 11, 13 },
            { MENU_CMD_COLORS, 0xE },
            { MENU_CMD_TEXT_PTR, 0x0, aConfirmOffer },
            { MENU_CMD_ONSELECT_FIRE, _PlayerOffersOnSelect12 },
            MENU_CMD_END,
        },
        {
            { 102, -71, 100, 15 },
            { MENU_CMD_NAV, -1, -1, 12, -1 },
            { MENU_CMD_COLORS, 0xC },
            { MENU_CMD_TEXT_PTR, 0x0, aExit },
            { MENU_CMD_ONSELECT_FIRE, _PlayerOffersOnSelect13 },
            MENU_CMD_END,
        } },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_AFTER_DRAW, _PlayerOffersAfterDrawCommon },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 55, 304, 8 },
          MENU_CMD_END,
      },
        {
            { 0, 75, 304, 8 },
            MENU_CMD_END,
        },
        {
            { 0, 120, 304, 8 },
            MENU_CMD_END,
        },
        {
            { 0, 140, 304, 8 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    {
        { 0, 0, 304, 8 },
        { MENU_CMD_COLORS, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_BYTE_COORDS,
    {
        { 2, 0, 10, 8 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 13, 0, 10, 8 },
        { MENU_CMD_SPRITE_FG, 0 },
        MENU_CMD_END,
    },
    {
        { 24, 0, 0, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { -47, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    {
        { -45, 0, 14, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -21, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0xF },
        MENU_CMD_END,
    },
    { {
          { -24, 0, 7, 7 },
          { MENU_CMD_SPRITE_FG, 181 },
          MENU_CMD_END,
      },
        {
            { -17, 0, 7, 7 },
            { MENU_CMD_SPRITE_FG, 182 },
            MENU_CMD_END,
        },
        {
            { -45, 0, 9, 8 },
            { MENU_CMD_SPRITE_FG, 189 },
            MENU_CMD_END,
        } },
    {
        { -13, 0, 30, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    {
        { 272, 0, 32, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_ViewSquad viewSquadMenu = {
    { _ViewSquadInit, 0, 0, 2 },
    MENU_MARK_BYTE_COORDS,
    {
        { 57, 0, -66, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 82, 16, -116, 10 },
        { MENU_CMD_COLORS, 0xA },
        { MENU_CMD_TEXT_PTR, 0x10, 0 },
        MENU_CMD_END,
    },
    {
        { 122, -71, 60, 15 },
        { MENU_CMD_NAV, -1, 3, 20, 3 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _ViewSquadExitContinue },
        MENU_CMD_END,
    },
    { 122, -71, 60, 15 },
    { MENU_CMD_NAV, -1, -1, 2, -1 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xC },
    { MENU_CMD_TEXT_PTR, 0x0, aContinue },
    { MENU_CMD_ONSELECT_FIRE, _ViewSquadExitContinue },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 0, 27 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_AFTER_DRAW, _ViewSquadAfterDrawCommon },
    { MENU_CMD_ONSELECT_FIRE, _ViewSquadCommon },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 0, 304, 8 },
          { MENU_CMD_NAV, -1, -1, -1, 5 },
          MENU_CMD_END,
      },
        {
            { 0, 9, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 4, 6 },
            MENU_CMD_END,
        },
        {
            { 0, 18, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 5, 7 },
            MENU_CMD_END,
        },
        {
            { 0, 27, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 6, 8 },
            MENU_CMD_END,
        },
        {
            { 0, 36, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 7, 9 },
            MENU_CMD_END,
        },
        {
            { 0, 45, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 8, 10 },
            MENU_CMD_END,
        },
        {
            { 0, 54, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 9, 11 },
            MENU_CMD_END,
        },
        {
            { 0, 63, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 10, 12 },
            MENU_CMD_END,
        },
        {
            { 0, 72, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 11, 13 },
            MENU_CMD_END,
        },
        {
            { 0, 81, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 12, 14 },
            MENU_CMD_END,
        },
        {
            { 0, 90, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 13, 15 },
            MENU_CMD_END,
        },
        {
            { 0, 101, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 14, 16 },
            MENU_CMD_END,
        },
        {
            { 0, 110, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 15, 17 },
            MENU_CMD_END,
        },
        {
            { 0, 119, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 16, 18 },
            MENU_CMD_END,
        },
        {
            { 0, 128, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 17, 19 },
            MENU_CMD_END,
        },
        {
            { 0, 137, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 18, 20 },
            MENU_CMD_END,
        } },
    { 23, 148, 281, 8 },
    { MENU_CMD_NAV, 21, -1, 19, 2 },
    { MENU_CMD_LEFT_SKIP, 22, 2 },
    { MENU_CMD_INVISIBLE },
    MENU_CMD_END,
    MENU_MARK_INIT_ENTRY,
    MENU_MARK_BYTE_COORDS,
    { 13, -108, 10, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_NAV, -1, 20, 19, 2 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _ViewSquadOnSelect21 },
    { MENU_CMD_AFTER_DRAW, _ViewSquad21AfterDraw },
    MENU_CMD_END,
    {
        { 0, 0, 0, 0 },
        { MENU_CMD_INVISIBLE },
        { MENU_CMD_NAV, -1, -1, 21, -1 },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 0 },
    MENU_MARK_WIDE_COORDS,
    { 0, 0, 304, 8 },
    { MENU_CMD_COLORS, 0x0 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    {
        { 2, 0, 10, 8 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 13, 0, 10, 8 },
        { MENU_CMD_SPRITE_FG, 0 },
        MENU_CMD_END,
    },
    {
        { 24, 0, 0, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { -47, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    {
        { -45, 0, 14, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -21, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0xF },
        MENU_CMD_END,
    },
    { {
          { -24, 0, 7, 7 },
          { MENU_CMD_SPRITE_FG, 181 },
          MENU_CMD_END,
      },
        {
            { -17, 0, 7, 7 },
            { MENU_CMD_SPRITE_FG, 182 },
            MENU_CMD_END,
        },
        {
            { -45, 0, 9, 8 },
            { MENU_CMD_SPRITE_FG, 189 },
            MENU_CMD_END,
        } },
    {
        { -13, 0, 30, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { 272, 0, 32, 8 },
    { MENU_CMD_TEXT_PTR, 0x8000, 0 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_BuyOtherPlayers buyOtherPlayersMenu = {
    { _BuyOtherPlayersInit, _BuyOtherPlayersFinish, 0, 2 },
    MENU_MARK_BYTE_COORDS,
    {
        { 67, 0, -86, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 82, 16, -116, 10 },
        { MENU_CMD_COLORS, 0xA },
        { MENU_CMD_TEXT_PTR, 0x10, 0 },
        MENU_CMD_END,
    },
    {
        { 122, -71, 60, 15 },
        { MENU_CMD_NAV, -1, -1, 18, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _j_SetExitMenuFlag_10 },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 27 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_AFTER_DRAW, _BuyOtherPlayersAfterDrawCommon },
    { MENU_CMD_ONSELECT_FIRE, _BuyOtherPlayersCommon },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 0, 304, 8 },
          { MENU_CMD_NAV, -1, -1, -1, 4 },
          MENU_CMD_END,
      },
        {
            { 0, 9, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 3, 5 },
            MENU_CMD_END,
        },
        {
            { 0, 18, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 4, 6 },
            MENU_CMD_END,
        },
        {
            { 0, 27, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 5, 7 },
            MENU_CMD_END,
        },
        {
            { 0, 36, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 6, 8 },
            MENU_CMD_END,
        },
        {
            { 0, 45, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 7, 9 },
            MENU_CMD_END,
        },
        {
            { 0, 54, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 8, 10 },
            MENU_CMD_END,
        },
        {
            { 0, 63, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 9, 11 },
            MENU_CMD_END,
        },
        {
            { 0, 72, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 10, 12 },
            MENU_CMD_END,
        },
        {
            { 0, 81, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 11, 13 },
            MENU_CMD_END,
        },
        {
            { 0, 90, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 12, 14 },
            MENU_CMD_END,
        },
        {
            { 0, 101, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 13, 15 },
            MENU_CMD_END,
        },
        {
            { 0, 110, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 14, 16 },
            MENU_CMD_END,
        },
        {
            { 0, 119, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 15, 17 },
            MENU_CMD_END,
        },
        {
            { 0, 128, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 16, 18 },
            MENU_CMD_END,
        },
        {
            { 0, 137, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 17, 2 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 0 },
    { 0, 0, 304, 8 },
    { MENU_CMD_COLORS, 0x0 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    {
        { 2, 0, 10, 8 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 13, 0, 10, 8 },
        { MENU_CMD_SPRITE_FG, 0 },
        MENU_CMD_END,
    },
    {
        { 24, 0, 0, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { -47, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    {
        { -45, 0, 14, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -21, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0xF },
        MENU_CMD_END,
    },
    { {
          { -24, 0, 7, 7 },
          { MENU_CMD_SPRITE_FG, 181 },
          MENU_CMD_END,
      },
        {
            { -17, 0, 7, 7 },
            { MENU_CMD_SPRITE_FG, 182 },
            MENU_CMD_END,
        },
        {
            { -45, 0, 9, 8 },
            { MENU_CMD_SPRITE_FG, 189 },
            MENU_CMD_END,
        } },
    {
        { -13, 0, 30, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { 272, 0, 32, 8 },
    { MENU_CMD_TEXT_PTR, 0x8000, 0 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_ShowPlayerMarket showPlayerMarketMenu = {
    { _ShowPlayerMarketInit, _ShowPlayerMarket, 0, 7 },
    MENU_MARK_BYTE_COORDS,
    {
        { 62, 0, -76, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 92, 20, 120, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x10 },
        MENU_CMD_END,
    },
    {
        { -104, 32, 0, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, aCriteria },
        MENU_CMD_END,
    },
    {
        { 8, 44, 59, 10 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aType },
        MENU_CMD_END,
    },
    {
        { 68, 44, 80, 10 },
        { MENU_CMD_NAV, -1, 6, -1, 7 },
        { MENU_CMD_COLORS, 0xD },
        { MENU_CMD_STRING_TABLE, 0x0, &positionFilterStringTable },
        { MENU_CMD_ONSELECT_FIRE, _PlayerMarketFilterPositionOnSelect },
        MENU_CMD_END,
    },
    {
        { -98, 44, 59, 10 },
        { MENU_CMD_COLORS, 0x9 },
        { MENU_CMD_TEXT_PTR, 0x0, aSkills },
        MENU_CMD_END,
    },
    { -38, 44, 80, 10 },
    { MENU_CMD_NAV, 4, -1, -1, 7 },
    { MENU_CMD_UP_SKIP, 6, 2 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_STRING_TABLE, 0x0, &skillFilterStringTable },
    { MENU_CMD_ONSELECT_FIRE, _PlayerMarketFilterAgeGroupOnSelect },
    MENU_CMD_END,
    {
        { 102, 59, 100, 10 },
        { MENU_CMD_NAV, 4, 6, 4, 10 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aList },
        { MENU_CMD_ONSELECT_FIRE, _PlayerMarketScrollOnSelect },
        MENU_CMD_END,
    },
    {
        { 112, -90, 80, 15 },
        { MENU_CMD_NAV, -1, -1, 19, 9 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aViewSquad },
        { MENU_CMD_ONSELECT_FIRE, _PlayerMarketViewSquadOnSelect },
        MENU_CMD_END,
    },
    {
        { 112, -71, 80, 15 },
        { MENU_CMD_NAV, -1, -1, 8, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _ExitPlayerMarketMenu },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 73 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_AFTER_DRAW, _PlayerMarketItemBeforeDraw },
    { MENU_CMD_ONSELECT_FIRE, _PlayerMarketItemOnSelect },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 0, 304, 8 },
          { MENU_CMD_NAV, -1, -1, 7, 11 },
          MENU_CMD_END,
      },
        {
            { 0, 9, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 10, 12 },
            MENU_CMD_END,
        },
        {
            { 0, 18, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 11, 13 },
            MENU_CMD_END,
        },
        {
            { 0, 27, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 12, 14 },
            MENU_CMD_END,
        },
        {
            { 0, 36, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 13, 15 },
            MENU_CMD_END,
        },
        {
            { 0, 45, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 14, 16 },
            MENU_CMD_END,
        },
        {
            { 0, 54, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 15, 17 },
            MENU_CMD_END,
        },
        {
            { 0, 63, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 16, 18 },
            MENU_CMD_END,
        },
        {
            { 0, 72, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 17, 19 },
            MENU_CMD_END,
        },
        {
            { 0, 81, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 18, 8 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 0 },
    { 0, 0, 304, 8 },
    { MENU_CMD_COLORS, 0x0 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    {
        { 2, 0, 10, 8 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 13, 0, 10, 8 },
        { MENU_CMD_SPRITE_FG, 0 },
        MENU_CMD_END,
    },
    {
        { 24, 0, 0, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { -47, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    {
        { -45, 0, 14, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -21, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0xF },
        MENU_CMD_END,
    },
    { {
          { -24, 0, 7, 7 },
          { MENU_CMD_SPRITE_FG, 181 },
          MENU_CMD_END,
      },
        {
            { -17, 0, 7, 7 },
            { MENU_CMD_SPRITE_FG, 182 },
            MENU_CMD_END,
        },
        {
            { -45, 0, 9, 8 },
            { MENU_CMD_SPRITE_FG, 189 },
            MENU_CMD_END,
        } },
    {
        { -13, 0, 30, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { 272, 0, 32, 8 },
    { MENU_CMD_TEXT_PTR, 0x8000, 0 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_BuyPlayer buyPlayerMenu = {
    { _BuyPlayerInit, _BuyPlayerInit, 0, 2 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aBuyPlayer },
        MENU_CMD_END,
    },
    {
        { 92, 32, 120, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x10 },
        MENU_CMD_END,
    },
    {
        { 57, 55, -66, 15 },
        { MENU_CMD_NAV, -1, -1, -1, 3 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aViewDomesticPl },
        { MENU_CMD_ONSELECT_FIRE, _ViewDomesticPlayerMarket },
        MENU_CMD_END,
    },
    {
        { 57, 80, -66, 15 },
        { MENU_CMD_NAV, -1, -1, 2, 4 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aViewForeignPla },
        { MENU_CMD_ONSELECT_FIRE, _ViewForeignPlayerMarket },
        MENU_CMD_END,
    },
    {
        { 57, 105, -66, 15 },
        { MENU_CMD_NAV, -1, -1, 3, 5 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aBuyOtherDomest },
        { MENU_CMD_ONSELECT_FIRE, _BuyOtherDomesticPlayer },
        MENU_CMD_END,
    },
    {
        { 57, -126, -66, 15 },
        { MENU_CMD_NAV, -1, -1, 4, 6 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aBuyOtherForeig },
        { MENU_CMD_ONSELECT_FIRE, _BuyOtherForeignPlayer },
        MENU_CMD_END,
    },
    {
        { 102, -101, 100, 15 },
        { MENU_CMD_NAV, -1, -1, 5, 7 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aViewSquad },
        { MENU_CMD_ONSELECT_FIRE, _BuyPlayerViewSquad },
        MENU_CMD_END,
    },
    {
        { 122, -71, 60, 15 },
        { MENU_CMD_NAV, -1, -1, 6, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _BuyPlayerExit },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_TransferMarket transferMarketMenu = {
    { _TransferMarketInit, _TransferMarketFinish, 0, 2 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aTransferMarket },
        MENU_CMD_END,
    },
    {
        { 92, 24, 120, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x10 },
        MENU_CMD_END,
    },
    {
        { 102, 40, 100, 15 },
        { MENU_CMD_NAV, -1, -1, -1, 3 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aBuyPlayer },
        { MENU_CMD_ONSELECT_FIRE, _BuyPlayerMenu },
        MENU_CMD_END,
    },
    {
        { 102, 65, 100, 15 },
        { MENU_CMD_NAV, -1, -1, 2, 4 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aViewOffers },
        { MENU_CMD_ONSELECT_FIRE, _ViewOffersMenu },
        MENU_CMD_END,
    },
    {
        { 62, 90, -76, 15 },
        { MENU_CMD_NAV, -1, -1, 3, 9 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aPutPlayerOnTra_0 },
        { MENU_CMD_ONSELECT_FIRE, _PutPlayerOnMarketMenu },
        MENU_CMD_END,
    },
    {
        { 0, -71, 121, 15 },
        { MENU_CMD_NAV, -1, 6, 13, -1 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aGiveFreeTransf },
        { MENU_CMD_ONSELECT_FIRE, _GiveFreeTransfer },
        MENU_CMD_END,
    },
    {
        { 122, -71, 60, 15 },
        { MENU_CMD_NAV, 5, 7, 13, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _jmp_SetExitMenuFlag_8 },
        MENU_CMD_END,
    },
    {
        { -73, -71, 121, 15 },
        { MENU_CMD_NAV, 6, -1, 13, -1 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aTakeOffMarket },
        { MENU_CMD_ONSELECT_FIRE, _TakeOffMarketMenu },
        MENU_CMD_END,
    },
    {
        { -104, 115, 0, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, aPlayersCurrent },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 130 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_AFTER_DRAW, _TransferMarketAfterDrawCommon },
    { MENU_CMD_ONSELECT_FIRE, _TransferMarketCommon },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 0, 304, 8 },
          { MENU_CMD_NAV, -1, -1, 4, 10 },
          MENU_CMD_END,
      },
        {
            { 0, 9, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 9, 11 },
            MENU_CMD_END,
        },
        {
            { 0, 18, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 10, 12 },
            MENU_CMD_END,
        },
        {
            { 0, 27, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 11, 13 },
            MENU_CMD_END,
        },
        {
            { 0, 36, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 12, 6 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 0 },
    { 0, 0, 304, 8 },
    { MENU_CMD_COLORS, 0x0 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    {
        { 2, 0, 10, 8 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 13, 0, 10, 8 },
        { MENU_CMD_SPRITE_FG, 0 },
        MENU_CMD_END,
    },
    {
        { 24, 0, 0, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { -47, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    {
        { -45, 0, 14, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -21, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0xF },
        MENU_CMD_END,
    },
    { {
          { -24, 0, 7, 7 },
          { MENU_CMD_SPRITE_FG, 181 },
          MENU_CMD_END,
      },
        {
            { -17, 0, 7, 7 },
            { MENU_CMD_SPRITE_FG, 182 },
            MENU_CMD_END,
        },
        {
            { -45, 0, 9, 8 },
            { MENU_CMD_SPRITE_FG, 189 },
            MENU_CMD_END,
        } },
    {
        { -13, 0, 30, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { 272, 0, 32, 8 },
    { MENU_CMD_TEXT_PTR, 0x8000, 0 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_SelectNationalTeam selectNationalTeamMenu = {
    { _SelectNationalTeamInit, _SelectNationalTeamFinish, 0, 4 },
    { 57, -10, 190, 15 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_NO_PTR, 0x10 },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { 32, 11, -16, 10 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    {
        { -104, 16, 0, 6 },
        { MENU_CMD_TEXT_PTR, 0x0, aToReplace },
        MENU_CMD_END,
    },
    { {
          { 0, -71, 121, 15 },
          { MENU_CMD_NAV, -1, 4, 24, -1 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aAddPlayer },
          { MENU_CMD_ONSELECT_FIRE, _NationalTeamAddPlayer },
          MENU_CMD_END,
      },
        {
            { 122, -71, 60, 15 },
            { MENU_CMD_NAV, 3, 5, 24, 5 },
            { MENU_CMD_COLORS, 0xC },
            { MENU_CMD_TEXT_PTR, 0x0, aExit },
            { MENU_CMD_ONSELECT_FIRE, _SelectNationalTeamExit },
            MENU_CMD_END,
        } },
    { 122, -71, 60, 15 },
    { MENU_CMD_NAV, 4, 6, 4, -1 },
    { MENU_CMD_RIGHT_SKIP, 5, 0 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_TEXT_PTR, 0x0, aAbort },
    { MENU_CMD_ONSELECT_FIRE, _SelectNationalTeamAbort },
    MENU_CMD_END,
    {
        { -73, -71, 121, 15 },
        { MENU_CMD_NAV, 5, -1, 24, -1 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aRemovePlayers },
        { MENU_CMD_ONSELECT_FIRE, _NationalTeamRemovePlayers },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 27 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_AFTER_DRAW, _SelectNationalTeamAfterDrawCommon },
    { MENU_CMD_ONSELECT_FIRE, _SelectNationalTeamCommon },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    {
        { 0, -24, 1, 8 },
        MENU_CMD_END,
    },
    { {
          { 0, 0, 304, 8 },
          { MENU_CMD_NAV, -1, -1, -1, 9 },
          MENU_CMD_END,
      },
        {
            { 0, 9, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 8, 10 },
            MENU_CMD_END,
        },
        {
            { 0, 18, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 9, 11 },
            MENU_CMD_END,
        },
        {
            { 0, 27, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 10, 12 },
            MENU_CMD_END,
        },
        {
            { 0, 36, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 11, 13 },
            MENU_CMD_END,
        },
        {
            { 0, 45, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 12, 14 },
            MENU_CMD_END,
        },
        {
            { 0, 54, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 13, 15 },
            MENU_CMD_END,
        },
        {
            { 0, 63, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 14, 16 },
            MENU_CMD_END,
        },
        {
            { 0, 72, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 15, 17 },
            MENU_CMD_END,
        },
        {
            { 0, 81, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 16, 18 },
            MENU_CMD_END,
        },
        {
            { 0, 90, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 17, 19 },
            MENU_CMD_END,
        },
        {
            { 0, 101, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 18, 20 },
            MENU_CMD_END,
        },
        {
            { 0, 110, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 19, 21 },
            MENU_CMD_END,
        },
        {
            { 0, 119, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 20, 22 },
            MENU_CMD_END,
        },
        {
            { 0, 128, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 21, 23 },
            MENU_CMD_END,
        },
        {
            { 0, 137, 304, 8 },
            { MENU_CMD_NAV, -1, -1, 22, 24 },
            MENU_CMD_END,
        } },
    { 23, 148, 281, 8 },
    { MENU_CMD_NAV, 25, -1, 23, 4 },
    { MENU_CMD_LEFT_SKIP, 26, 2 },
    { MENU_CMD_INVISIBLE },
    MENU_CMD_END,
    MENU_MARK_INIT_ENTRY,
    MENU_MARK_BYTE_COORDS,
    { 13, -108, 10, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_NAV, -1, 24, 23, 4 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _SelectNationalTeamOnSelect25 },
    { MENU_CMD_AFTER_DRAW, _SelectNationalTeam25AfterDraw },
    MENU_CMD_END,
    {
        { 0, 0, 0, 0 },
        { MENU_CMD_INVISIBLE },
        { MENU_CMD_NAV, -1, -1, 25, -1 },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 0 },
    MENU_MARK_WIDE_COORDS,
    {
        { 0, 0, 304, 8 },
        { MENU_CMD_COLORS, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_BYTE_COORDS,
    {
        { 2, 0, 10, 8 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 13, 0, 10, 8 },
        { MENU_CMD_SPRITE_FG, 0 },
        MENU_CMD_END,
    },
    {
        { 24, 0, 0, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { -47, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    {
        { -45, 0, 14, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -21, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0xF },
        MENU_CMD_END,
    },
    { {
          { -24, 0, 7, 7 },
          { MENU_CMD_SPRITE_FG, 181 },
          MENU_CMD_END,
      },
        {
            { -17, 0, 7, 7 },
            { MENU_CMD_SPRITE_FG, 182 },
            MENU_CMD_END,
        },
        {
            { -45, 0, 9, 8 },
            { MENU_CMD_SPRITE_FG, 189 },
            MENU_CMD_END,
        } },
    {
        { -13, 0, 30, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    {
        { 272, 0, 32, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_NationalTeamAddPlayer nationalTeamAddPlayerMenu = {
    { _NationalTeamAddPlayerInit, 0, 0, 1 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aAddPlayer },
        MENU_CMD_END,
    },
    {
        { 57, 70, -66, 15 },
        { MENU_CMD_NAV, -1, -1, -1, 2 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aAddInternation },
        { MENU_CMD_ONSELECT_FIRE, _NationalTeamAddPlayerHomeOnSelect },
        MENU_CMD_END,
    },
    {
        { 57, 95, -66, 15 },
        { MENU_CMD_NAV, -1, -1, 1, 3 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aAddInternation_0 },
        { MENU_CMD_ONSELECT_FIRE, _NationalTeamAddPlayerAbroadOnSelect },
        MENU_CMD_END,
    },
    {
        { 122, -71, 60, 15 },
        { MENU_CMD_NAV, -1, -1, 2, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _NationalTeamAddPlayerExit },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_PlayMatch playMatchMenu = {
    { _PlayMatchMenuInit, _PlayMatchMenuReinit, 0, 9 },
    { MENU_MARK_START_MENU, 0, -20 },
    MENU_MARK_BYTE_COORDS,
    { 3, 6, 16, 16 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_SPRITE_FG, 0 },
    MENU_CMD_END,
    {
        { 82, 0, -116, 12 },
        { MENU_CMD_COLORS, 0x0 },
        { MENU_CMD_TEXT_PTR, 0x10, 0 },
        MENU_CMD_END,
    },
    {
        { -104, 14, 0, 6 },
        { MENU_CMD_TEXT_NO_PTR, 0x0 },
        MENU_CMD_END,
    },
    { {
          { 0, -44, 55, 8 },
          { MENU_CMD_TEXT_PTR, 0x8000, aCoach_4 },
          MENU_CMD_END,
      },
        {
            { 44, -44, -114, 8 },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            MENU_CMD_END,
        } },
    {
        { 0, -99, 105, 10 },
        { MENU_CMD_NAV, -1, 8, 29, 6 },
        { MENU_CMD_COLORS, 0x8 },
        { MENU_CMD_TEXT_PTR, 0x0, aUseMatchTeam },
        { MENU_CMD_ONSELECT_FIRE, _UseMatchTeamSelect },
        MENU_CMD_END,
    },
    { 0, -73, 42, 18 },
    { MENU_CMD_NAV, -1, 50, 5, 9 },
    { MENU_CMD_UP_SKIP, 6, 2 },
    { MENU_CMD_RIGHT_SKIP, 9, 3 },
    { MENU_CMD_COLORS, 0xD },
    {
        MENU_CMD_MULTILINE,
        { 0x0 },
        0,
    },
    { MENU_CMD_ONSELECT_FIRE, _TogglePlayerCoach },
    MENU_CMD_END,
    {
        { -84, -34, 62, 15 },
        { MENU_CMD_NAV, 8, 10, 54, -1 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aViewOppo },
        { MENU_CMD_ONSELECT_FIRE, _ViewOpponentsSelect },
        MENU_CMD_END,
    },
    { 113, -34, 52, 15 },
    { MENU_CMD_NAV, 9, 7, 6, -1 },
    { MENU_CMD_RIGHT_SKIP, 9, 0 },
    { MENU_CMD_COLORS, 0xC },
    { MENU_CMD_TEXT_PTR, 0x0, aExit },
    { MENU_CMD_ONSELECT_FIRE, _ExitPlayMatch },
    MENU_CMD_END,
    { 0, -38, 106, 23 },
    { MENU_CMD_NAV, -1, 8, 6, -1 },
    { MENU_CMD_UP_SKIP, 9, 2 },
    { MENU_CMD_RIGHT_SKIP, 9, 1 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, 0 },
    { MENU_CMD_ONSELECT_FIRE, _PlayMatchSelected },
    MENU_CMD_END,
    {
        { -21, -34, 68, 15 },
        { MENU_CMD_NAV, 7, -1, 55, -1 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aEditTactics },
        { MENU_CMD_ONSELECT_FIRE, _EditTacticsInPlayMatchMenu },
        MENU_CMD_END,
    },
    { 0, 0, 0, 0 },
    { MENU_CMD_ONSELECT_FIRE, _ReturnFromViewOpponent },
    MENU_CMD_END,
    {
        { 0, -84, -84, 9 },
        { MENU_CMD_COLORS, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_AFTER_DRAW, _DrawPlayerEntries },
    { MENU_CMD_ONSELECT_FIRE, _OnSwapPlayer },
    MENU_CMD_END,
    { {
          { 23, 24, 82, 8 },
          { MENU_CMD_NAV, -1, 38, -1, 14 },
          MENU_CMD_END,
      },
        {
            { 23, 33, 82, 8 },
            { MENU_CMD_NAV, -1, 38, 13, 15 },
            MENU_CMD_END,
        },
        {
            { 23, 42, 82, 8 },
            { MENU_CMD_NAV, -1, 38, 14, 16 },
            MENU_CMD_END,
        },
        {
            { 23, 51, 82, 8 },
            { MENU_CMD_NAV, -1, 38, 15, 17 },
            MENU_CMD_END,
        },
        {
            { 23, 60, 82, 8 },
            { MENU_CMD_NAV, -1, 38, 16, 18 },
            MENU_CMD_END,
        },
        {
            { 23, 69, 82, 8 },
            { MENU_CMD_NAV, -1, 38, 17, 19 },
            MENU_CMD_END,
        },
        {
            { 23, 78, 82, 8 },
            { MENU_CMD_NAV, -1, 38, 18, 20 },
            MENU_CMD_END,
        },
        {
            { 23, 87, 82, 8 },
            { MENU_CMD_NAV, -1, 38, 19, 21 },
            MENU_CMD_END,
        },
        {
            { 23, 96, 82, 8 },
            { MENU_CMD_NAV, -1, 38, 20, 22 },
            MENU_CMD_END,
        },
        {
            { 23, 105, 82, 8 },
            { MENU_CMD_NAV, -1, 38, 21, 23 },
            MENU_CMD_END,
        },
        {
            { 23, 114, 82, 8 },
            { MENU_CMD_NAV, -1, 38, 22, 24 },
            MENU_CMD_END,
        } },
    { 23, 125, 82, 8 },
    { MENU_CMD_NAV, -1, 38, 23, 25 },
    { MENU_CMD_UP_SKIP, 29, 2 },
    MENU_CMD_END,
    { 23, -122, 82, 8 },
    { MENU_CMD_NAV, -1, 38, 24, 26 },
    { MENU_CMD_UP_SKIP, 29, 2 },
    MENU_CMD_END,
    { 23, -113, 82, 8 },
    { MENU_CMD_NAV, -1, 38, 25, 27 },
    { MENU_CMD_UP_SKIP, 29, 2 },
    MENU_CMD_END,
    { 23, -104, 82, 8 },
    { MENU_CMD_NAV, -1, 38, 26, 28 },
    { MENU_CMD_UP_SKIP, 29, 2 },
    MENU_CMD_END,
    { 23, -95, 82, 8 },
    { MENU_CMD_NAV, -1, 41, 27, 29 },
    { MENU_CMD_UP_SKIP, 29, 2 },
    MENU_CMD_END,
    { 23, -84, 82, 8 },
    { MENU_CMD_NAV, 30, 44, 28, 5 },
    { MENU_CMD_UP_SKIP, 29, 2 },
    { MENU_CMD_INVISIBLE },
    MENU_CMD_END,
    MENU_MARK_INIT_ENTRY,
    { 0, -84, 12, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_NAV, -1, 29, 28, 5 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _PlayMatchOnSelect30 },
    { MENU_CMD_AFTER_DRAW, _PlayMatch30AfterDraw },
    MENU_CMD_END,
    { 0, 0, 12, 8 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    MENU_CMD_END,
    {
        { 13, 0, 10, 8 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 23, 0, 82, 8 },
        { MENU_CMD_COLORS, 0xA },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { 106, 0, 14, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -127, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0xF },
        MENU_CMD_END,
    },
    {
        { -117, 0, 32, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    { 0, 0, 12, 8 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 172, 131 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_ONSELECT_FIRE, _PlayMatchChangeTactics },
    MENU_CMD_END,
    { 0, 0, 43, 9 },
    { MENU_CMD_NAV, 27, 39, -1, 41 },
    { MENU_CMD_TEXT_PTR, 0x0, a442 },
    MENU_CMD_END,
    { 44, 0, 43, 9 },
    { MENU_CMD_NAV, 38, 40, -1, 42 },
    { MENU_CMD_TEXT_PTR, 0x0, a541 },
    MENU_CMD_END,
    { 88, 0, 43, 9 },
    { MENU_CMD_NAV, 39, -1, -1, 43 },
    { MENU_CMD_TEXT_PTR, 0x0, a451 },
    MENU_CMD_END,
    { 0, 10, 43, 9 },
    { MENU_CMD_NAV, 27, 42, 38, 44 },
    { MENU_CMD_TEXT_PTR, 0x0, a532 },
    MENU_CMD_END,
    { 44, 10, 43, 9 },
    { MENU_CMD_NAV, 41, 43, 39, 45 },
    { MENU_CMD_TEXT_PTR, 0x0, a352 },
    MENU_CMD_END,
    { 88, 10, 43, 9 },
    { MENU_CMD_NAV, 42, -1, 40, 46 },
    { MENU_CMD_TEXT_PTR, 0x0, a433 },
    MENU_CMD_END,
    { 0, 20, 43, 9 },
    { MENU_CMD_NAV, 28, 45, 41, 47 },
    { MENU_CMD_TEXT_PTR, 0x0, a424 },
    MENU_CMD_END,
    { 44, 20, 43, 9 },
    { MENU_CMD_NAV, 44, 46, 42, 48 },
    { MENU_CMD_TEXT_PTR, 0x0, a343 },
    MENU_CMD_END,
    { 88, 20, 43, 9 },
    { MENU_CMD_NAV, 45, -1, 43, 49 },
    { MENU_CMD_TEXT_PTR, 0x0, aSweep },
    MENU_CMD_END,
    { 0, 30, 43, 9 },
    { MENU_CMD_NAV, 29, 48, 44, 50 },
    { MENU_CMD_TEXT_PTR, 0x0, a523 },
    MENU_CMD_END,
    { 44, 30, 43, 9 },
    { MENU_CMD_NAV, 47, 49, 45, 50 },
    { MENU_CMD_TEXT_PTR, 0x0, aAttack },
    MENU_CMD_END,
    { 88, 30, 43, 9 },
    { MENU_CMD_NAV, 48, -1, 46, 51 },
    { MENU_CMD_TEXT_PTR, 0x0, aDefend },
    MENU_CMD_END,
    { 0, 40, 65, 9 },
    { MENU_CMD_NAV, 6, 51, 47, 52 },
    { MENU_CMD_TEXT_PTR, 0x0, (char*)teamTacticsUSERA },
    MENU_CMD_END,
    { 66, 40, 65, 9 },
    { MENU_CMD_NAV, 50, -1, 49, 53 },
    { MENU_CMD_TEXT_PTR, 0x0, (char*)teamTacticsUSERB },
    MENU_CMD_END,
    { 0, 50, 65, 9 },
    { MENU_CMD_NAV, 6, 53, 50, 54 },
    { MENU_CMD_TEXT_PTR, 0x0, (char*)teamTacticsUSERC },
    MENU_CMD_END,
    { 66, 50, 65, 9 },
    { MENU_CMD_NAV, 52, -1, 51, 55 },
    { MENU_CMD_TEXT_PTR, 0x0, (char*)teamTacticsUSERD },
    MENU_CMD_END,
    { 0, 60, 65, 9 },
    { MENU_CMD_NAV, 6, 55, 52, 7 },
    { MENU_CMD_TEXT_PTR, 0x0, (char*)teamTacticsUSERE },
    MENU_CMD_END,
    { 66, 60, 65, 9 },
    { MENU_CMD_NAV, 54, -1, 53, 10 },
    { MENU_CMD_TEXT_PTR, 0x0, (char*)teamTacticsUSERF },
    MENU_CMD_END,
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 172, 4 },
    {
        { 0, 0, -124, 125 },
        { MENU_CMD_SPRITE_FG, 225 },
        MENU_CMD_END,
    },
    { {
          { 66, 12, 0, 6 },
          { MENU_CMD_TEXT_NO_PTR, 0x0 },
          MENU_CMD_END,
      },
        {
            { 3, 25, 0, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        },
        {
            { 63, 35, 0, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x4000 },
            MENU_CMD_END,
        },
        {
            { 69, 35, 0, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        },
        {
            { -127, 25, 0, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x4000 },
            MENU_CMD_END,
        },
        {
            { 3, 55, 0, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        },
        {
            { 63, 65, 0, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x4000 },
            MENU_CMD_END,
        },
        {
            { 69, 65, 0, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        },
        {
            { -127, 55, 0, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x4000 },
            MENU_CMD_END,
        },
        {
            { 63, 85, 0, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x4000 },
            MENU_CMD_END,
        },
        {
            { 69, 85, 0, 6 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    MENU_MARK_END_MENU,
};
Menu_JobOffers jobOffersMenu = {
    { _JobOffersInit, _JobOffersFinish, 0, 0xC },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aJobOffers },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 22 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _JobOffersCommon },
    { MENU_CMD_AFTER_DRAW, _JobOffersAfterDrawCommon },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 0, 304, 12 },
          { MENU_CMD_NAV, -1, -1, -1, 2 },
          MENU_CMD_END,
      },
        {
            { 0, 15, 304, 12 },
            { MENU_CMD_NAV, -1, -1, 1, 3 },
            MENU_CMD_END,
        },
        {
            { 0, 30, 304, 12 },
            { MENU_CMD_NAV, -1, -1, 2, 4 },
            MENU_CMD_END,
        },
        {
            { 0, 45, 304, 12 },
            { MENU_CMD_NAV, -1, -1, 3, 5 },
            MENU_CMD_END,
        },
        {
            { 0, 60, 304, 12 },
            { MENU_CMD_NAV, -1, -1, 4, 7 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 0 },
    MENU_MARK_BYTE_COORDS,
    {
        { -104, 94, 0, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, aJobOffersBeing },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 109 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_AFTER_DRAW, _JobOffers_DrawOldOfferLine },
    { MENU_CMD_ONSELECT_FIRE, _OldJobOffersCommon },
    MENU_CMD_END,
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 0, 304, 12 },
          { MENU_CMD_NAV, -1, -1, 5, 8 },
          MENU_CMD_END,
      },
        {
            { 0, 15, 304, 12 },
            { MENU_CMD_NAV, -1, -1, 7, 9 },
            MENU_CMD_END,
        },
        {
            { 0, 30, 304, 12 },
            { MENU_CMD_NAV, -1, -1, 8, 10 },
            MENU_CMD_END,
        },
        {
            { 0, 45, 304, 12 },
            { MENU_CMD_NAV, -1, -1, 9, 11 },
            MENU_CMD_END,
        },
        {
            { 0, 60, 304, 12 },
            { MENU_CMD_NAV, -1, -1, 10, 12 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 0 },
    MENU_MARK_BYTE_COORDS,
    {
        { 122, -71, 60, 15 },
        { MENU_CMD_NAV, -1, -1, 11, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _JobOffersExit },
        MENU_CMD_END,
    },
    {
        { 0, 0, 15, 12 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    {
        { 15, 0, 289, 12 },
        { MENU_CMD_TEXT_NO_PTR, 0x8000 },
        MENU_CMD_END,
    },
    MENU_MARK_BYTE_COORDS,
    {
        { 0, 0, 15, 12 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    {
        { 15, 0, 289, 12 },
        { MENU_CMD_TEXT_NO_PTR, 0x8000 },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_GotJobOffer gotJobOfferMenu = {
    { _GotJobOfferInit, _GotJobOfferFinish, 0, 0xA },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aJobOffer },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 42, 304, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { 0, 57, 304, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        },
        {
            { 0, 67, 304, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        },
        {
            { 0, 82, 304, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        },
        {
            { 0, 92, 304, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        },
        {
            { 0, 102, 304, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        },
        {
            { 0, 117, 304, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    MENU_MARK_BYTE_COORDS,
    {
        { 0, -71, 57, 15 },
        { MENU_CMD_NAV, -1, 9, -1, -1 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aReject },
        { MENU_CMD_ONSELECT_FIRE, _RejectJobOffer },
        MENU_CMD_END,
    },
    {
        { 58, -71, 64, 15 },
        { MENU_CMD_NAV, 8, 10, -1, -1 },
        { MENU_CMD_COLORS, 0x5 },
        { MENU_CMD_TEXT_PTR, 0x0, aViewLeague },
        { MENU_CMD_ONSELECT_FIRE, _ViewFutureTeamLeague },
        MENU_CMD_END,
    },
    {
        { 123, -71, 58, 15 },
        { MENU_CMD_NAV, 9, 11, -1, -1 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aConsider },
        { MENU_CMD_ONSELECT_FIRE, _ConsiderJobOffer },
        MENU_CMD_END,
    },
    {
        { 123, -71, 58, 15 },
        { MENU_CMD_NAV, 10, 12, -1, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _j_SetExitMenuFlag_14 },
        MENU_CMD_END,
    },
    {
        { -74, -71, 64, 15 },
        { MENU_CMD_NAV, 11, 13, -1, -1 },
        { MENU_CMD_COLORS, 0x5 },
        { MENU_CMD_TEXT_PTR, 0x0, aViewTeam },
        { MENU_CMD_ONSELECT_FIRE, _ViewFutureTeam },
        MENU_CMD_END,
    },
    {
        { -9, -71, 57, 15 },
        { MENU_CMD_NAV, 12, -1, -1, -1 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aAccept },
        { MENU_CMD_ONSELECT_FIRE, _AcceptJobOffer },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_ChairmanMemos chairmanMemosMenu = {
    { _ChairmanMemosInit, _ChairmanMemosOnReturn, 0, 6 },
    MENU_MARK_BYTE_COORDS,
    {
        { 52, 0, -56, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aMemoFromTheClu },
        MENU_CMD_END,
    },
    MENU_MARK_WIDE_COORDS,
    { {
          { 0, 40, 304, 8 },
          { MENU_CMD_TEXT_NO_PTR, 0x8000 },
          MENU_CMD_END,
      },
        {
            { 0, 60, 304, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        },
        {
            { 0, 70, 304, 8 },
            { MENU_CMD_TEXT_NO_PTR, 0x8000 },
            MENU_CMD_END,
        } },
    { 0, 100, 304, 8 },
    { MENU_CMD_TEXT_PTR, 0x8000, aBestRegards },
    MENU_CMD_END,
    { 0, 115, 304, 8 },
    { MENU_CMD_TEXT_PTR, 0x8000, aYourChairman },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { 102, -71, 100, 15 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_TEXT_PTR, 0x0, aContinue },
    { MENU_CMD_ONSELECT_FIRE, _ChairmanMemosContinue },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_ClubBusiness clubBusinessMenu = {
    { _ClubBusinessInit, 0, 0, 1 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aBankStatement },
        MENU_CMD_END,
    },
    { 102, -71, 100, 15 },
    { MENU_CMD_COLORS, 0xC },
    { MENU_CMD_TEXT_PTR, 0x0, aContinue },
    { MENU_CMD_ONSELECT_FIRE, _j_SetExitMenuFlag13 },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, 0, 42 },
    MENU_MARK_WIDE_COORDS,
    { 0, -5, 229, 8 },
    { MENU_CMD_TEXT_PTR, 0x4000, aOldBalance },
    MENU_CMD_END,
    MENU_MARK_BYTE_COORDS,
    { 0, 20, -27, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_TEXT_PTR, 0x4000, aCompetitionBon },
    MENU_CMD_END,
    {
        { 0, 15, -27, 8 },
        { MENU_CMD_TEXT_PTR, 0x4000, aGateReceipts },
        MENU_CMD_END,
    },
    {
        { 0, 30, -27, 8 },
        { MENU_CMD_TEXT_PTR, 0x4000, aGeneralRunning },
        MENU_CMD_END,
    },
    { 0, 45, -27, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_TEXT_PTR, 0x4000, aAdditionalInve_0 },
    MENU_CMD_END,
    {
        { 0, 45, -27, 8 },
        { MENU_CMD_TEXT_PTR, 0x4000, aPlayerSales },
        MENU_CMD_END,
    },
    {
        { 0, 60, -27, 8 },
        { MENU_CMD_TEXT_PTR, 0x4000, aPlayerPurchase },
        MENU_CMD_END,
    },
    {
        { 0, 75, -27, 8 },
        { MENU_CMD_TEXT_PTR, 0x4000, aPlayerWagesBil },
        MENU_CMD_END,
    },
    {
        { 0, 95, -27, 8 },
        { MENU_CMD_TEXT_PTR, 0x4000, aTotalProfit },
        MENU_CMD_END,
    },
    {
        { 0, 115, -27, 8 },
        { MENU_CMD_TEXT_PTR, 0x4000, aNewBalance },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 42 },
    MENU_MARK_WIDE_COORDS,
    {
        { 229, -5, 75, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    MENU_MARK_BYTE_COORDS,
    { -27, 20, 75, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_TEXT_NO_PTR, 0x4000 },
    MENU_CMD_END,
    {
        { -27, 15, 75, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    {
        { -27, 30, 75, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    { -27, 45, 75, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_TEXT_NO_PTR, 0x4000 },
    MENU_CMD_END,
    {
        { -27, 45, 75, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    {
        { -27, 60, 75, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    {
        { -27, 75, 75, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    {
        { -27, 95, 75, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0x4000 },
        MENU_CMD_END,
    },
    { -22, 114, 70, 10 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_TEXT_NO_PTR, 0x4000 },
    MENU_CMD_END,
    MENU_MARK_END_MENU,
};
Menu_ChooseTactics chooseTacticsMenu = {
    { _ChooseTacticsInit, 0, 0, 1 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aEditTactics },
        MENU_CMD_END,
    },
    {
        { 82, 40, -116, 15 },
        { MENU_CMD_NAV, -1, -1, -1, 2 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        { MENU_CMD_ONSELECT_FIRE, _User_A_Tactics },
        MENU_CMD_END,
    },
    {
        { 82, 60, -116, 15 },
        { MENU_CMD_NAV, -1, -1, 1, 3 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        { MENU_CMD_ONSELECT_FIRE, _User_B_Tactics },
        MENU_CMD_END,
    },
    {
        { 82, 80, -116, 15 },
        { MENU_CMD_NAV, -1, -1, 2, 4 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        { MENU_CMD_ONSELECT_FIRE, _User_C_Tactics },
        MENU_CMD_END,
    },
    {
        { 82, 100, -116, 15 },
        { MENU_CMD_NAV, -1, -1, 3, 5 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        { MENU_CMD_ONSELECT_FIRE, _User_D_Tactics },
        MENU_CMD_END,
    },
    {
        { 82, 120, -116, 15 },
        { MENU_CMD_NAV, -1, -1, 4, 6 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        { MENU_CMD_ONSELECT_FIRE, _User_E_Tactics },
        MENU_CMD_END,
    },
    {
        { 82, -116, -116, 15 },
        { MENU_CMD_NAV, -1, -1, 5, 7 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        { MENU_CMD_ONSELECT_FIRE, _User_F_Tactics },
        MENU_CMD_END,
    },
    {
        { 102, -71, 100, 15 },
        { MENU_CMD_NAV, -1, -1, 6, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _ExitChooseTacticsMenu },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_EditTactics editTacticsMenu = {
    { _EditTacticsInit, _EditTacticsOnRestore, 0, 8 },
    MENU_MARK_BYTE_COORDS,
    { -121, 0, 13, 16 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_SPRITE_FG, 0 },
    MENU_CMD_END,
    { -102, 0, -106, 12 },
    { MENU_CMD_COLORS, 0x17 },
    { MENU_CMD_TEXT_NO_PTR, 0x0 },
    MENU_CMD_END,
    { 0, 0, 0, 0 },
    { MENU_CMD_INPUT_FUNC, 0xFFFF, _EditTacicsMovePlayersOrBall },
    { MENU_CMD_AFTER_DRAW, _EditTactics2AfterDraw },
    MENU_CMD_END,
    { 0, 0, 0, 0 },
    { MENU_CMD_INPUT_FUNC, 0xFFFF, _CopyTactics },
    { MENU_CMD_AFTER_DRAW, _EditTactics2AfterDraw },
    MENU_CMD_END,
    { {
          { -125, -75, 57, 9 },
          { MENU_CMD_NAV, 2, 5, 27, 7 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aCopy },
          { MENU_CMD_ONSELECT_FIRE, _SelectCopyTactics },
          MENU_CMD_END,
      },
        {
            { -67, -75, 57, 9 },
            { MENU_CMD_NAV, 4, 6, 27, 8 },
            { MENU_CMD_COLORS, 0xE },
            { MENU_CMD_TEXT_PTR, 0x0, aUndo },
            { MENU_CMD_ONSELECT_FIRE, _UndoTactics },
            MENU_CMD_END,
        } },
    { -9, -75, 57, 9 },
    { MENU_CMD_NAV, 5, -1, 27, 9 },
    { MENU_CMD_COLORS, 0xD },
    { MENU_CMD_STRING_TABLE, 0x0, &flipStrTable },
    { MENU_CMD_ONSELECT_FIRE, _FlipOnOff },
    MENU_CMD_END,
    { {
          { -125, -65, 57, 9 },
          { MENU_CMD_NAV, 2, 8, 4, -1 },
          { MENU_CMD_COLORS, 0xE },
          { MENU_CMD_TEXT_PTR, 0x0, aImport },
          { MENU_CMD_ONSELECT_FIRE, _ImportTactics },
          MENU_CMD_END,
      },
        {
            { -67, -65, 57, 9 },
            { MENU_CMD_NAV, 7, 9, 5, -1 },
            { MENU_CMD_COLORS, 0xE },
            { MENU_CMD_TEXT_PTR, 0x0, aSaveExit },
            { MENU_CMD_ONSELECT_FIRE, _SaveAndExitEditTactics },
            MENU_CMD_END,
        },
        {
            { -9, -65, 57, 9 },
            { MENU_CMD_NAV, 8, -1, 6, -1 },
            { MENU_CMD_COLORS, 0xA },
            { MENU_CMD_TEXT_PTR, 0x0, aAbort },
            { MENU_CMD_ONSELECT_FIRE, _AbortEditTactics },
            MENU_CMD_END,
        } },
    { MENU_MARK_START_MENU, 131, 18 },
    {
        { 0, -108, -83, 9 },
        { MENU_CMD_COLORS, 0x0 },
        MENU_CMD_END,
    },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_AFTER_DRAW, _EditTacticsAfterDrawPlayerNames },
    { MENU_CMD_ONSELECT_FIRE, _EditTacticsPlayerNamesCommon },
    MENU_CMD_END,
    { {
          { 23, 0, 82, 8 },
          { MENU_CMD_NAV, 2, -1, -1, 12 },
          MENU_CMD_END,
      },
        {
            { 23, 9, 82, 8 },
            { MENU_CMD_NAV, 29, -1, 11, 13 },
            MENU_CMD_END,
        },
        {
            { 23, 18, 82, 8 },
            { MENU_CMD_NAV, 30, -1, 12, 14 },
            MENU_CMD_END,
        },
        {
            { 23, 27, 82, 8 },
            { MENU_CMD_NAV, 31, -1, 13, 15 },
            MENU_CMD_END,
        },
        {
            { 23, 36, 82, 8 },
            { MENU_CMD_NAV, 32, -1, 14, 16 },
            MENU_CMD_END,
        },
        {
            { 23, 45, 82, 8 },
            { MENU_CMD_NAV, 33, -1, 15, 17 },
            MENU_CMD_END,
        },
        {
            { 23, 54, 82, 8 },
            { MENU_CMD_NAV, 34, -1, 16, 18 },
            MENU_CMD_END,
        },
        {
            { 23, 63, 82, 8 },
            { MENU_CMD_NAV, 35, -1, 17, 19 },
            MENU_CMD_END,
        },
        {
            { 23, 72, 82, 8 },
            { MENU_CMD_NAV, 36, -1, 18, 20 },
            MENU_CMD_END,
        },
        {
            { 23, 81, 82, 8 },
            { MENU_CMD_NAV, 37, -1, 19, 21 },
            MENU_CMD_END,
        },
        {
            { 23, 90, 82, 8 },
            { MENU_CMD_NAV, 38, -1, 20, 22 },
            MENU_CMD_END,
        },
        {
            { 23, 101, 82, 8 },
            { MENU_CMD_NAV, 2, -1, 21, 23 },
            MENU_CMD_END,
        },
        {
            { 23, 110, 82, 8 },
            { MENU_CMD_NAV, 2, -1, 22, 24 },
            MENU_CMD_END,
        },
        {
            { 23, 119, 82, 8 },
            { MENU_CMD_NAV, 2, -1, 23, 25 },
            MENU_CMD_END,
        },
        {
            { 23, -128, 82, 8 },
            { MENU_CMD_NAV, 2, -1, 24, 26 },
            MENU_CMD_END,
        } },
    { 23, -119, 82, 8 },
    { MENU_CMD_NAV, 2, -1, 25, 27 },
    { MENU_CMD_UP_SKIP, 27, 2 },
    MENU_CMD_END,
    { 23, -108, 82, 8 },
    { MENU_CMD_NAV, 28, -1, 26, 5 },
    { MENU_CMD_INVISIBLE },
    MENU_CMD_END,
    { 0, -108, 12, 8 },
    { MENU_CMD_INVISIBLE },
    { MENU_CMD_NAV, 2, 27, 38, 4 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_ONSELECT_FIRE, _EditTacticsOnSelect28 },
    { MENU_CMD_AFTER_DRAW, _EditTactics28AfterDraw },
    MENU_CMD_END,
    MENU_MARK_INIT_ENTRY,
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0x0 },
    { MENU_CMD_AFTER_DRAW, _EditTacticsAfterDrawPlayerFace },
    { MENU_CMD_ONSELECT_FIRE, _EditTacticsCommon },
    MENU_CMD_END,
    { {
          { 0, 9, 12, 8 },
          { MENU_CMD_NAV, 2, 12, -1, 30 },
          MENU_CMD_END,
      },
        {
            { 0, 18, 12, 8 },
            { MENU_CMD_NAV, 2, 13, 29, 31 },
            MENU_CMD_END,
        },
        {
            { 0, 27, 12, 8 },
            { MENU_CMD_NAV, 2, 14, 30, 32 },
            MENU_CMD_END,
        },
        {
            { 0, 36, 12, 8 },
            { MENU_CMD_NAV, 2, 15, 31, 33 },
            MENU_CMD_END,
        },
        {
            { 0, 45, 12, 8 },
            { MENU_CMD_NAV, 2, 16, 32, 34 },
            MENU_CMD_END,
        },
        {
            { 0, 54, 12, 8 },
            { MENU_CMD_NAV, 2, 17, 33, 35 },
            MENU_CMD_END,
        },
        {
            { 0, 63, 12, 8 },
            { MENU_CMD_NAV, 2, 18, 34, 36 },
            MENU_CMD_END,
        },
        {
            { 0, 72, 12, 8 },
            { MENU_CMD_NAV, 2, 19, 35, 37 },
            MENU_CMD_END,
        },
        {
            { 0, 81, 12, 8 },
            { MENU_CMD_NAV, 2, 20, 36, 38 },
            MENU_CMD_END,
        },
        {
            { 0, 90, 12, 8 },
            { MENU_CMD_NAV, 2, 21, 37, 28 },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 0 },
    { MENU_MARK_START_MENU, 131, 0 },
    { 0, 0, 12, 8 },
    { MENU_CMD_COLORS, 0xA },
    { MENU_CMD_SPRITE_FG, 0 },
    MENU_CMD_END,
    {
        { 13, 0, 10, 8 },
        { MENU_CMD_NUMBER, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { 23, 0, 82, 8 },
        { MENU_CMD_COLORS, 0xA },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    {
        { 106, 0, 14, 8 },
        { MENU_CMD_TEXT_PTR, 0x0, 0 },
        MENU_CMD_END,
    },
    {
        { -127, 0, 0, 8 },
        { MENU_CMD_TEXT_NO_PTR, 0xF },
        MENU_CMD_END,
    },
    {
        { -117, 0, 32, 8 },
        { MENU_CMD_TEXT_PTR, 0x8000, 0 },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 0 },
    MENU_MARK_END_MENU,
};
Menu_ImportTactics importTacticsMenu = {
    { _ImportTacticsInit, 0, 0, 2 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aImportTactics },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 85, 60 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_COLORS, 0xE },
    { MENU_CMD_ONSELECT_FIRE, _ImportTacticsSelectTactics },
    MENU_CMD_END,
    { {
          { 0, 0, 43, 9 },
          { MENU_CMD_NAV, -1, 2, -1, 4 },
          { MENU_CMD_TEXT_PTR, 0x0, a442 },
          MENU_CMD_END,
      },
        {
            { 45, 0, 43, 9 },
            { MENU_CMD_NAV, 1, 3, -1, 5 },
            { MENU_CMD_TEXT_PTR, 0x0, a541 },
            MENU_CMD_END,
        },
        {
            { 90, 0, 43, 9 },
            { MENU_CMD_NAV, 2, -1, -1, 6 },
            { MENU_CMD_TEXT_PTR, 0x0, a451 },
            MENU_CMD_END,
        },
        {
            { 0, 11, 43, 9 },
            { MENU_CMD_NAV, -1, 5, 1, 7 },
            { MENU_CMD_TEXT_PTR, 0x0, a532 },
            MENU_CMD_END,
        },
        {
            { 45, 11, 43, 9 },
            { MENU_CMD_NAV, 4, 6, 2, 8 },
            { MENU_CMD_TEXT_PTR, 0x0, a352 },
            MENU_CMD_END,
        },
        {
            { 90, 11, 43, 9 },
            { MENU_CMD_NAV, 5, -1, 3, 9 },
            { MENU_CMD_TEXT_PTR, 0x0, a433 },
            MENU_CMD_END,
        },
        {
            { 0, 22, 43, 9 },
            { MENU_CMD_NAV, -1, 8, 4, 10 },
            { MENU_CMD_TEXT_PTR, 0x0, a424 },
            MENU_CMD_END,
        },
        {
            { 45, 22, 43, 9 },
            { MENU_CMD_NAV, 7, 9, 5, 11 },
            { MENU_CMD_TEXT_PTR, 0x0, a343 },
            MENU_CMD_END,
        },
        {
            { 90, 22, 43, 9 },
            { MENU_CMD_NAV, 8, -1, 6, 12 },
            { MENU_CMD_TEXT_PTR, 0x0, aSweep },
            MENU_CMD_END,
        },
        {
            { 0, 33, 43, 9 },
            { MENU_CMD_NAV, -1, 11, 7, 13 },
            { MENU_CMD_TEXT_PTR, 0x0, a523 },
            MENU_CMD_END,
        },
        {
            { 45, 33, 43, 9 },
            { MENU_CMD_NAV, 10, 12, 8, 13 },
            { MENU_CMD_TEXT_PTR, 0x0, aAttack },
            MENU_CMD_END,
        },
        {
            { 90, 33, 43, 9 },
            { MENU_CMD_NAV, 11, -1, 9, 14 },
            { MENU_CMD_TEXT_PTR, 0x0, aDefend },
            MENU_CMD_END,
        },
        {
            { 0, 44, 65, 9 },
            { MENU_CMD_NAV, -1, 14, 10, 15 },
            { MENU_CMD_TEXT_PTR, 0x0, (char*)teamTacticsUSERA },
            MENU_CMD_END,
        },
        {
            { 67, 44, 66, 9 },
            { MENU_CMD_NAV, 13, -1, 12, 16 },
            { MENU_CMD_TEXT_PTR, 0x0, (char*)teamTacticsUSERB },
            MENU_CMD_END,
        },
        {
            { 0, 55, 65, 9 },
            { MENU_CMD_NAV, -1, 16, 13, 17 },
            { MENU_CMD_TEXT_PTR, 0x0, (char*)teamTacticsUSERC },
            MENU_CMD_END,
        },
        {
            { 67, 55, 66, 9 },
            { MENU_CMD_NAV, 15, -1, 14, 18 },
            { MENU_CMD_TEXT_PTR, 0x0, (char*)teamTacticsUSERD },
            MENU_CMD_END,
        },
        {
            { 0, 66, 65, 9 },
            { MENU_CMD_NAV, -1, 18, 15, 19 },
            { MENU_CMD_TEXT_PTR, 0x0, (char*)teamTacticsUSERE },
            MENU_CMD_END,
        },
        {
            { 67, 66, 66, 9 },
            { MENU_CMD_NAV, 17, -1, 16, 19 },
            { MENU_CMD_TEXT_PTR, 0x0, (char*)teamTacticsUSERF },
            MENU_CMD_END,
        } },
    MENU_MARK_INIT_ENTRY,
    {
        { 0, 78, -124, 9 },
        { MENU_CMD_NAV, -1, -1, 17, 20 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aLoad },
        { MENU_CMD_ONSELECT_FIRE, _LoadTactics },
        MENU_CMD_END,
    },
    { MENU_MARK_START_MENU, 0, 0 },
    {
        { 102, -71, 100, 15 },
        { MENU_CMD_NAV, -1, -1, 19, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _ExitImportTactics },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_Stadium stadiumMenu = {
    { _ShowStadiumInit, 0, 0, 0x3B },
    { MENU_MARK_START_MENU, 0, -15 },
    MENU_MARK_BYTE_COORDS,
    { {
          { -104, 0, 0, 10 },
          { MENU_CMD_TEXT_PTR, 0x10, 0 },
          MENU_CMD_END,
      },
        {
            { -104, 12, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x10, 0 },
            MENU_CMD_END,
        } },
    { 0, 28, -119, 12 },
    { MENU_CMD_COLORS, 0x5 },
    { MENU_CMD_TEXT_NO_PTR, 0x4010 },
    MENU_CMD_END,
    {
        { -114, 28, 20, 12 },
        { MENU_CMD_TEXT_PTR, 0x10, aV },
        MENU_CMD_END,
    },
    { -89, 28, -119, 12 },
    { MENU_CMD_COLORS, 0x5 },
    { MENU_CMD_TEXT_NO_PTR, 0x8010 },
    MENU_CMD_END,
    { MENU_MARK_START_MENU, -1, 35 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_BEFORE_DRAW, _StadiumBeforeDrawLeftTeam },
    MENU_CMD_END,
    { -114, 0, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -114, 13, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -114, 26, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -114, 39, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -114, 52, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -114, 65, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -114, 78, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -114, 91, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -114, 104, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -114, 117, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -114, -126, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 30 },
    { {
          { -124, 0, 0, 10 },
          { MENU_CMD_TEXT_PTR, 0x4000, 0 },
          MENU_CMD_END,
      },
        {
            { -124, 13, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4000, 0 },
            MENU_CMD_END,
        },
        {
            { -124, 26, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4000, 0 },
            MENU_CMD_END,
        },
        {
            { -124, 39, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4000, 0 },
            MENU_CMD_END,
        },
        {
            { -124, 52, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4000, 0 },
            MENU_CMD_END,
        },
        {
            { -124, 65, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4000, 0 },
            MENU_CMD_END,
        },
        {
            { -124, 78, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4000, 0 },
            MENU_CMD_END,
        },
        {
            { -124, 91, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4000, 0 },
            MENU_CMD_END,
        },
        {
            { -124, 104, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4000, 0 },
            MENU_CMD_END,
        },
        {
            { -124, 117, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4000, 0 },
            MENU_CMD_END,
        },
        {
            { -124, -126, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4000, 0 },
            MENU_CMD_END,
        },
        {
            { -124, -111, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4001, 0 },
            MENU_CMD_END,
        },
        {
            { -124, -102, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4001, 0 },
            MENU_CMD_END,
        },
        {
            { -124, -93, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4001, 0 },
            MENU_CMD_END,
        },
        {
            { -124, -84, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4001, 0 },
            MENU_CMD_END,
        },
        {
            { -124, -75, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x4001, 0 },
            MENU_CMD_END,
        } },
    { MENU_MARK_START_MENU, 0, 35 },
    MENU_MARK_START_TEMPLATE,
    { 0, 0, 0, 0 },
    { MENU_CMD_BEFORE_DRAW, _StadiumBeforeDrawRightTeam },
    MENU_CMD_END,
    { -94, 0, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -94, 13, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -94, 26, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -94, 39, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -94, 52, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -94, 65, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -94, 78, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -94, 91, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -94, 104, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -94, 117, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    { -94, -126, 0, 0 },
    { MENU_CMD_SPRITE_COPY, (char*)stadiumMenuLittlePlayer },
    MENU_CMD_END,
    MENU_MARK_INIT_ENTRY,
    { MENU_MARK_START_MENU, 0, 30 },
    { {
          { -84, 0, 0, 10 },
          { MENU_CMD_TEXT_PTR, 0x8000, 0 },
          MENU_CMD_END,
      },
        {
            { -84, 13, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            MENU_CMD_END,
        },
        {
            { -84, 26, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            MENU_CMD_END,
        },
        {
            { -84, 39, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            MENU_CMD_END,
        },
        {
            { -84, 52, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            MENU_CMD_END,
        },
        {
            { -84, 65, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            MENU_CMD_END,
        },
        {
            { -84, 78, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            MENU_CMD_END,
        },
        {
            { -84, 91, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            MENU_CMD_END,
        },
        {
            { -84, 104, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            MENU_CMD_END,
        },
        {
            { -84, 117, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            MENU_CMD_END,
        },
        {
            { -84, -126, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8000, 0 },
            MENU_CMD_END,
        },
        {
            { -84, -111, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8001, 0 },
            MENU_CMD_END,
        },
        {
            { -84, -102, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8001, 0 },
            MENU_CMD_END,
        },
        {
            { -84, -93, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8001, 0 },
            MENU_CMD_END,
        },
        {
            { -84, -84, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8001, 0 },
            MENU_CMD_END,
        },
        {
            { -84, -75, 0, 10 },
            { MENU_CMD_TEXT_PTR, 0x8001, 0 },
            MENU_CMD_END,
        } },
    {
        { 0, 0, 0, 0 },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};
Menu_Highlights highlightsMenu = {
    { _HighlightsMenuInit, _HighlightsMenuOnReturn, 0, 1 },
    MENU_MARK_BYTE_COORDS,
    {
        { 92, 0, 120, 15 },
        { MENU_CMD_COLORS, 0x17 },
        { MENU_CMD_TEXT_PTR, 0x0, aHighlights },
        MENU_CMD_END,
    },
    {
        { 82, 60, -116, 15 },
        { MENU_CMD_NAV, -1, -1, -1, 2 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aViewHighlights },
        { MENU_CMD_ONSELECT_FIRE, _JmpShowHighlights },
        MENU_CMD_END,
    },
    {
        { 82, 90, -116, 15 },
        { MENU_CMD_NAV, -1, -1, 1, 3 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aLoadHighlights },
        { MENU_CMD_ONSELECT_FIRE, _LoadHighlightsMenu },
        MENU_CMD_END,
    },
    {
        { 82, 120, -116, 15 },
        { MENU_CMD_NAV, -1, -1, 2, 4 },
        { MENU_CMD_COLORS, 0xE },
        { MENU_CMD_TEXT_PTR, 0x0, aSaveHighlights },
        { MENU_CMD_ONSELECT_FIRE, _SaveHighlightsMenu },
        MENU_CMD_END,
    },
    {
        { 102, -71, 100, 15 },
        { MENU_CMD_NAV, -1, -1, 3, -1 },
        { MENU_CMD_COLORS, 0xC },
        { MENU_CMD_TEXT_PTR, 0x0, aExit },
        { MENU_CMD_ONSELECT_FIRE, _ExitHighlights },
        MENU_CMD_END,
    },
    MENU_MARK_END_MENU,
};