/*
 * competition_data.h — Extern declarations for competition data
 */
#ifndef COMPETITION_DATA_H
#define COMPETITION_DATA_H

#include "game_types.h"

/* ======================================================================
 * Major International Cups
 * ====================================================================== */

extern CompetitionCup2_4_2_32 euroCup;
extern CompetitionCup_6_2_64 cupWinnersCup;
extern CompetitionCup_6_2_64 uefaCup;

/* World Cup */
extern CompetitionCup2_6_2_64 worldCup2;
extern CompetitionCup2_6_2_48 worldCup;

/* World Cup Qualifying Groups */
extern CompetitionCup_6_2_12 worldCupQualifyingGroup1;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup2;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup3;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup4;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup5;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup6;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup7;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup8;

/* World Cup Regional Qualifications */
extern CompetitionCup2_6_2_24 northAmericaWorldCupQualification;
extern CompetitionCup2_6_2_16 southAmericaWorldCupQualification;
extern CompetitionCup2_6_2_64 africaWorldCupQualification;
extern CompetitionCup2_6_2_60 asiaWorldCupQualification;
extern CompetitionCup_6_2_8 oceaniaWorldCupQualification;

/* Invitation / Tour / Friendly */
extern CompetitionCup_6_2_8 invitationTournament;
extern CompetitionCup_6_2_8 tour;
extern CompetitionCup_6_2_12 friendly;

/* ======================================================================
 * European Championships
 * ====================================================================== */

extern CompetitionCup2_4_2_32 europeanChampionships2;
extern CompetitionCup2_4_2_32 europeanChampionships;

/* EC Qualifying Groups */
extern CompetitionCup_6_2_12 ecQualifyingGroup1;
extern CompetitionCup_6_2_12 ecQualifyingGroup2;
extern CompetitionCup_6_2_12 ecQualifyingGroup3;
extern CompetitionCup_6_2_12 ecQualifyingGroup4;
extern CompetitionCup_6_2_12 ecQualifyingGroup5;
extern CompetitionCup_6_2_12 ecQualifyingGroup6;
extern CompetitionCup_6_2_12 ecQualifyingGroup7;
extern CompetitionCup_6_2_8 ecQualifyingGroup8;

/* ======================================================================
 * Continental Cups
 * ====================================================================== */

extern CompetitionCup2_4_2_24 copaAmerica;
extern CompetitionCup2_6_2 africanNationsCup;
extern CompetitionCup2_4_2 northAmericanCup;
extern CompetitionCup2_6_2 asianCup;
extern CompetitionCup_6_2 oceaniaCup;

/* ======================================================================
 * Customs League
 * ====================================================================== */

extern CompetitionLeague customsLeague;

/* ======================================================================
 * National Leagues (1-division)
 * ====================================================================== */

extern CompetitionLeague albanianLeague;
extern CompetitionLeague austrianLeague;
extern CompetitionLeague belarussiaLleague;
extern CompetitionLeague belgianLeague;
extern CompetitionLeague bulgarianLeague;
extern CompetitionLeague croatianLeague;
extern CompetitionLeague cypriotLeague;
extern CompetitionLeague czechLeague;
extern CompetitionLeague danishLeague;
extern CompetitionLeague estonianLeague;
extern CompetitionLeague faroeIslesLeague;
extern CompetitionLeague finnishLeague;
extern CompetitionLeague greekLeague;
extern CompetitionLeague hungarianLeague;
extern CompetitionLeague icelandicLeague;
extern CompetitionLeague israeliLeague;
extern CompetitionLeague latvianLeague;
extern CompetitionLeague lithuanianLeague;
extern CompetitionLeague luxembourgLeague;
extern CompetitionLeague malteseLeague;
extern CompetitionLeague northernIrishLeague;
extern CompetitionLeague norwegianLeague;
extern CompetitionLeague polishLeague;
extern CompetitionLeague portugeseLeague;
extern CompetitionLeague irishLeague;
extern CompetitionLeague romanianLeague;
extern CompetitionLeague russianLeague;
extern CompetitionLeague sanMarinoLeague;
extern CompetitionLeague slovakianLeague;
extern CompetitionLeague slovenianLeague;
extern CompetitionLeague swedishLeague;
extern CompetitionLeague swissLeague;
extern CompetitionLeague turkishLeague;
extern CompetitionLeague ukrainianLeague;
extern CompetitionLeague welshLeague;
extern CompetitionLeague yugoslavianLeague;

/* Non-European 1-division leagues */
extern CompetitionLeague algerianLeague;
extern CompetitionLeague bolivianLeague;
extern CompetitionLeague brasilianLeague;
extern CompetitionLeague chileanLeague;
extern CompetitionLeague colombianLeague;
extern CompetitionLeague ecuadorLeague;
extern CompetitionLeague indianLeague;
extern CompetitionLeague paraguayanLeague;
extern CompetitionLeague peruvianLeague;
extern CompetitionLeague southAfricanLeague;
extern CompetitionLeague surinamLeague;
extern CompetitionLeague taiwaneseLeague;
extern CompetitionLeague americanLeague;
extern CompetitionLeague venezuelanLeague;
extern CompetitionLeague ghanaLeague;

/* ======================================================================
 * Multi-division leagues
 * ====================================================================== */

extern CompetitionLeague frenchLeague;
extern CompetitionLeague germanLeague;
extern CompetitionLeague italianLeague;
extern CompetitionLeague japaneseLeague;
extern CompetitionLeague argentinianLeague;
extern CompetitionLeague mexicanLeague;
extern CompetitionLeague elSalvadorLeague;
extern CompetitionLeague australianLeague;
extern CompetitionLeague newZealandLeague;

/* ======================================================================
 * Complex leagues with playoffs
 * ====================================================================== */

/* englishLeagues / englishLeagues1..3: untangled into EnglishLeaguesBlock
 * fields (._main/._div1/._div2/._div3) — see below + competition_data.c. */
extern Playoffs div1Playoffs;
extern Playoffs div2Playoffs;
extern Playoffs div3Playoffs;

/* EnglishLeaguesBlock: composite of the four English league structs into one
 * BSS run so the .equ aliases above resolve at consistent byte offsets. On
 * 64-bit the .equ offsets (102/182/262) become stale because pointer-bearing
 * substructs widen — typed access via the block fields below avoids that. */
typedef struct __attribute__((packed)) {
    CompetitionLeague _main;
    Leagues_4_2_24             _div1;
    Leagues_4_2_24             _div2;
    Leagues_4                  _div3;
} EnglishLeaguesBlock;

extern EnglishLeaguesBlock englishLeaguesBlock;

extern CompetitionLeague dutchLeague;
extern Leagues_4 dutchLeagues1;
extern Playoffs dutchPlayoffs;

extern CompetitionLeague scottishLeague;
extern Leagues_2 scottishLeague1;
extern Playoffs scottishPlayoffs;

extern CompetitionLeague spanishLeague;
extern Leagues_4 spanishLeague1;
extern Playoffs spanishPlayoffs;

extern CompetitionLeague uruguayanLeague;
extern Leagues_2 uruguayanLeague1;
extern Playoffs uruguayanPlayoffs;

/* ======================================================================
 * National Cups
 * ====================================================================== */

extern CompetitionCup_4 albanianCup;
extern CompetitionCup_4 austrianCup;
extern CompetitionCup_4 belarusianCup;
extern CompetitionCup_4 belgianCup;
extern CompetitionCup_4 bulgarianCup;
extern CompetitionCup_4 croatianCup;
extern CompetitionCup_4 cypriotCup;
extern CompetitionCup_4 czechCup;
extern CompetitionCup_4 danishCup;
extern CompetitionCup_4 estonianCup;
extern CompetitionCup_4 faroeIslesCup;
extern CompetitionCup_4 finnishCup;
extern CompetitionCup_4 greekCup;
extern CompetitionCup_4 hungarianCup;
extern CompetitionCup_4 icelandicCup;
extern CompetitionCup_4 irishCup;
extern CompetitionCup_4 israeliCup;
extern CompetitionCup_4 latvianCup;
extern CompetitionCup_4 lithuanianCup;
extern CompetitionCup_4 luxembourgCup;
extern CompetitionCup_4 malteseCup;
extern CompetitionCup_4 northernIrishCup;
extern CompetitionCup_4 norwegianCup;
extern CompetitionCup_4 polishCup;
extern CompetitionCup_4 portugeseCup;
extern CompetitionCup_4 romanianCup;
extern CompetitionCup_4 russianCup;
extern CompetitionCup_4 sanMarinoCup;
extern CompetitionCup_4 slovakianCup;
extern CompetitionCup_4 slovenianCup;
extern CompetitionCup_4 swedishCup;
extern CompetitionCup_4 swissCup;
extern CompetitionCup_4 turkishCup;
extern CompetitionCup_4 ukrainianCup;
extern CompetitionCup_4 welshCup;
extern CompetitionCup_4 yugoslavianCup;
extern CompetitionCup_4 algerianCup;
extern CompetitionCup_4 southAfricanCup;
extern CompetitionCup_4 taiwaneseCup;

/* Cups with names */
extern CompetitionCup_6_2 englishCup;
extern CompetitionCup_6_2 englishLeagueCup;
extern CompetitionCup_6_2 englishCup_1;
extern CompetitionCup_6_2 englishLeagueCup_1;
extern CompetitionCup_6_2 frenchCup;
extern CompetitionCup_6_2 germanCup;
extern CompetitionCup_6_2 italianCup;
extern CompetitionCup_6_2 scottishCup;
extern CompetitionCup_6_2 scottishLeagueCup;
extern CompetitionCup_6_2 spanishCup;
extern CompetitionCup_6 dutchCup;
extern CompetitionCup_6 australianCup;
extern CompetitionCup_4_2 japaneseCup;
extern CompetitionCup_4_2 japaneseEmperorsCup;

/* ======================================================================
 * Competition "Copy" variables (BSS, used at runtime for career/season)
 * ====================================================================== */

extern CompetitionCup2_4_2_32 euroCupCopy;
extern CompetitionCup_6_2_64 cupWinnersCupCopy;
extern CompetitionCup_6_2_64 uefaCupCopy;
extern CompetitionCup_6_2_8 invitationTournamentCopy;
extern CompetitionCup_6_2_8 tourCopy;
extern CompetitionCup_6_2_12 friendlyCopy;
extern CompetitionCup2_4_2_32 europeanChampionships2Copy;
extern CompetitionCup_6_2_12 ecQualifyingGroup1Copy;
extern CompetitionCup_6_2_12 ecQualifyingGroup2Copy;
extern CompetitionCup_6_2_12 ecQualifyingGroup3Copy;
extern CompetitionCup_6_2_12 ecQualifyingGroup4Copy;
extern CompetitionCup_6_2_12 ecQualifyingGroup5Copy;
extern CompetitionCup_6_2_12 ecQualifyingGroup6Copy;
extern CompetitionCup_6_2_12 ecQualifyingGroup7Copy;
extern CompetitionCup_6_2_8 ecQualifyingGroup8Copy;
extern CompetitionCup2_4_2_24 copaAmericaCopy;
extern CompetitionCup2_6_2 africanNationsCupCopy;
extern CompetitionCup2_4_2 northAmericanCupCopy;
extern CompetitionCup2_6_2 asianCupCopy;
extern CompetitionCup_6_2 oceaniaCupCopy;
extern CompetitionCup2_6_2_64 worldCup2Copy;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup1Copy;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup2Copy;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup3Copy;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup4Copy;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup5Copy;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup6Copy;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup7Copy;
extern CompetitionCup_6_2_12 worldCupQualifyingGroup8Copy;
extern CompetitionCup2_6_2_24 northAmericaWorldCupQualificationCopy;
extern CompetitionCup2_6_2_16 southAmericaWorldCupQualificationCopy;
extern CompetitionCup2_6_2_64 africaWorldCupQualificationCopy;
extern CompetitionCup2_6_2_60 asiaWorldCupQualificationCopy;
extern CompetitionCup_6_2_8 oceaniaWorldCupQualificationCopy;

/* ======================================================================
 * Career Contests
 * ====================================================================== */

extern CompetitionCareer careerContests[35];

/* ======================================================================
 * Master Competition Pointer Arrays
 * ====================================================================== */

extern Competition *nationalTeamCompetitionsAndQualifications[39];
extern CompetitionCup *continentalCups[7];

/* ======================================================================
 * Qualifying Groups and Region Tables
 * ====================================================================== */

extern QualifyingGroup worldCupQualifyingGroups;
extern QualifyingGroup ecQualifyingGroups;
extern QualifyingGroup customsTable;
extern QualifyingGroup worldTable;
extern QualifyingGroup europeTable;
extern QualifyingGroup africaTable;
extern QualifyingGroup southAmericaTable;
extern QualifyingGroup northAmericaTable;
extern QualifyingGroup asiaTable;
extern QualifyingGroup oceaniaTable;

/* ======================================================================
 * Country Competition Tables
 * ====================================================================== */

extern Competition *albaniaTable[4];
extern Competition *austriaTable[4];
extern Competition *belgiumTable[4];
extern Competition *belarusTable[4];
extern Competition *bulgariaTable[4];
extern Competition *croatiaTable[4];
extern Competition *cyprusTable[4];
extern Competition *czechRepublicTable[4];
extern Competition *denmarkTable[4];
extern Competition *englandTable[5];
extern Competition *estoniaTable[4];
extern Competition *faroeIslandsTable[4];
extern Competition *finlandTable[4];
extern Competition *franceTable[4];
extern Competition *germanyTable[4];
extern Competition *greeceTable[4];
extern Competition *hungaryTable[4];
extern Competition *icelandTable[4];
extern Competition *republicIrelandTable[4];
extern Competition *israelTable[4];
extern Competition *italyTable[4];
extern Competition *latviaTable[4];
extern Competition *lithuaniaTable[4];
extern Competition *luxembourgTable[4];
extern Competition *maltaTable[4];
extern Competition *hollandTable[4];
extern Competition *northernIrelandTable[4];
extern Competition *norwayTable[4];
extern Competition *polandTable[4];
extern Competition *portugalTable[4];
extern Competition *romaniaTable[4];
extern Competition *russiaTable[4];
extern Competition *sanMarinoTable[4];
extern Competition *scotlandTable[5];
extern Competition *slovakiaTable[4];
extern Competition *sloveniaTable[4];
extern Competition *spainTable[4];
extern Competition *swedenTable[4];
extern Competition *switzerlandTable[4];
extern Competition *turkeyTable[4];
extern Competition *ukraineTable[4];
extern Competition *walesTable[4];
extern Competition *yugoslaviaTable[4];
extern Competition *algeriaTable[4];
extern Competition *argentinaTable[3];
extern Competition *australiaTable[4];
extern Competition *boliviaTable[3];
extern Competition *brasilTable[3];
extern Competition *chileTable[3];
extern Competition *colombiaTable[3];
extern Competition *ecuadorTable[3];
extern Competition *elSalvadorTable[3];
extern Competition *indiaTable[3];
extern Competition *japanTable[5];
extern Competition *mexicoTable[3];
extern Competition *newZealandTable[3];
extern Competition *paraguayTable[3];
extern Competition *peruTable[3];
extern Competition *southAfricaTable[4];
extern Competition *surinamTable[3];
extern Competition *taiwanTable[4];
extern Competition *uruguayTable[3];
extern Competition *usaTable[3];
extern Competition *venezuelaTable[3];
extern Competition *ghanaTable[3];

/* ======================================================================
 * Season flags and scheduling data
 * ====================================================================== */

extern int16_t seasonCycleNationalFlagsA[5];
extern uint16_t seasonCycleNationalFlagsB[5];
extern uint8_t careerTeamFlagsBySeason0[20];
extern uint8_t careerTeamFlagsBySeason1[20];
extern uint32_t nationalSelectorSeasonMaskA;
extern uint32_t nationalSelectorSeasonMaskB;

/* ======================================================================
 * Countries list and career remap
 * ====================================================================== */

extern CountryNumbers allCountriesList[66];
extern uint8_t careerCompetitionIdRemapTable[66];

/* ======================================================================
 * Euro Cup Qualifying Seed Lists
 * ====================================================================== */

extern uint8_t euroCupQualSeedList4[4];
extern uint8_t euroCupQualSeedList6[6];
extern uint8_t euroCupQualSeedList11[11];
extern uint8_t euroCupQualSeedList22[23];
extern uint8_t euroCupQualSeedListReduced4[4];
extern uint8_t euroCupQualSeedListReduced6[6];
extern uint8_t euroCupQualSeedListReduced11[11];
extern uint8_t euroCupQualSeedListReduced22[23];
/* euroCupQualSeedBucket15/18/22/22Full: untangled into EuroCupQualSeedBucketBlock
 * fields (.bucket15/.bucket18/.bucket22/.bucket22Full) — typedef in game_types.h,
 * definition in competition_data.c. */

/* ======================================================================
 * Cup Pick Tables (per-region competition selection for career)
 * ====================================================================== */

extern CompetitionCup *cupPickOceaniaInviteFriendly[5];
extern CompetitionCup *cupPickOceaniaOceaniaCupFriendly[5];
extern CompetitionCup *cupPickOceaniaWorldCupFriendly[5];
extern CompetitionCup *cupPickOceaniaWorldCupInvite[5];
extern CompetitionCup *cupPickOceaniaOceaniaCupNonFinalist[5];
extern CompetitionCup *cupPickOceaniaOceaniaCupFinalist[5];

extern CompetitionCup *cupPickNAmericaInviteFriendly[5];
extern CompetitionCup *cupPickNAmericaNaCupFriendly[5];
extern CompetitionCup *cupPickNAmericaWorldCupFriendly[5];
extern CompetitionCup *cupPickNAmericaWorldCupInvite[5];
extern CompetitionCup *cupPickNAmericaNaCupNonFinalist[5];
extern CompetitionCup *cupPickNAmericaNaCupFinalist[5];

extern CompetitionCup *cupPickAsiaInviteFriendly[5];
extern CompetitionCup *cupPickAsiaAsianCupFriendly[5];
extern CompetitionCup *cupPickAsiaWorldCupFriendly[5];
extern CompetitionCup *cupPickAsiaWorldCupInvite[5];
extern CompetitionCup *cupPickAsiaAsianCupNonFinalist[5];
extern CompetitionCup *cupPickAsiaAsianCupFinalist[5];

extern CompetitionCup *cupPickAfricaInviteFriendly[5];
extern CompetitionCup *cupPickAfricaAfricanCupFriendly[5];
extern CompetitionCup *cupPickAfricaWorldCupFriendly[5];
extern CompetitionCup *cupPickAfricaWorldCupInvite[5];
extern CompetitionCup *cupPickAfricaAfricanCupNonFinalist[5];
extern CompetitionCup *cupPickAfricaAfricanCupFinalist[5];

extern CompetitionCup *cupPickSAmericaInviteFriendly[5];
extern CompetitionCup *cupPickSAmericaCopaAmericaFriendly[5];
extern CompetitionCup *cupPickSAmericaWorldCupFriendly[5];
extern CompetitionCup *cupPickSAmericaWorldCupInvite[5];
extern CompetitionCup *cupPickSAmericaCopaAmericaNonFinalist[5];
extern CompetitionCup *cupPickSAmericaCopaAmericaFinalist[5];

extern CompetitionCup *ecQualifyingGroupsTable[8];
extern CompetitionCup *cupPickEuropeNoCupFriendly[5];
extern CompetitionCup *cupPickListEuropeInviteTournamentFriendly[5];
extern CompetitionCup *cupPickLisEuropeEuroChampsFriendly[5];
extern CompetitionCup *cupPickListEuropeEuroChampsInviteFriendly[5];
extern CompetitionCup *cupPickListEuropeWorldCupFriendly[5];
extern CompetitionCup *cupPickLisEuropeWorldCupInviteFriendly[5];
extern CompetitionCup *cupPickEuropeNoCupFriendlyAlt[5];
extern CompetitionCup *cupPickListEuropeInviteTournamentFriendlyAlt[5];
extern CompetitionCup *worldCupQualifyingGroupsTable[8];

/* Tour string table */
extern char *tourStringTable[6];

/* Career region weights */
extern uint8_t careerRegionPickWeightsTable[32];
extern uint8_t careerRegionPickWeightsTable2[32];

/* ======================================================================
 * DIY Cup Choice Variables (zero-init)
 * ====================================================================== */

extern uint16_t diyCupLegsChoice0;
extern uint16_t diyCupExtraTimeChoice0;
extern uint16_t diyCupPenaltiesChoice0;
extern uint16_t diyCupLegsChoice1;
extern uint16_t diyCupExtraTimeChoice1;
extern uint16_t diyCupPenaltiesChoice1;
extern uint16_t diyCupLegsChoice2;
extern uint16_t diyCupExtraTimeChoice2;
extern uint16_t diyCupPenaltiesChoice2;
extern uint16_t diyCupLegsChoice3;
extern uint16_t diyCupExtraTimeChoice3;
extern uint16_t diyCupPenaltiesChoice3;
extern uint16_t diyCupLegsChoice4;
extern uint16_t diyCupExtraTimeChoice4;
extern uint16_t diyCupPenaltiesChoice4;
extern uint16_t diyCupLegsChoice5;
extern uint16_t diyCupExtraTimeChoice5;
extern uint16_t diyCupPenaltiesChoice5;

extern uint16_t diyEditParamB;
extern uint16_t diyEditParamC;

#endif /* COMPETITION_DATA_H */
