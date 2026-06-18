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
#include "swos_types.h"
#include "swos_platform.h"  /* _ASM_PFX */
#include "competition_data.h"

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
extern Playoffs div1Playoffs, div2Playoffs, div3Playoffs;
/* englishLeagues1/2/3 forward decls removed — access via englishLeaguesBlock._divN */
extern Playoffs dutchPlayoffs;
extern Leagues dutchLeagues1;
extern Leagues scottishLeague1;
extern Playoffs scottishPlayoffs;
extern Leagues spanishLeague1;
extern Playoffs spanishPlayoffs;
extern Leagues uruguayanLeague1;
extern Playoffs uruguayanPlayoffs;

/* ======================================================================
 * Major International Cups
 * ====================================================================== */

CompetitionCup2 euroCup = {
    { { 0, 2, -1, 64, 32, CUP2_NAME_TABLE_OFFSET, 0, CUP2_TEAMLIST_TABLE_OFFSET, 1 }, 1, 1, 2, 3, 53, 4,
      { { 16, 4, 4 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 0, 0 }, { 0, 0, 0 } }, 0 },
    { 0, -108, -108, 20 },
    { aEuropeanChampi_1, aEuropeanCup },
    { {13,1}, {25,0}, {37,5}, {33,33}, {35,6}, {30,18}, {14,4}, {28,23},
      {1,5}, {38,9}, {20,22}, {8,20}, {36,8}, {27,11}, {20,0}, {29,12} }
};

CompetitionCup cupWinnersCup = {
    { { 1, 1, -1, 64, 32, 15, 0, 21, 1 }, 0, 32, 1, 53, 1 },
    { -108, -108, -108, -108, 20, 0 },
    { aEuropeanCupWin, aEuroCwCup },
    { {15,0}, {78,2}, {7,2}, {34,11}, {35,8}, {5,0}, {29,2}, {40,16},
      {17,8}, {36,0}, {2,2}, {27,1}, {19,15}, {38,10}, {11,4}, {25,21},
      {30,8}, {20,18}, {14,22}, {41,7}, {31,10}, {4,11}, {12,8}, {8,18},
      {3,6}, {16,8}, {37,12}, {39,10}, {1,7}, {6,10}, {21,6}, {13,30} }
};

CompetitionCup uefaCup = {
    { { 2, 1, -1, 64, 32, 15, 0, 21, 1 }, 0, 32, 1, 53, 1 },
    { -108, -108, -108, -108, 20, 0 },
    { aEufaCup, aEufaCup_0 },
    { {31,19}, {29,3}, {28,7}, {38,5}, {6,8}, {35,44}, {29,24}, {2,0},
      {14,23}, {20,40}, {36,7}, {37,10}, {35,19}, {25,10}, {2,4}, {30,10},
      {20,21}, {1,2}, {33,0}, {7,3}, {16,5}, {8,22}, {14,30}, {38,21},
      {20,23}, {35,42}, {14,17}, {31,16}, {13,22}, {29,20}, {13,23}, {14,25} }
};

/* ======================================================================
 * World Cup
 * ====================================================================== */

CompetitionCup2 worldCup2 = {
    { { 3, 2, -1, -120, -112, 35, 0, 41, 0 }, 1, 0, 1, 3, 53, 6,
      { { 32, 8, 4 }, { 16, 0, 16 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, -1, 2 }, { 2, 0, 2 } }, 1 },
    { 0, 20, 20, 20, 20, 20 },
    { aWorldCup, aWorldCup_0 }
};

CompetitionCup2 worldCup = {
    { { 4, 2, -1, -120, -112, 35, 0, 41, 1 }, 0, 0, 1, 3, 53, 6,
      { { 24, 6, 4 }, { 16, 0, 16 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, -1, 2 }, { 2, 0, 2 } }, 1 },
    { 0, 20, 20, 20, 20, 20 },
    { aWorldCup, aWorldCup_0 },
    { {80,36}, {82,4}, {83,13}, {80,44}, {82,2}, {80,37}, {81,6}, {80,43},
      {80,17}, {82,1}, {80,42}, {84,24}, {82,0}, {80,18}, {81,22}, {80,6},
      {80,23}, {80,35}, {80,32}, {83,8}, {80,5}, {81,19}, {80,19}, {84,22} }
};

/* ======================================================================
 * World Cup Qualifying Groups
 * ====================================================================== */

CompetitionCup worldCupQualifyingGroup1 = {
    { { 5, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_2, aWCQualifier_0 }
};

CompetitionCup worldCupQualifyingGroup2 = {
    { { 6, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_3, aWCQualifier_0 }
};

CompetitionCup worldCupQualifyingGroup3 = {
    { { 7, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_4, aWCQualifier_0 }
};

CompetitionCup worldCupQualifyingGroup4 = {
    { { 8, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_5, aWCQualifier_0 }
};

CompetitionCup worldCupQualifyingGroup5 = {
    { { 9, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_6, aWCQualifier_0 }
};

CompetitionCup worldCupQualifyingGroup6 = {
    { { 10, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_7, aWCQualifier_0 }
};

CompetitionCup worldCupQualifyingGroup7 = {
    { { 11, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_8, aWCQualifier_0 }
};

CompetitionCup worldCupQualifyingGroup8 = {
    { { 12, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aWorldCupQualif_9, aWCQualifier_0 }
};

/* ======================================================================
 * World Cup Regional Qualifications
 * ====================================================================== */

CompetitionCup2 northAmericaWorldCupQualification = {
    { { 13, 2, 83, 72, 16, 35, 0, 41, 0 }, 1, 0, 2, 3, 53, 2,
      { { 12, 0, 12 }, { 6, 1, 6 }, { 3, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
    { -108, 20, 20, 20, 20, 20 },
    { aWorldCupQualif_1, aWCQualifier_0 }
};

CompetitionCup2 southAmericaWorldCupQualification = {
    { { 14, 2, 82, 72, 16, 35, 0, 41, 0 }, 1, 0, 2, 3, 53, 1,
      { { 8, 1, 8 }, { 4, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
    { 20, 20, 20, 20, 20, 20 },
    { aWorldCupQualif_1, aWCQualifier_0 }
};

CompetitionCup2 africaWorldCupQualification = {
    { { 15, 2, 81, 72, 16, 35, 0, 41, 0 }, 1, 0, 2, 3, 53, 3,
      { { 32, 0, 32 }, { 16, 0, 16 }, { 8, 1, 8 }, { 5, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
    { -108, -108, 20, 20, 20, 20 },
    { aWorldCupQualif_1, aWCQualifier_0 }
};

CompetitionCup2 asiaWorldCupQualification = {
    { { 16, 2, 84, 72, 16, 35, 0, 41, 0 }, 1, 0, 2, 3, 53, 3,
      { { 24, 0, 24 }, { 12, 0, 12 }, { 6, 1, 6 }, { 2, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
    { -108, -108, 20, 20, 20, 20 },
    { aWorldCupQualif_1, aWCQualifier_0 }
};

CompetitionCup oceaniaWorldCupQualification = {
    { { 17, 0, 85, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 4 },
    { 1, 0, 0, 0, 0, 0 }, { aWorldCupQualif_1, aWCQualifier_0 }
};

/* ======================================================================
 * Invitation / Tour / Friendly
 * ====================================================================== */

CompetitionCup invitationTournament = {
    { { 18, 0, -1, -104, -104, 15, 0, 21, 0 }, 1, 1, 3, 53, 4 },
    { 0 }, { aInvitationTour, aInvTournament }
};

CompetitionCup tour = {
    { { 19, 0, -1, -104, -104, 15, 0, 21, 0 }, 1, -1, 3, 53, 4 },
    { 0 }, { 0, aTour }
};

CompetitionCup friendly = {
    { { 20, 0, -1, 56, 0, 15, 0, 21, 0 }, 1, -1, 3, 53, 6 },
    { 0 }, { aFriendly, aFriendly_0 }
};

/* ======================================================================
 * European Championships
 * ====================================================================== */

CompetitionCup2 europeanChampionships2 = {
    { { 21, 2, 80, -120, -120, CUP2_NAME_TABLE_OFFSET, 0, CUP2_TEAMLIST_TABLE_OFFSET, 0 }, 1, 0, 1, 3, 53, 4,
      { { 16, 4, 4 }, { 8, 0, 0 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
    { 0, 20, 20, 20 },
    { aEuropeanChampi_0, aECFinals }
};

CompetitionCup2 europeanChampionships = {
    { { 22, 2, 80, -120, -120, CUP2_NAME_TABLE_OFFSET, 0, CUP2_TEAMLIST_TABLE_OFFSET, 1 }, 1, 0, 1, 3, 53, 4,
      { { 16, 4, 4 }, { 8, 0, 0 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
    { 0, 20, 20, 20 },
    { aEuropeanChampi_0, aECFinals },
    { {80,11}, {80,44}, {80,19}, {80,39}, {80,42}, {80,6}, {80,36}, {80,15},
      {80,17}, {80,9}, {80,23}, {80,37}, {80,10}, {80,34}, {80,45}, {80,7} }
};

/* ======================================================================
 * EC Qualifying Groups
 * ====================================================================== */

CompetitionCup ecQualifyingGroup1 = {
    { { 23, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr, aECQualifier_0 }
};

CompetitionCup ecQualifyingGroup2 = {
    { { 24, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr_0, aECQualifier_0 }
};

CompetitionCup ecQualifyingGroup3 = {
    { { 25, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr_1, aECQualifier_0 }
};

CompetitionCup ecQualifyingGroup4 = {
    { { 26, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr_2, aECQualifier_0 }
};

CompetitionCup ecQualifyingGroup5 = {
    { { 27, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr_3, aECQualifier_0 }
};

CompetitionCup ecQualifyingGroup6 = {
    { { 28, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr_4, aECQualifier_0 }
};

CompetitionCup ecQualifyingGroup7 = {
    { { 29, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 6 },
    { 2, 0, 0, 0, 0, 0 }, { aECQualifyingGr_5, aECQualifier_0 }
};

CompetitionCup ecQualifyingGroup8 = {
    { { 30, 0, 80, 72, 16, 15, 0, 21, 0 }, 1, 2, 3, 53, 4 },
    { 1, 0, 0, 0, 0, 0 }, { aECQualifyingGr_6, aECQualifier_0 }
};

/* ======================================================================
 * Continental Cups
 * ====================================================================== */

CompetitionCup2 copaAmerica = {
    { { 31, 2, 82, -120, -112, CUP2_NAME_TABLE_OFFSET, 0, CUP2_TEAMLIST_TABLE_OFFSET, 0 }, 1, 0, 1, 3, 53, 4,
      { { 12, 3, 4 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
    { 0, 4, 4, 4 },
    { aCopaAmerica, aCopaAmerica_0 },
    { {83,8}, {83,13}, {82,8}, {82,9}, {82,0}, {82,1}, {82,2}, {82,3},
      {82,4}, {82,5}, {82,6}, {82,7} }
};

CompetitionCup2 africanNationsCup = {
    { { 32, 2, 81, -120, -112, 35, 0, 0, 0 }, 1, 0, 1, 3, 53, 5,
      { { 32, 8, 4 }, { 16, 0, 16 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 1, 1 } }, 1 },
    { 0, 20, 20, 20, 20, 0 },
    { aAfricanNations, aAfricanCup }
};

CompetitionCup2 northAmericanCup = {
    { { 33, 2, 83, -120, -112, CUP2_NAME_TABLE_OFFSET, 0, 0, 0 }, 1, 0, 1, 3, 53, 4,
      { { 12, 3, 4 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 1, 1 }, { 1, 1, 1 } }, 1 },
    { 0, 20, 20, 20 },
    { aNorthAmericanC, aNAmericanCup }
};

CompetitionCup2 asianCup = {
    { { 34, 2, 84, -120, -112, 35, 0, 0, 0 }, 1, 0, 1, 3, 53, 5,
      { { 30, 5, 6 }, { 16, 0, 16 }, { 8, 0, 8 }, { 4, 0, 4 }, { 2, 0, 2 }, { 1, 1, 1 } }, 1 },
    { 0, 20, 20, 20, 20, 0 },
    { aAsianCup, aAsianCup_0 }
};

CompetitionCup oceaniaCup = {
    { { 35, 0, 85, -120, -112, 15, 0, 0, 0 }, 1, 1, 3, 53, 4 },
    { 0 }, { aOceaniaCup, aOceaniaCup_0 }
};

/* ======================================================================
 * Customs League
 * ====================================================================== */

CompetitionLeague customsLeague = {
    { { 36, 0, 72, 64, 32, LEAGUE_NAME_TABLE_OFFSET, 0, 0, 0 }, 3, 2, 3, 53 },
    { { 20, 0, 0, 0, 0, 0 }, { 22, 0, 0, 0, 0, 0 }, { 22, 0, 0, 0, 0, 0 } },
    0, { aDivision1, aDivision1, aDivision2, aDivision2, aDivision3, aDivision3 }
};

/* ======================================================================
 * National Leagues (1-division)
 * ====================================================================== */

CompetitionLeague albanianLeague    = { { { 37, 0, 0, 64, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague austrianLeague    = { { { 38, 0, 1, 56, 40, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague belarussiaLleague = { { { 39, 0, 76, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 69 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague belgianLeague     = { { { 40, 0, 2, 56, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague bulgarianLeague   = { { { 41, 0, 3, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague croatianLeague    = { { { 42, 0, 4, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague cypriotLeague     = { { { 43, 0, 5, 64, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 14, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague czechLeague       = { { { 44, 0, 6, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague danishLeague      = { { { 45, 0, 7, 56, 40, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague estonianLeague    = { { { 50, 0, 10, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague faroeIslesLeague  = { { { 51, 0, 11, 24, 64, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague finnishLeague     = { { { 52, 0, 12, 24, 72, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague greekLeague       = { { { 55, 0, 15, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague hungarianLeague   = { { { 58, 0, 16, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague icelandicLeague   = { { { 59, 0, 17, 32, 64, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague israeliLeague     = { { { 60, 0, 19, 64, 32, 0, 0, 0, 0 }, 1, 3, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague latvianLeague     = { { { 62, 0, 21, 32, 64, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague lithuanianLeague  = { { { 63, 0, 22, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague luxembourgLeague  = { { { 64, 0, 23, 56, 40, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague malteseLeague     = { { { 65, 0, 24, 64, 24, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague northernIrishLeague = { { { 66, 0, 26, 64, 24, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague norwegianLeague   = { { { 67, 0, 27, 24, 72, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 14, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague polishLeague      = { { { 68, 0, 28, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague portugeseLeague   = { { { 69, 0, 29, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague irishLeague       = { { { 70, 0, 18, 56, 24, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague romanianLeague    = { { { 71, 0, 30, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague russianLeague     = { { { 72, 0, 31, 16, 80, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague sanMarinoLeague   = { { { 73, 0, 32, 72, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague slovakianLeague   = { { { 76, 0, 78, 56, 40, 0, 0, 0, 0 }, 1, 2, 2, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague slovenianLeague   = { { { 77, 0, 34, 56, 40, 0, 0, 0, 0 }, 1, 2, 2, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague swedishLeague     = { { { 80, 0, 36, 24, 72, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 14, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague swissLeague       = { { { 81, 0, 37, 48, 40, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague turkishLeague     = { { { 82, 0, 38, 56, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague ukrainianLeague   = { { { 83, 0, 39, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 18, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague welshLeague       = { { { 84, 0, 40, 56, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 20, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague yugoslavianLeague = { { { 85, 0, 41, 64, 40, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };

/* Non-European 1-division leagues */
CompetitionLeague algerianLeague    = { { { 86, 0, 42, 64, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague bolivianLeague    = { { { 89, 0, 45, 32, 88, 0, 0, 0, 0 }, 1, 2, 2, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague brasilianLeague   = { { { 90, 0, 46, 0, 40, 0, 0, 0, 0 }, 1, 2, 2, 53 }, { { 24, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague chileanLeague     = { { { 91, 0, 48, 24, 88, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague colombianLeague   = { { { 92, 0, 49, 24, 88, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague ecuadorLeague     = { { { 93, 0, 50, 24, 88, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague indianLeague      = { { { 95, 0, 75, 0, 24, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 6, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague paraguayanLeague  = { { { 99, 0, 64, 64, 32, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague peruvianLeague    = { { { 100, 0, 65, 24, 88, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague southAfricanLeague = { { { 101, 0, 69, 0, 80, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 16, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague surinamLeague     = { { { 102, 0, 66, 24, 72, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague taiwaneseLeague   = { { { 103, 0, 67, 64, 32, 0, 0, 0, 0 }, 1, 4, 3, 53 }, { { 12, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague americanLeague    = { { { 106, 0, 73, 24, 72, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 10, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague venezuelanLeague  = { { { 107, 0, 77, 24, 88, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 22, 0, 0, 0, 0, 0 } }, 0 };
CompetitionLeague ghanaLeague       = { { { 108, 0, 79, 56, 40, 0, 0, 0, 0 }, 1, 2, 3, 53 }, { { 8, 0, 0, 0, 0, 0 } }, 0 };

/* ======================================================================
 * Multi-division leagues
 * ====================================================================== */

CompetitionLeague frenchLeague = {
    { { 53, 0, 13, 56, 40, LEAGUE_NAME_TABLE_OFFSET, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 20, 0, 0, 3, 0, 0 }, { 22, 3, 0, 0, 0, 0 } }, 0,
    { aPremiereDivisi, aPremiere, aDeuxiemeDivisi, aDeuxieme }
};

CompetitionLeague germanLeague = {
    { { 54, 0, 14, 56, 40, LEAGUE_NAME_TABLE_OFFSET, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 18, 0, 0, 3, 0, 0 }, { 18, 3, 0, 0, 0, 0 } }, 0,
    { a1Liga, a1Liga, a2Liga, a2Liga }
};

CompetitionLeague italianLeague = {
    { { 61, 0, 20, 64, 32, LEAGUE_NAME_TABLE_OFFSET, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 18, 0, 0, 4, 0, 0 }, { 20, 4, 0, 0, 0, 0 } }, 0,
    { aSerieA, aSerieA, aSerieB, aSerieB }
};

CompetitionLeague japaneseLeague = {
    { { 96, 0, 55, 16, 80, LEAGUE_NAME_TABLE_OFFSET, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 14, 0, 0, 0, 0, 0 }, { 14, 0, 0, 0, 0, 0 } }, 0,
    { aJLeague, aJLeague_0, aLeague_2, aLeague_3 }
};

CompetitionLeague argentinianLeague = {
    { { 87, 0, 43, 64, 32, 0, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 20, 0, 0, 2, 0, 0 }, { 20, 2, 0, 0, 0, 0 } }, 0
};

CompetitionLeague mexicanLeague = {
    { { 97, 0, 60, 56, 32, 0, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 20, 0, 0, 2, 0, 0 }, { 20, 2, 0, 0, 0, 0 } }, 0
};

CompetitionLeague elSalvadorLeague = {
    { { 94, 0, 51, 56, 32, 0, 0, 0, 0 }, 4, 2, 3, 53 },
    { { 10, 0, 0, 1, 0, 0 }, { 12, 1, 0, 1, 0, 0 }, { 12, 1, 0, 1, 0, 0 }, { 12, 1, 0, 0, 0, 0 } }, 0
};

CompetitionLeague australianLeague = {
    { { 88, 0, 44, 16, 80, LEAGUE_NAME_TABLE_OFFSET, 0, 0, 0 }, 4, 2, 3, 53 },
    { { 12, 0, 0, 0, 0, 0 }, { 12, 0, 0, 0, 0, 0 }, { 14, 0, 0, 0, 0, 0 }, { 12, 0, 0, 0, 0, 0 } }, 0,
    { a1stDivision, a1stDivision, aSouthDivision, aSouthDivision_0, aNswDivision, aNsw, aQueenslandDivi, aQueensland }
};

CompetitionLeague newZealandLeague = {
    { { 98, 0, 62, 16, 72, LEAGUE_NAME_TABLE_OFFSET, 0, 0, 0 }, 3, 2, 3, 53 },
    { { 10, 0, 0, 0, 0, 0 }, { 10, 0, 0, 0, 0, 0 }, { 10, 0, 0, 0, 0, 0 } }, 0,
    { aNorthernRegion, aNorthern, aCentralRegion, aCentral, aSouthernRegion, aSouthern }
};

/* ======================================================================
 * Complex leagues with playoffs
 * ====================================================================== */

/* On Amiga, the four English-league structs sit consecutively in BSS
 * (_main 102 + _div1 80 + _div2 80 + _div3 80 bytes after the Leagues merge —
 * _div3 was 48 as a bare Leagues_4, now carries the zeroed playoffs/nextGroup/
 * qualifierIndexPairs tail like the others; it is the block's last member so the
 * growth only extends the block).
 * processDiyLeagueQualificationEntry walks past the end of _main using
 * the per-division qualification offset in `divisions[i].unk1`,
 * expecting to land inside _div1 or _div2 (whose `playoffs` field at
 * +48 supplies the qualification competition pointer). Four separate
 * linker-scattered globals broke that walk → qualComp garbage → SIGSEGV.
 *
 * UNTANGLED (.equ -> typed field access): all four are fields of one
 * packed EnglishLeaguesBlock (typedef + extern in competition_data.h).
 * The old `.equ englishLeaguesN, englishLeaguesBlock + <literal>` aliases
 * used 32-bit byte offsets (102/182/262) — but _main/_div1/_div2 carry
 * pointers that widen on 64-bit, so on the SDL build those literal
 * offsets no longer landed on the right sub-struct (e.g. block+102 fell
 * inside _main's widened tail, not _div1). Cross-references now go
 * through the named fields (._main/._div1/._div2/._div3): address-
 * identical on 32-bit (Amiga byte-for-byte) and correct on 64-bit. */
EnglishLeaguesBlock englishLeaguesBlock = {
    ._main = {
        { { 46, 0, 8, 56, 32, LEAGUE_NAME_TABLE_OFFSET, 0, 0, 0 }, 4, 2, 3, 51 },
        { { 20, 0, 0, 3, 0, 0 }, { 24, 2, 4, 4, 0, 46 }, { 24, 3, 4, 4, 0, 120 }, { 24, 3, 4, 0, 0, -62 } }, 0,
        { aPremierLeague, aPremier, aDivisionOne, aDivisionOne, aDivisionTwo, aDivisionTwo, aDivisionThree, aDivisionThree },
        { (char *)&div1Playoffs, (char *)&englishLeaguesBlock._div1 },
        { 0, 0, 0, 3, 0, 1, 0, 2, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    ._div1 = {
        (CompetitionLeague *)&englishLeaguesBlock._main, { 0, 0, 0, 1, 0, 0, 0, 0 },
        (CompetitionLeague *)&englishLeaguesBlock._main, { 0, 0, 0, 1, 0, 0, 0, 3 },
        (CompetitionLeague *)&englishLeaguesBlock._main, { 0, 0, 0, 1, 0, 0, 0, 1 },
        (CompetitionLeague *)&englishLeaguesBlock._main, { 0, 0, 0, 1, 0, 0, 0, 2 },
        &div2Playoffs, (Leagues *)&englishLeaguesBlock._div2,
        { 0, 4, 0, 7, 0, 5, 0, 6, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    ._div2 = {
        (CompetitionLeague *)&englishLeaguesBlock._main, { 0, 0, 0, 2, 0, 0, 0, 0 },
        (CompetitionLeague *)&englishLeaguesBlock._main, { 0, 0, 0, 2, 0, 0, 0, 3 },
        (CompetitionLeague *)&englishLeaguesBlock._main, { 0, 0, 0, 2, 0, 0, 0, 1 },
        (CompetitionLeague *)&englishLeaguesBlock._main, { 0, 0, 0, 2, 0, 0, 0, 2 },
        &div3Playoffs, &englishLeaguesBlock._div3,
        { 0, 8, 0, 11, 0, 9, 0, 10, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    ._div3 = {
        (CompetitionLeague *)&englishLeaguesBlock._main, { 0, 0, 0, 3, 0, 0, 0, 0 },
        (CompetitionLeague *)&englishLeaguesBlock._main, { 0, 0, 0, 3, 0, 0, 0, 3 },
        (CompetitionLeague *)&englishLeaguesBlock._main, { 0, 0, 0, 3, 0, 0, 0, 1 },
        (CompetitionLeague *)&englishLeaguesBlock._main, { 0, 0, 0, 3, 0, 0, 0, 2 }
    }
};

/* (the englishLeagues / englishLeagues1..3 .equ aliases are gone — all
 * access is via englishLeaguesBlock._main/._div1/._div2/._div3 now) */

Playoffs div1Playoffs = { { 47, 1, 8, -128, -124, 11, 0, 17, 1 }, 0, 4, 1, 51, 1, { -108, 20 }, { aDiv1Playoffs, aPlayoffs }, { 0 } };
Playoffs div2Playoffs = { { 48, 1, 8, -128, -124, 11, 0, 17, 1 }, 0, 4, 1, 51, 1, { -108, 20 }, { aDiv2Playoffs, aPlayoffs }, { 0 } };
Playoffs div3Playoffs = { { 49, 1, 8, -128, -124, 11, 0, 17, 1 }, 0, 4, 1, 51, 1, { -108, 20 }, { aDiv3Playoffs, aPlayoffs }, { 0 } };

/* Dutch league */
CompetitionLeague dutchLeague = {
    { { 56, 0, 25, 56, 32, 0, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 18, 0, 0, 1, 2, 8 }, { 18, 1, 2, 0, 0, 2 } }, 0,
    { 0 },   /* name[10]: dutch uses default division names (nameTableOffset=0) */
    { (char *)&dutchPlayoffs, (char *)&dutchLeagues1 },   /* playoffLinkPtrs */
    { 0, 0, 0, 3, 0, 1, 0, 2, 0, -1, 0, -1, 0, -1, 0, -1, 1, 0, 1, 0, 1, 0, 1, 0 }   /* promotionLinkData */
};

Leagues dutchLeagues1 = {
    (CompetitionLeague *)&dutchLeague, { 0 },
    (CompetitionLeague *)&dutchLeague, { 0, 0, 0, 1, 0, 0, 0, 1 },
    (CompetitionLeague *)&dutchLeague, { 0, 0, 0, 0, 0, 0, 0, 1 },
    (CompetitionLeague *)&dutchLeague, { 0, 0, 0, 1, 0, 0, 0, 0 }
};

Playoffs dutchPlayoffs = { { 57, 1, 25, -128, -128, 11, 0, 17, 1 }, 1, 4, 1, 53, 1, { -108, -108 }, { aPlayoffs, aPlayoffs }, { 0 } };

/* Scottish league */
CompetitionLeague scottishLeague = {
    { { 74, 0, 33, 56, 32, 0, 0, 0, 0 }, 4, 4, 3, 51 },
    { { 10, 0, 0, 1, 1, 20 }, { 10, 1, 1, 2, 0, 14 }, { 10, 2, 0, 2, 0, 0 }, { 10, 2, 0, 0, 0, 0 } }, 0,
    { 0 },   /* name[10]: scottish uses default division names (nameTableOffset=0) */
    { (char *)&scottishPlayoffs, (char *)&scottishLeague1 },   /* playoffLinkPtrs */
    { 0, 0, 0, 1, 0, -1, 1, 0 }   /* promotionLinkData (first 8 of 24) */
};

Leagues scottishLeague1 = {
    (CompetitionLeague *)&scottishLeague, { 0 },
    (CompetitionLeague *)&scottishLeague, { 0, 0, 0, 1, 0, 0, 0, 0 }
};

Playoffs scottishPlayoffs = { { 75, 1, 33, -128, -128, 11, 0, 17, 1 }, 0, 2, 1, 51, 1, { 20, 0 }, { aPlayoffs, aPlayoffs }, { 0 } };

/* Spanish league */
CompetitionLeague spanishLeague = {
    { { 78, 0, 35, 64, 40, LEAGUE_NAME_TABLE_OFFSET, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 22, 0, 0, 2, 2, 24 }, { 20, 2, 2, 0, 0, 18 } }, 0,
    { aPrimeraDivisio, aPrimera, aSegundaDivisio, aSegunda },
    { (char *)&spanishPlayoffs, (char *)&spanishLeague1 },
    { 0, 0, 0, 3, 0, 1, 0, 2, 0, -1, 0, -1, 0, -1, 0, -1, 1, 0, 1, 0, 1, 0, 1, 0 }
};

Leagues spanishLeague1 = {
    (CompetitionLeague *)&spanishLeague, { 0 },
    (CompetitionLeague *)&spanishLeague, { 0, 0, 0, 1, 0, 0, 0, 1 },
    (CompetitionLeague *)&spanishLeague, { 0, 0, 0, 0, 0, 0, 0, 1 },
    (CompetitionLeague *)&spanishLeague, { 0, 0, 0, 1, 0, 0, 0, 0 }
};

Playoffs spanishPlayoffs = { { 79, 1, 35, -120, -120, 11, 0, 17, 1 }, 1, 4, 1, 53, 1, { -108, -108 }, { aPlayoffs, aPlayoffs }, { 0 } };

/* Uruguayan league */
CompetitionLeague uruguayanLeague = {
    { { 104, 0, 71, 24, 88, 0, 0, 0, 0 }, 2, 2, 3, 53 },
    { { 12, 0, 0, 1, 1, 8 }, { 14, 1, 1, 0, 0, 2 } }, 0,
    { 0 },   /* name[10]: uruguayan uses default division names (nameTableOffset=0) */
    { (char *)&uruguayanPlayoffs, (char *)&uruguayanLeague1 },   /* playoffLinkPtrs */
    { 0, 0, 0, 1, 0, -1, 1, 0 }   /* promotionLinkData (first 8 of 24) */
};

Leagues uruguayanLeague1 = {
    (CompetitionLeague *)&uruguayanLeague, { 0 },
    (CompetitionLeague *)&uruguayanLeague, { 0, 0, 0, 1, 0, 0, 0, 0 }
};

Playoffs uruguayanPlayoffs = { { 105, 1, 71, 88, 88, 11, 0, 17, 1 }, 0, 2, 1, 53, 1, { -108, 0 }, { aPlayoffs, aPlayoffs }, { 0 } };

/* ======================================================================
 * National Cups
 * ====================================================================== */

CompetitionCup albanianCup     = { { { 109, 1, 0, 88, 32, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { 84, 84, -108, 20 } };
CompetitionCup austrianCup     = { { { 110, 1, 1, -128, -120, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup belarusianCup   = { { { 111, 1, 76, 72, 40, 0, 0, 0, 0 }, 0, 16, 0, 53, 1 }, { 84, 84, 84, 20 } };
CompetitionCup belgianCup      = { { { 112, 1, 2, 80, 32, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { 84, 84, -108, 20 } };
CompetitionCup bulgarianCup    = { { { 113, 1, 3, 80, 32, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { -108, -108, -108, 20 } };
CompetitionCup croatianCup     = { { { 114, 1, 4, 80, 40, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { -108, -108, -108, -108 } };
CompetitionCup cypriotCup      = { { { 115, 1, 5, 104, -128, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { -108, -108, 20, 0 } };
CompetitionCup czechCup        = { { { 116, 1, 6, 72, 40, 0, 0, 0, 0 }, 0, 16, 0, 53, 1 }, { 84, 84, 84, 20 } };
CompetitionCup danishCup       = { { { 117, 1, 7, 112, -120, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { 84, -108, 20, 0 } };
CompetitionCup estonianCup     = { { { 122, 1, 10, -128, -120, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { 84, -108, 20, 0 } };
CompetitionCup faroeIslesCup   = { { { 123, 1, 11, 56, 64, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { 84, -108, 20, 0 } };
CompetitionCup finnishCup      = { { { 124, 1, 12, 32, 40, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup greekCup        = { { { 127, 1, 15, 64, 40, 0, 0, 0, 0 }, 0, 16, 1, 53, 2 }, { -108, -108, -108, 20 } };
CompetitionCup hungarianCup    = { { { -128, 1, 16, 104, 40, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { -108, -108, -108, -108 } };
CompetitionCup icelandicCup    = { { { -127, 1, 17, 56, 64, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup irishCup        = { { { -126, 1, 18, 112, -128, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 104, 104, 40, 0 } };
CompetitionCup israeliCup      = { { { -125, 1, 19, 120, -128, 0, 0, 0, 0 }, 0, 16, 0, 53, 1 }, { 84, 84, 84, 20 } };
CompetitionCup latvianCup      = { { { -123, 1, 21, 56, 64, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { -108, -108, 20, 0 } };
CompetitionCup lithuanianCup   = { { { -122, 1, 22, -128, -120, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { -108, -108, 20, 0 } };
CompetitionCup luxembourgCup   = { { { -121, 1, 23, 120, -120, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup malteseCup      = { { { -120, 1, 24, -128, -120, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup northernIrishCup = { { { -118, 1, 26, 104, -128, 0, 0, 0, 0 }, 0, 16, 0, 53, 6 }, { 104, 104, 104, 40 } };
CompetitionCup norwegianCup    = { { { -117, 1, 27, 64, 80, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup polishCup       = { { { -116, 1, 28, 80, 40, 0, 0, 0, 0 }, 0, 16, 1, 53, 5 }, { 84, -108, -108, 20 } };
CompetitionCup portugeseCup    = { { { -115, 1, 29, 96, -120, 0, 0, 0, 0 }, 0, 16, 0, 53, 6 }, { 84, 84, 84, 20 } };
CompetitionCup romanianCup     = { { { -114, 1, 30, 112, -120, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { 84, -108, -108, 20 } };
CompetitionCup russianCup      = { { { -113, 1, 31, 16, 40, 0, 0, 0, 0 }, 0, 16, 0, 53, 3 }, { 84, 84, 84, 20 } };
CompetitionCup sanMarinoCup    = { { { -112, 1, 32, 120, -128, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup slovakianCup    = { { { -109, 1, 78, -128, -120, 0, 0, 0, 0 }, 0, 8, 1, 53, 1 }, { 84, -108, 20, 0 } };
CompetitionCup slovenianCup    = { { { -108, 1, 34, 120, -120, 0, 0, 0, 0 }, 0, 16, 0, 53, 2 }, { 84, 84, 84, 20 } };
CompetitionCup swedishCup      = { { { -106, 1, 36, 32, 40, 0, 0, 0, 0 }, 0, 16, 0, 53, 5 }, { 84, 84, 84, 20 } };
CompetitionCup swissCup        = { { { -105, 1, 37, 120, -120, 0, 0, 0, 0 }, 0, 8, 0, 53, 1 }, { 84, 84, 20, 0 } };
CompetitionCup turkishCup      = { { { -104, 1, 38, 88, 24, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { 84, 84, -108, -108 } };
CompetitionCup ukrainianCup    = { { { -103, 1, 39, 120, -120, 0, 0, 0, 0 }, 0, 16, 1, 53, 1 }, { -108, -108, -108, 20 } };
CompetitionCup welshCup        = { { { -102, 1, 40, 88, 32, 0, 0, 0, 0 }, 0, 16, 1, 53, 4 }, { 104, 104, -108, 40 } };
CompetitionCup yugoslavianCup  = { { { -101, 1, 41, 80, 32, 0, 0, 0, 0 }, 0, 8, 1, 53, 2 }, { -108, -108, -108, 0 } };
CompetitionCup algerianCup     = { { { -100, 1, 42, 96, -128, 0, 0, 0, 0 }, 0, 16, 1, 53, 2 }, { 84, 84, -108, 20 } };
CompetitionCup southAfricanCup = { { { -96, 1, 69, 56, 80, 0, 0, 0, 0 }, 0, 16, 1, 53, 2 }, { 84, 84, 84, 20 } };
CompetitionCup taiwaneseCup    = { { { -95, 1, 67, 96, 120, 0, 0, 0, 0 }, 0, 8, 1, 53, 2 }, { 84, 84, 20, 0 } };

/* Cups with names */
CompetitionCup englishCup       = { { { 118, 1, 8, 96, -128, 15, 0, 0, 0 }, 0, 64, 0, 51, 3 }, { 104, 104, 104, 104, 40, 40 }, { aCup_4, aCup_5 } };
CompetitionCup englishLeagueCup = { { { 119, 1, 8, 64, 16, 15, 0, 0, 0 }, 0, 64, 2, 51, 2 }, { -108, 104, 104, 104, -108, 40 }, { aLeagueCup, aLeagueCup_0 } };
CompetitionCup englishCup_1     = { { { 120, 1, 8, 96, -128, 15, 0, 0, 0 }, 0, 32, 0, 51, 4 }, { 104, 104, 104, 40, 40, 0 }, { aCup_4, aCup_5 } };
CompetitionCup englishLeagueCup_1 = { { { 121, 1, 8, 64, 16, 15, 0, 0, 0 }, 0, 32, 2, 51, 3 }, { 104, 104, 104, -108, 40, 0 }, { aLeagueCup, aLeagueCup_0 } };

CompetitionCup frenchCup     = { { { 125, 1, 13, 104, -120, 15, 0, 0, 0 }, 0, 32, 0, 53, 1 }, { 84, 84, 84, 84, 20, 0 }, { aCoupeDeFrance, aCoupe } };
CompetitionCup germanCup     = { { { 126, 1, 14, 64, 40, 15, 0, 0, 0 }, 0, 32, 0, 53, 3 }, { 84, 84, 84, 84, 20, 0 }, { aDFBPokal, aPokal } };
CompetitionCup italianCup    = { { { -124, 1, 20, 56, 32, 15, 0, 0, 0 }, 0, 32, 1, 53, 2 }, { -108, -108, -108, -108, 20, 0 }, { aCoppaItalia, aCoppa } };
CompetitionCup scottishCup   = { { { -111, 1, 33, 96, -128, 15, 0, 0, 0 }, 0, 32, 0, 51, 3 }, { 104, 104, 104, 104, 40, 0 }, { aCup_6, aCup_7 } };
CompetitionCup scottishLeagueCup = { { { -110, 1, 33, 56, 72, 15, 0, 0, 0 }, 0, 32, 0, 51, 2 }, { 104, 104, 104, 40, 40, 0 }, { aLeagueCup_1, aLeagueCup_2 } };
CompetitionCup spanishCup    = { { { -107, 1, 35, 88, 40, 15, 0, 0, 0 }, 0, 32, 1, 53, 5 }, { -108, -108, -108, -108, 20, 0 }, { aCopa, aCopa_0 } };

CompetitionCup dutchCup       = { { { -119, 1, 25, 88, 32, 0, 0, 0, 0 }, 0, 32, 0, 53, 3 }, { 84, 84, 84, 84, 20, 0 } };
CompetitionCup australianCup  = { { { -99, 1, 44, 40, 80, 0, 0, 0, 0 }, 0, 32, 1, 53, 2 }, { 84, 84, 84, 84, 20, 0 } };

CompetitionCup japaneseCup        = { { { -98, 1, 55, 48, 56, CUP_NAME_TABLE_OFFSET, 0, 0, 0 }, 0, 8, 1, 53, 2 }, { 84, 84, 20, 0 }, { aCup_8, aCup_9 } };
CompetitionCup japaneseEmperorsCup = { { { -97, 1, 55, 88, 0, CUP_NAME_TABLE_OFFSET, 0, 0, 0 }, 0, 16, 1, 53, 2 }, { 84, 84, 84, 20 }, { aEmperorSCup, aEmpCup } };

/* ======================================================================
 * Competition "Copy" variables (used at runtime for career/season)
 * ====================================================================== */

/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): euroCupCopy */
extern CompetitionCup2 euroCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): cupWinnersCupCopy */
extern CompetitionCup cupWinnersCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): uefaCupCopy */
extern CompetitionCup uefaCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): invitationTournamentCopy */
extern CompetitionCup invitationTournamentCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): tourCopy */
extern CompetitionCup tourCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): friendlyCopy */
extern CompetitionCup friendlyCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): europeanChampionships2Copy */
extern CompetitionCup2 europeanChampionships2Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup1Copy */
extern CompetitionCup ecQualifyingGroup1Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup2Copy */
extern CompetitionCup ecQualifyingGroup2Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup3Copy */
extern CompetitionCup ecQualifyingGroup3Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup4Copy */
extern CompetitionCup ecQualifyingGroup4Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup5Copy */
extern CompetitionCup ecQualifyingGroup5Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup6Copy */
extern CompetitionCup ecQualifyingGroup6Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup7Copy */
extern CompetitionCup ecQualifyingGroup7Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): ecQualifyingGroup8Copy */
extern CompetitionCup ecQualifyingGroup8Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): copaAmericaCopy */
extern CompetitionCup2 copaAmericaCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): africanNationsCupCopy */
extern CompetitionCup2 africanNationsCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): northAmericanCupCopy */
extern CompetitionCup2 northAmericanCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): asianCupCopy */
extern CompetitionCup2 asianCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): oceaniaCupCopy */
extern CompetitionCup oceaniaCupCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCup2Copy */
extern CompetitionCup2 worldCup2Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup1Copy */
extern CompetitionCup worldCupQualifyingGroup1Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup2Copy */
extern CompetitionCup worldCupQualifyingGroup2Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup3Copy */
extern CompetitionCup worldCupQualifyingGroup3Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup4Copy */
extern CompetitionCup worldCupQualifyingGroup4Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup5Copy */
extern CompetitionCup worldCupQualifyingGroup5Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup6Copy */
extern CompetitionCup worldCupQualifyingGroup6Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup7Copy */
extern CompetitionCup worldCupQualifyingGroup7Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): worldCupQualifyingGroup8Copy */
extern CompetitionCup worldCupQualifyingGroup8Copy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): northAmericaWorldCupQualificationCopy */
extern CompetitionCup2 northAmericaWorldCupQualificationCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): southAmericaWorldCupQualificationCopy */
extern CompetitionCup2 southAmericaWorldCupQualificationCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): africaWorldCupQualificationCopy */
extern CompetitionCup2 africaWorldCupQualificationCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): asiaWorldCupQualificationCopy */
extern CompetitionCup2 asiaWorldCupQualificationCopy;  /* aliased into careerFileBufferBuf */
/* MOVED to careerFileBufferBuf overlay (see swos_globals_career_overlay.c): oceaniaWorldCupQualificationCopy */
extern CompetitionCup oceaniaWorldCupQualificationCopy;  /* aliased into careerFileBufferBuf */

/* ======================================================================
 * Career Contests
 * ====================================================================== */

/* 34 real entries + a 0xFFFFFFFF sentinel as the loop terminator
 * (matches `dc.l -1` after the last entry in SWOS2.s:118137). */
CompetitionCareer careerContests[35] = {
    /* Length must use sizeof on the source variable so the copy stays in
     * sync with the struct's actual layout. Every CompetitionCup*_2_N type
     * has a `char *text[2]` pair that grows from 8 to 16 bytes on 64-bit,
     * making the struct 8 bytes larger — a hardcoded byte literal here
     * truncates the trailing team list and causes auto-advance to spin
     * forever loading bogus team IDs (data/team.255 etc.). */
    { (Competition *)&euroCup, (Competition *)&euroCupCopy, sizeof(euroCup) },
    { (Competition *)&cupWinnersCup, (Competition *)&cupWinnersCupCopy, sizeof(cupWinnersCup) },
    { (Competition *)&uefaCup, (Competition *)&uefaCupCopy, sizeof(uefaCup) },
    { (Competition *)&invitationTournament, (Competition *)&invitationTournamentCopy, sizeof(invitationTournament) },
    { (Competition *)&tour, (Competition *)&tourCopy, sizeof(tour) },
    { (Competition *)&friendly, (Competition *)&friendlyCopy, sizeof(friendly) },
    { (Competition *)&europeanChampionships2, (Competition *)&europeanChampionships2Copy, sizeof(europeanChampionships2) },
    { (Competition *)&ecQualifyingGroup1, (Competition *)&ecQualifyingGroup1Copy, sizeof(ecQualifyingGroup1) },
    { (Competition *)&ecQualifyingGroup2, (Competition *)&ecQualifyingGroup2Copy, sizeof(ecQualifyingGroup2) },
    { (Competition *)&ecQualifyingGroup3, (Competition *)&ecQualifyingGroup3Copy, sizeof(ecQualifyingGroup3) },
    { (Competition *)&ecQualifyingGroup4, (Competition *)&ecQualifyingGroup4Copy, sizeof(ecQualifyingGroup4) },
    { (Competition *)&ecQualifyingGroup5, (Competition *)&ecQualifyingGroup5Copy, sizeof(ecQualifyingGroup5) },
    { (Competition *)&ecQualifyingGroup6, (Competition *)&ecQualifyingGroup6Copy, sizeof(ecQualifyingGroup6) },
    { (Competition *)&ecQualifyingGroup7, (Competition *)&ecQualifyingGroup7Copy, sizeof(ecQualifyingGroup7) },
    { (Competition *)&ecQualifyingGroup8, (Competition *)&ecQualifyingGroup8Copy, sizeof(ecQualifyingGroup8) },
    { (Competition *)&copaAmerica, (Competition *)&copaAmericaCopy, sizeof(copaAmerica) },
    { (Competition *)&africanNationsCup, (Competition *)&africanNationsCupCopy, sizeof(africanNationsCup) },
    { (Competition *)&northAmericanCup, (Competition *)&northAmericanCupCopy, sizeof(northAmericanCup) },
    { (Competition *)&asianCup, (Competition *)&asianCupCopy, sizeof(asianCup) },
    { (Competition *)&oceaniaCup, (Competition *)&oceaniaCupCopy, sizeof(oceaniaCup) },
    { (Competition *)&worldCup2, (Competition *)&worldCup2Copy, sizeof(worldCup2) },
    { (Competition *)&worldCupQualifyingGroup1, (Competition *)&worldCupQualifyingGroup1Copy, sizeof(worldCupQualifyingGroup1) },
    { (Competition *)&worldCupQualifyingGroup2, (Competition *)&worldCupQualifyingGroup2Copy, sizeof(worldCupQualifyingGroup2) },
    { (Competition *)&worldCupQualifyingGroup3, (Competition *)&worldCupQualifyingGroup3Copy, sizeof(worldCupQualifyingGroup3) },
    { (Competition *)&worldCupQualifyingGroup4, (Competition *)&worldCupQualifyingGroup4Copy, sizeof(worldCupQualifyingGroup4) },
    { (Competition *)&worldCupQualifyingGroup5, (Competition *)&worldCupQualifyingGroup5Copy, sizeof(worldCupQualifyingGroup5) },
    { (Competition *)&worldCupQualifyingGroup6, (Competition *)&worldCupQualifyingGroup6Copy, sizeof(worldCupQualifyingGroup6) },
    { (Competition *)&worldCupQualifyingGroup7, (Competition *)&worldCupQualifyingGroup7Copy, sizeof(worldCupQualifyingGroup7) },
    { (Competition *)&worldCupQualifyingGroup8, (Competition *)&worldCupQualifyingGroup8Copy, sizeof(worldCupQualifyingGroup8) },
    { (Competition *)&northAmericaWorldCupQualification, (Competition *)&northAmericaWorldCupQualificationCopy, sizeof(northAmericaWorldCupQualification) },
    { (Competition *)&southAmericaWorldCupQualification, (Competition *)&southAmericaWorldCupQualificationCopy, sizeof(southAmericaWorldCupQualification) },
    { (Competition *)&africaWorldCupQualification, (Competition *)&africaWorldCupQualificationCopy, sizeof(africaWorldCupQualification) },
    { (Competition *)&asiaWorldCupQualification, (Competition *)&asiaWorldCupQualificationCopy, sizeof(asiaWorldCupQualification) },
    { (Competition *)&oceaniaWorldCupQualification, (Competition *)&oceaniaWorldCupQualificationCopy, sizeof(oceaniaWorldCupQualification) },
    { (Competition *)0xFFFFFFFFu, (Competition *)0, 0 },  /* sentinel */
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

Competition *continentalCups[7] = {
    (Competition *)&europeanChampionships, (Competition *)&copaAmerica,
    (Competition *)&northAmericanCup, (Competition *)&africanNationsCup,
    (Competition *)&asianCup, (Competition *)&oceaniaCup, (Competition *)-1
};

/* ======================================================================
 * Qualifying Groups and Region Tables
 * ====================================================================== */

QualifyingGroup worldCupQualifyingGroups = {
    { (Competition *)&worldCupQualifyingGroup1, (Competition *)&worldCupQualifyingGroup2,
      (Competition *)&worldCupQualifyingGroup3, (Competition *)&worldCupQualifyingGroup4,
      (Competition *)&worldCupQualifyingGroup5, (Competition *)&worldCupQualifyingGroup6,
      (Competition *)&worldCupQualifyingGroup7, (Competition *)&worldCupQualifyingGroup8,
      (Competition *)-1 },
    { 0xFF, CN_ALBANIA }
};

QualifyingGroup ecQualifyingGroups = {
    { (Competition *)&ecQualifyingGroup1, (Competition *)&ecQualifyingGroup2,
      (Competition *)&ecQualifyingGroup3, (Competition *)&ecQualifyingGroup4,
      (Competition *)&ecQualifyingGroup5, (Competition *)&ecQualifyingGroup6,
      (Competition *)&ecQualifyingGroup7, (Competition *)&ecQualifyingGroup8,
      (Competition *)-1 },
    { 0xFF, CN_ALBANIA }
};

QualifyingGroup customsTable = { { (Competition *)&customsLeague, (Competition *)-2, (Competition *)-1 } };

QualifyingGroup worldTable = {
    { (Competition *)&worldCup, (Competition *)-1 },
    { CN_EUROPE, CN_NORTH_AMERICA, CN_SOUTH_AMERICA, CN_AFRICA, CN_ASIA, CN_OCEANIA, 0xFF, CN_ALBANIA }
};

QualifyingGroup europeTable = {
    { (Competition *)0xFC, (Competition *)&europeanChampionships,
      (Competition *)&euroCup, (Competition *)&cupWinnersCup,
      (Competition *)&uefaCup, (Competition *)-1 },
    { CN_ALBANIA, CN_AUSTRIA, CN_BELARUS, CN_BELGIUM, CN_BULGARIA, CN_CROATIA,
      CN_CYPRUS, CN_CZECH_REPUBLIC, CN_DENMARK, CN_ENGLAND, CN_ESTONIA, CN_FAROE_ISLANDS,
      CN_FINLAND, CN_FRANCE, CN_GERMANY, CN_GREECE, CN_HOLLAND, CN_HUNGARY,
      CN_ICELAND, CN_ISRAEL, CN_ITALY, CN_LATVIA, CN_LITHUANIA, CN_LUXEMBOURG,
      CN_MALTA, CN_NORTHERN_IRELAND, CN_NORWAY, CN_POLAND, CN_PORTUGAL,
      CN_REPUBLIC_IRELAND, CN_ROMANIA, CN_RUSSIA, CN_SAN_MARINO, CN_SCOTLAND,
      CN_SLOVAKIA, CN_SLOVENIA, CN_SPAIN, CN_SWEDEN, CN_SWITZERLAND, CN_TURKEY,
      CN_UKRAINE, CN_WALES, CN_YUGOSLAVIA, 0xFF }
};

QualifyingGroup africaTable = {
    { (Competition *)&africaWorldCupQualification, (Competition *)&africanNationsCup, (Competition *)-1 },
    { CN_ALGERIA, CN_GHANA, CN_SOUTH_AFRICA, 0xFF }
};

QualifyingGroup southAmericaTable = {
    { (Competition *)&southAmericaWorldCupQualification, (Competition *)&copaAmerica, (Competition *)-1 },
    { CN_ARGENTINA, CN_BOLIVIA, CN_BRAZIL, CN_CHILE, CN_COLOMBIA, CN_ECUADOR,
      CN_PARAGUAY, CN_PERU, CN_SURINAM, CN_URUGUAY, CN_VENEZUELA, 0xFF }
};

QualifyingGroup northAmericaTable = {
    { (Competition *)&northAmericaWorldCupQualification, (Competition *)&northAmericanCup, (Competition *)-1 },
    { CN_EL_SALVADOR, CN_MEXICO, CN_USA, 0xFF }
};

QualifyingGroup asiaTable = {
    { (Competition *)&asiaWorldCupQualification, (Competition *)&asianCup, (Competition *)-1 },
    { CN_INDIA, CN_JAPAN, CN_TAIWAN, 0xFF }
};

QualifyingGroup oceaniaTable = {
    { (Competition *)&oceaniaWorldCupQualification, (Competition *)&oceaniaCup, (Competition *)-1 },
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
Competition *englandTable[5]         = { (Competition *)&englishLeaguesBlock._main, (Competition *)-2, (Competition *)&englishCup, (Competition *)&englishLeagueCup, (Competition *)-1 };
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

/* Pack all four buckets into one contiguous block. The bucket-walk in
 * Career_PrepareEuropeanCupQualificationState scans
 * `euroCupQualGroupSeedBuckets_15_18_22[0].ptr` (= bucket15) byte-by-byte
 * looking for the user's country code, terminating only at the 0xFF
 * sentinel at the end of bucket22Full. On Amiga the four buckets sit
 * consecutively in BSS (15+18+22+23 = 78 bytes ending in 0xFF), so the
 * scan walks the full set. On SDL the linker scattered them — bucket15
 * actually came LAST in memory layout — so the scan walked off the end
 * of bucket15 into unrelated globals, hitting some random 0xFF and
 * miscounting the user's country occurrences. Result: for non-European
 * careers like Ghana the replacement loop accidentally wrote one
 * Ghana-coded entry into uefaCupEligibleCompetitionIds, no European
 * country ever consumed it, UEFA Cup ended up 1 short of 32, and
 * Career_AdvanceToNextSeason_UpdateCompetitions's careerFileBuffer ==
 * 0x50 invariant tripped panic 0x8999. Pack into one struct; consumers
 * use the named bucket15/18/22/22Full fields.
 *
 * UNTANGLED (.equ -> typed field): the EuroCupQualSeedBucketBlock typedef
 * + extern live in game_types.h (shared by competition_data.c and the
 * consumer table in swos_amiga_ptr_imports.c). The old
 * `.equ euroCupQualSeedBucketN, ...block + <literal>` aliases are gone —
 * all uint8 buckets so the literal offsets (15/33/55) were already
 * 64-bit-safe, but typed field access removes the .equ entirely. */
EuroCupQualSeedBucketBlock euroCupQualSeedBucketBlock = {
    .bucket15     = { 14, 14, 14, 14, 20, 20, 20, 20, 35, 35, 35, 35, 8, 8, 8 },
    .bucket18     = { 2, 2, 2, 13, 13, 13, 29, 29, 29, 30, 30, 30, 31, 31, 31, 41, 41, 41 },
    .bucket22     = { 1, 1, 3, 3, 6, 6, 7, 7, 15, 15, 25, 25, 28, 28, 33, 33, 36, 36, 37, 37, 38, 38 },
    .bucket22Full = { 16, 12, 0, 17, 5, 27, 26, 24, 18, 23, 4, 10, 11, 19, 21, 22, 32, 34, 39, 40, 76, 78, 255 }
};

/* ======================================================================
 * Cup Pick Tables (per-region competition selection for career)
 * ====================================================================== */

Competition *cupPickOceaniaInviteFriendly[5] = { (Competition *)&invitationTournamentCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickOceaniaOceaniaCupFriendly[5] = { (Competition *)&oceaniaCupCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickOceaniaWorldCupFriendly[5] = { (Competition *)&worldCup2Copy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickOceaniaWorldCupInvite[5] = { (Competition *)&worldCup2Copy, (Competition *)-2, (Competition *)&invitationTournamentCopy, (Competition *)&friendlyCopy, (Competition *)-1 };
Competition *cupPickOceaniaOceaniaCupNonFinalist[5] = { 0, (Competition *)-2, (Competition *)&oceaniaCupCopy, (Competition *)-1, 0 };
Competition *cupPickOceaniaOceaniaCupFinalist[5] = { (Competition *)&oceaniaCupCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };

Competition *cupPickNAmericaInviteFriendly[5] = { (Competition *)&invitationTournamentCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickNAmericaNaCupFriendly[5] = { (Competition *)&northAmericanCupCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickNAmericaWorldCupFriendly[5] = { (Competition *)&worldCup2Copy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickNAmericaWorldCupInvite[5] = { (Competition *)&worldCup2Copy, (Competition *)-2, (Competition *)&invitationTournamentCopy, (Competition *)&friendlyCopy, (Competition *)-1 };
Competition *cupPickNAmericaNaCupNonFinalist[5] = { 0, (Competition *)-2, (Competition *)&northAmericanCupCopy, (Competition *)-1, 0 };
Competition *cupPickNAmericaNaCupFinalist[5] = { (Competition *)&northAmericanCupCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };

Competition *cupPickAsiaInviteFriendly[5] = { (Competition *)&invitationTournamentCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickAsiaAsianCupFriendly[5] = { (Competition *)&asianCupCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickAsiaWorldCupFriendly[5] = { (Competition *)&worldCup2Copy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickAsiaWorldCupInvite[5] = { (Competition *)&worldCup2Copy, (Competition *)-2, (Competition *)&invitationTournamentCopy, (Competition *)&friendlyCopy, (Competition *)-1 };
Competition *cupPickAsiaAsianCupNonFinalist[5] = { 0, (Competition *)-2, (Competition *)&asianCupCopy, (Competition *)-1, 0 };
Competition *cupPickAsiaAsianCupFinalist[5] = { (Competition *)&asianCupCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };

Competition *cupPickAfricaInviteFriendly[5] = { (Competition *)&invitationTournamentCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickAfricaAfricanCupFriendly[5] = { (Competition *)&africanNationsCupCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickAfricaWorldCupFriendly[5] = { (Competition *)&worldCup2Copy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickAfricaWorldCupInvite[5] = { (Competition *)&worldCup2Copy, (Competition *)-2, (Competition *)&invitationTournamentCopy, (Competition *)&friendlyCopy, (Competition *)-1 };
Competition *cupPickAfricaAfricanCupNonFinalist[5] = { 0, (Competition *)-2, (Competition *)&africanNationsCupCopy, (Competition *)-1, 0 };
Competition *cupPickAfricaAfricanCupFinalist[5] = { (Competition *)&africanNationsCupCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };

Competition *cupPickSAmericaInviteFriendly[5] = { (Competition *)&invitationTournamentCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickSAmericaCopaAmericaFriendly[5] = { (Competition *)&copaAmericaCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickSAmericaWorldCupFriendly[5] = { (Competition *)&worldCup2Copy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickSAmericaWorldCupInvite[5] = { (Competition *)&worldCup2Copy, (Competition *)-2, (Competition *)&invitationTournamentCopy, (Competition *)&friendlyCopy, (Competition *)-1 };
Competition *cupPickSAmericaCopaAmericaNonFinalist[5] = { 0, (Competition *)-2, (Competition *)&copaAmericaCopy, (Competition *)-1, 0 };
Competition *cupPickSAmericaCopaAmericaFinalist[5] = { (Competition *)&copaAmericaCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };

Competition *ecQualifyingGroupsTable[8] = {
    (Competition *)&ecQualifyingGroup1Copy, (Competition *)&ecQualifyingGroup2Copy,
    (Competition *)&ecQualifyingGroup3Copy, (Competition *)&ecQualifyingGroup4Copy,
    (Competition *)&ecQualifyingGroup5Copy, (Competition *)&ecQualifyingGroup6Copy,
    (Competition *)&ecQualifyingGroup7Copy, (Competition *)&ecQualifyingGroup8Copy
};

Competition *cupPickEuropeNoCupFriendly[5] = { 0, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickListEuropeInviteTournamentFriendly[5] = { (Competition *)&invitationTournamentCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickLisEuropeEuroChampsFriendly[5] = { (Competition *)&europeanChampionships2Copy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickListEuropeEuroChampsInviteFriendly[5] = { (Competition *)&europeanChampionships2Copy, (Competition *)-2, (Competition *)&invitationTournamentCopy, (Competition *)&friendlyCopy, (Competition *)-1 };
Competition *cupPickListEuropeWorldCupFriendly[5] = { (Competition *)&worldCup2Copy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickLisEuropeWorldCupInviteFriendly[5] = { (Competition *)&worldCup2Copy, (Competition *)-2, (Competition *)&invitationTournamentCopy, (Competition *)&friendlyCopy, (Competition *)-1 };
Competition *cupPickEuropeNoCupFriendlyAlt[5] = { 0, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };
Competition *cupPickListEuropeInviteTournamentFriendlyAlt[5] = { (Competition *)&invitationTournamentCopy, (Competition *)-2, (Competition *)&friendlyCopy, (Competition *)-1, 0 };

Competition *worldCupQualifyingGroupsTable[8] = {
    (Competition *)&worldCupQualifyingGroup1Copy, (Competition *)&worldCupQualifyingGroup2Copy,
    (Competition *)&worldCupQualifyingGroup3Copy, (Competition *)&worldCupQualifyingGroup4Copy,
    (Competition *)&worldCupQualifyingGroup5Copy, (Competition *)&worldCupQualifyingGroup6Copy,
    (Competition *)&worldCupQualifyingGroup7Copy, (Competition *)&worldCupQualifyingGroup8Copy
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
void *competitionsTable[256] = {
    [0]  = albaniaTable,
    [1]  = austriaTable,
    [2]  = belgiumTable,
    [3]  = bulgariaTable,
    [4]  = croatiaTable,
    [5]  = cyprusTable,
    [6]  = czechRepublicTable,
    [7]  = denmarkTable,
    [8]  = englandTable,
    [10] = estoniaTable,
    [11] = faroeIslandsTable,
    [12] = finlandTable,
    [13] = franceTable,
    [14] = germanyTable,
    [15] = greeceTable,
    [16] = hungaryTable,
    [17] = icelandTable,
    [18] = republicIrelandTable,
    [19] = israelTable,
    [20] = italyTable,
    [21] = latviaTable,
    [22] = lithuaniaTable,
    [23] = luxembourgTable,
    [24] = maltaTable,
    [25] = hollandTable,
    [26] = northernIrelandTable,
    [27] = norwayTable,
    [28] = polandTable,
    [29] = portugalTable,
    [30] = romaniaTable,
    [31] = russiaTable,
    [32] = sanMarinoTable,
    [33] = scotlandTable,
    [34] = sloveniaTable,
    [35] = spainTable,
    [36] = swedenTable,
    [37] = switzerlandTable,
    [38] = turkeyTable,
    [39] = ukraineTable,
    [40] = walesTable,
    [41] = yugoslaviaTable,
    [42] = algeriaTable,
    [43] = argentinaTable,
    [44] = australiaTable,
    [45] = boliviaTable,
    [46] = brasilTable,
    [48] = chileTable,
    [49] = colombiaTable,
    [50] = ecuadorTable,
    [51] = elSalvadorTable,
    [55] = japanTable,
    [60] = mexicoTable,
    [62] = newZealandTable,
    [64] = paraguayTable,
    [65] = peruTable,
    [66] = surinamTable,
    [67] = taiwanTable,
    [69] = southAfricaTable,
    [71] = uruguayTable,
    [72] = &customsTable,
    [73] = usaTable,
    [75] = indiaTable,
    [76] = belarusTable,
    [77] = venezuelaTable,
    [78] = slovakiaTable,
    [79] = ghanaTable,
    [80] = &europeTable,
    [81] = &africaTable,
    [82] = &southAmericaTable,
    [83] = &northAmericaTable,
    [84] = &asiaTable,
    [85] = &oceaniaTable,
    [100] = &customsTable,
    [252] = &worldCupQualifyingGroups,
    [253] = &ecQualifyingGroups,
    [254] = &worldTable,
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
