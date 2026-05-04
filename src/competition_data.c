/*
 * data/competition_data.c — Competition data for SWOS C port
 *
 * All competition definitions extracted from data/Data.c.
 * This file is NOT part of the build yet — it lives in data/ for preparation.
 * When ready, move to src/ and add to the Makefile.
 *
 * String pointers (e.g., aEuropeanChampi_1) are extern references to
 * string constants defined elsewhere. They will need proper extern
 * declarations or a shared header when this file joins the build.
 */
#include "game_types.h"
#include <stdint.h>

/* ======================================================================
 * Forward declarations — strings defined in assembly/Data.c
 * These will need proper externs when integrated into the build.
 * ====================================================================== */

/* Competition name strings */
extern char aEuropeanChampi_1[];
extern char aEuropeanCup[];
extern char aEuropeanCupWin[];
extern char aEuroCwCup[];
extern char aEufaCup[];
extern char aEufaCup_0[];
extern char aWorldCup[];
extern char aWorldCup_0[];
extern char aWorldCupQualif_1[];
extern char aWorldCupQualif_2[];
extern char aWorldCupQualif_3[];
extern char aWorldCupQualif_4[];
extern char aWorldCupQualif_5[];
extern char aWorldCupQualif_6[];
extern char aWorldCupQualif_7[];
extern char aWorldCupQualif_8[];
extern char aWorldCupQualif_9[];
extern char aWCQualifier_0[];
extern char aInvitationTour[];
extern char aInvTournament[];
extern char aTour[];
extern char aFriendly[];
extern char aFriendly_0[];
extern char aEuropeanChampi_0[];
extern char aECFinals[];
extern char aECQualifyingGr[];
extern char aECQualifyingGr_0[];
extern char aECQualifyingGr_1[];
extern char aECQualifyingGr_2[];
extern char aECQualifyingGr_3[];
extern char aECQualifyingGr_4[];
extern char aECQualifyingGr_5[];
extern char aECQualifyingGr_6[];
extern char aECQualifier_0[];
extern char aCopaAmerica[];
extern char aCopaAmerica_0[];
extern char aAfricanNations[];
extern char aAfricanCup[];
extern char aNorthAmericanC[];
extern char aNAmericanCup[];
extern char aAsianCup[];
extern char aAsianCup_0[];
extern char aOceaniaCup[];
extern char aOceaniaCup_0[];
extern char aEuropeanTour[];
extern char aAfricanTour[];
extern char aSouthAmericanT[];
extern char aNorthAmericanT[];
extern char aAsianTour[];
extern char aOceaniaTour[];

/* League division name strings */
extern char aDivision1[];
extern char aDivision2[];
extern char aDivision3[];
extern char aPremierLeague[];
extern char aPremier[];
extern char aDivisionOne[];
extern char aDivisionTwo[];
extern char aDivisionThree[];
extern char aPremiereDivisi[];
extern char aPremiere[];
extern char aDeuxiemeDivisi[];
extern char aDeuxieme[];
extern char a1Liga[];
extern char a2Liga[];
extern char aSerieA[];
extern char aSerieB[];
extern char aPrimeraDivisio[];
extern char aPrimera[];
extern char aSegundaDivisio[];
extern char aSegunda[];
extern char a1stDivision[];
extern char aSouthDivision[];
extern char aSouthDivision_0[];
extern char aNswDivision[];
extern char aNsw[];
extern char aQueenslandDivi[];
extern char aQueensland[];
extern char aJLeague[];
extern char aJLeague_0[];
extern char aLeague_2[];
extern char aLeague_3[];
extern char aNorthernRegion[];
extern char aNorthern[];
extern char aCentralRegion[];
extern char aCentral[];
extern char aSouthernRegion[];
extern char aSouthern[];

/* Playoff/cup name strings */
extern char aDiv1Playoffs[];
extern char aDiv2Playoffs[];
extern char aDiv3Playoffs[];
extern char aPlayoffs[];
extern char aCup_4[];
extern char aCup_5[];
extern char aLeagueCup[];
extern char aLeagueCup_0[];
extern char aCup_6[];
extern char aCup_7[];
extern char aLeagueCup_1[];
extern char aLeagueCup_2[];
extern char aCoupeDeFrance[];
extern char aCoupe[];
extern char aDFBPokal[];
extern char aPokal[];
extern char aCoppaItalia[];
extern char aCoppa[];
extern char aCopa[];
extern char aCopa_0[];
extern char aCup_8[];
extern char aCup_9[];
extern char aEmperorSCup[];
extern char aEmpCup[];

/* TMD file paths */
extern const char aDataEurocupTmd[];
extern const char aDataEurocwcTmd[];
extern const char aDataUefacupTmd[];

#if COMPETITION_DATA_C

/* Forward declarations for variables used before definition */
extern Playoffs_8 div1Playoffs, div2Playoffs, div3Playoffs;
extern Leagues_4_2_24 englishLeagues1, englishLeagues2;
extern Leagues_4 englishLeagues3;
extern Playoffs_8 dutchPlayoffs;
extern Leagues_4 dutchLeagues1;
extern Leagues_2 scottishLeague1;
extern Playoffs_4 scottishPlayoffs;
extern Leagues_4 spanishLeague1;
extern Playoffs_8 spanishPlayoffs;
extern Leagues_2 uruguayanLeague1;
extern Playoffs_4 uruguayanPlayoffs;

/* ======================================================================
 * Major International Cups
 * ====================================================================== */

CompetitionCup2_4_2_32 euroCup = {
    { { { 0, 2, -1, 64, 32, 33, 0, 39, 1 }, 1, 1, 2, 3, 53, 4,
        { { 16, 4, 4 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 0, 0 }, { 0, 0, 0 } }, 0 },
      { 0, -108, -108, 20 } },
    { aEuropeanChampi_1, aEuropeanCup },
    { 13, 1, 25, 0, 37, 5, 33, 33, 35, 6, 30, 18, 14, 4, 28, 23,
      1, 5, 38, 9, 20, 22, 8, 20, 36, 8, 27, 11, 20, 0, 29, 12 }
};

CompetitionCup_6_2_64 cupWinnersCup = {
    { { 1, 1, -1, 64, 32, 15, 0, 21, 1 }, 0, 32, 1, 53, 1 },
    { -108, -108, -108, -108, 20, 0 },
    { aEuropeanCupWin, aEuroCwCup },
    { 15, 0, 78, 2, 7, 2, 34, 11, 35, 8, 5, 0, 29, 2, 40, 16,
      17, 8, 36, 0, 2, 2, 27, 1, 19, 15, 38, 10, 11, 4, 25, 21,
      30, 8, 20, 18, 14, 22, 41, 7, 31, 10, 4, 11, 12, 8, 8, 18,
      3, 6, 16, 8, 37, 12, 39, 10, 1, 7, 6, 10, 21, 6, 13, 30 }
};

CompetitionCup_6_2_64 uefaCup = {
    { { 2, 1, -1, 64, 32, 15, 0, 21, 1 }, 0, 32, 1, 53, 1 },
    { -108, -108, -108, -108, 20, 0 },
    { aEufaCup, aEufaCup_0 },
    { 31, 19, 29, 3, 28, 7, 38, 5, 6, 8, 35, 44, 29, 24, 2, 0,
      14, 23, 20, 40, 36, 7, 37, 10, 35, 19, 25, 10, 2, 4, 30, 10,
      20, 21, 1, 2, 33, 0, 7, 3, 16, 5, 8, 22, 14, 30, 38, 21,
      20, 23, 35, 42, 14, 17, 31, 16, 13, 22, 29, 20, 13, 23, 14, 25 }
};

/* ======================================================================
 * World Cup
 * ====================================================================== */

CompetitionCup2_6_2_64 worldCup2 = {
    { { { 3, 2, -1, -120, -112, 35, 0, 41, 0 }, 1, 0, 1, 3, 53, 6,
        { { 32, 8, 4 }, { 16, 0, 16 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, -1, 2 }, { 2, 0, 2 } }, 1 },
      { 0, 20, 20, 20, 20, 20 } },
    { aWorldCup, aWorldCup_0 },
    { 0 }
};

CompetitionCup2_6_2_48 worldCup = {
    { { { 4, 2, -1, -120, -112, 35, 0, 41, 1 }, 0, 0, 1, 3, 53, 6,
        { { 24, 6, 4 }, { 16, 0, 16 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, -1, 2 }, { 2, 0, 2 } }, 1 },
      { 0, 20, 20, 20, 20, 20 } },
    { aWorldCup, aWorldCup_0 },
    { 80, 36, 82, 4, 83, 13, 80, 44, 82, 2, 80, 37, 81, 6, 80, 43,
      80, 17, 82, 1, 80, 42, 84, 24, 82, 0, 80, 18, 81, 22, 80, 6,
      80, 23, 80, 35, 80, 32, 83, 8, 80, 5, 81, 19, 80, 19, 84, 22 }
};

/* ======================================================================
 * World Cup Qualifying Groups
 * ====================================================================== */

CompetitionCup_6_2_12 worldCupQualifyingGroup1 = {
    { { 5, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_2, aWCQualifier_0 }, { 0 }
};

CompetitionCup_6_2_12 worldCupQualifyingGroup2 = {
    { { 6, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_3, aWCQualifier_0 }, { 0 }
};

CompetitionCup_6_2_12 worldCupQualifyingGroup3 = {
    { { 7, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_4, aWCQualifier_0 }, { 0 }
};

CompetitionCup_6_2_12 worldCupQualifyingGroup4 = {
    { { 8, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_5, aWCQualifier_0 }, { 0 }
};

CompetitionCup_6_2_12 worldCupQualifyingGroup5 = {
    { { 9, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_6, aWCQualifier_0 }, { 0 }
};

CompetitionCup_6_2_12 worldCupQualifyingGroup6 = {
    { { 10, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_7, aWCQualifier_0 }, { 0 }
};

CompetitionCup_6_2_12 worldCupQualifyingGroup7 = {
    { { 11, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_8, aWCQualifier_0 }, { 0 }
};

CompetitionCup_6_2_12 worldCupQualifyingGroup8 = {
    { { 12, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_9, aWCQualifier_0 }, { 0 }
};

/* ======================================================================
 * World Cup Regional Qualifications
 * ====================================================================== */

CompetitionCup2_6_2_24 northAmericaWorldCupQualification = {
    { { { 13, 2, 83, 72, 16, 35, 0, 41, 0 }, 1, 0, 2, 3, 53, 2,
        { { 12, 0, 12 }, { 6, 1, 6 }, { 3, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
      { -108, 20, 20, 20, 20, 20 } },
    { aWorldCupQualif_1, aWCQualifier_0 }, { 0 }
};

CompetitionCup2_6_2_16 southAmericaWorldCupQualification = {
    { { { 14, 2, 82, 72, 16, 35, 0, 41, 0 }, 1, 0, 2, 3, 53, 1,
        { { 8, 1, 8 }, { 4, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
      { 20, 20, 20, 20, 20, 20 } },
    { aWorldCupQualif_1, aWCQualifier_0 }, { 0 }
};

CompetitionCup2_6_2_64 africaWorldCupQualification = {
    { { { 15, 2, 81, 72, 16, 35, 0, 41, 0 }, 1, 0, 2, 3, 53, 3,
        { { 32, 0, 32 }, { 16, 0, 16 }, { 8, 1, 8 }, { 5, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
      { -108, -108, 20, 20, 20, 20 } },
    { aWorldCupQualif_1, aWCQualifier_0 }, { 0 }
};

CompetitionCup2_6_2_60 asiaWorldCupQualification = {
    { { { 16, 2, 84, 72, 16, 35, 0, 41, 0 }, 1, 0, 2, 3, 53, 3,
        { { 24, 0, 24 }, { 12, 0, 12 }, { 6, 1, 6 }, { 2, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
      { -108, -108, 20, 20, 20, 20 } },
    { aWorldCupQualif_1, aWCQualifier_0 }, { 0 }
};

CompetitionCup_6_2_8 oceaniaWorldCupQualification = {
    { { 17, 0, 85, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 4 },
    { 1, 0, 0, 0, 0, 0 }, { aWorldCupQualif_1, aWCQualifier_0 }, { 0 }
};

/* ======================================================================
 * Invitation / Tour / Friendly
 * ====================================================================== */

CompetitionCup_6_2_8 invitationTournament = {
    { { 18, 0, -1, -104, -104, 15, 0, 21, 0 }, 1, 1, 3, 53, 4 },
    { 0 }, { aInvitationTour, aInvTournament }, { 0 }
};

CompetitionCup_6_2_8 tour = {
    { { 19, 0, -1, -104, -104, 15, 0, 21, 0 }, 1, -1, 3, 53, 4 },
    { 0 }, { 0, aTour }, { 0 }
};

CompetitionCup_6_2_12 friendly = {
    { { 20, 0, -1, 56, 0, 15, 0, 21, 0 }, 1, -1, 3, 53, 6 },
    { 0 }, { aFriendly, aFriendly_0 }, { 0 }
};

/* ======================================================================
 * European Championships
 * ====================================================================== */

CompetitionCup2_4_2_32 europeanChampionships2 = {
    { { { 21, 2, 80, -120, -120, 33, 0, 39, 0 }, 1, 0, 1, 3, 53, 4,
        { { 16, 4, 4 }, { 8, 0, 0 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
      { 0, 20, 20, 20 } },
    { aEuropeanChampi_0, aECFinals }, { 0 }
};

CompetitionCup2_4_2_32 europeanChampionships = {
    { { { 22, 2, 80, -120, -120, 33, 0, 39, 1 }, 1, 0, 1, 3, 53, 4,
        { { 16, 4, 4 }, { 8, 0, 0 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
      { 0, 20, 20, 20 } },
    { aEuropeanChampi_0, aECFinals },
    { 80, 11, 80, 44, 80, 19, 80, 39, 80, 42, 80, 6, 80, 36, 80, 15,
      80, 17, 80, 9, 80, 23, 80, 37, 80, 10, 80, 34, 80, 45, 80, 7 }
};

/* ======================================================================
 * EC Qualifying Groups
 * ====================================================================== */

CompetitionCup_6_2_12 ecQualifyingGroup1 = {
    { { 23, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr, aECQualifier_0 }, { 0 }
};

CompetitionCup_6_2_12 ecQualifyingGroup2 = {
    { { 24, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr_0, aECQualifier_0 }, { 0 }
};

CompetitionCup_6_2_12 ecQualifyingGroup3 = {
    { { 25, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr_1, aECQualifier_0 }, { 0 }
};

CompetitionCup_6_2_12 ecQualifyingGroup4 = {
    { { 26, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr_2, aECQualifier_0 }, { 0 }
};

CompetitionCup_6_2_12 ecQualifyingGroup5 = {
    { { 27, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr_3, aECQualifier_0 }, { 0 }
};

CompetitionCup_6_2_12 ecQualifyingGroup6 = {
    { { 28, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr_4, aECQualifier_0 }, { 0 }
};

CompetitionCup_6_2_12 ecQualifyingGroup7 = {
    { { 29, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr_5, aECQualifier_0 }, { 0 }
};

CompetitionCup_6_2_8 ecQualifyingGroup8 = {
    { { 30, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 4 },
    { 1, 0, 0, 0, 0, 0 }, { aECQualifyingGr_6, aECQualifier_0 }, { 0 }
};

/* ======================================================================
 * Continental Cups
 * ====================================================================== */

CompetitionCup2_4_2_24 copaAmerica = {
    { { { 31, 2, 82, -120, -112, 33, 0, 39, 0 }, 1, 0, 1, 3, 53, 4,
        { { 12, 3, 4 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
      { 0, 4, 4, 4 } },
    { aCopaAmerica, aCopaAmerica_0 },
    { 83, 8, 83, 13, 82, 8, 82, 9, 82, 0, 82, 1, 82, 2, 82, 3, 82, 4, 82, 5, 82, 6, 82, 7 }
};

CompetitionCup2_6_2 africanNationsCup = {
    { { { 32, 2, 81, -120, -112, 35, 0, 0, 0 }, 1, 0, 1, 3, 53, 5,
        { { 32, 8, 4 }, { 16, 0, 16 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 1, 1 } }, 1 },
      { 0, 20, 20, 20, 20, 0 } },
    { aAfricanNations, aAfricanCup }
};

CompetitionCup2_4_2 northAmericanCup = {
    { { { 33, 2, 83, -120, -112, 33, 0, 0, 0 }, 1, 0, 1, 3, 53, 4,
        { { 12, 3, 4 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
      { 0, 20, 20, 20 } },
    { aNorthAmericanC, aNAmericanCup }
};

CompetitionCup2_6_2 asianCup = {
    { { { 34, 2, 84, -120, -112, 35, 0, 0, 0 }, 1, 0, 1, 3, 53, 5,
        { { 30, 5, 6 }, { 16, 0, 16 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 1, 1 } }, 1 },
      { 0, 20, 20, 20, 20, 0 } },
    { aAsianCup, aAsianCup_0 }
};

CompetitionCup_6_2 oceaniaCup = {
    { { 35, 0, 85, -120, -112, 15, 0, 0, 0 }, 1, 1, 3, 53, 4 },
    { 0 }, { aOceaniaCup, aOceaniaCup_0 }
};

/* ======================================================================
 * Customs League
 * ====================================================================== */

CompetitionLeague_3_6 customsLeague = {
    { { 36, 0, 72, 64, 32, 27, 0, 0, 0 }, 3, 2, 3, 53 },
    { { 20, 0, 0, 0, 0, 0 }, { 22, 0, 0, 0, 0, 0 }, { 22, 0, 0, 0, 0, 0 } },
    0, { aDivision1, aDivision1, aDivision2, aDivision2, aDivision3, aDivision3 }
};

/* ======================================================================
 * National Leagues (1-division)
 * ====================================================================== */

CompetitionLeague_1 albanianLeague    = { { { 37, 0, 0, 64, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 austrianLeague    = { { { 38, 0, 1, 56, 40, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 belarussiaLleague = { { { 39, 0, 76, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 69 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 belgianLeague     = { { { 40, 0, 2, 56, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 bulgarianLeague   = { { { 41, 0, 3, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 croatianLeague    = { { { 42, 0, 4, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 cypriotLeague     = { { { 43, 0, 5, 64, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 14, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 czechLeague       = { { { 44, 0, 6, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 danishLeague      = { { { 45, 0, 7, 56, 40, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 estonianLeague    = { { { 50, 0, 10, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 faroeIslesLeague  = { { { 51, 0, 11, 24, 64, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 finnishLeague     = { { { 52, 0, 12, 24, 72, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 greekLeague       = { { { 55, 0, 15, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 hungarianLeague   = { { { 58, 0, 16, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 icelandicLeague   = { { { 59, 0, 17, 32, 64, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 israeliLeague     = { { { 60, 0, 19, 64, 32, 0, 0, 0, 0 }, 1, 3, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 latvianLeague     = { { { 62, 0, 21, 32, 64, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 lithuanianLeague  = { { { 63, 0, 22, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 luxembourgLeague  = { { { 64, 0, 23, 56, 40, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 malteseLeague     = { { { 65, 0, 24, 64, 24, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 northernIrishLeague = { { { 66, 0, 26, 64, 24, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 norwegianLeague   = { { { 67, 0, 27, 24, 72, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 14, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 polishLeague      = { { { 68, 0, 28, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 portugeseLeague   = { { { 69, 0, 29, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 irishLeague       = { { { 70, 0, 18, 56, 24, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 romanianLeague    = { { { 71, 0, 30, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 russianLeague     = { { { 72, 0, 31, 16, 80, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 sanMarinoLeague   = { { { 73, 0, 32, 72, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 slovakianLeague   = { { { 76, 0, 78, 56, 40, 0, 0, 0, 0 }, 1, 2, 2, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 slovenianLeague   = { { { 77, 0, 34, 56, 40, 0, 0, 0, 0 }, 1, 2, 2, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 swedishLeague     = { { { 80, 0, 36, 24, 72, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 14, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 swissLeague       = { { { 81, 0, 37, 48, 40, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 turkishLeague     = { { { 82, 0, 38, 56, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 ukrainianLeague   = { { { 83, 0, 39, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 welshLeague       = { { { 84, 0, 40, 56, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 20, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 yugoslavianLeague = { { { 85, 0, 41, 64, 40, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };

/* Non-European 1-division leagues */
CompetitionLeague_1 algerianLeague    = { { { 86, 0, 42, 64, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 bolivianLeague    = { { { 89, 0, 45, 32, 88, 0, 0, 0, 0 }, 1, 2, 2, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 brasilianLeague   = { { { 90, 0, 46, 0, 40, 0, 0, 0, 0 }, 1, 2, 2, 53 }, { { 24, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 chileanLeague     = { { { 91, 0, 48, 24, 88, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 colombianLeague   = { { { 92, 0, 49, 24, 88, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 ecuadorLeague     = { { { 93, 0, 50, 24, 88, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 indianLeague      = { { { 95, 0, 75, 0, 24, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 6, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 paraguayanLeague  = { { { 99, 0, 64, 64, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 peruvianLeague    = { { { 100, 0, 65, 24, 88, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 southAfricanLeague = { { { 101, 0, 69, 0, 80, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 surinamLeague     = { { { 102, 0, 66, 24, 72, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 taiwaneseLeague   = { { { 103, 0, 67, 64, 32, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 americanLeague    = { { { 106, 0, 73, 24, 72, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 venezuelanLeague  = { { { 107, 0, 77, 24, 88, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 22, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague_1 ghanaLeague       = { { { 108, 0, 79, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 8, 0, 0, 0, 0, 0 } }, 0 };

/* ======================================================================
 * Multi-division leagues
 * ====================================================================== */

CompetitionLeague_2_4 frenchLeague = {
    { { 53, 0, 13, 56, 40, 21, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 20, 0, 0, 3, 0, 0 }, { 22, 3, 0, 0, 0, 0 } }, 0,
    { aPremiereDivisi, aPremiere, aDeuxiemeDivisi, aDeuxieme }
};

CompetitionLeague_2_4 germanLeague = {
    { { 54, 0, 14, 56, 40, 21, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 18, 0, 0, 3, 0, 0 }, { 18, 3, 0, 0, 0, 0 } }, 0,
    { a1Liga, a1Liga, a2Liga, a2Liga }
};

CompetitionLeague_2_4 italianLeague = {
    { { 61, 0, 20, 64, 32, 21, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 18, 0, 0, 4, 0, 0 }, { 20, 4, 0, 0, 0, 0 } }, 0,
    { aSerieA, aSerieA, aSerieB, aSerieB }
};

CompetitionLeague_2_4 japaneseLeague = {
    { { 96, 0, 55, 16, 80, 21, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 14, 0, 0, 0, 0, 0 }, { 14, 0, 0, 0, 0, 0 } }, 0,
    { aJLeague, aJLeague_0, aLeague_2, aLeague_3 }
};

CompetitionLeague_2 argentinianLeague = {
    { { 87, 0, 43, 64, 32, 0, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 20, 0, 0, 2, 0, 0 }, { 20, 2, 0, 0, 0, 0 } }, 0
};

CompetitionLeague_2 mexicanLeague = {
    { { 97, 0, 60, 56, 32, 0, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 20, 0, 0, 2, 0, 0 }, { 20, 2, 0, 0, 0, 0 } }, 0
};

CompetitionLeague_4 elSalvadorLeague = {
    { { 94, 0, 51, 56, 32, 0, 0, 0, 0 }, 4, 2, 3, 53 },
    { { 10, 0, 0, 1, 0, 0 }, { 12, 1, 0, 1, 0, 0 }, { 12, 1, 0, 1, 0, 0 }, { 12, 1, 0, 0, 0, 0 } }, 0
};

CompetitionLeague_4_8 australianLeague = {
    { { 88, 0, 44, 16, 80, 33, 0, 0, 0 }, 4, 2, 3, 53 },
    { { 12, 0, 0, 0, 0, 0 }, { 12, 0, 0, 0, 0, 0 }, { 14, 0, 0, 0, 0, 0 }, { 12, 0, 0, 0, 0, 0 } }, 0,
    { a1stDivision, a1stDivision, aSouthDivision, aSouthDivision_0, aNswDivision, aNsw, aQueenslandDivi, aQueensland }
};

CompetitionLeague_3_6 newZealandLeague = {
    { { 98, 0, 62, 16, 72, 27, 0, 0, 0 }, 3, 2, 3, 53 },
    { { 10, 0, 0, 0, 0, 0 }, { 10, 0, 0, 0, 0, 0 }, { 10, 0, 0, 0, 0, 0 } }, 0,
    { aNorthernRegion, aNorthern, aCentralRegion, aCentral, aSouthernRegion, aSouthern }
};

/* ======================================================================
 * Complex leagues with playoffs
 * ====================================================================== */

/* On Amiga, englishLeagues / englishLeagues1 / englishLeagues2 /
 * englishLeagues3 sit consecutively in BSS (102 + 80 + 80 + 48 bytes).
 * processDiyLeagueQualificationEntry walks past the end of the
 * englishLeagues struct using the per-division qualification offset
 * stored in `divisions[i].unk1`, expecting to land inside englishLeagues1
 * or englishLeagues2 (whose `playoffs` field at offset +48 supplies the
 * actual qualification competition pointer). On SDL the linker scatters
 * the four structs to different addresses, so the past-the-end read
 * lands in unrelated memory and qualComp = garbage → SIGSEGV.
 *
 * Force the Amiga layout by embedding all four into one packed
 * composite struct and exposing each via a global alias to a fixed
 * offset within the composite. The .equ aliases keep the existing
 * `englishLeagues` / `englishLeagues1` / etc. extern decls working
 * unchanged. */
typedef struct __attribute__((packed)) {
    CompetitionLeague_4_8_2_24 _main;
    Leagues_4_2_24             _div1;
    Leagues_4_2_24             _div2;
    Leagues_4                  _div3;
} EnglishLeaguesBlock;

#define ENGLISH_BLOCK_OFFSET_MAIN  0
#define ENGLISH_BLOCK_OFFSET_DIV1  (sizeof(CompetitionLeague_4_8_2_24))
#define ENGLISH_BLOCK_OFFSET_DIV2  (ENGLISH_BLOCK_OFFSET_DIV1 + sizeof(Leagues_4_2_24))
#define ENGLISH_BLOCK_OFFSET_DIV3  (ENGLISH_BLOCK_OFFSET_DIV2 + sizeof(Leagues_4_2_24))

extern CompetitionLeague_4_8_2_24 englishLeagues;
extern Leagues_4_2_24 englishLeagues1;
extern Leagues_4_2_24 englishLeagues2;
extern Leagues_4 englishLeagues3;

EnglishLeaguesBlock englishLeaguesBlock = {
    ._main = {
        { { 46, 0, 8, 56, 32, 33, 0, 0, 0 }, 4, 2, 3, 51 },
        { { 20, 0, 0, 3, 0, 0 }, { 24, 2, 4, 4, 0, 46 }, { 24, 3, 4, 4, 0, 120 }, { 24, 3, 4, 0, 0, -62 } }, 0,
        { aPremierLeague, aPremier, aDivisionOne, aDivisionOne, aDivisionTwo, aDivisionTwo, aDivisionThree, aDivisionThree },
        { (char *)&div1Playoffs, (char *)&englishLeagues1 },
        { 0, 0, 0, 3, 0, 1, 0, 2, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    ._div1 = {
        { (CompetitionLeague *)&englishLeagues, { 0, 0, 0, 1, 0, 0, 0, 0 },
          (CompetitionLeague *)&englishLeagues, { 0, 0, 0, 1, 0, 0, 0, 3 },
          (CompetitionLeague *)&englishLeagues, { 0, 0, 0, 1, 0, 0, 0, 1 },
          (CompetitionLeague *)&englishLeagues, { 0, 0, 0, 1, 0, 0, 0, 2 } },
        &div2Playoffs, (Leagues_4 *)&englishLeagues2,
        { 0, 4, 0, 7, 0, 5, 0, 6, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    ._div2 = {
        { (CompetitionLeague *)&englishLeagues, { 0, 0, 0, 2, 0, 0, 0, 0 },
          (CompetitionLeague *)&englishLeagues, { 0, 0, 0, 2, 0, 0, 0, 3 },
          (CompetitionLeague *)&englishLeagues, { 0, 0, 0, 2, 0, 0, 0, 1 },
          (CompetitionLeague *)&englishLeagues, { 0, 0, 0, 2, 0, 0, 0, 2 } },
        &div3Playoffs, &englishLeagues3,
        { 0, 8, 0, 11, 0, 9, 0, 10, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    ._div3 = {
        (CompetitionLeague *)&englishLeagues, { 0, 0, 0, 3, 0, 0, 0, 0 },
        (CompetitionLeague *)&englishLeagues, { 0, 0, 0, 3, 0, 0, 0, 3 },
        (CompetitionLeague *)&englishLeagues, { 0, 0, 0, 3, 0, 0, 0, 1 },
        (CompetitionLeague *)&englishLeagues, { 0, 0, 0, 3, 0, 0, 0, 2 }
    }
};

__asm__(".globl englishLeagues\n"
        ".equ   englishLeagues, englishLeaguesBlock\n");
/* offset 102 = sizeof(CompetitionLeague_4_8_2_24) */
__asm__(".globl englishLeagues1\n"
        ".equ   englishLeagues1, englishLeaguesBlock + 102\n");
/* offset 182 = 102 + 80 */
__asm__(".globl englishLeagues2\n"
        ".equ   englishLeagues2, englishLeaguesBlock + 182\n");
/* offset 262 = 102 + 80 + 80 */
__asm__(".globl englishLeagues3\n"
        ".equ   englishLeagues3, englishLeaguesBlock + 262\n");

Playoffs_8 div1Playoffs = { { 47, 1, 8, -128, -124, 11, 0, 17, 1, 0, 4, 1, 51, 1, -108, 20 }, { aDiv1Playoffs, aPlayoffs }, { 0 } };
Playoffs_8 div2Playoffs = { { 48, 1, 8, -128, -124, 11, 0, 17, 1, 0, 4, 1, 51, 1, -108, 20 }, { aDiv2Playoffs, aPlayoffs }, { 0 } };
Playoffs_8 div3Playoffs = { { 49, 1, 8, -128, -124, 11, 0, 17, 1, 0, 4, 1, 51, 1, -108, 20 }, { aDiv3Playoffs, aPlayoffs }, { 0 } };

/* Dutch league */
CompetitionLeague_2_2_24 dutchLeague = {
    { { 56, 0, 25, 56, 32, 0, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 18, 0, 0, 1, 2, 8 }, { 18, 1, 2, 0, 0, 2 } }, 0,
    { (char *)&dutchPlayoffs, (char *)&dutchLeagues1 },
    { 0, 0, 0, 3, 0, 1, 0, 2, 0, -1, 0, -1, 0, -1, 0, -1, 1, 0, 1, 0, 1, 0, 1, 0 }
};

Leagues_4 dutchLeagues1 = {
    (CompetitionLeague *)&dutchLeague, { 0 },
    (CompetitionLeague *)&dutchLeague, { 0, 0, 0, 1, 0, 0, 0, 1 },
    (CompetitionLeague *)&dutchLeague, { 0, 0, 0, 0, 0, 0, 0, 1 },
    (CompetitionLeague *)&dutchLeague, { 0, 0, 0, 1, 0, 0, 0, 0 }
};

Playoffs_8 dutchPlayoffs = { { 57, 1, 25, -128, -128, 11, 0, 17, 1, 1, 4, 1, 53, 1, -108, -108 }, { aPlayoffs, aPlayoffs }, { 0 } };

/* Scottish league */
CompetitionLeague_4_2_8 scottishLeague = {
    { { 74, 0, 33, 56, 32, 0, 0, 0, 0 }, 4, 4, 3, 51 },
    { { 10, 0, 0, 1, 1, 20 }, { 10, 1, 1, 2, 0, 14 }, { 10, 2, 0, 2, 0, 0 }, { 10, 2, 0, 0, 0, 0 } }, 0,
    { (char *)&scottishPlayoffs, (char *)&scottishLeague1 },
    { 0, 0, 0, 1, 0, -1, 1, 0 }
};

Leagues_2 scottishLeague1 = {
    (CompetitionLeague *)&scottishLeague, { 0 },
    (CompetitionLeague *)&scottishLeague, { 0, 0, 0, 1, 0, 0, 0, 0 }
};

Playoffs_4 scottishPlayoffs = { { 75, 1, 33, -128, -128, 11, 0, 17, 1, 0, 2, 1, 51, 1, 20, 0 }, { aPlayoffs, aPlayoffs }, { 0 } };

/* Spanish league */
CompetitionLeague_2_4_2_24 spanishLeague = {
    { { 78, 0, 35, 64, 40, 21, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 22, 0, 0, 2, 2, 24 }, { 20, 2, 2, 0, 0, 18 } }, 0,
    { aPrimeraDivisio, aPrimera, aSegundaDivisio, aSegunda },
    { (char *)&spanishPlayoffs, (char *)&spanishLeague1 },
    { 0, 0, 0, 3, 0, 1, 0, 2, 0, -1, 0, -1, 0, -1, 0, -1, 1, 0, 1, 0, 1, 0, 1, 0 }
};

Leagues_4 spanishLeague1 = {
    (CompetitionLeague *)&spanishLeague, { 0 },
    (CompetitionLeague *)&spanishLeague, { 0, 0, 0, 1, 0, 0, 0, 1 },
    (CompetitionLeague *)&spanishLeague, { 0, 0, 0, 0, 0, 0, 0, 1 },
    (CompetitionLeague *)&spanishLeague, { 0, 0, 0, 1, 0, 0, 0, 0 }
};

Playoffs_8 spanishPlayoffs = { { 79, 1, 35, -120, -120, 11, 0, 17, 1, 1, 4, 1, 53, 1, -108, -108 }, { aPlayoffs, aPlayoffs }, { 0 } };

/* Uruguayan league */
CompetitionLeague_2_2_8 uruguayanLeague = {
    { { 104, 0, 71, 24, 88, 0, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 12, 0, 0, 1, 1, 8 }, { 14, 1, 1, 0, 0, 2 } }, 0,
    { (char *)&uruguayanPlayoffs, (char *)&uruguayanLeague1 },
    { 0, 0, 0, 1, 0, -1, 1, 0 }
};

Leagues_2 uruguayanLeague1 = {
    (CompetitionLeague *)&uruguayanLeague, { 0 },
    (CompetitionLeague *)&uruguayanLeague, { 0, 0, 0, 1, 0, 0, 0, 0 }
};

Playoffs_4 uruguayanPlayoffs = { { 105, 1, 71, 88, 88, 11, 0, 17, 1, 0, 2, 1, 53, 1, -108, 0 }, { aPlayoffs, aPlayoffs }, { 0 } };

/* ======================================================================
 * National Cups
 * ====================================================================== */

CompetitionCup_4 albanianCup     = { { { 109, 1, 0, 88, 32, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { 84, 84, -108, 20 } };
CompetitionCup_4 austrianCup     = { { { 110, 1, 1, -128, -120, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup_4 belarusianCup   = { { { 111, 1, 76, 72, 40, 0, 0, 0, 0 }, 0, 16, 0, 53, 1 }, { 84, 84, 84, 20 } };
CompetitionCup_4 belgianCup      = { { { 112, 1, 2, 80, 32, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { 84, 84, -108, 20 } };
CompetitionCup_4 bulgarianCup    = { { { 113, 1, 3, 80, 32, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { -108, -108, -108, 20 } };
CompetitionCup_4 croatianCup     = { { { 114, 1, 4, 80, 40, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { -108, -108, -108, -108 } };
CompetitionCup_4 cypriotCup      = { { { 115, 1, 5, 104, -128, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { -108, -108, 20, 0 } };
CompetitionCup_4 czechCup        = { { { 116, 1, 6, 72, 40, 0, 0, 0, 0 }, 0, 16, 0, 53, 1 }, { 84, 84, 84, 20 } };
CompetitionCup_4 danishCup       = { { { 117, 1, 7, 112, -120, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { 84, -108, 20, 0 } };
CompetitionCup_4 estonianCup     = { { { 122, 1, 10, -128, -120, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { 84, -108, 20, 0 } };
CompetitionCup_4 faroeIslesCup   = { { { 123, 1, 11, 56, 64, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { 84, -108, 20, 0 } };
CompetitionCup_4 finnishCup      = { { { 124, 1, 12, 32, 40, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup_4 greekCup        = { { { 127, 1, 15, 64, 40, 0, 0, 0, 0 }, 0, 16, 1, 53, 2 }, { -108, -108, -108, 20 } };
CompetitionCup_4 hungarianCup    = { { { -128, 1, 16, 104, 40, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { -108, -108, -108, -108 } };
CompetitionCup_4 icelandicCup    = { { { -127, 1, 17, 56, 64, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup_4 irishCup        = { { { -126, 1, 18, 112, -128, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 104, 104, 40, 0 } };
CompetitionCup_4 israeliCup      = { { { -125, 1, 19, 120, -128, 0, 0, 0, 0 }, 0, 16, 0, 53, 1 }, { 84, 84, 84, 20 } };
CompetitionCup_4 latvianCup      = { { { -123, 1, 21, 56, 64, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { -108, -108, 20, 0 } };
CompetitionCup_4 lithuanianCup   = { { { -122, 1, 22, -128, -120, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { -108, -108, 20, 0 } };
CompetitionCup_4 luxembourgCup   = { { { -121, 1, 23, 120, -120, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup_4 malteseCup      = { { { -120, 1, 24, -128, -120, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup_4 northernIrishCup = { { { -118, 1, 26, 104, -128, 0, 0, 0, 0 }, 0, 16, 0, 53, 6 }, { 104, 104, 104, 40 } };
CompetitionCup_4 norwegianCup    = { { { -117, 1, 27, 64, 80, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup_4 polishCup       = { { { -116, 1, 28, 80, 40, 0, 0, 0, 0 }, 0, 16, 1, 53, 5 }, { 84, -108, -108, 20 } };
CompetitionCup_4 portugeseCup    = { { { -115, 1, 29, 96, -120, 0, 0, 0, 0 }, 0, 16, 0, 53, 6 }, { 84, 84, 84, 20 } };
CompetitionCup_4 romanianCup     = { { { -114, 1, 30, 112, -120, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { 84, -108, -108, 20 } };
CompetitionCup_4 russianCup      = { { { -113, 1, 31, 16, 40, 0, 0, 0, 0 }, 0, 16, 0, 53, 3 }, { 84, 84, 84, 20 } };
CompetitionCup_4 sanMarinoCup    = { { { -112, 1, 32, 120, -128, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup_4 slovakianCup    = { { { -109, 1, 78, -128, -120, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { 84, -108, 20, 0 } };
CompetitionCup_4 slovenianCup    = { { { -108, 1, 34, 120, -120, 0, 0, 0, 0 }, 0, 16, 0, 53, 2 }, { 84, 84, 84, 20 } };
CompetitionCup_4 swedishCup      = { { { -106, 1, 36, 32, 40, 0, 0, 0, 0 }, 0, 16, 0, 53, 5 }, { 84, 84, 84, 20 } };
CompetitionCup_4 swissCup        = { { { -105, 1, 37, 120, -120, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup_4 turkishCup      = { { { -104, 1, 38, 88, 24, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { 84, 84, -108, -108 } };
CompetitionCup_4 ukrainianCup    = { { { -103, 1, 39, 120, -120, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { -108, -108, -108, 20 } };
CompetitionCup_4 welshCup        = { { { -102, 1, 40, 88, 32, 0, 0, 0, 0 }, 0, 16, 1, 53, 4 }, { 104, 104, -108, 40 } };
CompetitionCup_4 yugoslavianCup  = { { { -101, 1, 41, 80, 32, 0, 0, 0, 0 }, 0, 8, 1, 53, 2 }, { -108, -108, -108, 0 } };
CompetitionCup_4 algerianCup     = { { { -100, 1, 42, 96, -128, 0, 0, 0, 0 }, 0, 16, 1, 53, 2 }, { 84, 84, -108, 20 } };
CompetitionCup_4 southAfricanCup = { { { -96, 1, 69, 56, 80, 0, 0, 0, 0 }, 0, 16, 1, 53, 2 }, { 84, 84, 84, 20 } };
CompetitionCup_4 taiwaneseCup    = { { { -95, 1, 67, 96, 120, 0, 0, 0, 0 }, 0, 8, 1, 53, 2 }, { 84, 84, 20, 0 } };

/* Cups with names */
CompetitionCup_6_2 englishCup       = { { { 118, 1, 8, 96, -128, 15, 0, 0, 0 }, 0, 64, 0, 51, 3 }, { 104, 104, 104, 104, 40, 40 }, { aCup_4, aCup_5 } };
CompetitionCup_6_2 englishLeagueCup = { { { 119, 1, 8, 64, 16, 15, 0, 0, 0 }, 0, 64, 2, 51, 2 }, { -108, 104, 104, 104, -108, 40 }, { aLeagueCup, aLeagueCup_0 } };
CompetitionCup_6_2 englishCup_1     = { { { 120, 1, 8, 96, -128, 15, 0, 0, 0 }, 0, 32, 0, 51, 4 }, { 104, 104, 104, 40, 40, 0 }, { aCup_4, aCup_5 } };
CompetitionCup_6_2 englishLeagueCup_1 = { { { 121, 1, 8, 64, 16, 15, 0, 0, 0 }, 0, 32, 2, 51, 3 }, { 104, 104, 104, -108, 40, 0 }, { aLeagueCup, aLeagueCup_0 } };

CompetitionCup_6_2 frenchCup     = { { { 125, 1, 13, 104, -120, 15, 0, 0, 0 }, 0, 32, 0, 53, 1 }, { 84, 84, 84, 84, 20, 0 }, { aCoupeDeFrance, aCoupe } };
CompetitionCup_6_2 germanCup     = { { { 126, 1, 14, 64, 40, 15, 0, 0, 0 }, 0, 32, 0, 53, 3 }, { 84, 84, 84, 84, 20, 0 }, { aDFBPokal, aPokal } };
CompetitionCup_6_2 italianCup    = { { { -124, 1, 20, 56, 32, 15, 0, 0, 0 }, 0, 32, 1, 53, 2 }, { -108, -108, -108, -108, 20, 0 }, { aCoppaItalia, aCoppa } };
CompetitionCup_6_2 scottishCup   = { { { -111, 1, 33, 96, -128, 15, 0, 0, 0 }, 0, 32, 0, 51, 3 }, { 104, 104, 104, 104, 40, 0 }, { aCup_6, aCup_7 } };
CompetitionCup_6_2 scottishLeagueCup = { { { -110, 1, 33, 56, 72, 15, 0, 0, 0 }, 0, 32, 0, 51, 2 }, { 104, 104, 104, 40, 40, 0 }, { aLeagueCup_1, aLeagueCup_2 } };
CompetitionCup_6_2 spanishCup    = { { { -107, 1, 35, 88, 40, 15, 0, 0, 0 }, 0, 32, 1, 53, 5 }, { -108, -108, -108, -108, 20, 0 }, { aCopa, aCopa_0 } };

CompetitionCup_6 dutchCup       = { { { -119, 1, 25, 88, 32, 0, 0, 0, 0 }, 0, 32, 0, 53, 3 }, { 84, 84, 84, 84, 20, 0 } };
CompetitionCup_6 australianCup  = { { { -99, 1, 44, 40, 80, 0, 0, 0, 0 }, 0, 32, 1, 53, 2 }, { 84, 84, 84, 84, 20, 0 } };

CompetitionCup_4_2 japaneseCup        = { { { -98, 1, 55, 48, 56, 13, 0, 0, 0 }, 0, 8, 1, 53, 2 }, { 84, 84, 20, 0 }, { aCup_8, aCup_9 } };
CompetitionCup_4_2 japaneseEmperorsCup = { { { -97, 1, 55, 88, 0, 13, 0, 0, 0 }, 0, 16, 1, 53, 2 }, { 84, 84, 84, 20 }, { aEmperorSCup, aEmpCup } };

/* ======================================================================
 * Competition "Copy" variables (used at runtime for career/season)
 * ====================================================================== */

/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): euroCupCopy */
extern CompetitionCup2_4_2_32 euroCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): cupWinnersCupCopy */
extern CompetitionCup_6_2_64 cupWinnersCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): uefaCupCopy */
extern CompetitionCup_6_2_64 uefaCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): invitationTournamentCopy */
extern CompetitionCup_6_2_8 invitationTournamentCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): tourCopy */
extern CompetitionCup_6_2_8 tourCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): friendlyCopy */
extern CompetitionCup_6_2_12 friendlyCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): europeanChampionships2Copy */
extern CompetitionCup2_4_2_32 europeanChampionships2Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup1Copy */
extern CompetitionCup_6_2_12 ecQualifyingGroup1Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup2Copy */
extern CompetitionCup_6_2_12 ecQualifyingGroup2Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup3Copy */
extern CompetitionCup_6_2_12 ecQualifyingGroup3Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup4Copy */
extern CompetitionCup_6_2_12 ecQualifyingGroup4Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup5Copy */
extern CompetitionCup_6_2_12 ecQualifyingGroup5Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup6Copy */
extern CompetitionCup_6_2_12 ecQualifyingGroup6Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup7Copy */
extern CompetitionCup_6_2_12 ecQualifyingGroup7Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup8Copy */
extern CompetitionCup_6_2_8 ecQualifyingGroup8Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): copaAmericaCopy */
extern CompetitionCup2_4_2_24 copaAmericaCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): africanNationsCupCopy */
extern CompetitionCup2_6_2 africanNationsCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): northAmericanCupCopy */
extern CompetitionCup2_4_2 northAmericanCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): asianCupCopy */
extern CompetitionCup2_6_2 asianCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): oceaniaCupCopy */
extern CompetitionCup_6_2 oceaniaCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCup2Copy */
extern CompetitionCup2_6_2_64 worldCup2Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup1Copy */
extern CompetitionCup_6_2_12 worldCupQualifyingGroup1Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup2Copy */
extern CompetitionCup_6_2_12 worldCupQualifyingGroup2Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup3Copy */
extern CompetitionCup_6_2_12 worldCupQualifyingGroup3Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup4Copy */
extern CompetitionCup_6_2_12 worldCupQualifyingGroup4Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup5Copy */
extern CompetitionCup_6_2_12 worldCupQualifyingGroup5Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup6Copy */
extern CompetitionCup_6_2_12 worldCupQualifyingGroup6Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup7Copy */
extern CompetitionCup_6_2_12 worldCupQualifyingGroup7Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup8Copy */
extern CompetitionCup_6_2_12 worldCupQualifyingGroup8Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): northAmericaWorldCupQualificationCopy */
extern CompetitionCup2_6_2_24 northAmericaWorldCupQualificationCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): southAmericaWorldCupQualificationCopy */
extern CompetitionCup2_6_2_16 southAmericaWorldCupQualificationCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): africaWorldCupQualificationCopy */
extern CompetitionCup2_6_2_64 africaWorldCupQualificationCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): asiaWorldCupQualificationCopy */
extern CompetitionCup2_6_2_60 asiaWorldCupQualificationCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): oceaniaWorldCupQualificationCopy */
extern CompetitionCup_6_2_8 oceaniaWorldCupQualificationCopy;  /* aliased into careerFileBufferBuf */

/* ======================================================================
 * Career Contests
 * ====================================================================== */

/* 34 real entries + a 0xFFFFFFFF sentinel as the loop terminator
 * (matches `dc.l -1` after the last entry in SWOS2.s:118137). */
CompetitionCareer careerContests[35] = {
    { (CompetitionCup *)&euroCup, (CompetitionCup *)&euroCupCopy, 0x4E },
    { (CompetitionCup *)&cupWinnersCup, (CompetitionCup *)&cupWinnersCupCopy, 0x5C },
    { (CompetitionCup *)&uefaCup, (CompetitionCup *)&uefaCupCopy, 0x5C },
    { (CompetitionCup *)&invitationTournament, (CompetitionCup *)&invitationTournamentCopy, 0x24 },
    { (CompetitionCup *)&tour, (CompetitionCup *)&tourCopy, 0x24 },
    { (CompetitionCup *)&friendly, (CompetitionCup *)&friendlyCopy, 0x28 },
    { (CompetitionCup *)&europeanChampionships2, (CompetitionCup *)&europeanChampionships2Copy, 0x4E },
    { (CompetitionCup *)&ecQualifyingGroup1, (CompetitionCup *)&ecQualifyingGroup1Copy, 0x28 },
    { (CompetitionCup *)&ecQualifyingGroup2, (CompetitionCup *)&ecQualifyingGroup2Copy, 0x28 },
    { (CompetitionCup *)&ecQualifyingGroup3, (CompetitionCup *)&ecQualifyingGroup3Copy, 0x28 },
    { (CompetitionCup *)&ecQualifyingGroup4, (CompetitionCup *)&ecQualifyingGroup4Copy, 0x28 },
    { (CompetitionCup *)&ecQualifyingGroup5, (CompetitionCup *)&ecQualifyingGroup5Copy, 0x28 },
    { (CompetitionCup *)&ecQualifyingGroup6, (CompetitionCup *)&ecQualifyingGroup6Copy, 0x28 },
    { (CompetitionCup *)&ecQualifyingGroup7, (CompetitionCup *)&ecQualifyingGroup7Copy, 0x28 },
    { (CompetitionCup *)&ecQualifyingGroup8, (CompetitionCup *)&ecQualifyingGroup8Copy, 0x24 },
    { (CompetitionCup *)&copaAmerica, (CompetitionCup *)&copaAmericaCopy, 0x46 },
    { (CompetitionCup *)&africanNationsCup, (CompetitionCup *)&africanNationsCupCopy, 0x30 },
    { (CompetitionCup *)&northAmericanCup, (CompetitionCup *)&northAmericanCupCopy, 0x2E },
    { (CompetitionCup *)&asianCup, (CompetitionCup *)&asianCupCopy, 0x30 },
    { (CompetitionCup *)&oceaniaCup, (CompetitionCup *)&oceaniaCupCopy, 0x1C },
    { (CompetitionCup *)&worldCup2, (CompetitionCup *)&worldCup2Copy, 0x70 },
    { (CompetitionCup *)&worldCupQualifyingGroup1, (CompetitionCup *)&worldCupQualifyingGroup1Copy, 0x28 },
    { (CompetitionCup *)&worldCupQualifyingGroup2, (CompetitionCup *)&worldCupQualifyingGroup2Copy, 0x28 },
    { (CompetitionCup *)&worldCupQualifyingGroup3, (CompetitionCup *)&worldCupQualifyingGroup3Copy, 0x28 },
    { (CompetitionCup *)&worldCupQualifyingGroup4, (CompetitionCup *)&worldCupQualifyingGroup4Copy, 0x28 },
    { (CompetitionCup *)&worldCupQualifyingGroup5, (CompetitionCup *)&worldCupQualifyingGroup5Copy, 0x28 },
    { (CompetitionCup *)&worldCupQualifyingGroup6, (CompetitionCup *)&worldCupQualifyingGroup6Copy, 0x28 },
    { (CompetitionCup *)&worldCupQualifyingGroup7, (CompetitionCup *)&worldCupQualifyingGroup7Copy, 0x28 },
    { (CompetitionCup *)&worldCupQualifyingGroup8, (CompetitionCup *)&worldCupQualifyingGroup8Copy, 0x28 },
    { (CompetitionCup *)&northAmericaWorldCupQualification, (CompetitionCup *)&northAmericaWorldCupQualificationCopy, 0x48 },
    { (CompetitionCup *)&southAmericaWorldCupQualification, (CompetitionCup *)&southAmericaWorldCupQualificationCopy, 0x40 },
    { (CompetitionCup *)&africaWorldCupQualification, (CompetitionCup *)&africaWorldCupQualificationCopy, 0x70 },
    { (CompetitionCup *)&asiaWorldCupQualification, (CompetitionCup *)&asiaWorldCupQualificationCopy, 0x6C },
    { (CompetitionCup *)&oceaniaWorldCupQualification, (CompetitionCup *)&oceaniaWorldCupQualificationCopy, 0x24 },
    { (CompetitionCup *)0xFFFFFFFFu, (CompetitionCup *)0, 0 },  /* sentinel */
};

/* ======================================================================
 * Master Competition Pointer Arrays
 * ====================================================================== */

Competition *nationalTeamCompetitionsAndQualifications[39] = {
    (Competition *)&worldCup, (Competition *)&europeanChampionships,
    (Competition *)&copaAmerica, (Competition *)&northAmericanCup,
    (Competition *)&northAmericanCupCopy, (Competition *)&africanNationsCup,
    (Competition *)&africanNationsCupCopy, (Competition *)&asianCup,
    (Competition *)&asianCupCopy, (Competition *)&oceaniaCup,
    (Competition *)&oceaniaCupCopy, (Competition *)&friendlyCopy,
    (Competition *)&invitationTournamentCopy, (Competition *)&tourCopy,
    (Competition *)&copaAmericaCopy, (Competition *)&europeanChampionships2Copy,
    (Competition *)&ecQualifyingGroup1Copy, (Competition *)&ecQualifyingGroup2Copy,
    (Competition *)&ecQualifyingGroup3Copy, (Competition *)&ecQualifyingGroup4Copy,
    (Competition *)&ecQualifyingGroup5Copy, (Competition *)&ecQualifyingGroup6Copy,
    (Competition *)&ecQualifyingGroup7Copy, (Competition *)&ecQualifyingGroup8Copy,
    (Competition *)&worldCup2Copy,
    (Competition *)&worldCupQualifyingGroup1Copy, (Competition *)&worldCupQualifyingGroup2Copy,
    (Competition *)&worldCupQualifyingGroup3Copy, (Competition *)&worldCupQualifyingGroup4Copy,
    (Competition *)&worldCupQualifyingGroup5Copy, (Competition *)&worldCupQualifyingGroup6Copy,
    (Competition *)&worldCupQualifyingGroup7Copy, (Competition *)&worldCupQualifyingGroup8Copy,
    (Competition *)&northAmericaWorldCupQualificationCopy,
    (Competition *)&southAmericaWorldCupQualificationCopy,
    (Competition *)&africaWorldCupQualificationCopy,
    (Competition *)&asiaWorldCupQualificationCopy,
    (Competition *)&oceaniaWorldCupQualificationCopy,
    (Competition *)-1
};

CompetitionCup *continentalCups[7] = {
    (CompetitionCup *)&europeanChampionships, (CompetitionCup *)&copaAmerica,
    (CompetitionCup *)&northAmericanCup, (CompetitionCup *)&africanNationsCup,
    (CompetitionCup *)&asianCup, (CompetitionCup *)&oceaniaCup, (CompetitionCup *)-1
};

/* ======================================================================
 * Qualifying Groups and Region Tables
 * ====================================================================== */

QualifyingGroup_9_2 worldCupQualifyingGroups = {
    { (CompetitionCup *)&worldCupQualifyingGroup1, (CompetitionCup *)&worldCupQualifyingGroup2,
      (CompetitionCup *)&worldCupQualifyingGroup3, (CompetitionCup *)&worldCupQualifyingGroup4,
      (CompetitionCup *)&worldCupQualifyingGroup5, (CompetitionCup *)&worldCupQualifyingGroup6,
      (CompetitionCup *)&worldCupQualifyingGroup7, (CompetitionCup *)&worldCupQualifyingGroup8,
      (CompetitionCup *)-1 },
    { 0xFF, CN_ALBANIA }
};

QualifyingGroup_9_2 ecQualifyingGroups = {
    { (CompetitionCup *)&ecQualifyingGroup1, (CompetitionCup *)&ecQualifyingGroup2,
      (CompetitionCup *)&ecQualifyingGroup3, (CompetitionCup *)&ecQualifyingGroup4,
      (CompetitionCup *)&ecQualifyingGroup5, (CompetitionCup *)&ecQualifyingGroup6,
      (CompetitionCup *)&ecQualifyingGroup7, (CompetitionCup *)&ecQualifyingGroup8,
      (CompetitionCup *)-1 },
    { 0xFF, CN_ALBANIA }
};

QualifyingGroup_3 customsTable = { { (CompetitionCup *)&customsLeague, (CompetitionCup *)-2, (CompetitionCup *)-1 } };

QualifyingGroup_2_8 worldTable = {
    { (CompetitionCup *)&worldCup, (CompetitionCup *)-1 },
    { CN_EUROPE, CN_NORTH_AMERICA, CN_SOUTH_AMERICA, CN_AFRICA, CN_ASIA, CN_OCEANIA, 0xFF, CN_ALBANIA }
};

QualifyingGroup_6_44 europeTable = {
    { (CompetitionCup *)0xFC, (CompetitionCup *)&europeanChampionships,
      (CompetitionCup *)&euroCup, (CompetitionCup *)&cupWinnersCup,
      (CompetitionCup *)&uefaCup, (CompetitionCup *)-1 },
    { CN_ALBANIA, CN_AUSTRIA, CN_BELARUS, CN_BELGIUM, CN_BULGARIA, CN_CROATIA,
      CN_CYPRUS, CN_CZECH_REPUBLIC, CN_DENMARK, CN_ENGLAND, CN_ESTONIA, CN_FAROE_ISLANDS,
      CN_FINLAND, CN_FRANCE, CN_GERMANY, CN_GREECE, CN_HOLLAND, CN_HUNGARY,
      CN_ICELAND, CN_ISRAEL, CN_ITALY, CN_LATVIA, CN_LITHUANIA, CN_LUXEMBOURG,
      CN_MALTA, CN_NORTHERN_IRELAND, CN_NORWAY, CN_POLAND, CN_PORTUGAL,
      CN_REPUBLIC_IRELAND, CN_ROMANIA, CN_RUSSIA, CN_SAN_MARINO, CN_SCOTLAND,
      CN_SLOVAKIA, CN_SLOVENIA, CN_SPAIN, CN_SWEDEN, CN_SWITZERLAND, CN_TURKEY,
      CN_UKRAINE, CN_WALES, CN_YUGOSLAVIA, 0xFF }
};

QualifyingGroup_3_4 africaTable = {
    { (CompetitionCup *)&africaWorldCupQualification, (CompetitionCup *)&africanNationsCup, (CompetitionCup *)-1 },
    { CN_ALGERIA, CN_GHANA, CN_SOUTH_AFRICA, 0xFF }
};

QualifyingGroup_3_12 southAmericaTable = {
    { (CompetitionCup *)&southAmericaWorldCupQualification, (CompetitionCup *)&copaAmerica, (CompetitionCup *)-1 },
    { CN_ARGENTINA, CN_BOLIVIA, CN_BRAZIL, CN_CHILE, CN_COLOMBIA, CN_ECUADOR,
      CN_PARAGUAY, CN_PERU, CN_SURINAM, CN_URUGUAY, CN_VENEZUELA, 0xFF }
};

QualifyingGroup_3_4 northAmericaTable = {
    { (CompetitionCup *)&northAmericaWorldCupQualification, (CompetitionCup *)&northAmericanCup, (CompetitionCup *)-1 },
    { CN_EL_SALVADOR, CN_MEXICO, CN_USA, 0xFF }
};

QualifyingGroup_3_4 asiaTable = {
    { (CompetitionCup *)&asiaWorldCupQualification, (CompetitionCup *)&asianCup, (CompetitionCup *)-1 },
    { CN_INDIA, CN_JAPAN, CN_TAIWAN, 0xFF }
};

QualifyingGroup_3_4 oceaniaTable = {
    { (CompetitionCup *)&oceaniaWorldCupQualification, (CompetitionCup *)&oceaniaCup, (CompetitionCup *)-1 },
    { CN_AUSTRALIA, CN_NEW_ZEALAND, 0xFF, CN_ALBANIA }
};

/* ======================================================================
 * Country Competition Tables
 * ====================================================================== */

Competition *albaniaTable[4]         = { (Competition *)&albanianLeague, (Competition *)-2, (Competition *)&albanianCup, (Competition *)-1 };
Competition *austriaTable[4]         = { (Competition *)&austrianLeague, (Competition *)-2, (Competition *)&austrianCup, (Competition *)-1 };
Competition *belgiumTable[4]         = { (Competition *)&belgianLeague, (Competition *)-2, (Competition *)&belgianCup, (Competition *)-1 };
Competition *belarusTable[4]         = { (Competition *)&belarussiaLleague, (Competition *)-2, (Competition *)&belarusianCup, (Competition *)-1 };
Competition *bulgariaTable[4]        = { (Competition *)&bulgarianLeague, (Competition *)-2, (Competition *)&bulgarianCup, (Competition *)-1 };
Competition *croatiaTable[4]         = { (Competition *)&croatianLeague, (Competition *)-2, (Competition *)&croatianCup, (Competition *)-1 };
Competition *cyprusTable[4]          = { (Competition *)&cypriotLeague, (Competition *)-2, (Competition *)&cypriotCup, (Competition *)-1 };
Competition *czechRepublicTable[4]   = { (Competition *)&czechLeague, (Competition *)-2, (Competition *)&czechCup, (Competition *)-1 };
Competition *denmarkTable[4]         = { (Competition *)&danishLeague, (Competition *)-2, (Competition *)&danishCup, (Competition *)-1 };
Competition *englandTable[5]         = { (Competition *)&englishLeagues, (Competition *)-2, (Competition *)&englishCup, (Competition *)&englishLeagueCup, (Competition *)-1 };
Competition *estoniaTable[4]         = { (Competition *)&estonianLeague, (Competition *)-2, (Competition *)&estonianCup, (Competition *)-1 };
Competition *faroeIslandsTable[4]    = { (Competition *)&faroeIslesLeague, (Competition *)-2, (Competition *)&faroeIslesCup, (Competition *)-1 };
Competition *finlandTable[4]         = { (Competition *)&finnishLeague, (Competition *)-2, (Competition *)&finnishCup, (Competition *)-1 };
Competition *franceTable[4]          = { (Competition *)&frenchLeague, (Competition *)-2, (Competition *)&frenchCup, (Competition *)-1 };
Competition *germanyTable[4]         = { (Competition *)&germanLeague, (Competition *)-2, (Competition *)&germanCup, (Competition *)-1 };
Competition *greeceTable[4]          = { (Competition *)&greekLeague, (Competition *)-2, (Competition *)&greekCup, (Competition *)-1 };
Competition *hungaryTable[4]         = { (Competition *)&hungarianLeague, (Competition *)-2, (Competition *)&hungarianCup, (Competition *)-1 };
Competition *icelandTable[4]         = { (Competition *)&icelandicLeague, (Competition *)-2, (Competition *)&icelandicCup, (Competition *)-1 };
Competition *republicIrelandTable[4] = { (Competition *)&irishLeague, (Competition *)-2, (Competition *)&irishCup, (Competition *)-1 };
Competition *israelTable[4]          = { (Competition *)&israeliLeague, (Competition *)-2, (Competition *)&israeliCup, (Competition *)-1 };
Competition *italyTable[4]           = { (Competition *)&italianLeague, (Competition *)-2, (Competition *)&italianCup, (Competition *)-1 };
Competition *latviaTable[4]          = { (Competition *)&latvianLeague, (Competition *)-2, (Competition *)&latvianCup, (Competition *)-1 };
Competition *lithuaniaTable[4]       = { (Competition *)&lithuanianLeague, (Competition *)-2, (Competition *)&lithuanianCup, (Competition *)-1 };
Competition *luxembourgTable[4]      = { (Competition *)&luxembourgLeague, (Competition *)-2, (Competition *)&luxembourgCup, (Competition *)-1 };
Competition *maltaTable[4]           = { (Competition *)&malteseLeague, (Competition *)-2, (Competition *)&malteseCup, (Competition *)-1 };
Competition *hollandTable[4]         = { (Competition *)&dutchLeague, (Competition *)-2, (Competition *)&dutchCup, (Competition *)-1 };
Competition *northernIrelandTable[4] = { (Competition *)&northernIrishLeague, (Competition *)-2, (Competition *)&northernIrishCup, (Competition *)-1 };
Competition *norwayTable[4]          = { (Competition *)&norwegianLeague, (Competition *)-2, (Competition *)&norwegianCup, (Competition *)-1 };
Competition *polandTable[4]          = { (Competition *)&polishLeague, (Competition *)-2, (Competition *)&polishCup, (Competition *)-1 };
Competition *portugalTable[4]        = { (Competition *)&portugeseLeague, (Competition *)-2, (Competition *)&portugeseCup, (Competition *)-1 };
Competition *romaniaTable[4]         = { (Competition *)&romanianLeague, (Competition *)-2, (Competition *)&romanianCup, (Competition *)-1 };
Competition *russiaTable[4]          = { (Competition *)&russianLeague, (Competition *)-2, (Competition *)&russianCup, (Competition *)-1 };
Competition *sanMarinoTable[4]       = { (Competition *)&sanMarinoLeague, (Competition *)-2, (Competition *)&sanMarinoCup, (Competition *)-1 };
Competition *scotlandTable[5]        = { (Competition *)&scottishLeague, (Competition *)-2, (Competition *)&scottishCup, (Competition *)&scottishLeagueCup, (Competition *)-1 };
Competition *slovakiaTable[4]        = { (Competition *)&slovakianLeague, (Competition *)-2, (Competition *)&slovakianCup, (Competition *)-1 };
Competition *sloveniaTable[4]        = { (Competition *)&slovenianLeague, (Competition *)-2, (Competition *)&slovenianCup, (Competition *)-1 };
Competition *spainTable[4]           = { (Competition *)&spanishLeague, (Competition *)-2, (Competition *)&spanishCup, (Competition *)-1 };
Competition *swedenTable[4]          = { (Competition *)&swedishLeague, (Competition *)-2, (Competition *)&swedishCup, (Competition *)-1 };
Competition *switzerlandTable[4]     = { (Competition *)&swissLeague, (Competition *)-2, (Competition *)&swissCup, (Competition *)-1 };
Competition *turkeyTable[4]          = { (Competition *)&turkishLeague, (Competition *)-2, (Competition *)&turkishCup, (Competition *)-1 };
Competition *ukraineTable[4]         = { (Competition *)&ukrainianLeague, (Competition *)-2, (Competition *)&ukrainianCup, (Competition *)-1 };
Competition *walesTable[4]           = { (Competition *)&welshLeague, (Competition *)-2, (Competition *)&welshCup, (Competition *)-1 };
Competition *yugoslaviaTable[4]      = { (Competition *)&yugoslavianLeague, (Competition *)-2, (Competition *)&yugoslavianCup, (Competition *)-1 };
Competition *algeriaTable[4]         = { (Competition *)&algerianLeague, (Competition *)-2, (Competition *)&algerianCup, (Competition *)-1 };
Competition *argentinaTable[3]       = { (Competition *)&argentinianLeague, (Competition *)-2, (Competition *)-1 };
Competition *australiaTable[4]       = { (Competition *)&australianLeague, (Competition *)-2, (Competition *)&australianCup, (Competition *)-1 };
Competition *boliviaTable[3]         = { (Competition *)&bolivianLeague, (Competition *)-2, (Competition *)-1 };
Competition *brasilTable[3]          = { (Competition *)&brasilianLeague, (Competition *)-2, (Competition *)-1 };
Competition *chileTable[3]           = { (Competition *)&chileanLeague, (Competition *)-2, (Competition *)-1 };
Competition *colombiaTable[3]        = { (Competition *)&colombianLeague, (Competition *)-2, (Competition *)-1 };
Competition *ecuadorTable[3]         = { (Competition *)&ecuadorLeague, (Competition *)-2, (Competition *)-1 };
Competition *elSalvadorTable[3]      = { (Competition *)&elSalvadorLeague, (Competition *)-2, (Competition *)-1 };
Competition *indiaTable[3]           = { (Competition *)&indianLeague, (Competition *)-2, (Competition *)-1 };
Competition *japanTable[5]           = { (Competition *)&japaneseLeague, (Competition *)-2, (Competition *)&japaneseCup, (Competition *)&japaneseEmperorsCup, (Competition *)-1 };
Competition *mexicoTable[3]          = { (Competition *)&mexicanLeague, (Competition *)-2, (Competition *)-1 };
Competition *newZealandTable[3]      = { (Competition *)&newZealandLeague, (Competition *)-2, (Competition *)-1 };
Competition *paraguayTable[3]        = { (Competition *)&paraguayanLeague, (Competition *)-2, (Competition *)-1 };
Competition *peruTable[3]            = { (Competition *)&peruvianLeague, (Competition *)-2, (Competition *)-1 };
Competition *southAfricaTable[4]     = { (Competition *)&southAfricanLeague, (Competition *)-2, (Competition *)&southAfricanCup, (Competition *)-1 };
Competition *surinamTable[3]         = { (Competition *)&surinamLeague, (Competition *)-2, (Competition *)-1 };
Competition *taiwanTable[4]          = { (Competition *)&taiwaneseLeague, (Competition *)-2, (Competition *)&taiwaneseCup, (Competition *)-1 };
Competition *uruguayTable[3]         = { (Competition *)&uruguayanLeague, (Competition *)-2, (Competition *)-1 };
Competition *usaTable[3]             = { (Competition *)&americanLeague, (Competition *)-2, (Competition *)-1 };
Competition *venezuelaTable[3]       = { (Competition *)&venezuelanLeague, (Competition *)-2, (Competition *)-1 };
Competition *ghanaTable[3]           = { (Competition *)&ghanaLeague, (Competition *)-2, (Competition *)-1 };

/* ======================================================================
 * Season flags and scheduling data
 * ====================================================================== */

int16_t seasonCycleNationalFlagsA[5] = { -1, -1, -1, -1, -1 };
uint16_t seasonCycleNationalFlagsB[5] = { 0x5005, 0x5025, 0x5022, 0x5011, 0x500B };

uint8_t careerTeamFlagsBySeason0[20] = { 64, 128, 1, 2, 64, 128, 1, 2, 64, 128, 1, 2, 64, 128, 1, 2, 64, 128, 1, 2 };
uint8_t careerTeamFlagsBySeason1[20] = { 66, 128, 2, 0, 66, 128, 2, 0, 66, 128, 2, 0, 66, 128, 2, 0, 66, 128, 2, 0 };

uint32_t nationalSelectorSeasonMaskA = 0x2AAAA;
uint32_t nationalSelectorSeasonMaskB = 0x2EEEE;

/* ======================================================================
 * Countries list and career remap
 * ====================================================================== */

CountryNumbers allCountriesList[66] = {
    CN_BRAZIL, CN_ALBANIA, CN_AUSTRIA, CN_BELGIUM, CN_BELARUS, CN_BULGARIA,
    CN_CROATIA, CN_CYPRUS, CN_CZECH_REPUBLIC, CN_DENMARK, CN_ENGLAND, CN_ESTONIA,
    CN_FAROE_ISLANDS, CN_FINLAND, CN_FRANCE, CN_GERMANY, CN_GREECE, CN_HUNGARY,
    CN_ICELAND, CN_REPUBLIC_IRELAND, CN_ISRAEL, CN_ITALY, CN_LATVIA, CN_LITHUANIA,
    CN_LUXEMBOURG, CN_MALTA, CN_HOLLAND, CN_NORTHERN_IRELAND, CN_NORWAY, CN_POLAND,
    CN_PORTUGAL, CN_ROMANIA, CN_RUSSIA, CN_SAN_MARINO, CN_SCOTLAND, CN_SLOVAKIA,
    CN_SLOVENIA, CN_SPAIN, CN_SWEDEN, CN_SWITZERLAND, CN_TURKEY, CN_UKRAINE,
    CN_WALES, CN_YUGOSLAVIA, CN_ALGERIA, CN_SOUTH_AFRICA, CN_GHANA, CN_ARGENTINA,
    CN_BOLIVIA, CN_BRAZIL, CN_CHILE, CN_COLOMBIA, CN_ECUADOR, CN_PARAGUAY,
    CN_PERU, CN_SURINAM, CN_URUGUAY, CN_VENEZUELA, CN_EL_SALVADOR, CN_MEXICO,
    CN_USA, CN_JAPAN, CN_TAIWAN, CN_INDIA, CN_AUSTRALIA, CN_NEW_ZEALAND
};

uint8_t careerCompetitionIdRemapTable[66] = {
    0, 1, 76, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 78, 34, 35, 36, 37, 38, 39, 40, 41, 42, 79, 69, 43, 45,
    46, 48, 49, 50, 64, 65, 66, 71, 77, 51, 60, 73, 75, 55, 67, 44,
    62, 255
};

/* ======================================================================
 * Euro Cup Qualifying Seed Lists
 * ====================================================================== */

uint8_t euroCupQualSeedList4[4]  = { 14, 20, 35, 8 };
uint8_t euroCupQualSeedList6[6]  = { 2, 13, 29, 30, 31, 41 };
uint8_t euroCupQualSeedList11[11] = { 1, 3, 6, 7, 15, 25, 28, 33, 36, 37, 38 };
uint8_t euroCupQualSeedList22[23] = { 16, 12, 0, 17, 5, 27, 26, 24, 18, 23, 4, 10, 11, 19, 21, 22, 32, 34, 39, 40, 76, 78, 0 };

uint8_t euroCupQualSeedListReduced4[4]  = { 14, 20, 35, 8 };
uint8_t euroCupQualSeedListReduced6[6]  = { 2, 13, 29, 30, 31, 41 };
uint8_t euroCupQualSeedListReduced11[11] = { 1, 3, 6, 7, 15, 25, 28, 33, 36, 37, 38 };
uint8_t euroCupQualSeedListReduced22[23] = { 16, 12, 0, 17, 5, 27, 26, 24, 18, 23, 4, 10, 11, 19, 21, 22, 32, 34, 39, 40, 76, 78, 0 };

uint8_t euroCupQualSeedBucket15[15] = { 14, 14, 14, 14, 20, 20, 20, 20, 35, 35, 35, 35, 8, 8, 8 };
uint8_t euroCupQualSeedBucket18[18] = { 2, 2, 2, 13, 13, 13, 29, 29, 29, 30, 30, 30, 31, 31, 31, 41, 41, 41 };
uint8_t euroCupQualSeedBucket22[22] = { 1, 1, 3, 3, 6, 6, 7, 7, 15, 15, 25, 25, 28, 28, 33, 33, 36, 36, 37, 37, 38, 38 };
uint8_t euroCupQualSeedBucket22Full[23] = { 16, 12, 0, 17, 5, 27, 26, 24, 18, 23, 4, 10, 11, 19, 21, 22, 32, 34, 39, 40, 76, 78, 255 };

/* ======================================================================
 * Cup Pick Tables (per-region competition selection for career)
 * ====================================================================== */

CompetitionCup *cupPickOceaniaInviteFriendly[5] = { (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickOceaniaOceaniaCupFriendly[5] = { (CompetitionCup *)&oceaniaCupCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickOceaniaWorldCupFriendly[5] = { (CompetitionCup *)&worldCup2Copy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickOceaniaWorldCupInvite[5] = { (CompetitionCup *)&worldCup2Copy, (CompetitionCup *)-2, (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1 };
CompetitionCup *cupPickOceaniaOceaniaCupNonFinalist[5] = { 0, (CompetitionCup *)-2, (CompetitionCup *)&oceaniaCupCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickOceaniaOceaniaCupFinalist[5] = { (CompetitionCup *)&oceaniaCupCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };

CompetitionCup *cupPickNAmericaInviteFriendly[5] = { (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickNAmericaNaCupFriendly[5] = { (CompetitionCup *)&northAmericanCupCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickNAmericaWorldCupFriendly[5] = { (CompetitionCup *)&worldCup2Copy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickNAmericaWorldCupInvite[5] = { (CompetitionCup *)&worldCup2Copy, (CompetitionCup *)-2, (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1 };
CompetitionCup *cupPickNAmericaNaCupNonFinalist[5] = { 0, (CompetitionCup *)-2, (CompetitionCup *)&northAmericanCupCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickNAmericaNaCupFinalist[5] = { (CompetitionCup *)&northAmericanCupCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };

CompetitionCup *cupPickAsiaInviteFriendly[5] = { (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickAsiaAsianCupFriendly[5] = { (CompetitionCup *)&asianCupCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickAsiaWorldCupFriendly[5] = { (CompetitionCup *)&worldCup2Copy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickAsiaWorldCupInvite[5] = { (CompetitionCup *)&worldCup2Copy, (CompetitionCup *)-2, (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1 };
CompetitionCup *cupPickAsiaAsianCupNonFinalist[5] = { 0, (CompetitionCup *)-2, (CompetitionCup *)&asianCupCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickAsiaAsianCupFinalist[5] = { (CompetitionCup *)&asianCupCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };

CompetitionCup *cupPickAfricaInviteFriendly[5] = { (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickAfricaAfricanCupFriendly[5] = { (CompetitionCup *)&africanNationsCupCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickAfricaWorldCupFriendly[5] = { (CompetitionCup *)&worldCup2Copy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickAfricaWorldCupInvite[5] = { (CompetitionCup *)&worldCup2Copy, (CompetitionCup *)-2, (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1 };
CompetitionCup *cupPickAfricaAfricanCupNonFinalist[5] = { 0, (CompetitionCup *)-2, (CompetitionCup *)&africanNationsCupCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickAfricaAfricanCupFinalist[5] = { (CompetitionCup *)&africanNationsCupCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };

CompetitionCup *cupPickSAmericaInviteFriendly[5] = { (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickSAmericaCopaAmericaFriendly[5] = { (CompetitionCup *)&copaAmericaCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickSAmericaWorldCupFriendly[5] = { (CompetitionCup *)&worldCup2Copy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickSAmericaWorldCupInvite[5] = { (CompetitionCup *)&worldCup2Copy, (CompetitionCup *)-2, (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1 };
CompetitionCup *cupPickSAmericaCopaAmericaNonFinalist[5] = { 0, (CompetitionCup *)-2, (CompetitionCup *)&copaAmericaCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickSAmericaCopaAmericaFinalist[5] = { (CompetitionCup *)&copaAmericaCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };

CompetitionCup *ecQualifyingGroupsTable[8] = {
    (CompetitionCup *)&ecQualifyingGroup1Copy, (CompetitionCup *)&ecQualifyingGroup2Copy,
    (CompetitionCup *)&ecQualifyingGroup3Copy, (CompetitionCup *)&ecQualifyingGroup4Copy,
    (CompetitionCup *)&ecQualifyingGroup5Copy, (CompetitionCup *)&ecQualifyingGroup6Copy,
    (CompetitionCup *)&ecQualifyingGroup7Copy, (CompetitionCup *)&ecQualifyingGroup8Copy
};

CompetitionCup *cupPickEuropeNoCupFriendly[5] = { 0, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickListEuropeInviteTournamentFriendly[5] = { (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickLisEuropeEuroChampsFriendly[5] = { (CompetitionCup *)&europeanChampionships2Copy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickListEuropeEuroChampsInviteFriendly[5] = { (CompetitionCup *)&europeanChampionships2Copy, (CompetitionCup *)-2, (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1 };
CompetitionCup *cupPickListEuropeWorldCupFriendly[5] = { (CompetitionCup *)&worldCup2Copy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickLisEuropeWorldCupInviteFriendly[5] = { (CompetitionCup *)&worldCup2Copy, (CompetitionCup *)-2, (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1 };
CompetitionCup *cupPickEuropeNoCupFriendlyAlt[5] = { 0, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };
CompetitionCup *cupPickListEuropeInviteTournamentFriendlyAlt[5] = { (CompetitionCup *)&invitationTournamentCopy, (CompetitionCup *)-2, (CompetitionCup *)&friendlyCopy, (CompetitionCup *)-1, 0 };

CompetitionCup *worldCupQualifyingGroupsTable[8] = {
    (CompetitionCup *)&worldCupQualifyingGroup1Copy, (CompetitionCup *)&worldCupQualifyingGroup2Copy,
    (CompetitionCup *)&worldCupQualifyingGroup3Copy, (CompetitionCup *)&worldCupQualifyingGroup4Copy,
    (CompetitionCup *)&worldCupQualifyingGroup5Copy, (CompetitionCup *)&worldCupQualifyingGroup6Copy,
    (CompetitionCup *)&worldCupQualifyingGroup7Copy, (CompetitionCup *)&worldCupQualifyingGroup8Copy
};

/* Tour string table */
extern char aEuropeanTour[];
extern char aAfricanTour[];
extern char aSouthAmericanT[];
extern char aNorthAmericanT[];
extern char aAsianTour[];
extern char aOceaniaTour[];
char *tourStringTable[6] = { aEuropeanTour, aAfricanTour, aSouthAmericanT, aNorthAmericanT, aAsianTour, aOceaniaTour };

/* Career region weights */
uint8_t careerRegionPickWeightsTable[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4 };
uint8_t careerRegionPickWeightsTable2[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4 };

/* ======================================================================
 * competitionsTable[256] — country-code → country competition list.
 *
 * Indexed by team-file country byte (CN_*). Most slots are zero (no
 * country at that index). The special entries are 72 (customsTable for
 * ORIGINAL_CUSTOM_TEAMS), 100 (customsTable for EDITED_CUSTOM_TEAMS),
 * 80–85 (region tables), 252–254 (qualifying-group / world tables).
 *
 * Without this, _SelectTeamsFinalMenu reads competitionsTable[254]=0 for
 * the CLUB_TEAMS path, panics with code 0x8632, and falls into
 * _PanicHaltLoop → _FadeOutToBlack → spin forever waiting for both
 * players' fire buttons (looks like a black screen on SDL). Layout
 * transcribed verbatim from SWOS2.s lines 124186–124443. */
uint32_t competitionsTable[256] = {
    [0]  = (uint32_t)(uintptr_t)albaniaTable,
    [1]  = (uint32_t)(uintptr_t)austriaTable,
    [2]  = (uint32_t)(uintptr_t)belgiumTable,
    [3]  = (uint32_t)(uintptr_t)bulgariaTable,
    [4]  = (uint32_t)(uintptr_t)croatiaTable,
    [5]  = (uint32_t)(uintptr_t)cyprusTable,
    [6]  = (uint32_t)(uintptr_t)czechRepublicTable,
    [7]  = (uint32_t)(uintptr_t)denmarkTable,
    [8]  = (uint32_t)(uintptr_t)englandTable,
    [10] = (uint32_t)(uintptr_t)estoniaTable,
    [11] = (uint32_t)(uintptr_t)faroeIslandsTable,
    [12] = (uint32_t)(uintptr_t)finlandTable,
    [13] = (uint32_t)(uintptr_t)franceTable,
    [14] = (uint32_t)(uintptr_t)germanyTable,
    [15] = (uint32_t)(uintptr_t)greeceTable,
    [16] = (uint32_t)(uintptr_t)hungaryTable,
    [17] = (uint32_t)(uintptr_t)icelandTable,
    [18] = (uint32_t)(uintptr_t)republicIrelandTable,
    [19] = (uint32_t)(uintptr_t)israelTable,
    [20] = (uint32_t)(uintptr_t)italyTable,
    [21] = (uint32_t)(uintptr_t)latviaTable,
    [22] = (uint32_t)(uintptr_t)lithuaniaTable,
    [23] = (uint32_t)(uintptr_t)luxembourgTable,
    [24] = (uint32_t)(uintptr_t)maltaTable,
    [25] = (uint32_t)(uintptr_t)hollandTable,
    [26] = (uint32_t)(uintptr_t)northernIrelandTable,
    [27] = (uint32_t)(uintptr_t)norwayTable,
    [28] = (uint32_t)(uintptr_t)polandTable,
    [29] = (uint32_t)(uintptr_t)portugalTable,
    [30] = (uint32_t)(uintptr_t)romaniaTable,
    [31] = (uint32_t)(uintptr_t)russiaTable,
    [32] = (uint32_t)(uintptr_t)sanMarinoTable,
    [33] = (uint32_t)(uintptr_t)scotlandTable,
    [34] = (uint32_t)(uintptr_t)sloveniaTable,
    [35] = (uint32_t)(uintptr_t)spainTable,
    [36] = (uint32_t)(uintptr_t)swedenTable,
    [37] = (uint32_t)(uintptr_t)switzerlandTable,
    [38] = (uint32_t)(uintptr_t)turkeyTable,
    [39] = (uint32_t)(uintptr_t)ukraineTable,
    [40] = (uint32_t)(uintptr_t)walesTable,
    [41] = (uint32_t)(uintptr_t)yugoslaviaTable,
    [42] = (uint32_t)(uintptr_t)algeriaTable,
    [43] = (uint32_t)(uintptr_t)argentinaTable,
    [44] = (uint32_t)(uintptr_t)australiaTable,
    [45] = (uint32_t)(uintptr_t)boliviaTable,
    [46] = (uint32_t)(uintptr_t)brasilTable,
    [48] = (uint32_t)(uintptr_t)chileTable,
    [49] = (uint32_t)(uintptr_t)colombiaTable,
    [50] = (uint32_t)(uintptr_t)ecuadorTable,
    [51] = (uint32_t)(uintptr_t)elSalvadorTable,
    [55] = (uint32_t)(uintptr_t)japanTable,
    [60] = (uint32_t)(uintptr_t)mexicoTable,
    [62] = (uint32_t)(uintptr_t)newZealandTable,
    [64] = (uint32_t)(uintptr_t)paraguayTable,
    [65] = (uint32_t)(uintptr_t)peruTable,
    [66] = (uint32_t)(uintptr_t)surinamTable,
    [67] = (uint32_t)(uintptr_t)taiwanTable,
    [69] = (uint32_t)(uintptr_t)southAfricaTable,
    [71] = (uint32_t)(uintptr_t)uruguayTable,
    [72] = (uint32_t)(uintptr_t)&customsTable,
    [73] = (uint32_t)(uintptr_t)usaTable,
    [75] = (uint32_t)(uintptr_t)indiaTable,
    [76] = (uint32_t)(uintptr_t)belarusTable,
    [77] = (uint32_t)(uintptr_t)venezuelaTable,
    [78] = (uint32_t)(uintptr_t)slovakiaTable,
    [79] = (uint32_t)(uintptr_t)ghanaTable,
    [80] = (uint32_t)(uintptr_t)&europeTable,
    [81] = (uint32_t)(uintptr_t)&africaTable,
    [82] = (uint32_t)(uintptr_t)&southAmericaTable,
    [83] = (uint32_t)(uintptr_t)&northAmericaTable,
    [84] = (uint32_t)(uintptr_t)&asiaTable,
    [85] = (uint32_t)(uintptr_t)&oceaniaTable,
    [100] = (uint32_t)(uintptr_t)&customsTable,
    [252] = (uint32_t)(uintptr_t)&worldCupQualifyingGroups,
    [253] = (uint32_t)(uintptr_t)&ecQualifyingGroups,
    [254] = (uint32_t)(uintptr_t)&worldTable,
};

/* ======================================================================
 * DIY Cup Choice Variables (zero-init)
 * ====================================================================== */

uint16_t diyCupLegsChoice0 = 0;
uint16_t diyCupExtraTimeChoice0 = 0;
uint16_t diyCupPenaltiesChoice0 = 0;
uint16_t diyCupLegsChoice1 = 0;
uint16_t diyCupExtraTimeChoice1 = 0;
uint16_t diyCupPenaltiesChoice1 = 0;
uint16_t diyCupLegsChoice2 = 0;
uint16_t diyCupExtraTimeChoice2 = 0;
uint16_t diyCupPenaltiesChoice2 = 0;
uint16_t diyCupLegsChoice3 = 0;
uint16_t diyCupExtraTimeChoice3 = 0;
uint16_t diyCupPenaltiesChoice3 = 0;
uint16_t diyCupLegsChoice4 = 0;
uint16_t diyCupExtraTimeChoice4 = 0;
uint16_t diyCupPenaltiesChoice4 = 0;
uint16_t diyCupLegsChoice5 = 0;
uint16_t diyCupExtraTimeChoice5 = 0;
uint16_t diyCupPenaltiesChoice5 = 0;

uint16_t diyEditParamB = 0;
uint16_t diyEditParamC = 0;

#endif /* COMPETITION_DATA_C */
