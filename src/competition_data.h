/*
 * competition_data.h — Extern declarations for competition data
 */
#ifndef COMPETITION_DATA_H
#define COMPETITION_DATA_H

#include "game_types.h"

/* ======================================================================
 * Major International Cups
 * ====================================================================== */

extern CompetitionCup2 euroCup;
extern CompetitionCup cupWinnersCup;
extern CompetitionCup uefaCup;

/* World Cup */
extern CompetitionCup2 worldCup2;
extern CompetitionCup2 worldCup;

/* World Cup Qualifying Groups */
extern CompetitionCup worldCupQualifyingGroup1;
extern CompetitionCup worldCupQualifyingGroup2;
extern CompetitionCup worldCupQualifyingGroup3;
extern CompetitionCup worldCupQualifyingGroup4;
extern CompetitionCup worldCupQualifyingGroup5;
extern CompetitionCup worldCupQualifyingGroup6;
extern CompetitionCup worldCupQualifyingGroup7;
extern CompetitionCup worldCupQualifyingGroup8;

/* World Cup Regional Qualifications */
extern CompetitionCup2 northAmericaWorldCupQualification;
extern CompetitionCup2 southAmericaWorldCupQualification;
extern CompetitionCup2 africaWorldCupQualification;
extern CompetitionCup2 asiaWorldCupQualification;
extern CompetitionCup oceaniaWorldCupQualification;

/* Invitation / Tour / Friendly */
extern CompetitionCup invitationTournament;
extern CompetitionCup tour;
extern CompetitionCup friendly;

/* ======================================================================
 * European Championships
 * ====================================================================== */

extern CompetitionCup2 europeanChampionships2;
extern CompetitionCup2 europeanChampionships;

/* EC Qualifying Groups */
extern CompetitionCup ecQualifyingGroup1;
extern CompetitionCup ecQualifyingGroup2;
extern CompetitionCup ecQualifyingGroup3;
extern CompetitionCup ecQualifyingGroup4;
extern CompetitionCup ecQualifyingGroup5;
extern CompetitionCup ecQualifyingGroup6;
extern CompetitionCup ecQualifyingGroup7;
extern CompetitionCup ecQualifyingGroup8;

/* ======================================================================
 * Continental Cups
 * ====================================================================== */

extern CompetitionCup2 copaAmerica;
extern CompetitionCup2 africanNationsCup;
extern CompetitionCup2 northAmericanCup;
extern CompetitionCup2 asianCup;
extern CompetitionCup oceaniaCup;

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

extern CompetitionCup albanianCup;
extern CompetitionCup austrianCup;
extern CompetitionCup belarusianCup;
extern CompetitionCup belgianCup;
extern CompetitionCup bulgarianCup;
extern CompetitionCup croatianCup;
extern CompetitionCup cypriotCup;
extern CompetitionCup czechCup;
extern CompetitionCup danishCup;
extern CompetitionCup estonianCup;
extern CompetitionCup faroeIslesCup;
extern CompetitionCup finnishCup;
extern CompetitionCup greekCup;
extern CompetitionCup hungarianCup;
extern CompetitionCup icelandicCup;
extern CompetitionCup irishCup;
extern CompetitionCup israeliCup;
extern CompetitionCup latvianCup;
extern CompetitionCup lithuanianCup;
extern CompetitionCup luxembourgCup;
extern CompetitionCup malteseCup;
extern CompetitionCup northernIrishCup;
extern CompetitionCup norwegianCup;
extern CompetitionCup polishCup;
extern CompetitionCup portugeseCup;
extern CompetitionCup romanianCup;
extern CompetitionCup russianCup;
extern CompetitionCup sanMarinoCup;
extern CompetitionCup slovakianCup;
extern CompetitionCup slovenianCup;
extern CompetitionCup swedishCup;
extern CompetitionCup swissCup;
extern CompetitionCup turkishCup;
extern CompetitionCup ukrainianCup;
extern CompetitionCup welshCup;
extern CompetitionCup yugoslavianCup;
extern CompetitionCup algerianCup;
extern CompetitionCup southAfricanCup;
extern CompetitionCup taiwaneseCup;

/* Cups with names */
extern CompetitionCup englishCup;
extern CompetitionCup englishLeagueCup;
extern CompetitionCup englishCup_1;
extern CompetitionCup englishLeagueCup_1;
extern CompetitionCup frenchCup;
extern CompetitionCup germanCup;
extern CompetitionCup italianCup;
extern CompetitionCup scottishCup;
extern CompetitionCup scottishLeagueCup;
extern CompetitionCup spanishCup;
extern CompetitionCup dutchCup;
extern CompetitionCup australianCup;
extern CompetitionCup japaneseCup;
extern CompetitionCup japaneseEmperorsCup;

/* ======================================================================
 * Competition "Copy" variables (BSS, used at runtime for career/season)
 * ====================================================================== */

extern CompetitionCup2 euroCupCopy;
extern CompetitionCup cupWinnersCupCopy;
extern CompetitionCup uefaCupCopy;
extern CompetitionCup invitationTournamentCopy;
extern CompetitionCup tourCopy;
extern CompetitionCup friendlyCopy;
extern CompetitionCup2 europeanChampionships2Copy;
extern CompetitionCup ecQualifyingGroup1Copy;
extern CompetitionCup ecQualifyingGroup2Copy;
extern CompetitionCup ecQualifyingGroup3Copy;
extern CompetitionCup ecQualifyingGroup4Copy;
extern CompetitionCup ecQualifyingGroup5Copy;
extern CompetitionCup ecQualifyingGroup6Copy;
extern CompetitionCup ecQualifyingGroup7Copy;
extern CompetitionCup ecQualifyingGroup8Copy;
extern CompetitionCup2 copaAmericaCopy;
extern CompetitionCup2 africanNationsCupCopy;
extern CompetitionCup2 northAmericanCupCopy;
extern CompetitionCup2 asianCupCopy;
extern CompetitionCup oceaniaCupCopy;
extern CompetitionCup2 worldCup2Copy;
extern CompetitionCup worldCupQualifyingGroup1Copy;
extern CompetitionCup worldCupQualifyingGroup2Copy;
extern CompetitionCup worldCupQualifyingGroup3Copy;
extern CompetitionCup worldCupQualifyingGroup4Copy;
extern CompetitionCup worldCupQualifyingGroup5Copy;
extern CompetitionCup worldCupQualifyingGroup6Copy;
extern CompetitionCup worldCupQualifyingGroup7Copy;
extern CompetitionCup worldCupQualifyingGroup8Copy;
extern CompetitionCup2 northAmericaWorldCupQualificationCopy;
extern CompetitionCup2 southAmericaWorldCupQualificationCopy;
extern CompetitionCup2 africaWorldCupQualificationCopy;
extern CompetitionCup2 asiaWorldCupQualificationCopy;
extern CompetitionCup oceaniaWorldCupQualificationCopy;

/* ======================================================================
 * Career Contests
 * ====================================================================== */

extern CompetitionCareer careerContests[35];

/* ======================================================================
 * Master Competition Pointer Arrays
 * ====================================================================== */

extern Competition *nationalTeamCompetitionsAndQualifications[39];
extern Competition *continentalCups[7];

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

extern Competition *cupPickOceaniaInviteFriendly[5];
extern Competition *cupPickOceaniaOceaniaCupFriendly[5];
extern Competition *cupPickOceaniaWorldCupFriendly[5];
extern Competition *cupPickOceaniaWorldCupInvite[5];
extern Competition *cupPickOceaniaOceaniaCupNonFinalist[5];
extern Competition *cupPickOceaniaOceaniaCupFinalist[5];

extern Competition *cupPickNAmericaInviteFriendly[5];
extern Competition *cupPickNAmericaNaCupFriendly[5];
extern Competition *cupPickNAmericaWorldCupFriendly[5];
extern Competition *cupPickNAmericaWorldCupInvite[5];
extern Competition *cupPickNAmericaNaCupNonFinalist[5];
extern Competition *cupPickNAmericaNaCupFinalist[5];

extern Competition *cupPickAsiaInviteFriendly[5];
extern Competition *cupPickAsiaAsianCupFriendly[5];
extern Competition *cupPickAsiaWorldCupFriendly[5];
extern Competition *cupPickAsiaWorldCupInvite[5];
extern Competition *cupPickAsiaAsianCupNonFinalist[5];
extern Competition *cupPickAsiaAsianCupFinalist[5];

extern Competition *cupPickAfricaInviteFriendly[5];
extern Competition *cupPickAfricaAfricanCupFriendly[5];
extern Competition *cupPickAfricaWorldCupFriendly[5];
extern Competition *cupPickAfricaWorldCupInvite[5];
extern Competition *cupPickAfricaAfricanCupNonFinalist[5];
extern Competition *cupPickAfricaAfricanCupFinalist[5];

extern Competition *cupPickSAmericaInviteFriendly[5];
extern Competition *cupPickSAmericaCopaAmericaFriendly[5];
extern Competition *cupPickSAmericaWorldCupFriendly[5];
extern Competition *cupPickSAmericaWorldCupInvite[5];
extern Competition *cupPickSAmericaCopaAmericaNonFinalist[5];
extern Competition *cupPickSAmericaCopaAmericaFinalist[5];

extern Competition *ecQualifyingGroupsTable[8];
extern Competition *cupPickEuropeNoCupFriendly[5];
extern Competition *cupPickListEuropeInviteTournamentFriendly[5];
extern Competition *cupPickLisEuropeEuroChampsFriendly[5];
extern Competition *cupPickListEuropeEuroChampsInviteFriendly[5];
extern Competition *cupPickListEuropeWorldCupFriendly[5];
extern Competition *cupPickLisEuropeWorldCupInviteFriendly[5];
extern Competition *cupPickEuropeNoCupFriendlyAlt[5];
extern Competition *cupPickListEuropeInviteTournamentFriendlyAlt[5];
extern Competition *worldCupQualifyingGroupsTable[8];

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
