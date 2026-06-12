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
#include "swos_types.h"
#include "game_types.h"   /* DIYTournamentState (diyFileBufferCopy typed struct) */

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
void *skinAndHairColorConversionTable[3] = {
    (bigNumbersColorConversionTable + 48),
    (bigNumbersColorConversionTable + 61),
    (bigNumbersColorConversionTable + 74),
};

extern const char aTspvhcfstpvhcf[];
void *topSkillLettersEven[7] = {
    ((void *)aTspvhcfstpvhcf + 0x00),
    ((void *)aTspvhcfstpvhcf + 0x0E),
    ((void *)aTspvhcfstpvhcf + 0x1C),
    ((void *)aTspvhcfstpvhcf + 0x2A),
    ((void *)aTspvhcfstpvhcf + 0x38),
    ((void *)aTspvhcfstpvhcf + 0x46),
    ((void *)aTspvhcfstpvhcf + 0x54),
};
void *topSkillLettersOdd[7] = {
    ((void *)aTspvhcfstpvhcf + 0x07),
    ((void *)aTspvhcfstpvhcf + 0x15),
    ((void *)aTspvhcfstpvhcf + 0x23),
    ((void *)aTspvhcfstpvhcf + 0x31),
    ((void *)aTspvhcfstpvhcf + 0x3F),
    ((void *)aTspvhcfstpvhcf + 0x4D),
    ((void *)aTspvhcfstpvhcf + 0x5B),
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
extern uint16_t dryPalette[3];
extern char ecQualifyingGroup1Copy[];
extern char ecQualifyingGroup2Copy[];
extern char ecQualifyingGroup3Copy[];
extern char ecQualifyingGroup4Copy[];
extern char ecQualifyingGroup5Copy[];
extern char ecQualifyingGroup6Copy[];
extern char ecQualifyingGroup7Copy[];
extern char ecQualifyingGroup8Copy[];
extern char euroCupCopy[];
/* euroCupQualSeedBucket15/18/22/22Full: now euroCupQualSeedBucketBlock.bucketN
 * (EuroCupQualSeedBucketBlock in game_types.h, included above). */
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
extern uint16_t frozenPalette[3];
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
extern uint16_t hardPalette[3];
extern char invitationTournamentCopy[];
extern char menuMusic[];
extern char menuPalette[];
extern uint16_t muddyPalette[3];
extern uint16_t normalPalette[3];
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
extern uint16_t softPalette[3];
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
extern uint16_t wetPalette[3];
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

const uint16_t *pitchPalettePtrs[7] = {
    frozenPalette,
    muddyPalette,
    wetPalette,
    softPalette,
    normalPalette,
    dryPalette,
    hardPalette,
};
void *swcPitchMapFiles[6] = {
    aGrafsSwcpich1M,
    aGrafsSwcpich2M,
    aGrafsSwcpich3M,
    aGrafsSwcpich4M,
    aGrafsSwcpich5M,
    aGrafsSwcpich6M,
};
void *paletteTable[7] = {
    menuPalette,
    blackPalette,
    gamePalette,
    titleScreenPalette,
    panicPalette,
    titleTextPalette,
    titleLogoPalette,
};
void *playerCardChancesPerGameLength[4] = {
    playerCardChances3min,
    playerCardChances5min,
    playerCardChances7min,
    playerCardChances10min,
};
void *defaultNamesTable[6] = {
    aChris,
    aJon,
    aGraeme,
    aMark,
    aDavid,
    aDavid_0,
};
void *defaultSurnamesTable[6] = {
    aChapman,
    aHare,
    aBoxall,
    aDearsley,
    aWilkin,
    aNottingham,
};
void *simGoalProbByTacticPtrs[18] = {
    simGoalProbThreshA,
    simGoalProbThreshA,
    simGoalProbThreshB,
    simGoalProbThreshB,
    simGoalProbThreshC,
    simGoalProbThreshC,
    simGoalProbThreshC,
    simGoalProbThreshD,
    simGoalProbThreshD,
    simGoalProbThreshD,
    simGoalProbThreshE,
    simGoalProbThreshE,
    simGoalProbThreshE,
    simGoalProbThreshE,
    simGoalProbThreshE,
    simGoalProbThreshE,
    simGoalProbThreshE,
    simGoalProbThreshE,
};
void *simExtraGoalProbByTacticPtrs[16] = {
    simExtraGoalProbThreshA,
    simExtraGoalProbThreshA,
    simExtraGoalProbThreshB,
    simExtraGoalProbThreshB,
    simExtraGoalProbThreshC,
    simExtraGoalProbThreshC,
    simExtraGoalProbThreshC,
    simExtraGoalProbThreshD,
    simExtraGoalProbThreshD,
    simExtraGoalProbThreshD,
    simExtraGoalProbThreshE,
    simExtraGoalProbThreshE,
    simExtraGoalProbThreshE,
    simExtraGoalProbThreshE,
    simExtraGoalProbThreshE,
    simExtraGoalProbThreshE,
};
void *goalScorerDistributionByBallOutTactic[18] = {
    goalScorerDist442,
    goalScorerDist541,
    goalScorerDist451,
    goalScorerDist532,
    goalScorerDist352,
    goalScorerDist433,
    goalScorerDist424,
    goalScorerDist343,
    goalScorerDistSweep,
    goalScorerDist523,
    goalScorerDistAttack,
    goalScorerDistDefend,
    goalScorerDist442,
    goalScorerDist442,
    goalScorerDist442,
    goalScorerDist442,
    goalScorerDist442,
    goalScorerDist442,
};
void *careerTeamFlagsByRegionTable[6] = {
    careerTeamFlagsBySeason0,
    careerTeamFlagsBySeason1,
    careerTeamFlagsBySeason1,
    careerTeamFlagsBySeason1,
    careerTeamFlagsBySeason1,
    careerTeamFlagsBySeason1,
};
void *trainingPosOrderTables[8] = {
    trainingPosOrder0,
    trainingPosOrder1,
    trainingPosOrder2,
    trainingPosOrder3,
    trainingPosOrder4,
    trainingPosOrder5,
    trainingPosOrder6,
    trainingPosOrder7,
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
    (void *)aJan, (void *)aFeb, \
    (void *)aMar, (void *)aApr, \
    (void *)aMay, (void *)aJun, \
    (void *)aJul, (void *)aAug, \
    (void *)aSep, (void *)aOct, \
    (void *)aNov, (void *)aDec

extern char diyPitchTypeOrSeason_1[];
struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[2];
} diyPitchTypeOrSeasonStrTable = {
    diyPitchTypeOrSeason_1, 0,
    { (void *)aPitchType, (void *)aSeason_1 }
};

/* diyLeaguePitchTypeStrTable: index var is `diyCompetitionStart.base.pitchType`
 * (asm `diyCompetitionStart + 0x36`; the raw 32-bit offset read numTeams on
 * 64-bit, giving an out-of-bounds entries[] read — typed field is arch-safe).
 * base = -1 (so index 0 → entries[1] = "FROZEN" while index -1 → "RANDOM"). */
struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[8];
} diyLeaguePitchTypeStrTable = {
    (void *)&((DIYCompetitionEx *)diyCompetitionStart)->base.pitchType, (uint16_t)-1,
    {
        (void *)aRandom, (void *)aFrozen,
        (void *)aMuddy,  (void *)aWet,
        (void *)aSoft,   (void *)aNormal,
        (void *)aDry,    (void *)aHard,
    }
};

struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[12];
} diyLeagueMonthsStrTable = {
    diyLeagueStartingMonth, 0,
    { MONTHS }
};

struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[12];
} diyLeagueEndingMonthStrTable = {
    diyLeagueEndingMonth, 0,
    { MONTHS }
};

/* DIY CUP / Tournament — same auto-extractor truncation as the
 * other StringTable structs, recovered from SWOS2.s lines 70279,
 * 70320, 75328. */
extern DIYTournamentState diyFileBufferCopy;
extern const char aAfter90Mins[], aAfterExtraTime[];
extern const char aOff[];
extern const char aRandom[], aFrozen[], aMuddy[], aWet[];
extern const char aSoft[], aNormal[], aDry[], aHard[];

#define PITCH_8 \
    (void *)aRandom, (void *)aFrozen, \
    (void *)aMuddy,  (void *)aWet, \
    (void *)aSoft,   (void *)aNormal, \
    (void *)aDry,    (void *)aHard

/* diyCupPitchTypeStringTable: indexPtr = `diyCompetitionStart + 0x36`
 * = `&diyCompetitionStart.base.pitchType`; base = -1; 8 entries. */
struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[8];
} diyCupPitchTypeStringTable = {
    (void *)&((DIYCompetitionEx *)diyCompetitionStart)->base.pitchType, (uint16_t)-1,
    { PITCH_8 }
};

/* diyCupAwayGoalsStringTable: indexPtr = `diyCompetitionStart + 0x5E`
 * (which our struct names `playEachTeam` — Amiga reuses that field
 * for AWAY-GOALS-MODE in cup configurations); base = 0; 3 entries. */
struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[3];
} diyCupAwayGoalsStringTable = {
    (void *)&((DIYCompetitionEx *)diyCompetitionStart)->base.playEachTeam, 0,
    {
        (void *)aOff,
        (void *)aAfter90Mins,
        (void *)aAfterExtraTime,
    }
};

/* diyPitchTypeStringTable: indexPtr = `diyFileBufferCopy + 0x36`. */
struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[8];
} diyPitchTypeStringTable = {
    ((void *)&diyFileBufferCopy.header.pitchType), (uint16_t)-1,
    { PITCH_8 }
};

#undef PITCH_8

void *groupStringTable[8] = {
    aGroupA,
    aGroupB,
    aGroupC,
    aGroupD,
    aGroupE,
    aGroupF,
    aGroupG,
    aGroupH,
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
    (void *)aJan, (void *)aFeb, \
    (void *)aMar, (void *)aApr, \
    (void *)aMay, (void *)aJun, \
    (void *)aJul, (void *)aAug, \
    (void *)aSep, (void *)aOct, \
    (void *)aNov, (void *)aDec
#define LEGS_3 \
    (void *)a1Leg, (void *)a1Leg, (void *)a2Legs
#define EXTRATIME_3 \
    (void *)aNoExtraTime, (void *)aExtraTime, \
    (void *)aETIfReplay
#define PENALTIES_3 \
    (void *)aNoPenalties, (void *)aPenalties, \
    (void *)aPensIfReplay

struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[2];
} diyCupPitchTypeOrSeasonStringTable = {
    diyCupPitchTypeOrSeasonSelection, 0,
    { (void *)aPitchType, (void *)aSeason_1 }
};

struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[12];
} diyCupStartMonthStringTable = {
    diyCupStartMonthIndex, 0, { MONTHS }
};
struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[12];
} diyCupEndMonthStringTable = {
    diyCupEndMonthIndex, 0, { MONTHS }
};

#define DEFINE_LEGS(N) \
struct __attribute__((packed)) { \
    void    *indexPtr; uint16_t base; void    *entries[3]; \
} diyCupLegsStringTable##N = { diyCupLegsChoice##N, 0, { LEGS_3 } }
DEFINE_LEGS(0); DEFINE_LEGS(1); DEFINE_LEGS(2);
DEFINE_LEGS(3); DEFINE_LEGS(4); DEFINE_LEGS(5);
#undef DEFINE_LEGS

#define DEFINE_ET(N) \
struct __attribute__((packed)) { \
    void    *indexPtr; uint16_t base; void    *entries[3]; \
} diyCupExtraTimeStringTable##N = { diyCupExtraTimeChoice##N, 0, { EXTRATIME_3 } }
DEFINE_ET(0); DEFINE_ET(1); DEFINE_ET(2);
DEFINE_ET(3); DEFINE_ET(4); DEFINE_ET(5);
#undef DEFINE_ET

#define DEFINE_PEN(N) \
struct __attribute__((packed)) { \
    void    *indexPtr; uint16_t base; void    *entries[3]; \
} diyCupPenaltiesStringTable##N = { diyCupPenaltiesChoice##N, 0, { PENALTIES_3 } }
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
    void    *indexPtr; uint16_t base; void    *entries[2]; \
} NAME = { \
    (VAR), 0, \
    { (void *)aOff, (void *)aOn } \
}

/* pitchType menu: base = -2 so index -2 → "SEASONAL", -1 → "RANDOM",
 * 0..6 → FROZEN..HARD (matches SWOS2.s line 73164). */
struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[9];
} pitchTypeStrTable = {
    pitchType, (uint16_t)-2,
    {
        (void *)aSeasonal, (void *)aRandom,
        (void *)aFrozen,   (void *)aMuddy,
        (void *)aWet,      (void *)aSoft,
        (void *)aNormal,   (void *)aDry,
        (void *)aHard,
    }
};

struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[4];
} gameLengthStrTable = {
    gameLength, 0,
    {
        (void *)a3Mins,  (void *)a5Mins,
        (void *)a7Mins,  (void *)a10Mins,
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
    (void *)aJan, (void *)aFeb, \
    (void *)aMar, (void *)aApr, \
    (void *)aMay, (void *)aJun, \
    (void *)aJul, (void *)aAug, \
    (void *)aSep, (void *)aOct, \
    (void *)aNov, (void *)aDec
#define LEGS_3 \
    (void *)a1Leg, (void *)a1Leg, (void *)a2Legs
#define EXTRATIME_3 \
    (void *)aNoExtraTime, (void *)aExtraTime, (void *)aETIfReplay
#define PENALTIES_3 \
    (void *)aNoPenalties, (void *)aPenalties, (void *)aPensIfReplay
#define PTS_2 \
    (void *)a2PtsForAWin, (void *)a3PtsForAWin
#define PLAYCOUNT_4 \
    (void *)aPlayEachTeamX1, (void *)aPlayEachTeamX2, \
    (void *)aPlayEachTeamX3, (void *)aPlayEachTeamX4

struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[2];
} diyPitchSeasonStrTab = {
    diyTournamentUseSeasonalPitchEffective, 0,
    { (void *)aPitchType, (void *)aSeason_1 }
};
struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[12];
} diyStartMonthStrTab = {
    diyTournamentStartMonthPage, 0, { MONTHS }
};
struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[12];
} diyEndMonthStrTab = {
    diyTournamentEndMonthPage, 0, { MONTHS }
};
/* diyAwayGoalsStrTab: indexPtr = diyFileBufferCopy + 0x4A. */
struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[3];
} diyAwayGoalsStrTab = {
    ((void *)&diyFileBufferCopy.header.menuState), 0,
    {
        (void *)aOff,
        (void *)aAfter90Mins,
        (void *)aAfterExtraTime,
    }
};

/* Six-stage tournament tables. Each stage is 10 bytes (DIYStage):
 *   +0 legs, +2 extraTime, +4 penalties, +6 ptsForWin, +8 playCount.
 * (NB our DIYStage struct names ptsForWin/playCount in the OPPOSITE
 * order — the byte offsets here match the Amiga layout regardless.) */
#define STAGE_PTR(N, OFF)  ((uint8_t *)diyTournamentStages + (N) * 0xA + (OFF))

#define DEFINE_LEGS(N) \
struct __attribute__((packed)) { void    *indexPtr; uint16_t base; void    *entries[3]; } \
diyStage##N##LegsStrTab = { STAGE_PTR((N) - 1, 0), 0, { LEGS_3 } }
DEFINE_LEGS(1); DEFINE_LEGS(2); DEFINE_LEGS(3);
DEFINE_LEGS(4); DEFINE_LEGS(5); DEFINE_LEGS(6);
#undef DEFINE_LEGS

#define DEFINE_ET(N) \
struct __attribute__((packed)) { void    *indexPtr; uint16_t base; void    *entries[3]; } \
diyStage##N##ExtraTimeStrTab = { STAGE_PTR((N) - 1, 2), 0, { EXTRATIME_3 } }
DEFINE_ET(1); DEFINE_ET(2); DEFINE_ET(3);
DEFINE_ET(4); DEFINE_ET(5); DEFINE_ET(6);
#undef DEFINE_ET

#define DEFINE_PEN(N) \
struct __attribute__((packed)) { void    *indexPtr; uint16_t base; void    *entries[3]; } \
diyStage##N##PenaltiesStrTab = { STAGE_PTR((N) - 1, 4), 0, { PENALTIES_3 } }
DEFINE_PEN(1); DEFINE_PEN(2); DEFINE_PEN(3);
DEFINE_PEN(4); DEFINE_PEN(5); DEFINE_PEN(6);
#undef DEFINE_PEN

/* PtsForWin: indexPtr at stage offset +6, base = 2 (index 2 → "2 PTS"). */
#define DEFINE_PTS(N) \
struct __attribute__((packed)) { void    *indexPtr; uint16_t base; void    *entries[2]; } \
diyStage##N##PtsForWinStrTab = { STAGE_PTR((N) - 1, 6), 2, { PTS_2 } }
DEFINE_PTS(1); DEFINE_PTS(2); DEFINE_PTS(3);
DEFINE_PTS(4); DEFINE_PTS(5); DEFINE_PTS(6);
#undef DEFINE_PTS

/* PlayCount: indexPtr at stage offset +8, base = 1, 4 entries. */
#define DEFINE_PLAY(N) \
struct __attribute__((packed)) { void    *indexPtr; uint16_t base; void    *entries[4]; } \
diyStage##N##PlayCountStrTab = { STAGE_PTR((N) - 1, 8), 1, { PLAYCOUNT_4 } }
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
void *topSkillBitRotEven[7] = {
    skillBitRotEven1,
    skillBitRotEven2,
    skillBitRotEven3,
    skillBitRotEven4,
    skillBitRotEven5,
    skillBitRotEven6,
    skillBitRotEven7,
};
void *topSkillBitRotOdd[7] = {
    skillBitRotOdd1,
    skillBitRotOdd2,
    skillBitRotOdd3,
    skillBitRotOdd4,
    skillBitRotOdd5,
    skillBitRotOdd6,
    skillBitRotOdd7,
};
void *tacticPosMapPtrs[18] = {
    tacticPosMap0,
    tacticPosMap1,
    tacticPosMap2,
    tacticPosMap3,
    tacticPosMap4,
    tacticPosMap5,
    tacticPosMap6,
    tacticPosMap7,
    tacticPosMap8,
    tacticPosMap9,
    tacticPosMap10,
    tacticPosMap11,
    tacticPosMap0,
    tacticPosMap0,
    tacticPosMap0,
    tacticPosMap0,
    tacticPosMap0,
    tacticPosMap0,
};
/* offsetCoachOrPlayer: career-menu COACH / PLAYER-COACH selector
 * (SWOS2.s:85269). Auto-extractor emitted only the indexPtr; the
 * STRING_TABLE layout actually needs `{ indexPtr, base, entries[N] }`
 * packed, otherwise the menu reads past the indexPtr into adjacent
 * globals when picking a string. */
extern const char aCoach_0[], aPlayerCoach_0[];
struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[2];
} offsetCoachOrPlayer = {
    coachOrPlayer, 0,
    {
        (void *)aCoach_0,
        (void *)aPlayerCoach_0,
    }
};
void *countryStackPtr[1] = {
    countriesStack,
};
void *sellExchangeStrings[4] = {
    aWeHereAtAAreHa,
    aWeHereAtAAreHa_0,
    aSorryAAreUnwil,
    aSorryAAreUnwil_0,
};
/* positionFilterStringTable / skillFilterStringTable / countryFilterStringTable
 * — same auto-extractor truncation as the diyCup* tables: only the
 * first 4 bytes (the indexPtr) were emitted, but the menu engine
 * (menu_system.c:632 ENTRY_STRING_TABLE) reads at +0 (indexPtr),
 * +4 (base, i16), and +6 (4-byte string pointers indexed by
 * `*indexPtr - base`). With the truncated form, the +6 read landed
 * in adjacent BSS and produced NULL/garbage — _DrawMenuTextCentered
 * (entry 4 of the player-market menu) crashed on the position
 * filter when ShowPlayerMarketInit ran.
 *
 * Layouts recovered from SWOS2.s line 102593-102620. The country
 * filter table immediately follows skillFilterStringTable in the
 * original (no separate label) — we expose it as its own symbol
 * since nothing in our menu_data.c references it via stride. */
/* playerMarketFilterPosition / playerMarketFilterAgeGroup are
 * already extern-declared earlier in this file as `char[]`; the
 * actual definition is `uint16_t[1]` in swos_amiga_data_imports.c.
 * We just need their address — the menu engine reads through it as
 * `*(uint16_t *)indexPtr` regardless of declared type. */
extern char playerMarketFilterCountry[];
extern const char aAny[];
extern const char aGoalkeeper[], aDefender[], aMidfielder[], aAttacker[];
extern const char aPassing[], aShooting[], aHeading[], aTackling[];
extern const char aBallControl[], aSpeed[], aFinishing[];
extern const char aEuropean[], aNorthAmerican[], aSouthAmerican[];
extern const char aAfrican[], aAsian[], aOceanian[];

struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[5];
} positionFilterStringTable = {
    playerMarketFilterPosition, (uint16_t)-1,
    {
        (void *)aAny,
        (void *)aGoalkeeper,
        (void *)aDefender,
        (void *)aMidfielder,
        (void *)aAttacker,
    }
};

struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[8];
} skillFilterStringTable = {
    playerMarketFilterAgeGroup, (uint16_t)-1,
    {
        (void *)aAny,
        (void *)aPassing,
        (void *)aShooting,
        (void *)aHeading,
        (void *)aTackling,
        (void *)aBallControl,
        (void *)aSpeed,
        (void *)aFinishing,
    }
};

struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[7];
} countryFilterStringTable = {
    playerMarketFilterCountry, 0,
    {
        (void *)aEuropean,
        (void *)aNorthAmerican,
        (void *)aSouthAmerican,
        (void *)aAfrican,
        (void *)aAsian,
        (void *)aOceanian,
        (void *)aAny,
    }
};
void *positionGroupTable[4] = {
    positionGroupGoalkeeper,
    positionGroupDefender,
    positionGroupMidfielder,
    positionGroupAttacker,
};
void *sidePitchSurnamePositionsTable[18] = {
    sidePitchPositionsDefault,
    sidePitchPositions_5_4_1,
    sidePitchPositions_4_5_1,
    sidePitchPositions_5_3_2,
    sidePitchPositions_3_5_2,
    sidePitchPositions_4_3_3,
    sidePitchPositions_4_2_4,
    sidePitchPositions_3_4_3,
    sidePitchPositionsSweep,
    sidePitchPositions_5_2_3,
    sidePitchPositionsAttack,
    sidePitchPositionsDefend,
    sidePitchPositionsDefault,
    sidePitchPositionsDefault,
    sidePitchPositionsDefault,
    sidePitchPositionsDefault,
    sidePitchPositionsDefault,
    sidePitchPositionsDefault,
};
void *clubMessagesTable[76] = {
    aSorryAButTheCl,
    aAnyFurtherPlay,
    aComeOnAWeCanno,
    aOrTheWageBillW,
    aSorryAButTheDi,
    aMoreTimeToNego,
    aPleaseNoteATha_2,
    aAnyFurtherPlay_0,
    aPleaseTakeNote,
    aEnsureThatWeCl,
    aJustAReminderT,
    aOrWeWillHaveTo,
    aFollowingACris,
    aClearTheOverdr,
    aItIsWithGreatR_0,
    aDecidedToRelie,
    aPleaseTakeNote_0,
    aEnsureThatItIs,
    aRememberUnless,
    aWeWillHaveToRe,
    aFollowingAVote,
    a3MatchesToTurn,
    aItIsWithGreatR_1,
    aDecidedToRelie_0,
    aWellDoneAYouDi_0,
    aThisSeasonAndW_0,
    aAGoodSeasonAWe_0,
    aPleasedWithYou_0,
    aAnUpAndDownSea_0,
    aCanImproveOnTh_0,
    aNotAVeryGoodSe_0,
    aImproveTheTeam,
    aAVeryDisappoin_0,
    aDemandsMuchBet,
    aThanksForAllYo_0,
    aYouWellInYourN_0,
    aWelcomeToOurCl,
    aMuchSuccessFro,
    aWelcomeToOurCl_0,
    aSeasonInManage_0,
    aJustANoteToWis_0,
    aNewSeason_0,
    aWeAllHopeYouEn_0,
    aBeforeHandingT_0,
    aWeAllWishYouTh_0,
    aDistinguishedC_0,
    aPleaseNoteATha,
    aBHasLeftTheClu,
    aPleaseNoteATha_0,
    aBHasLeftTheClu_0,
    aPleaseNoteATha_1,
    aHasJoinedTheCl,
    aWellDoneAYouDi,
    aThisSeasonAndW,
    aAGoodSeasonAWe,
    aPleasedWithYou,
    aAnUpAndDownSea,
    aCanImproveOnTh,
    aNotAVeryGoodSe,
    aImproveOnOurPe,
    aAVeryDisappoin,
    aMuchBetterFrom,
    aThanksForAllYo,
    aYouWellInYourN,
    aWelcomeAWeLook,
    aSuccessFromOur,
    aWelcomeAWeHope,
    aSeasonInManage,
    aJustANoteToWis,
    aNewSeason,
    aWeAllHopeYouEn,
    aBeforeHandingT,
    aWeAllWishYouTh,
    aDistinguishedC,
    aItIsWithGreatR,
    aRenewYourContr,
};
/* flipStrTable: edit-tactics tactic-side flip selector. SWOS2.s:112717
 * (flipOnOff already extern'd above as char[]; just take its address.) */
extern const char aFlipOff[], aFlipOn[];
extern char aUSER_A[], aUSER_B[], aUSER_C[];
struct __attribute__((packed)) {
    void    *indexPtr; uint16_t base; void    *entries[5];
} flipStrTable = {
    flipOnOff, 0,
    {
        (void *)aFlipOff, (void *)aFlipOn,
        aUSER_A,  aUSER_B,
        aUSER_C,
    }
};
void *europeanCupsandNationalTeamCompetitionsAndQualifications[46] = {
    worldCup,
    europeanChampionships,
    copaAmerica,
    euroCup,
    euroCupCopy,
    cupWinnersCup,
    cupWinnersCupCopy,
    uefaCup,
    uefaCupCopy,
    copaAmericaCopy,
    africanNationsCup,
    africanNationsCupCopy,
    northAmericanCup,
    northAmericanCupCopy,
    asianCup,
    asianCupCopy,
    oceaniaCup,
    oceaniaCupCopy,
    europeanChampionships2Copy,
    ecQualifyingGroup1Copy,
    ecQualifyingGroup2Copy,
    ecQualifyingGroup3Copy,
    ecQualifyingGroup4Copy,
    ecQualifyingGroup5Copy,
    ecQualifyingGroup6Copy,
    ecQualifyingGroup7Copy,
    ecQualifyingGroup8Copy,
    worldCup2Copy,
    worldCupQualifyingGroup1Copy,
    worldCupQualifyingGroup2Copy,
    worldCupQualifyingGroup3Copy,
    worldCupQualifyingGroup4Copy,
    worldCupQualifyingGroup5Copy,
    worldCupQualifyingGroup6Copy,
    worldCupQualifyingGroup7Copy,
    worldCupQualifyingGroup8Copy,
    northAmericaWorldCupQualificationCopy,
    southAmericaWorldCupQualificationCopy,
    africaWorldCupQualificationCopy,
    asiaWorldCupQualificationCopy,
    oceaniaWorldCupQualificationCopy,
    invitationTournamentCopy,
    tourCopy,
    friendly,
    friendlyCopy,
    (void *)(uintptr_t)0xFFFFFFFFu,
};
void *nationalTeamCompetitionsAndQualifications1[39] = {
    worldCup,
    worldCup2Copy,
    europeanChampionships,
    europeanChampionships2Copy,
    copaAmerica,
    copaAmericaCopy,
    africanNationsCup,
    africanNationsCupCopy,
    northAmericanCup,
    northAmericanCupCopy,
    asianCup,
    asianCupCopy,
    oceaniaCup,
    oceaniaCupCopy,
    friendlyCopy,
    ecQualifyingGroup1Copy,
    ecQualifyingGroup2Copy,
    ecQualifyingGroup3Copy,
    ecQualifyingGroup4Copy,
    ecQualifyingGroup5Copy,
    ecQualifyingGroup6Copy,
    ecQualifyingGroup7Copy,
    ecQualifyingGroup8Copy,
    worldCupQualifyingGroup1Copy,
    worldCupQualifyingGroup2Copy,
    worldCupQualifyingGroup3Copy,
    worldCupQualifyingGroup4Copy,
    worldCupQualifyingGroup5Copy,
    worldCupQualifyingGroup6Copy,
    worldCupQualifyingGroup7Copy,
    worldCupQualifyingGroup8Copy,
    northAmericaWorldCupQualificationCopy,
    southAmericaWorldCupQualificationCopy,
    africaWorldCupQualificationCopy,
    asiaWorldCupQualificationCopy,
    oceaniaWorldCupQualificationCopy,
    invitationTournamentCopy,
    tourCopy,
    (void *)(uintptr_t)0xFFFFFFFFu,
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
    void    *ptr;
    uint16_t size;
    uint16_t count;
} SeedListGroupEntry;

SeedListGroupEntry euroCupQualGroupSeedLists_4_6_11[5] = {
    { euroCupQualSeedList4,  4,  4 },
    { euroCupQualSeedList6,  6,  5 },
    { euroCupQualSeedList11, 11, 5 },
    { euroCupQualSeedList22, 22, 2 },
    { (void *)(uintptr_t)-1, 0, 0 },
};

SeedListGroupEntry euroCupQualGroupSeedListsReduced_4_6_11[5] = {
    { euroCupQualSeedListReduced4,  4,  4  },
    { euroCupQualSeedListReduced6,  6,  6  },
    { euroCupQualSeedListReduced11, 11, 11 },
    { euroCupQualSeedListReduced22, 22, 11 },
    { (void *)(uintptr_t)-1, 0, 0 },
};

SeedListGroupEntry euroCupQualGroupSeedBuckets_15_18_22[5] = {
    { euroCupQualSeedBucketBlock.bucket15,     15, 10 },
    { euroCupQualSeedBucketBlock.bucket18,     18, 10 },
    { euroCupQualSeedBucketBlock.bucket22,     22, 8  },
    { euroCupQualSeedBucketBlock.bucket22Full, 22, 4  },
    { (void *)(uintptr_t)-1, 0, 0 },
};
void *positionsTable[18] = {
    positionsDefault,
    positions541,
    positions451,
    positions532,
    positions352,
    positions433,
    positions424,
    positions343,
    positionsSweep,
    positions523,
    positionsAttack,
    positionsDefend,
    positionsDefault,
    positionsDefault,
    positionsDefault,
    positionsDefault,
    positionsDefault,
    positionsDefault,
};
void *divisionStringTable[4] = {
    aPremierDivisio,
    aDivision1_0,
    aDivision2_0,
    aDivision3_0,
};
void *divStringTable[4] = {
    aPremDiv,
    aDiv1,
    aDiv2,
    aDiv3,
};
void *cupRoundStringTable[12] = {
    aFirstRound,
    aSecondRound,
    aThirdRound,
    aFourthRound,
    aFifthRound,
    aSixthRound,
    aQuarterFinal,
    aSemiFinal,
    aFinal_2,
    aFinalGroup,
    aThirdPlacePlay,
    aFinalGroups,
};
void *roundStringTable[12] = {
    aRound1,
    aRound2,
    aRound3,
    aRound4,
    aRound5,
    aRound6,
    aQFinal,
    aSFinal,
    aFinal_3,
    aFinalGroup_0,
    a3rdPlayOff,
    aFinalGroups_0,
};
void *rndStringTable[15] = {
    aRnd1,
    aRnd2,
    aRnd3,
    aRnd4,
    aRnd5,
    aRnd6,
    aQF,
    aSF,
    aFinal,
    aFinal_0,
    a3rd,
    aFinal_1,
    aReplay,
    a2ndReplay,
    a3rdReplay,
};
void *replayStringTable[3] = {
    aReplays,
    a2ndReplays,
    a3rdReplays,
};
void *playerPositionsStringTable[20] = {
    aG,
    aRb,
    aLb,
    aD_0,
    aRw,
    aLw,
    aM,
    aA_1,
    aEur,
    aNam,
    aSam,
    aAsi,
    aOce,
    aAfr,
    aEuro,
    aNam_0,
    aSam_0,
    aAsi_0,
    aOce_0,
    aAfr_0,
};
void *tacticsStringTable[18] = {
    a442,
    a541,
    a451,
    a532,
    a352,
    a433,
    a424,
    a343,
    aSweep,
    a523,
    aAttack,
    aDefend,
    aUSER_A,
    aUSER_B,
    aUSER_C,
    aUSER_D,
    aUSER_E,
    aUSER_F,
};
const uint8_t *fixtureSchedulePtrTable[23] = {
    fixtureSchedule2Teams,
    fixtureSchedule3Teams,
    fixtureSchedule4Teams,
    fixtureSchedule5Teams,
    fixtureSchedule6Teams,
    fixtureSchedule7Teams,
    fixtureSchedule8Teams,
    fixtureSchedule9Teams,
    fixtureSchedule10Teams,
    fixtureSchedule11Teams,
    fixtureSchedule12Teams,
    fixtureSchedule13Teams,
    fixtureSchedule14Teams,
    fixtureSchedule15Teams,
    fixtureSchedule16Teams,
    fixtureSchedule17Teams,
    fixtureSchedule18Teams,
    fixtureSchedule19Teams,
    fixtureSchedule20Teams,
    fixtureSchedule21Teams,
    fixtureSchedule22Teams,
    fixtureSchedule23Teams,
    fixtureSchedule24Teams,
};
void *teamFilenamesTable[4] = {
    aGrafsCjcteam1R,
    aGrafsCjcteam2R,
    aGrafsCjcteam1R,
    aGrafsCjcteam3R,
};
