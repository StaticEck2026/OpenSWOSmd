/*
 * src/swos_amiga_ptr_imports.c — auto-generated pointer tables.
 * Each entry maps an enum/index to a struct or string already defined
 * elsewhere in the codebase. Without these, callers read NULL and
 * segfault on the first dereference (same bug as positionsTable,
 * sidePitchSurnamePositionsTable, teamTacticsTable etc.).
 *
 * Generated from SWOS2.s; only tables whose targets are all defined
 * in our build are emitted here. Six tables (bottomBallOutOfPlayPositions,
 * topBallOutOfPlayPositions, kGoalieSkillTables, diskMenu,
 * diyStage1LegsStrTab, savedRandStatePtr) reference symbols not yet
 * ported and stay as zero stubs in swos_amiga_data_stubs.c.
 */
#include <stdint.h>

/* topSkillLettersEven/Odd — pointer tables that use base+offset syntax
 * in SWOS2.s (e.g. `dc.l aTspvhcfstpvhcf+$7`). The master string
 * aTspvhcfstpvhcf packs all per-position-group skill letters into one
 * contiguous buffer. The auto-extractor doesn't handle base+offset, so
 * these are hand-emitted. Without them _FormTopSkillsString reads NULL
 * and segfaults at letters[i]. */
extern uint16_t bigNumbersColorConversionTable[];
/* skinAndHairColorConversionTable: pointer table indexed by skin
 * color (0-2). Entries are byte-offsets ($60/$7A/$94) into the
 * bigNumbersColorConversionTable int16 array — i.e. int16 indices
 * 48/61/74. Without this _FillSkinColorConversionTable reads NULL
 * and segfaults. */
uint32_t skinAndHairColorConversionTable[3] = {
    (uint32_t)(uintptr_t)(bigNumbersColorConversionTable + 48),
    (uint32_t)(uintptr_t)(bigNumbersColorConversionTable + 61),
    (uint32_t)(uintptr_t)(bigNumbersColorConversionTable + 74),
};

extern const char aTspvhcfstpvhcf[];
uint32_t topSkillLettersEven[7] = {
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x00),
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x0E),
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x1C),
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x2A),
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x38),
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x46),
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x54),
};
uint32_t topSkillLettersOdd[7] = {
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x07),
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x15),
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x23),
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x31),
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x3F),
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x4D),
    (uint32_t)(uintptr_t)(aTspvhcfstpvhcf + 0x5B),
};

extern char a2ndReplay[];
extern char a2ndReplays[];
extern char a343[];
extern char a352[];
extern char a3MatchesToTurn[];
extern char a3rdPlayOff[];
extern char a3rdReplay[];
extern char a3rdReplays[];
extern char a3rd[];
extern char a424[];
extern char a433[];
extern char a442[];
extern char a451[];
extern char a523[];
extern char a532[];
extern char a541[];
extern char aAGoodSeasonAWe[];
extern char aAGoodSeasonAWe_0[];
extern char aAVeryDisappoin[];
extern char aAVeryDisappoin_0[];
extern char aA_1[];
extern char aAfr[];
extern char aAfr_0[];
extern char aAnUpAndDownSea[];
extern char aAnUpAndDownSea_0[];
extern char aAnyFurtherPlay[];
extern char aAnyFurtherPlay_0[];
extern char aAsi[];
extern char aAsi_0[];
extern char aAttack[];
extern char aBHasLeftTheClu[];
extern char aBHasLeftTheClu_0[];
extern char aBeforeHandingT[];
extern char aBeforeHandingT_0[];
extern char aBoxall[];
extern char aCanImproveOnTh[];
extern char aCanImproveOnTh_0[];
extern char aChapman[];
extern char aChris[];
extern char aClearTheOverdr[];
extern char aComeOnAWeCanno[];
extern char aD_0[];
extern char aDavid[];
extern char aDavid_0[];
extern char aDearsley[];
extern char aDecidedToRelie[];
extern char aDecidedToRelie_0[];
extern char aDefend[];
extern char aDemandsMuchBet[];
extern char aDistinguishedC[];
extern char aDistinguishedC_0[];
extern char aDiv1[];
extern char aDiv2[];
extern char aDiv3[];
extern char aDivision1_0[];
extern char aDivision2_0[];
extern char aDivision3_0[];
extern char aEnsureThatItIs[];
extern char aEnsureThatWeCl[];
extern char aEur[];
extern char aEuro[];
extern char aFifthRound[];
extern char aFinalGroup[];
extern char aFinalGroup_0[];
extern char aFinalGroups[];
extern char aFinalGroups_0[];
extern char aFinal[];
extern char aFinal_0[];
extern char aFinal_1[];
extern char aFinal_2[];
extern char aFinal_3[];
extern char aFirstRound[];
extern char aFollowingACris[];
extern char aFollowingAVote[];
extern char aFourthRound[];
extern char aG[];
extern char aGraeme[];
extern char aGrafsCjcteam1R[];
extern char aGrafsCjcteam2R[];
extern char aGrafsCjcteam3R[];
extern char aGrafsSwcpich1M[];
extern char aGrafsSwcpich2M[];
extern char aGrafsSwcpich3M[];
extern char aGrafsSwcpich4M[];
extern char aGrafsSwcpich5M[];
extern char aGrafsSwcpich6M[];
extern char aGroupA[];
extern char aGroupB[];
extern char aGroupC[];
extern char aGroupD[];
extern char aGroupE[];
extern char aGroupF[];
extern char aGroupG[];
extern char aGroupH[];
extern char aHare[];
extern char aHasJoinedTheCl[];
extern char aImproveOnOurPe[];
extern char aImproveTheTeam[];
extern char aItIsWithGreatR[];
extern char aItIsWithGreatR_0[];
extern char aItIsWithGreatR_1[];
extern char aJon[];
extern char aJustANoteToWis[];
extern char aJustANoteToWis_0[];
extern char aJustAReminderT[];
extern char aLb[];
extern char aLw[];
extern char aM[];
extern char aMark[];
extern char aMoreTimeToNego[];
extern char aMuchBetterFrom[];
extern char aMuchSuccessFro[];
extern char aNam[];
extern char aNam_0[];
extern char aNewSeason[];
extern char aNewSeason_0[];
extern char aNotAVeryGoodSe[];
extern char aNotAVeryGoodSe_0[];
extern char aNottingham[];
extern char aOce[];
extern char aOce_0[];
extern char aOrTheWageBillW[];
extern char aOrWeWillHaveTo[];
extern char aPleaseNoteATha[];
extern char aPleaseNoteATha_0[];
extern char aPleaseNoteATha_1[];
extern char aPleaseNoteATha_2[];
extern char aPleaseTakeNote[];
extern char aPleaseTakeNote_0[];
extern char aPleasedWithYou[];
extern char aPleasedWithYou_0[];
extern char aPremDiv[];
extern char aPremierDivisio[];
extern char aQF[];
extern char aQFinal[];
extern char aQuarterFinal[];
extern char aRb[];
extern char aRememberUnless[];
extern char aRenewYourContr[];
extern char aReplay[];
extern char aReplays[];
extern char aRnd1[];
extern char aRnd2[];
extern char aRnd3[];
extern char aRnd4[];
extern char aRnd5[];
extern char aRnd6[];
extern char aRound1[];
extern char aRound2[];
extern char aRound3[];
extern char aRound4[];
extern char aRound5[];
extern char aRound6[];
extern char aRw[];
extern char aSF[];
extern char aSFinal[];
extern char aSam[];
extern char aSam_0[];
extern char aSeasonInManage[];
extern char aSeasonInManage_0[];
extern char aSecondRound[];
extern char aSemiFinal[];
extern char aSixthRound[];
extern char aSorryAAreUnwil[];
extern char aSorryAAreUnwil_0[];
extern char aSorryAButTheCl[];
extern char aSorryAButTheDi[];
extern char aSuccessFromOur[];
extern char aSweep[];
extern char aThanksForAllYo[];
extern char aThanksForAllYo_0[];
extern char aThirdPlacePlay[];
extern char aThirdRound[];
extern char aThisSeasonAndW[];
extern char aThisSeasonAndW_0[];
extern char aUSER_A[];
extern char aUSER_B[];
extern char aUSER_C[];
extern char aUSER_D[];
extern char aUSER_E[];
extern char aUSER_F[];
extern char aWeAllHopeYouEn[];
extern char aWeAllHopeYouEn_0[];
extern char aWeAllWishYouTh[];
extern char aWeAllWishYouTh_0[];
extern char aWeHereAtAAreHa[];
extern char aWeHereAtAAreHa_0[];
extern char aWeWillHaveToRe[];
extern char aWelcomeAWeHope[];
extern char aWelcomeAWeLook[];
extern char aWelcomeToOurCl[];
extern char aWelcomeToOurCl_0[];
extern char aWellDoneAYouDi[];
extern char aWellDoneAYouDi_0[];
extern char aWilkin[];
extern char aYouWellInYourN[];
extern char aYouWellInYourN_0[];
extern char africaWorldCupQualificationCopy[];
extern char africanNationsCupCopy[];
extern char africanNationsCup[];
extern char allPlayerTeamsEqual[];
extern char asiaWorldCupQualificationCopy[];
extern char asianCupCopy[];
extern char asianCup[];
extern char autoReplays[];
extern char autoSaveHighlights[];
extern char blackPalette[];
extern char careerTeamFlagsBySeason0[];
extern char careerTeamFlagsBySeason1[];
extern char coachOrPlayer[];
extern char copaAmericaCopy[];
extern char copaAmerica[];
extern char countriesStack[];
extern char cupWinnersCupCopy[];
extern char cupWinnersCup[];
extern char diyCupEndMonthIndex[];
extern char diyCupExtraTimeChoice0[];
extern char diyCupExtraTimeChoice1[];
extern char diyCupExtraTimeChoice2[];
extern char diyCupExtraTimeChoice3[];
extern char diyCupExtraTimeChoice4[];
extern char diyCupExtraTimeChoice5[];
extern char diyCupLegsChoice0[];
extern char diyCupLegsChoice1[];
extern char diyCupLegsChoice2[];
extern char diyCupLegsChoice3[];
extern char diyCupLegsChoice4[];
extern char diyCupLegsChoice5[];
extern char diyCupPenaltiesChoice0[];
extern char diyCupPenaltiesChoice1[];
extern char diyCupPenaltiesChoice2[];
extern char diyCupPenaltiesChoice3[];
extern char diyCupPenaltiesChoice4[];
extern char diyCupPenaltiesChoice5[];
extern char diyCupPitchTypeOrSeasonSelection[];
extern char diyCupStartMonthIndex[];
extern char diyLeagueEndingMonth[];
extern char diyLeagueStartingMonth[];
extern char diyPitchTypeOrSeason_1[];
extern char diyTournamentEndMonthPage[];
extern char diyTournamentStartMonthPage[];
extern char diyTournamentUseSeasonalPitchEffective[];
extern char dryPalette[];
extern char ecQualifyingGroup1Copy[];
extern char ecQualifyingGroup2Copy[];
extern char ecQualifyingGroup3Copy[];
extern char ecQualifyingGroup4Copy[];
extern char ecQualifyingGroup5Copy[];
extern char ecQualifyingGroup6Copy[];
extern char ecQualifyingGroup7Copy[];
extern char ecQualifyingGroup8Copy[];
extern char euroCupCopy[];
extern char euroCupQualSeedBucket15[];
extern char euroCupQualSeedBucket18[];
extern char euroCupQualSeedBucket22[];
extern char euroCupQualSeedBucket22Full[];
extern char euroCupQualSeedList4[];
extern char euroCupQualSeedList6[];
extern char euroCupQualSeedList11[];
extern char euroCupQualSeedList22[];
extern char euroCupQualSeedListReduced4[];
extern char euroCupQualSeedListReduced6[];
extern char euroCupQualSeedListReduced11[];
extern char euroCupQualSeedListReduced22[];
extern char euroCup[];
extern char europeanChampionships2Copy[];
extern char europeanChampionships[];
extern char fixtureSchedule10Teams[];
extern char fixtureSchedule11Teams[];
extern char fixtureSchedule12Teams[];
extern char fixtureSchedule13Teams[];
extern char fixtureSchedule14Teams[];
extern char fixtureSchedule15Teams[];
extern char fixtureSchedule16Teams[];
extern char fixtureSchedule17Teams[];
extern char fixtureSchedule18Teams[];
extern char fixtureSchedule19Teams[];
extern char fixtureSchedule20Teams[];
extern char fixtureSchedule21Teams[];
extern char fixtureSchedule22Teams[];
extern char fixtureSchedule23Teams[];
extern char fixtureSchedule24Teams[];
extern char fixtureSchedule2Teams[];
extern char fixtureSchedule3Teams[];
extern char fixtureSchedule4Teams[];
extern char fixtureSchedule5Teams[];
extern char fixtureSchedule6Teams[];
extern char fixtureSchedule7Teams[];
extern char fixtureSchedule8Teams[];
extern char fixtureSchedule9Teams[];
extern char flipOnOff[];
extern char friendlyCopy[];
extern char friendly[];
extern char frozenPalette[];
extern char gameLength[];
extern char gamePalette[];
extern char goalScorerDist343[];
extern char goalScorerDist352[];
extern char goalScorerDist424[];
extern char goalScorerDist433[];
extern char goalScorerDist442[];
extern char goalScorerDist451[];
extern char goalScorerDist523[];
extern char goalScorerDist532[];
extern char goalScorerDist541[];
extern char goalScorerDistAttack[];
extern char goalScorerDistDefend[];
extern char goalScorerDistSweep[];
extern char hardPalette[];
extern char invitationTournamentCopy[];
extern char menuMusic[];
extern char menuPalette[];
extern char muddyPalette[];
extern char normalPalette[];
extern char northAmericaWorldCupQualificationCopy[];
extern char northAmericanCupCopy[];
extern char northAmericanCup[];
extern char oceaniaCupCopy[];
extern char oceaniaCup[];
extern char oceaniaWorldCupQualificationCopy[];
extern char panicPalette[];
extern char pitchType[];
extern char playerCardChances10min[];
extern char playerCardChances3min[];
extern char playerCardChances5min[];
extern char playerCardChances7min[];
extern char playerMarketFilterAgeGroup[];
extern char playerMarketFilterPosition[];
extern char positionGroupAttacker[];
extern char positionGroupDefender[];
extern char positionGroupGoalkeeper[];
extern char positionGroupMidfielder[];
extern char positions343[];
extern char positions352[];
extern char positions424[];
extern char positions433[];
extern char positions451[];
extern char positions523[];
extern char positions532[];
extern char positions541[];
extern char positionsAttack[];
extern char positionsDefault[];
extern char positionsDefend[];
extern char positionsSweep[];
extern char sidePitchPositionsAttack[];
extern char sidePitchPositionsDefault[];
extern char sidePitchPositionsDefend[];
extern char sidePitchPositionsSweep[];
extern char sidePitchPositions_3_4_3[];
extern char sidePitchPositions_3_5_2[];
extern char sidePitchPositions_4_2_4[];
extern char sidePitchPositions_4_3_3[];
extern char sidePitchPositions_4_5_1[];
extern char sidePitchPositions_5_2_3[];
extern char sidePitchPositions_5_3_2[];
extern char sidePitchPositions_5_4_1[];
extern char simExtraGoalProbThreshA[];
extern char simExtraGoalProbThreshB[];
extern char simExtraGoalProbThreshC[];
extern char simExtraGoalProbThreshD[];
extern char simExtraGoalProbThreshE[];
extern char simGoalProbThreshA[];
extern char simGoalProbThreshB[];
extern char simGoalProbThreshC[];
extern char simGoalProbThreshD[];
extern char simGoalProbThreshE[];
extern char skillBitRotEven1[];
extern char skillBitRotEven2[];
extern char skillBitRotEven3[];
extern char skillBitRotEven4[];
extern char skillBitRotEven5[];
extern char skillBitRotEven6[];
extern char skillBitRotEven7[];
extern char skillBitRotOdd1[];
extern char skillBitRotOdd2[];
extern char skillBitRotOdd3[];
extern char skillBitRotOdd4[];
extern char skillBitRotOdd5[];
extern char skillBitRotOdd6[];
extern char skillBitRotOdd7[];
extern char softPalette[];
extern char southAmericaWorldCupQualificationCopy[];
extern char tacticPosMap0[];
extern char tacticPosMap10[];
extern char tacticPosMap11[];
extern char tacticPosMap1[];
extern char tacticPosMap2[];
extern char tacticPosMap3[];
extern char tacticPosMap4[];
extern char tacticPosMap5[];
extern char tacticPosMap6[];
extern char tacticPosMap7[];
extern char tacticPosMap8[];
extern char tacticPosMap9[];
extern char titleLogoPalette[];
extern char titleScreenPalette[];
extern char titleTextPalette[];
extern char tourCopy[];
extern char trainingPosOrder0[];
extern char trainingPosOrder1[];
extern char trainingPosOrder2[];
extern char trainingPosOrder3[];
extern char trainingPosOrder4[];
extern char trainingPosOrder5[];
extern char trainingPosOrder6[];
extern char trainingPosOrder7[];
extern char uefaCupCopy[];
extern char uefaCup[];
extern char wetPalette[];
extern char worldCup2Copy[];
extern char worldCupQualifyingGroup1Copy[];
extern char worldCupQualifyingGroup2Copy[];
extern char worldCupQualifyingGroup3Copy[];
extern char worldCupQualifyingGroup4Copy[];
extern char worldCupQualifyingGroup5Copy[];
extern char worldCupQualifyingGroup6Copy[];
extern char worldCupQualifyingGroup7Copy[];
extern char worldCupQualifyingGroup8Copy[];
extern char worldCup[];

uint32_t pitchPalettePtrs[7] = {
    (uint32_t)(uintptr_t)frozenPalette,
    (uint32_t)(uintptr_t)muddyPalette,
    (uint32_t)(uintptr_t)wetPalette,
    (uint32_t)(uintptr_t)softPalette,
    (uint32_t)(uintptr_t)normalPalette,
    (uint32_t)(uintptr_t)dryPalette,
    (uint32_t)(uintptr_t)hardPalette,
};
uint32_t swcPitchMapFiles[6] = {
    (uint32_t)(uintptr_t)aGrafsSwcpich1M,
    (uint32_t)(uintptr_t)aGrafsSwcpich2M,
    (uint32_t)(uintptr_t)aGrafsSwcpich3M,
    (uint32_t)(uintptr_t)aGrafsSwcpich4M,
    (uint32_t)(uintptr_t)aGrafsSwcpich5M,
    (uint32_t)(uintptr_t)aGrafsSwcpich6M,
};
uint32_t paletteTable[7] = {
    (uint32_t)(uintptr_t)menuPalette,
    (uint32_t)(uintptr_t)blackPalette,
    (uint32_t)(uintptr_t)gamePalette,
    (uint32_t)(uintptr_t)titleScreenPalette,
    (uint32_t)(uintptr_t)panicPalette,
    (uint32_t)(uintptr_t)titleTextPalette,
    (uint32_t)(uintptr_t)titleLogoPalette,
};
uint32_t playerCardChancesPerGameLength[4] = {
    (uint32_t)(uintptr_t)playerCardChances3min,
    (uint32_t)(uintptr_t)playerCardChances5min,
    (uint32_t)(uintptr_t)playerCardChances7min,
    (uint32_t)(uintptr_t)playerCardChances10min,
};
uint32_t defaultNamesTable[6] = {
    (uint32_t)(uintptr_t)aChris,
    (uint32_t)(uintptr_t)aJon,
    (uint32_t)(uintptr_t)aGraeme,
    (uint32_t)(uintptr_t)aMark,
    (uint32_t)(uintptr_t)aDavid,
    (uint32_t)(uintptr_t)aDavid_0,
};
uint32_t defaultSurnamesTable[6] = {
    (uint32_t)(uintptr_t)aChapman,
    (uint32_t)(uintptr_t)aHare,
    (uint32_t)(uintptr_t)aBoxall,
    (uint32_t)(uintptr_t)aDearsley,
    (uint32_t)(uintptr_t)aWilkin,
    (uint32_t)(uintptr_t)aNottingham,
};
uint32_t simGoalProbByTacticPtrs[18] = {
    (uint32_t)(uintptr_t)simGoalProbThreshA,
    (uint32_t)(uintptr_t)simGoalProbThreshA,
    (uint32_t)(uintptr_t)simGoalProbThreshB,
    (uint32_t)(uintptr_t)simGoalProbThreshB,
    (uint32_t)(uintptr_t)simGoalProbThreshC,
    (uint32_t)(uintptr_t)simGoalProbThreshC,
    (uint32_t)(uintptr_t)simGoalProbThreshC,
    (uint32_t)(uintptr_t)simGoalProbThreshD,
    (uint32_t)(uintptr_t)simGoalProbThreshD,
    (uint32_t)(uintptr_t)simGoalProbThreshD,
    (uint32_t)(uintptr_t)simGoalProbThreshE,
    (uint32_t)(uintptr_t)simGoalProbThreshE,
    (uint32_t)(uintptr_t)simGoalProbThreshE,
    (uint32_t)(uintptr_t)simGoalProbThreshE,
    (uint32_t)(uintptr_t)simGoalProbThreshE,
    (uint32_t)(uintptr_t)simGoalProbThreshE,
    (uint32_t)(uintptr_t)simGoalProbThreshE,
    (uint32_t)(uintptr_t)simGoalProbThreshE,
};
uint32_t simExtraGoalProbByTacticPtrs[16] = {
    (uint32_t)(uintptr_t)simExtraGoalProbThreshA,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshA,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshB,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshB,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshC,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshC,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshC,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshD,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshD,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshD,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshE,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshE,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshE,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshE,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshE,
    (uint32_t)(uintptr_t)simExtraGoalProbThreshE,
};
uint32_t goalScorerDistributionByBallOutTactic[18] = {
    (uint32_t)(uintptr_t)goalScorerDist442,
    (uint32_t)(uintptr_t)goalScorerDist541,
    (uint32_t)(uintptr_t)goalScorerDist451,
    (uint32_t)(uintptr_t)goalScorerDist532,
    (uint32_t)(uintptr_t)goalScorerDist352,
    (uint32_t)(uintptr_t)goalScorerDist433,
    (uint32_t)(uintptr_t)goalScorerDist424,
    (uint32_t)(uintptr_t)goalScorerDist343,
    (uint32_t)(uintptr_t)goalScorerDistSweep,
    (uint32_t)(uintptr_t)goalScorerDist523,
    (uint32_t)(uintptr_t)goalScorerDistAttack,
    (uint32_t)(uintptr_t)goalScorerDistDefend,
    (uint32_t)(uintptr_t)goalScorerDist442,
    (uint32_t)(uintptr_t)goalScorerDist442,
    (uint32_t)(uintptr_t)goalScorerDist442,
    (uint32_t)(uintptr_t)goalScorerDist442,
    (uint32_t)(uintptr_t)goalScorerDist442,
    (uint32_t)(uintptr_t)goalScorerDist442,
};
uint32_t careerTeamFlagsByRegionTable[6] = {
    (uint32_t)(uintptr_t)careerTeamFlagsBySeason0,
    (uint32_t)(uintptr_t)careerTeamFlagsBySeason1,
    (uint32_t)(uintptr_t)careerTeamFlagsBySeason1,
    (uint32_t)(uintptr_t)careerTeamFlagsBySeason1,
    (uint32_t)(uintptr_t)careerTeamFlagsBySeason1,
    (uint32_t)(uintptr_t)careerTeamFlagsBySeason1,
};
uint32_t trainingPosOrderTables[8] = {
    (uint32_t)(uintptr_t)trainingPosOrder0,
    (uint32_t)(uintptr_t)trainingPosOrder1,
    (uint32_t)(uintptr_t)trainingPosOrder2,
    (uint32_t)(uintptr_t)trainingPosOrder3,
    (uint32_t)(uintptr_t)trainingPosOrder4,
    (uint32_t)(uintptr_t)trainingPosOrder5,
    (uint32_t)(uintptr_t)trainingPosOrder6,
    (uint32_t)(uintptr_t)trainingPosOrder7,
};
/* ---- String tables for MENU_CMD_STRING_TABLE entries ----------------
 * Layout (per SWOS2.s and the C unpacker in _DrawMenuItem):
 *   +0  uint32_t  indexPtr     ; pointer to a uint16_t holding current index
 *   +4  uint16_t  base         ; subtracted from current to get list offset
 *   +6  uint32_t  entries[N]   ; pointers to the strings to display
 *
 * The auto-extractor that originally produced this file emitted only the
 * first 4-byte field, so every STRING_TABLE-typed menu entry crashed in
 * _DrawMenuItem when it tried to follow the (out-of-bounds) entries[]
 * array. Hand-rebuild each one from SWOS2.s with the exact same byte
 * layout via a packed struct. */
extern const char aJan[], aFeb[], aMar[], aApr[], aMay[], aJun[];
extern const char aJul[], aAug[], aSep[], aOct[], aNov[], aDec[];

/* These are already extern'd as `char[]` earlier in this file by the
 * auto-extractor. We just take their addresses below — the unpacker
 * reads 2 bytes at the address as a uint16 index, regardless of the
 * declared element type. */
extern uint8_t  diyCompetitionStart[];
extern const char aPitchType[], aSeason_1[];
extern const char aRandom[], aFrozen[], aMuddy[], aWet[], aSoft[];
extern const char aNormal[], aDry[], aHard[];

#define MONTHS \
    (uint32_t)(uintptr_t)aJan, (uint32_t)(uintptr_t)aFeb, \
    (uint32_t)(uintptr_t)aMar, (uint32_t)(uintptr_t)aApr, \
    (uint32_t)(uintptr_t)aMay, (uint32_t)(uintptr_t)aJun, \
    (uint32_t)(uintptr_t)aJul, (uint32_t)(uintptr_t)aAug, \
    (uint32_t)(uintptr_t)aSep, (uint32_t)(uintptr_t)aOct, \
    (uint32_t)(uintptr_t)aNov, (uint32_t)(uintptr_t)aDec

extern char diyPitchTypeOrSeason_1[];
struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[2];
} diyPitchTypeOrSeasonStrTable = {
    (uint32_t)(uintptr_t)diyPitchTypeOrSeason_1, 0,
    { (uint32_t)(uintptr_t)aPitchType, (uint32_t)(uintptr_t)aSeason_1 }
};

/* diyLeaguePitchTypeStrTable: index var is `diyCompetitionStart + 0x36`,
 * base = -1 (so index 0 → entries[1] = "FROZEN" while index -1 → "RANDOM"). */
struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[8];
} diyLeaguePitchTypeStrTable = {
    (uint32_t)(uintptr_t)(diyCompetitionStart + 0x36), (uint16_t)-1,
    {
        (uint32_t)(uintptr_t)aRandom, (uint32_t)(uintptr_t)aFrozen,
        (uint32_t)(uintptr_t)aMuddy,  (uint32_t)(uintptr_t)aWet,
        (uint32_t)(uintptr_t)aSoft,   (uint32_t)(uintptr_t)aNormal,
        (uint32_t)(uintptr_t)aDry,    (uint32_t)(uintptr_t)aHard,
    }
};

struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[12];
} diyLeagueMonthsStrTable = {
    (uint32_t)(uintptr_t)diyLeagueStartingMonth, 0,
    { MONTHS }
};

struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[12];
} diyLeagueEndingMonthStrTable = {
    (uint32_t)(uintptr_t)diyLeagueEndingMonth, 0,
    { MONTHS }
};

/* DIY CUP / Tournament — same auto-extractor truncation as the
 * other StringTable structs, recovered from SWOS2.s lines 70279,
 * 70320, 75328. */
extern uint8_t diyFileBufferCopy[];
extern const char aAfter90Mins[], aAfterExtraTime[];
extern const char aOff[];
extern const char aRandom[], aFrozen[], aMuddy[], aWet[];
extern const char aSoft[], aNormal[], aDry[], aHard[];

#define PITCH_8 \
    (uint32_t)(uintptr_t)aRandom, (uint32_t)(uintptr_t)aFrozen, \
    (uint32_t)(uintptr_t)aMuddy,  (uint32_t)(uintptr_t)aWet, \
    (uint32_t)(uintptr_t)aSoft,   (uint32_t)(uintptr_t)aNormal, \
    (uint32_t)(uintptr_t)aDry,    (uint32_t)(uintptr_t)aHard

/* diyCupPitchTypeStringTable: indexPtr = `diyCompetitionStart + 0x36`
 * = `&diyCompetitionStart.base.pitchType`; base = -1; 8 entries. */
struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[8];
} diyCupPitchTypeStringTable = {
    (uint32_t)(uintptr_t)(diyCompetitionStart + 0x36), (uint16_t)-1,
    { PITCH_8 }
};

/* diyCupAwayGoalsStringTable: indexPtr = `diyCompetitionStart + 0x5E`
 * (which our struct names `playEachTeam` — Amiga reuses that field
 * for AWAY-GOALS-MODE in cup configurations); base = 0; 3 entries. */
struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[3];
} diyCupAwayGoalsStringTable = {
    (uint32_t)(uintptr_t)(diyCompetitionStart + 0x5E), 0,
    {
        (uint32_t)(uintptr_t)aOff,
        (uint32_t)(uintptr_t)aAfter90Mins,
        (uint32_t)(uintptr_t)aAfterExtraTime,
    }
};

/* diyPitchTypeStringTable: indexPtr = `diyFileBufferCopy + 0x36`. */
struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[8];
} diyPitchTypeStringTable = {
    (uint32_t)(uintptr_t)(diyFileBufferCopy + 0x36), (uint16_t)-1,
    { PITCH_8 }
};

#undef PITCH_8

uint32_t groupStringTable[8] = {
    (uint32_t)(uintptr_t)aGroupA,
    (uint32_t)(uintptr_t)aGroupB,
    (uint32_t)(uintptr_t)aGroupC,
    (uint32_t)(uintptr_t)aGroupD,
    (uint32_t)(uintptr_t)aGroupE,
    (uint32_t)(uintptr_t)aGroupF,
    (uint32_t)(uintptr_t)aGroupG,
    (uint32_t)(uintptr_t)aGroupH,
};
/* ---- DIY Cup string tables (SWOS2.s lines 70274..70433) ----
 * All 21 of these were previously emitted as 4-byte stubs holding
 * just the indexPtr; the unpacker reads `tablePtr + 6 + idx*4` and
 * walked off the end into unmapped memory. Recovered with proper
 * packed structs below. */
extern char diyCupPitchTypeOrSeasonSelection[];
extern char diyCupStartMonthIndex[];
extern char diyCupEndMonthIndex[];
extern char diyCupLegsChoice0[], diyCupLegsChoice1[], diyCupLegsChoice2[];
extern char diyCupLegsChoice3[], diyCupLegsChoice4[], diyCupLegsChoice5[];
extern char diyCupExtraTimeChoice0[], diyCupExtraTimeChoice1[], diyCupExtraTimeChoice2[];
extern char diyCupExtraTimeChoice3[], diyCupExtraTimeChoice4[], diyCupExtraTimeChoice5[];
extern char diyCupPenaltiesChoice0[], diyCupPenaltiesChoice1[], diyCupPenaltiesChoice2[];
extern char diyCupPenaltiesChoice3[], diyCupPenaltiesChoice4[], diyCupPenaltiesChoice5[];
extern const char a1Leg[], a2Legs[];
extern const char aNoExtraTime[], aExtraTime[], aETIfReplay[];
extern const char aNoPenalties[], aPenalties[], aPensIfReplay[];
extern const char aPitchType[], aSeason_1[];

#define MONTHS \
    (uint32_t)(uintptr_t)aJan, (uint32_t)(uintptr_t)aFeb, \
    (uint32_t)(uintptr_t)aMar, (uint32_t)(uintptr_t)aApr, \
    (uint32_t)(uintptr_t)aMay, (uint32_t)(uintptr_t)aJun, \
    (uint32_t)(uintptr_t)aJul, (uint32_t)(uintptr_t)aAug, \
    (uint32_t)(uintptr_t)aSep, (uint32_t)(uintptr_t)aOct, \
    (uint32_t)(uintptr_t)aNov, (uint32_t)(uintptr_t)aDec
#define LEGS_3 \
    (uint32_t)(uintptr_t)a1Leg, (uint32_t)(uintptr_t)a1Leg, (uint32_t)(uintptr_t)a2Legs
#define EXTRATIME_3 \
    (uint32_t)(uintptr_t)aNoExtraTime, (uint32_t)(uintptr_t)aExtraTime, \
    (uint32_t)(uintptr_t)aETIfReplay
#define PENALTIES_3 \
    (uint32_t)(uintptr_t)aNoPenalties, (uint32_t)(uintptr_t)aPenalties, \
    (uint32_t)(uintptr_t)aPensIfReplay

struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[2];
} diyCupPitchTypeOrSeasonStringTable = {
    (uint32_t)(uintptr_t)diyCupPitchTypeOrSeasonSelection, 0,
    { (uint32_t)(uintptr_t)aPitchType, (uint32_t)(uintptr_t)aSeason_1 }
};

struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[12];
} diyCupStartMonthStringTable = {
    (uint32_t)(uintptr_t)diyCupStartMonthIndex, 0, { MONTHS }
};
struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[12];
} diyCupEndMonthStringTable = {
    (uint32_t)(uintptr_t)diyCupEndMonthIndex, 0, { MONTHS }
};

#define DEFINE_LEGS(N) \
struct __attribute__((packed)) { \
    uint32_t indexPtr; uint16_t base; uint32_t entries[3]; \
} diyCupLegsStringTable##N = { (uint32_t)(uintptr_t)diyCupLegsChoice##N, 0, { LEGS_3 } }
DEFINE_LEGS(0); DEFINE_LEGS(1); DEFINE_LEGS(2);
DEFINE_LEGS(3); DEFINE_LEGS(4); DEFINE_LEGS(5);
#undef DEFINE_LEGS

#define DEFINE_ET(N) \
struct __attribute__((packed)) { \
    uint32_t indexPtr; uint16_t base; uint32_t entries[3]; \
} diyCupExtraTimeStringTable##N = { (uint32_t)(uintptr_t)diyCupExtraTimeChoice##N, 0, { EXTRATIME_3 } }
DEFINE_ET(0); DEFINE_ET(1); DEFINE_ET(2);
DEFINE_ET(3); DEFINE_ET(4); DEFINE_ET(5);
#undef DEFINE_ET

#define DEFINE_PEN(N) \
struct __attribute__((packed)) { \
    uint32_t indexPtr; uint16_t base; uint32_t entries[3]; \
} diyCupPenaltiesStringTable##N = { (uint32_t)(uintptr_t)diyCupPenaltiesChoice##N, 0, { PENALTIES_3 } }
DEFINE_PEN(0); DEFINE_PEN(1); DEFINE_PEN(2);
DEFINE_PEN(3); DEFINE_PEN(4); DEFINE_PEN(5);
#undef DEFINE_PEN

#undef MONTHS
#undef LEGS_3
#undef EXTRATIME_3
#undef PENALTIES_3
/* ---- options-menu string tables ----
 * The earlier extern block in this file declared several of these as
 * `extern char foo[]` from the auto-extractor. STRING_TABLE indexPtr
 * is a 4-byte pointer regardless of the variable's element type, so
 * we just take the address — the C unpacker reads 2 bytes at that
 * address as a uint16_t index. Don't redeclare the type here. */
extern const char aSeasonal[];
extern const char a3Mins[], a5Mins[], a7Mins[], a10Mins[];
extern const char aOff[], aOn[];

#define ON_OFF_TABLE(NAME, VAR) \
struct __attribute__((packed)) { \
    uint32_t indexPtr; uint16_t base; uint32_t entries[2]; \
} NAME = { \
    (uint32_t)(uintptr_t)(VAR), 0, \
    { (uint32_t)(uintptr_t)aOff, (uint32_t)(uintptr_t)aOn } \
}

/* pitchType menu: base = -2 so index -2 → "SEASONAL", -1 → "RANDOM",
 * 0..6 → FROZEN..HARD (matches SWOS2.s line 73164). */
struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[9];
} pitchTypeStrTable = {
    (uint32_t)(uintptr_t)pitchType, (uint16_t)-2,
    {
        (uint32_t)(uintptr_t)aSeasonal, (uint32_t)(uintptr_t)aRandom,
        (uint32_t)(uintptr_t)aFrozen,   (uint32_t)(uintptr_t)aMuddy,
        (uint32_t)(uintptr_t)aWet,      (uint32_t)(uintptr_t)aSoft,
        (uint32_t)(uintptr_t)aNormal,   (uint32_t)(uintptr_t)aDry,
        (uint32_t)(uintptr_t)aHard,
    }
};

struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[4];
} gameLengthStrTable = {
    (uint32_t)(uintptr_t)gameLength, 0,
    {
        (uint32_t)(uintptr_t)a3Mins,  (uint32_t)(uintptr_t)a5Mins,
        (uint32_t)(uintptr_t)a7Mins,  (uint32_t)(uintptr_t)a10Mins,
    }
};

ON_OFF_TABLE(autoReplaysStrTable,     autoReplays);
ON_OFF_TABLE(allPlTeamsEqualStrTable, allPlayerTeamsEqual);
ON_OFF_TABLE(autoSaveHlStrTable,      autoSaveHighlights);
ON_OFF_TABLE(menuMusicStrTable,       menuMusic);

#undef ON_OFF_TABLE
#undef MONTHS
/* ---- DIY Tournament string tables (SWOS2.s lines 75323..75554) ----
 * 34 packed StringTable structs that the auto-extractor truncated to
 * 4-byte stubs (3 here as `[1]` arrays, 31 as scalars in
 * swos_data_typed_stubs.c). Each table is indexPtr + base + entries[]
 * the way the unpacker expects (see project_string_table_truncation.md). */
/* Note: the auto-extracted block earlier in this file already extern'd
 * diyTournamentUseSeasonalPitchEffective / StartMonthPage / EndMonthPage
 * as `char[]`. The address-of works regardless of element type, so just
 * reuse those forward declarations (no re-extern here). */
extern uint8_t  diyTournamentStages[];   /* DIYStage[6] = 60 bytes */
extern const char a2PtsForAWin[], a3PtsForAWin[];
extern const char aPlayEachTeamX1[], aPlayEachTeamX2[], aPlayEachTeamX3[], aPlayEachTeamX4[];

#define MONTHS \
    (uint32_t)(uintptr_t)aJan, (uint32_t)(uintptr_t)aFeb, \
    (uint32_t)(uintptr_t)aMar, (uint32_t)(uintptr_t)aApr, \
    (uint32_t)(uintptr_t)aMay, (uint32_t)(uintptr_t)aJun, \
    (uint32_t)(uintptr_t)aJul, (uint32_t)(uintptr_t)aAug, \
    (uint32_t)(uintptr_t)aSep, (uint32_t)(uintptr_t)aOct, \
    (uint32_t)(uintptr_t)aNov, (uint32_t)(uintptr_t)aDec
#define LEGS_3 \
    (uint32_t)(uintptr_t)a1Leg, (uint32_t)(uintptr_t)a1Leg, (uint32_t)(uintptr_t)a2Legs
#define EXTRATIME_3 \
    (uint32_t)(uintptr_t)aNoExtraTime, (uint32_t)(uintptr_t)aExtraTime, (uint32_t)(uintptr_t)aETIfReplay
#define PENALTIES_3 \
    (uint32_t)(uintptr_t)aNoPenalties, (uint32_t)(uintptr_t)aPenalties, (uint32_t)(uintptr_t)aPensIfReplay
#define PTS_2 \
    (uint32_t)(uintptr_t)a2PtsForAWin, (uint32_t)(uintptr_t)a3PtsForAWin
#define PLAYCOUNT_4 \
    (uint32_t)(uintptr_t)aPlayEachTeamX1, (uint32_t)(uintptr_t)aPlayEachTeamX2, \
    (uint32_t)(uintptr_t)aPlayEachTeamX3, (uint32_t)(uintptr_t)aPlayEachTeamX4

struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[2];
} diyPitchSeasonStrTab = {
    (uint32_t)(uintptr_t)diyTournamentUseSeasonalPitchEffective, 0,
    { (uint32_t)(uintptr_t)aPitchType, (uint32_t)(uintptr_t)aSeason_1 }
};
struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[12];
} diyStartMonthStrTab = {
    (uint32_t)(uintptr_t)diyTournamentStartMonthPage, 0, { MONTHS }
};
struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[12];
} diyEndMonthStrTab = {
    (uint32_t)(uintptr_t)diyTournamentEndMonthPage, 0, { MONTHS }
};
/* diyAwayGoalsStrTab: indexPtr = diyFileBufferCopy + 0x4A. */
struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[3];
} diyAwayGoalsStrTab = {
    (uint32_t)(uintptr_t)(diyFileBufferCopy + 0x4A), 0,
    {
        (uint32_t)(uintptr_t)aOff,
        (uint32_t)(uintptr_t)aAfter90Mins,
        (uint32_t)(uintptr_t)aAfterExtraTime,
    }
};

/* Six-stage tournament tables. Each stage is 10 bytes (DIYStage):
 *   +0 legs, +2 extraTime, +4 penalties, +6 ptsForWin, +8 playCount.
 * (NB our DIYStage struct names ptsForWin/playCount in the OPPOSITE
 * order — the byte offsets here match the Amiga layout regardless.) */
#define STAGE_PTR(N, OFF)  ((uint8_t *)diyTournamentStages + (N) * 0xA + (OFF))

#define DEFINE_LEGS(N) \
struct __attribute__((packed)) { uint32_t indexPtr; uint16_t base; uint32_t entries[3]; } \
diyStage##N##LegsStrTab = { (uint32_t)(uintptr_t)STAGE_PTR((N) - 1, 0), 0, { LEGS_3 } }
DEFINE_LEGS(1); DEFINE_LEGS(2); DEFINE_LEGS(3);
DEFINE_LEGS(4); DEFINE_LEGS(5); DEFINE_LEGS(6);
#undef DEFINE_LEGS

#define DEFINE_ET(N) \
struct __attribute__((packed)) { uint32_t indexPtr; uint16_t base; uint32_t entries[3]; } \
diyStage##N##ExtraTimeStrTab = { (uint32_t)(uintptr_t)STAGE_PTR((N) - 1, 2), 0, { EXTRATIME_3 } }
DEFINE_ET(1); DEFINE_ET(2); DEFINE_ET(3);
DEFINE_ET(4); DEFINE_ET(5); DEFINE_ET(6);
#undef DEFINE_ET

#define DEFINE_PEN(N) \
struct __attribute__((packed)) { uint32_t indexPtr; uint16_t base; uint32_t entries[3]; } \
diyStage##N##PenaltiesStrTab = { (uint32_t)(uintptr_t)STAGE_PTR((N) - 1, 4), 0, { PENALTIES_3 } }
DEFINE_PEN(1); DEFINE_PEN(2); DEFINE_PEN(3);
DEFINE_PEN(4); DEFINE_PEN(5); DEFINE_PEN(6);
#undef DEFINE_PEN

/* PtsForWin: indexPtr at stage offset +6, base = 2 (index 2 → "2 PTS"). */
#define DEFINE_PTS(N) \
struct __attribute__((packed)) { uint32_t indexPtr; uint16_t base; uint32_t entries[2]; } \
diyStage##N##PtsForWinStrTab = { (uint32_t)(uintptr_t)STAGE_PTR((N) - 1, 6), 2, { PTS_2 } }
DEFINE_PTS(1); DEFINE_PTS(2); DEFINE_PTS(3);
DEFINE_PTS(4); DEFINE_PTS(5); DEFINE_PTS(6);
#undef DEFINE_PTS

/* PlayCount: indexPtr at stage offset +8, base = 1, 4 entries. */
#define DEFINE_PLAY(N) \
struct __attribute__((packed)) { uint32_t indexPtr; uint16_t base; uint32_t entries[4]; } \
diyStage##N##PlayCountStrTab = { (uint32_t)(uintptr_t)STAGE_PTR((N) - 1, 8), 1, { PLAYCOUNT_4 } }
DEFINE_PLAY(1); DEFINE_PLAY(2); DEFINE_PLAY(3);
DEFINE_PLAY(4); DEFINE_PLAY(5); DEFINE_PLAY(6);
#undef DEFINE_PLAY

#undef STAGE_PTR
#undef MONTHS
#undef LEGS_3
#undef EXTRATIME_3
#undef PENALTIES_3
#undef PTS_2
#undef PLAYCOUNT_4
uint32_t topSkillBitRotEven[7] = {
    (uint32_t)(uintptr_t)skillBitRotEven1,
    (uint32_t)(uintptr_t)skillBitRotEven2,
    (uint32_t)(uintptr_t)skillBitRotEven3,
    (uint32_t)(uintptr_t)skillBitRotEven4,
    (uint32_t)(uintptr_t)skillBitRotEven5,
    (uint32_t)(uintptr_t)skillBitRotEven6,
    (uint32_t)(uintptr_t)skillBitRotEven7,
};
uint32_t topSkillBitRotOdd[7] = {
    (uint32_t)(uintptr_t)skillBitRotOdd1,
    (uint32_t)(uintptr_t)skillBitRotOdd2,
    (uint32_t)(uintptr_t)skillBitRotOdd3,
    (uint32_t)(uintptr_t)skillBitRotOdd4,
    (uint32_t)(uintptr_t)skillBitRotOdd5,
    (uint32_t)(uintptr_t)skillBitRotOdd6,
    (uint32_t)(uintptr_t)skillBitRotOdd7,
};
uint32_t tacticPosMapPtrs[18] = {
    (uint32_t)(uintptr_t)tacticPosMap0,
    (uint32_t)(uintptr_t)tacticPosMap1,
    (uint32_t)(uintptr_t)tacticPosMap2,
    (uint32_t)(uintptr_t)tacticPosMap3,
    (uint32_t)(uintptr_t)tacticPosMap4,
    (uint32_t)(uintptr_t)tacticPosMap5,
    (uint32_t)(uintptr_t)tacticPosMap6,
    (uint32_t)(uintptr_t)tacticPosMap7,
    (uint32_t)(uintptr_t)tacticPosMap8,
    (uint32_t)(uintptr_t)tacticPosMap9,
    (uint32_t)(uintptr_t)tacticPosMap10,
    (uint32_t)(uintptr_t)tacticPosMap11,
    (uint32_t)(uintptr_t)tacticPosMap0,
    (uint32_t)(uintptr_t)tacticPosMap0,
    (uint32_t)(uintptr_t)tacticPosMap0,
    (uint32_t)(uintptr_t)tacticPosMap0,
    (uint32_t)(uintptr_t)tacticPosMap0,
    (uint32_t)(uintptr_t)tacticPosMap0,
};
/* offsetCoachOrPlayer: career-menu COACH / PLAYER-COACH selector
 * (SWOS2.s:85269). Auto-extractor emitted only the indexPtr; the
 * STRING_TABLE layout actually needs `{ indexPtr, base, entries[N] }`
 * packed, otherwise the menu reads past the indexPtr into adjacent
 * globals when picking a string. */
extern const char aCoach_0[], aPlayerCoach_0[];
struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[2];
} offsetCoachOrPlayer = {
    (uint32_t)(uintptr_t)coachOrPlayer, 0,
    {
        (uint32_t)(uintptr_t)aCoach_0,
        (uint32_t)(uintptr_t)aPlayerCoach_0,
    }
};
uint32_t countryStackPtr[1] = {
    (uint32_t)(uintptr_t)countriesStack,
};
uint32_t sellExchangeStrings[4] = {
    (uint32_t)(uintptr_t)aWeHereAtAAreHa,
    (uint32_t)(uintptr_t)aWeHereAtAAreHa_0,
    (uint32_t)(uintptr_t)aSorryAAreUnwil,
    (uint32_t)(uintptr_t)aSorryAAreUnwil_0,
};
uint32_t positionFilterStringTable[1] = {
    (uint32_t)(uintptr_t)playerMarketFilterPosition,
};
uint32_t skillFilterStringTable[1] = {
    (uint32_t)(uintptr_t)playerMarketFilterAgeGroup,
};
uint32_t positionGroupTable[4] = {
    (uint32_t)(uintptr_t)positionGroupGoalkeeper,
    (uint32_t)(uintptr_t)positionGroupDefender,
    (uint32_t)(uintptr_t)positionGroupMidfielder,
    (uint32_t)(uintptr_t)positionGroupAttacker,
};
uint32_t sidePitchSurnamePositionsTable[18] = {
    (uint32_t)(uintptr_t)sidePitchPositionsDefault,
    (uint32_t)(uintptr_t)sidePitchPositions_5_4_1,
    (uint32_t)(uintptr_t)sidePitchPositions_4_5_1,
    (uint32_t)(uintptr_t)sidePitchPositions_5_3_2,
    (uint32_t)(uintptr_t)sidePitchPositions_3_5_2,
    (uint32_t)(uintptr_t)sidePitchPositions_4_3_3,
    (uint32_t)(uintptr_t)sidePitchPositions_4_2_4,
    (uint32_t)(uintptr_t)sidePitchPositions_3_4_3,
    (uint32_t)(uintptr_t)sidePitchPositionsSweep,
    (uint32_t)(uintptr_t)sidePitchPositions_5_2_3,
    (uint32_t)(uintptr_t)sidePitchPositionsAttack,
    (uint32_t)(uintptr_t)sidePitchPositionsDefend,
    (uint32_t)(uintptr_t)sidePitchPositionsDefault,
    (uint32_t)(uintptr_t)sidePitchPositionsDefault,
    (uint32_t)(uintptr_t)sidePitchPositionsDefault,
    (uint32_t)(uintptr_t)sidePitchPositionsDefault,
    (uint32_t)(uintptr_t)sidePitchPositionsDefault,
    (uint32_t)(uintptr_t)sidePitchPositionsDefault,
};
uint32_t clubMessagesTable[76] = {
    (uint32_t)(uintptr_t)aSorryAButTheCl,
    (uint32_t)(uintptr_t)aAnyFurtherPlay,
    (uint32_t)(uintptr_t)aComeOnAWeCanno,
    (uint32_t)(uintptr_t)aOrTheWageBillW,
    (uint32_t)(uintptr_t)aSorryAButTheDi,
    (uint32_t)(uintptr_t)aMoreTimeToNego,
    (uint32_t)(uintptr_t)aPleaseNoteATha_2,
    (uint32_t)(uintptr_t)aAnyFurtherPlay_0,
    (uint32_t)(uintptr_t)aPleaseTakeNote,
    (uint32_t)(uintptr_t)aEnsureThatWeCl,
    (uint32_t)(uintptr_t)aJustAReminderT,
    (uint32_t)(uintptr_t)aOrWeWillHaveTo,
    (uint32_t)(uintptr_t)aFollowingACris,
    (uint32_t)(uintptr_t)aClearTheOverdr,
    (uint32_t)(uintptr_t)aItIsWithGreatR_0,
    (uint32_t)(uintptr_t)aDecidedToRelie,
    (uint32_t)(uintptr_t)aPleaseTakeNote_0,
    (uint32_t)(uintptr_t)aEnsureThatItIs,
    (uint32_t)(uintptr_t)aRememberUnless,
    (uint32_t)(uintptr_t)aWeWillHaveToRe,
    (uint32_t)(uintptr_t)aFollowingAVote,
    (uint32_t)(uintptr_t)a3MatchesToTurn,
    (uint32_t)(uintptr_t)aItIsWithGreatR_1,
    (uint32_t)(uintptr_t)aDecidedToRelie_0,
    (uint32_t)(uintptr_t)aWellDoneAYouDi_0,
    (uint32_t)(uintptr_t)aThisSeasonAndW_0,
    (uint32_t)(uintptr_t)aAGoodSeasonAWe_0,
    (uint32_t)(uintptr_t)aPleasedWithYou_0,
    (uint32_t)(uintptr_t)aAnUpAndDownSea_0,
    (uint32_t)(uintptr_t)aCanImproveOnTh_0,
    (uint32_t)(uintptr_t)aNotAVeryGoodSe_0,
    (uint32_t)(uintptr_t)aImproveTheTeam,
    (uint32_t)(uintptr_t)aAVeryDisappoin_0,
    (uint32_t)(uintptr_t)aDemandsMuchBet,
    (uint32_t)(uintptr_t)aThanksForAllYo_0,
    (uint32_t)(uintptr_t)aYouWellInYourN_0,
    (uint32_t)(uintptr_t)aWelcomeToOurCl,
    (uint32_t)(uintptr_t)aMuchSuccessFro,
    (uint32_t)(uintptr_t)aWelcomeToOurCl_0,
    (uint32_t)(uintptr_t)aSeasonInManage_0,
    (uint32_t)(uintptr_t)aJustANoteToWis_0,
    (uint32_t)(uintptr_t)aNewSeason_0,
    (uint32_t)(uintptr_t)aWeAllHopeYouEn_0,
    (uint32_t)(uintptr_t)aBeforeHandingT_0,
    (uint32_t)(uintptr_t)aWeAllWishYouTh_0,
    (uint32_t)(uintptr_t)aDistinguishedC_0,
    (uint32_t)(uintptr_t)aPleaseNoteATha,
    (uint32_t)(uintptr_t)aBHasLeftTheClu,
    (uint32_t)(uintptr_t)aPleaseNoteATha_0,
    (uint32_t)(uintptr_t)aBHasLeftTheClu_0,
    (uint32_t)(uintptr_t)aPleaseNoteATha_1,
    (uint32_t)(uintptr_t)aHasJoinedTheCl,
    (uint32_t)(uintptr_t)aWellDoneAYouDi,
    (uint32_t)(uintptr_t)aThisSeasonAndW,
    (uint32_t)(uintptr_t)aAGoodSeasonAWe,
    (uint32_t)(uintptr_t)aPleasedWithYou,
    (uint32_t)(uintptr_t)aAnUpAndDownSea,
    (uint32_t)(uintptr_t)aCanImproveOnTh,
    (uint32_t)(uintptr_t)aNotAVeryGoodSe,
    (uint32_t)(uintptr_t)aImproveOnOurPe,
    (uint32_t)(uintptr_t)aAVeryDisappoin,
    (uint32_t)(uintptr_t)aMuchBetterFrom,
    (uint32_t)(uintptr_t)aThanksForAllYo,
    (uint32_t)(uintptr_t)aYouWellInYourN,
    (uint32_t)(uintptr_t)aWelcomeAWeLook,
    (uint32_t)(uintptr_t)aSuccessFromOur,
    (uint32_t)(uintptr_t)aWelcomeAWeHope,
    (uint32_t)(uintptr_t)aSeasonInManage,
    (uint32_t)(uintptr_t)aJustANoteToWis,
    (uint32_t)(uintptr_t)aNewSeason,
    (uint32_t)(uintptr_t)aWeAllHopeYouEn,
    (uint32_t)(uintptr_t)aBeforeHandingT,
    (uint32_t)(uintptr_t)aWeAllWishYouTh,
    (uint32_t)(uintptr_t)aDistinguishedC,
    (uint32_t)(uintptr_t)aItIsWithGreatR,
    (uint32_t)(uintptr_t)aRenewYourContr,
};
/* flipStrTable: edit-tactics tactic-side flip selector. SWOS2.s:112717
 * (flipOnOff already extern'd above as char[]; just take its address.) */
extern const char aFlipOff[], aFlipOn[];
extern char aUSER_A[], aUSER_B[], aUSER_C[];
struct __attribute__((packed)) {
    uint32_t indexPtr; uint16_t base; uint32_t entries[5];
} flipStrTable = {
    (uint32_t)(uintptr_t)flipOnOff, 0,
    {
        (uint32_t)(uintptr_t)aFlipOff, (uint32_t)(uintptr_t)aFlipOn,
        (uint32_t)(uintptr_t)aUSER_A,  (uint32_t)(uintptr_t)aUSER_B,
        (uint32_t)(uintptr_t)aUSER_C,
    }
};
uint32_t europeanCupsandNationalTeamCompetitionsAndQualifications[46] = {
    (uint32_t)(uintptr_t)worldCup,
    (uint32_t)(uintptr_t)europeanChampionships,
    (uint32_t)(uintptr_t)copaAmerica,
    (uint32_t)(uintptr_t)euroCup,
    (uint32_t)(uintptr_t)euroCupCopy,
    (uint32_t)(uintptr_t)cupWinnersCup,
    (uint32_t)(uintptr_t)cupWinnersCupCopy,
    (uint32_t)(uintptr_t)uefaCup,
    (uint32_t)(uintptr_t)uefaCupCopy,
    (uint32_t)(uintptr_t)copaAmericaCopy,
    (uint32_t)(uintptr_t)africanNationsCup,
    (uint32_t)(uintptr_t)africanNationsCupCopy,
    (uint32_t)(uintptr_t)northAmericanCup,
    (uint32_t)(uintptr_t)northAmericanCupCopy,
    (uint32_t)(uintptr_t)asianCup,
    (uint32_t)(uintptr_t)asianCupCopy,
    (uint32_t)(uintptr_t)oceaniaCup,
    (uint32_t)(uintptr_t)oceaniaCupCopy,
    (uint32_t)(uintptr_t)europeanChampionships2Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup1Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup2Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup3Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup4Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup5Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup6Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup7Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup8Copy,
    (uint32_t)(uintptr_t)worldCup2Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup1Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup2Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup3Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup4Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup5Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup6Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup7Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup8Copy,
    (uint32_t)(uintptr_t)northAmericaWorldCupQualificationCopy,
    (uint32_t)(uintptr_t)southAmericaWorldCupQualificationCopy,
    (uint32_t)(uintptr_t)africaWorldCupQualificationCopy,
    (uint32_t)(uintptr_t)asiaWorldCupQualificationCopy,
    (uint32_t)(uintptr_t)oceaniaWorldCupQualificationCopy,
    (uint32_t)(uintptr_t)invitationTournamentCopy,
    (uint32_t)(uintptr_t)tourCopy,
    (uint32_t)(uintptr_t)friendly,
    (uint32_t)(uintptr_t)friendlyCopy,
    0xFFFFFFFFu,
};
uint32_t nationalTeamCompetitionsAndQualifications1[39] = {
    (uint32_t)(uintptr_t)worldCup,
    (uint32_t)(uintptr_t)worldCup2Copy,
    (uint32_t)(uintptr_t)europeanChampionships,
    (uint32_t)(uintptr_t)europeanChampionships2Copy,
    (uint32_t)(uintptr_t)copaAmerica,
    (uint32_t)(uintptr_t)copaAmericaCopy,
    (uint32_t)(uintptr_t)africanNationsCup,
    (uint32_t)(uintptr_t)africanNationsCupCopy,
    (uint32_t)(uintptr_t)northAmericanCup,
    (uint32_t)(uintptr_t)northAmericanCupCopy,
    (uint32_t)(uintptr_t)asianCup,
    (uint32_t)(uintptr_t)asianCupCopy,
    (uint32_t)(uintptr_t)oceaniaCup,
    (uint32_t)(uintptr_t)oceaniaCupCopy,
    (uint32_t)(uintptr_t)friendlyCopy,
    (uint32_t)(uintptr_t)ecQualifyingGroup1Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup2Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup3Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup4Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup5Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup6Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup7Copy,
    (uint32_t)(uintptr_t)ecQualifyingGroup8Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup1Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup2Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup3Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup4Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup5Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup6Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup7Copy,
    (uint32_t)(uintptr_t)worldCupQualifyingGroup8Copy,
    (uint32_t)(uintptr_t)northAmericaWorldCupQualificationCopy,
    (uint32_t)(uintptr_t)southAmericaWorldCupQualificationCopy,
    (uint32_t)(uintptr_t)africaWorldCupQualificationCopy,
    (uint32_t)(uintptr_t)asiaWorldCupQualificationCopy,
    (uint32_t)(uintptr_t)oceaniaWorldCupQualificationCopy,
    (uint32_t)(uintptr_t)invitationTournamentCopy,
    (uint32_t)(uintptr_t)tourCopy,
    0xFFFFFFFFu,
};
/* Each Amiga euroCupQual* "group seed list" table is a sequence of
 * 8-byte entries plus a 4-byte 0xFFFFFFFF terminator. Per SWOS2.s
 * (line 125058 etc.):
 *
 *   dc.l <seedListPtr>     ; 4 bytes BE pointer
 *   dc.b 0,sz,0,cnt        ; size and copy-count as 16-bit BE words
 *   ... repeated ...
 *   dc.b $FF,$FF,$FF,$FF   ; terminator
 *
 * _Career_FillRandomTeamListExcludingSelected reads (ptr, size, count)
 * tuples and breaks on 0xFFFFFFFF. The previous SDL stub had only the
 * first ptr and no metadata or terminator, so the function read garbage
 * size/count words, ran the inner shuffle on bogus indices, and SIGSEGV'd
 * walking off the end. Reconstruct the table as a packed C struct array
 * with terminator. Host-order writes work directly because the function
 * uses host uint16_t / uint32_t reads. */
typedef struct __attribute__((packed)) SeedListGroupEntry {
    uint32_t ptr;
    uint16_t size;
    uint16_t count;
} SeedListGroupEntry;

SeedListGroupEntry euroCupQualGroupSeedLists_4_6_11[5] = {
    { (uint32_t)(uintptr_t)euroCupQualSeedList4,  4,  4 },
    { (uint32_t)(uintptr_t)euroCupQualSeedList6,  6,  5 },
    { (uint32_t)(uintptr_t)euroCupQualSeedList11, 11, 5 },
    { (uint32_t)(uintptr_t)euroCupQualSeedList22, 22, 2 },
    { 0xFFFFFFFFu, 0, 0 },
};

SeedListGroupEntry euroCupQualGroupSeedListsReduced_4_6_11[5] = {
    { (uint32_t)(uintptr_t)euroCupQualSeedListReduced4,  4,  4  },
    { (uint32_t)(uintptr_t)euroCupQualSeedListReduced6,  6,  6  },
    { (uint32_t)(uintptr_t)euroCupQualSeedListReduced11, 11, 11 },
    { (uint32_t)(uintptr_t)euroCupQualSeedListReduced22, 22, 11 },
    { 0xFFFFFFFFu, 0, 0 },
};

SeedListGroupEntry euroCupQualGroupSeedBuckets_15_18_22[5] = {
    { (uint32_t)(uintptr_t)euroCupQualSeedBucket15,     15, 10 },
    { (uint32_t)(uintptr_t)euroCupQualSeedBucket18,     18, 10 },
    { (uint32_t)(uintptr_t)euroCupQualSeedBucket22,     22, 8  },
    { (uint32_t)(uintptr_t)euroCupQualSeedBucket22Full, 22, 4  },
    { 0xFFFFFFFFu, 0, 0 },
};
uint32_t positionsTable[18] = {
    (uint32_t)(uintptr_t)positionsDefault,
    (uint32_t)(uintptr_t)positions541,
    (uint32_t)(uintptr_t)positions451,
    (uint32_t)(uintptr_t)positions532,
    (uint32_t)(uintptr_t)positions352,
    (uint32_t)(uintptr_t)positions433,
    (uint32_t)(uintptr_t)positions424,
    (uint32_t)(uintptr_t)positions343,
    (uint32_t)(uintptr_t)positionsSweep,
    (uint32_t)(uintptr_t)positions523,
    (uint32_t)(uintptr_t)positionsAttack,
    (uint32_t)(uintptr_t)positionsDefend,
    (uint32_t)(uintptr_t)positionsDefault,
    (uint32_t)(uintptr_t)positionsDefault,
    (uint32_t)(uintptr_t)positionsDefault,
    (uint32_t)(uintptr_t)positionsDefault,
    (uint32_t)(uintptr_t)positionsDefault,
    (uint32_t)(uintptr_t)positionsDefault,
};
uint32_t divisionStringTable[4] = {
    (uint32_t)(uintptr_t)aPremierDivisio,
    (uint32_t)(uintptr_t)aDivision1_0,
    (uint32_t)(uintptr_t)aDivision2_0,
    (uint32_t)(uintptr_t)aDivision3_0,
};
uint32_t divStringTable[4] = {
    (uint32_t)(uintptr_t)aPremDiv,
    (uint32_t)(uintptr_t)aDiv1,
    (uint32_t)(uintptr_t)aDiv2,
    (uint32_t)(uintptr_t)aDiv3,
};
uint32_t cupRoundStringTable[12] = {
    (uint32_t)(uintptr_t)aFirstRound,
    (uint32_t)(uintptr_t)aSecondRound,
    (uint32_t)(uintptr_t)aThirdRound,
    (uint32_t)(uintptr_t)aFourthRound,
    (uint32_t)(uintptr_t)aFifthRound,
    (uint32_t)(uintptr_t)aSixthRound,
    (uint32_t)(uintptr_t)aQuarterFinal,
    (uint32_t)(uintptr_t)aSemiFinal,
    (uint32_t)(uintptr_t)aFinal_2,
    (uint32_t)(uintptr_t)aFinalGroup,
    (uint32_t)(uintptr_t)aThirdPlacePlay,
    (uint32_t)(uintptr_t)aFinalGroups,
};
uint32_t roundStringTable[12] = {
    (uint32_t)(uintptr_t)aRound1,
    (uint32_t)(uintptr_t)aRound2,
    (uint32_t)(uintptr_t)aRound3,
    (uint32_t)(uintptr_t)aRound4,
    (uint32_t)(uintptr_t)aRound5,
    (uint32_t)(uintptr_t)aRound6,
    (uint32_t)(uintptr_t)aQFinal,
    (uint32_t)(uintptr_t)aSFinal,
    (uint32_t)(uintptr_t)aFinal_3,
    (uint32_t)(uintptr_t)aFinalGroup_0,
    (uint32_t)(uintptr_t)a3rdPlayOff,
    (uint32_t)(uintptr_t)aFinalGroups_0,
};
uint32_t rndStringTable[15] = {
    (uint32_t)(uintptr_t)aRnd1,
    (uint32_t)(uintptr_t)aRnd2,
    (uint32_t)(uintptr_t)aRnd3,
    (uint32_t)(uintptr_t)aRnd4,
    (uint32_t)(uintptr_t)aRnd5,
    (uint32_t)(uintptr_t)aRnd6,
    (uint32_t)(uintptr_t)aQF,
    (uint32_t)(uintptr_t)aSF,
    (uint32_t)(uintptr_t)aFinal,
    (uint32_t)(uintptr_t)aFinal_0,
    (uint32_t)(uintptr_t)a3rd,
    (uint32_t)(uintptr_t)aFinal_1,
    (uint32_t)(uintptr_t)aReplay,
    (uint32_t)(uintptr_t)a2ndReplay,
    (uint32_t)(uintptr_t)a3rdReplay,
};
uint32_t replayStringTable[3] = {
    (uint32_t)(uintptr_t)aReplays,
    (uint32_t)(uintptr_t)a2ndReplays,
    (uint32_t)(uintptr_t)a3rdReplays,
};
uint32_t playerPositionsStringTable[20] = {
    (uint32_t)(uintptr_t)aG,
    (uint32_t)(uintptr_t)aRb,
    (uint32_t)(uintptr_t)aLb,
    (uint32_t)(uintptr_t)aD_0,
    (uint32_t)(uintptr_t)aRw,
    (uint32_t)(uintptr_t)aLw,
    (uint32_t)(uintptr_t)aM,
    (uint32_t)(uintptr_t)aA_1,
    (uint32_t)(uintptr_t)aEur,
    (uint32_t)(uintptr_t)aNam,
    (uint32_t)(uintptr_t)aSam,
    (uint32_t)(uintptr_t)aAsi,
    (uint32_t)(uintptr_t)aOce,
    (uint32_t)(uintptr_t)aAfr,
    (uint32_t)(uintptr_t)aEuro,
    (uint32_t)(uintptr_t)aNam_0,
    (uint32_t)(uintptr_t)aSam_0,
    (uint32_t)(uintptr_t)aAsi_0,
    (uint32_t)(uintptr_t)aOce_0,
    (uint32_t)(uintptr_t)aAfr_0,
};
uint32_t tacticsStringTable[18] = {
    (uint32_t)(uintptr_t)a442,
    (uint32_t)(uintptr_t)a541,
    (uint32_t)(uintptr_t)a451,
    (uint32_t)(uintptr_t)a532,
    (uint32_t)(uintptr_t)a352,
    (uint32_t)(uintptr_t)a433,
    (uint32_t)(uintptr_t)a424,
    (uint32_t)(uintptr_t)a343,
    (uint32_t)(uintptr_t)aSweep,
    (uint32_t)(uintptr_t)a523,
    (uint32_t)(uintptr_t)aAttack,
    (uint32_t)(uintptr_t)aDefend,
    (uint32_t)(uintptr_t)aUSER_A,
    (uint32_t)(uintptr_t)aUSER_B,
    (uint32_t)(uintptr_t)aUSER_C,
    (uint32_t)(uintptr_t)aUSER_D,
    (uint32_t)(uintptr_t)aUSER_E,
    (uint32_t)(uintptr_t)aUSER_F,
};
uint32_t fixtureSchedulePtrTable[23] = {
    (uint32_t)(uintptr_t)fixtureSchedule2Teams,
    (uint32_t)(uintptr_t)fixtureSchedule3Teams,
    (uint32_t)(uintptr_t)fixtureSchedule4Teams,
    (uint32_t)(uintptr_t)fixtureSchedule5Teams,
    (uint32_t)(uintptr_t)fixtureSchedule6Teams,
    (uint32_t)(uintptr_t)fixtureSchedule7Teams,
    (uint32_t)(uintptr_t)fixtureSchedule8Teams,
    (uint32_t)(uintptr_t)fixtureSchedule9Teams,
    (uint32_t)(uintptr_t)fixtureSchedule10Teams,
    (uint32_t)(uintptr_t)fixtureSchedule11Teams,
    (uint32_t)(uintptr_t)fixtureSchedule12Teams,
    (uint32_t)(uintptr_t)fixtureSchedule13Teams,
    (uint32_t)(uintptr_t)fixtureSchedule14Teams,
    (uint32_t)(uintptr_t)fixtureSchedule15Teams,
    (uint32_t)(uintptr_t)fixtureSchedule16Teams,
    (uint32_t)(uintptr_t)fixtureSchedule17Teams,
    (uint32_t)(uintptr_t)fixtureSchedule18Teams,
    (uint32_t)(uintptr_t)fixtureSchedule19Teams,
    (uint32_t)(uintptr_t)fixtureSchedule20Teams,
    (uint32_t)(uintptr_t)fixtureSchedule21Teams,
    (uint32_t)(uintptr_t)fixtureSchedule22Teams,
    (uint32_t)(uintptr_t)fixtureSchedule23Teams,
    (uint32_t)(uintptr_t)fixtureSchedule24Teams,
};
uint32_t teamFilenamesTable[4] = {
    (uint32_t)(uintptr_t)aGrafsCjcteam1R,
    (uint32_t)(uintptr_t)aGrafsCjcteam2R,
    (uint32_t)(uintptr_t)aGrafsCjcteam1R,
    (uint32_t)(uintptr_t)aGrafsCjcteam3R,
};
