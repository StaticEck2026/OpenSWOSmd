/*
 * GENERATED FILE — do not edit by hand.
 *
 * Source:  SWOS2.lst (original Amiga static-build listing)
 * Tool:    tools/extract_amiga_addresses.py
 *
 * Each entry maps an original-Amiga symbol name → that symbol's
 * Amiga absolute address → the current-build address of the same
 * symbol. The runtime LUT in src/save_pointer_lut.c uses this to
 * translate save-file pointer fields between Amiga-canonical and
 * current-build representations.
 *
 * See SAVE_DATA.md "Cross-build pointer LUT" for the design.
 */
#include <stdint.h>
#include <stddef.h>

/* Each LUT-resident symbol is forward-declared as `extern char name[]`
 * for the sole purpose of taking its address. The actual definition
 * lives elsewhere in the codebase (often as `uint32_t` or a struct);
 * the array-of-char declaration is compatible enough for address
 * taking under our build flags (-fno-strict-aliasing). The compiler
 * never accesses the contents through this declaration. */
extern char euroCup[];
extern char cupWinnersCup[];
extern char uefaCup[];
extern char worldCup2[];
extern char worldCup[];
extern char worldCupQualifyingGroup1[];
extern char worldCupQualifyingGroup2[];
extern char worldCupQualifyingGroup3[];
extern char worldCupQualifyingGroup4[];
extern char worldCupQualifyingGroup5[];
extern char worldCupQualifyingGroup6[];
extern char worldCupQualifyingGroup7[];
extern char worldCupQualifyingGroup8[];
extern char northAmericaWorldCupQualification[];
extern char southAmericaWorldCupQualification[];
extern char africaWorldCupQualification[];
extern char asiaWorldCupQualification[];
extern char oceaniaWorldCupQualification[];
extern char invitationTournament[];
extern char tour[];
extern char friendly[];
extern char europeanChampionships2[];
extern char europeanChampionships[];
extern char ecQualifyingGroup1[];
extern char ecQualifyingGroup2[];
extern char ecQualifyingGroup3[];
extern char ecQualifyingGroup4[];
extern char ecQualifyingGroup5[];
extern char ecQualifyingGroup6[];
extern char ecQualifyingGroup7[];
extern char ecQualifyingGroup8[];
extern char copaAmerica[];
extern char africanNationsCup[];
extern char northAmericanCup[];
extern char asianCup[];
extern char oceaniaCup[];
extern char customsLeague[];
extern char albanianLeague[];
extern char austrianLeague[];
extern char belarussiaLleague[];
extern char belgianLeague[];
extern char bulgarianLeague[];
extern char croatianLeague[];
extern char cypriotLeague[];
extern char czechLeague[];
extern char danishLeague[];
extern char englishLeagues[];
extern char englishLeagues1[];
extern char englishLeagues2[];
extern char englishLeagues3[];
extern char div1Playoffs[];
extern char div2Playoffs[];
extern char div3Playoffs[];
extern char estonianLeague[];
extern char faroeIslesLeague[];
extern char finnishLeague[];
extern char frenchLeague[];
extern char germanLeague[];
extern char greekLeague[];
extern char dutchLeague[];
extern char dutchLeagues1[];
extern char dutchPlayoffs[];
extern char hungarianLeague[];
extern char icelandicLeague[];
extern char israeliLeague[];
extern char italianLeague[];
extern char latvianLeague[];
extern char lithuanianLeague[];
extern char luxembourgLeague[];
extern char malteseLeague[];
extern char northernIrishLeague[];
extern char norwegianLeague[];
extern char polishLeague[];
extern char portugeseLeague[];
extern char irishLeague[];
extern char romanianLeague[];
extern char russianLeague[];
extern char sanMarinoLeague[];
extern char scottishLeague[];
extern char scottishLeague1[];
extern char scottishPlayoffs[];
extern char slovakianLeague[];
extern char slovenianLeague[];
extern char spanishLeague[];
extern char spanishLeague1[];
extern char spanishPlayoffs[];
extern char swedishLeague[];
extern char swissLeague[];
extern char turkishLeague[];
extern char ukrainianLeague[];
extern char welshLeague[];
extern char yugoslavianLeague[];
extern char algerianLeague[];
extern char argentinianLeague[];
extern char australianLeague[];
extern char bolivianLeague[];
extern char brasilianLeague[];
extern char chileanLeague[];
extern char colombianLeague[];
extern char ecuadorLeague[];
extern char elSalvadorLeague[];
extern char indianLeague[];
extern char japaneseLeague[];
extern char mexicanLeague[];
extern char newZealandLeague[];
extern char paraguayanLeague[];
extern char peruvianLeague[];
extern char southAfricanLeague[];
extern char surinamLeague[];
extern char taiwaneseLeague[];
extern char uruguayanLeague[];
extern char uruguayanLeague1[];
extern char uruguayanPlayoffs[];
extern char americanLeague[];
extern char venezuelanLeague[];
extern char ghanaLeague[];
extern char albanianCup[];
extern char austrianCup[];
extern char belarusianCup[];
extern char belgianCup[];
extern char bulgarianCup[];
extern char croatianCup[];
extern char cypriotCup[];
extern char czechCup[];
extern char danishCup[];
extern char englishCup[];
extern char englishLeagueCup[];
extern char estonianCup[];
extern char faroeIslesCup[];
extern char finnishCup[];
extern char frenchCup[];
extern char germanCup[];
extern char greekCup[];
extern char hungarianCup[];
extern char icelandicCup[];
extern char irishCup[];
extern char israeliCup[];
extern char italianCup[];
extern char latvianCup[];
extern char lithuanianCup[];
extern char luxembourgCup[];
extern char malteseCup[];
extern char dutchCup[];
extern char northernIrishCup[];
extern char norwegianCup[];
extern char polishCup[];
extern char portugeseCup[];
extern char romanianCup[];
extern char russianCup[];
extern char sanMarinoCup[];
extern char scottishCup[];
extern char scottishLeagueCup[];
extern char slovakianCup[];
extern char slovenianCup[];
extern char spanishCup[];
extern char swedishCup[];
extern char swissCup[];
extern char turkishCup[];
extern char ukrainianCup[];
extern char welshCup[];
extern char yugoslavianCup[];
extern char algerianCup[];
extern char australianCup[];
extern char japaneseCup[];
extern char japaneseEmperorsCup[];
extern char southAfricanCup[];
extern char taiwaneseCup[];
extern char competitionsTable[];
extern char worldCupQualifyingGroups[];
extern char ecQualifyingGroups[];
extern char customsTable[];
extern char worldTable[];
extern char europeTable[];
extern char africaTable[];
extern char southAmericaTable[];
extern char northAmericaTable[];
extern char asiaTable[];
extern char oceaniaTable[];
extern char careerFileBuffer[];
extern char tmdFileBuffer[];
extern char careerCupPickList[];
extern char currentCompetitionPtr[];
extern char secondaryCompetitionPtr[];
extern char tertiaryCompetitionPtr[];
extern char quaternaryCompetitionPtr[];
extern char diyCompetitionPtr[];
extern char diyCompetitionTablePtr[];
extern char careerCompetitionSlotPtrTable[];
extern char competitionContext[];
extern char secondaryCompStringOffset[];
extern char tertiaryCompetitionContext[];
extern char tertiaryCompStringOffset[];
extern char diyCompetitionContext[];
extern char euroCupsCompetitionContext[];
extern char quaternaryCompStringOffset[];
extern char euroCupCopy[];
extern char cupWinnersCupCopy[];
extern char uefaCupCopy[];
extern char invitationTournamentCopy[];
extern char tourCopy[];
extern char friendlyCopy[];
extern char europeanChampionships2Copy[];
extern char ecQualifyingGroup1Copy[];
extern char ecQualifyingGroup2Copy[];
extern char ecQualifyingGroup3Copy[];
extern char ecQualifyingGroup4Copy[];
extern char ecQualifyingGroup5Copy[];
extern char ecQualifyingGroup6Copy[];
extern char ecQualifyingGroup7Copy[];
extern char ecQualifyingGroup8Copy[];
extern char copaAmericaCopy[];
extern char africanNationsCupCopy[];
extern char northAmericanCupCopy[];
extern char asianCupCopy[];
extern char oceaniaCupCopy[];
extern char worldCup2Copy[];
extern char worldCupQualifyingGroup1Copy[];
extern char worldCupQualifyingGroup2Copy[];
extern char worldCupQualifyingGroup3Copy[];
extern char worldCupQualifyingGroup4Copy[];
extern char worldCupQualifyingGroup5Copy[];
extern char worldCupQualifyingGroup6Copy[];
extern char worldCupQualifyingGroup7Copy[];
extern char worldCupQualifyingGroup8Copy[];
extern char northAmericaWorldCupQualificationCopy[];
extern char southAmericaWorldCupQualificationCopy[];
extern char africaWorldCupQualificationCopy[];
extern char asiaWorldCupQualificationCopy[];
extern char oceaniaWorldCupQualificationCopy[];
extern char competitionFileBuffer[];
extern char primaryCompStringOffset[];

typedef struct AmigaAddrLutEntry {
    const char *name;
    void       *current_addr;   /* &symbol in this build */
    uint32_t    amiga_addr;     /* address in original Amiga binary */
    uint32_t    size;           /* declared size of symbol (0 = unknown) */
} AmigaAddrLutEntry;

const AmigaAddrLutEntry g_amiga_addr_map[] = {
    { "euroCup", (void *)euroCup, 0x0014F35Au, 0x4Eu },
    { "cupWinnersCup", (void *)cupWinnersCup, 0x0014F3A8u, 0x5Cu },
    { "uefaCup", (void *)uefaCup, 0x0014F404u, 0x5Cu },
    { "worldCup2", (void *)worldCup2, 0x0014F460u, 0x70u },
    { "worldCup", (void *)worldCup, 0x0014F4D0u, 0x60u },
    { "worldCupQualifyingGroup1", (void *)worldCupQualifyingGroup1, 0x0014F530u, 0x28u },
    { "worldCupQualifyingGroup2", (void *)worldCupQualifyingGroup2, 0x0014F558u, 0x28u },
    { "worldCupQualifyingGroup3", (void *)worldCupQualifyingGroup3, 0x0014F580u, 0x28u },
    { "worldCupQualifyingGroup4", (void *)worldCupQualifyingGroup4, 0x0014F5A8u, 0x28u },
    { "worldCupQualifyingGroup5", (void *)worldCupQualifyingGroup5, 0x0014F5D0u, 0x28u },
    { "worldCupQualifyingGroup6", (void *)worldCupQualifyingGroup6, 0x0014F5F8u, 0x28u },
    { "worldCupQualifyingGroup7", (void *)worldCupQualifyingGroup7, 0x0014F620u, 0x28u },
    { "worldCupQualifyingGroup8", (void *)worldCupQualifyingGroup8, 0x0014F648u, 0x28u },
    { "northAmericaWorldCupQualification", (void *)northAmericaWorldCupQualification, 0x0014F670u, 0x48u },
    { "southAmericaWorldCupQualification", (void *)southAmericaWorldCupQualification, 0x0014F6B8u, 0x40u },
    { "africaWorldCupQualification", (void *)africaWorldCupQualification, 0x0014F6F8u, 0x70u },
    { "asiaWorldCupQualification", (void *)asiaWorldCupQualification, 0x0014F768u, 0x6Cu },
    { "oceaniaWorldCupQualification", (void *)oceaniaWorldCupQualification, 0x0014F7D4u, 0x24u },
    { "invitationTournament", (void *)invitationTournament, 0x0014F7F8u, 0x24u },
    { "tour", (void *)tour, 0x0014F81Cu, 0x24u },
    { "friendly", (void *)friendly, 0x0014F840u, 0x28u },
    { "europeanChampionships2", (void *)europeanChampionships2, 0x0014F868u, 0x4Eu },
    { "europeanChampionships", (void *)europeanChampionships, 0x0014F8B6u, 0x4Eu },
    { "ecQualifyingGroup1", (void *)ecQualifyingGroup1, 0x0014F904u, 0x28u },
    { "ecQualifyingGroup2", (void *)ecQualifyingGroup2, 0x0014F92Cu, 0x28u },
    { "ecQualifyingGroup3", (void *)ecQualifyingGroup3, 0x0014F954u, 0x28u },
    { "ecQualifyingGroup4", (void *)ecQualifyingGroup4, 0x0014F97Cu, 0x28u },
    { "ecQualifyingGroup5", (void *)ecQualifyingGroup5, 0x0014F9A4u, 0x28u },
    { "ecQualifyingGroup6", (void *)ecQualifyingGroup6, 0x0014F9CCu, 0x28u },
    { "ecQualifyingGroup7", (void *)ecQualifyingGroup7, 0x0014F9F4u, 0x28u },
    { "ecQualifyingGroup8", (void *)ecQualifyingGroup8, 0x0014FA1Cu, 0x24u },
    { "copaAmerica", (void *)copaAmerica, 0x0014FA40u, 0x46u },
    { "africanNationsCup", (void *)africanNationsCup, 0x0014FA86u, 0x30u },
    { "northAmericanCup", (void *)northAmericanCup, 0x0014FAB6u, 0x2Eu },
    { "asianCup", (void *)asianCup, 0x0014FAE4u, 0x30u },
    { "oceaniaCup", (void *)oceaniaCup, 0x0014FB14u, 0x1Cu },
    { "customsLeague", (void *)customsLeague, 0x0014FB30u, 0x38u },
    { "albanianLeague", (void *)albanianLeague, 0x0014FB68u, 0x14u },
    { "austrianLeague", (void *)austrianLeague, 0x0014FB7Cu, 0x14u },
    { "belarussiaLleague", (void *)belarussiaLleague, 0x0014FB90u, 0x14u },
    { "belgianLeague", (void *)belgianLeague, 0x0014FBA4u, 0x14u },
    { "bulgarianLeague", (void *)bulgarianLeague, 0x0014FBB8u, 0x14u },
    { "croatianLeague", (void *)croatianLeague, 0x0014FBCCu, 0x14u },
    { "cypriotLeague", (void *)cypriotLeague, 0x0014FBE0u, 0x14u },
    { "czechLeague", (void *)czechLeague, 0x0014FBF4u, 0x14u },
    { "danishLeague", (void *)danishLeague, 0x0014FC08u, 0x14u },
    { "englishLeagues", (void *)englishLeagues, 0x0014FC1Cu, 0x66u },
    { "englishLeagues1", (void *)englishLeagues1, 0x0014FC82u, 0x50u },
    { "englishLeagues2", (void *)englishLeagues2, 0x0014FCD2u, 0x50u },
    { "englishLeagues3", (void *)englishLeagues3, 0x0014FD22u, 0x30u },
    { "div1Playoffs", (void *)div1Playoffs, 0x0014FD52u, 0x20u },
    { "div2Playoffs", (void *)div2Playoffs, 0x0014FD72u, 0x20u },
    { "div3Playoffs", (void *)div3Playoffs, 0x0014FD92u, 0x20u },
    { "estonianLeague", (void *)estonianLeague, 0x0014FDB2u, 0x14u },
    { "faroeIslesLeague", (void *)faroeIslesLeague, 0x0014FDC6u, 0x14u },
    { "finnishLeague", (void *)finnishLeague, 0x0014FDDAu, 0x14u },
    { "frenchLeague", (void *)frenchLeague, 0x0014FDEEu, 0x2Au },
    { "germanLeague", (void *)germanLeague, 0x0014FE18u, 0x2Au },
    { "greekLeague", (void *)greekLeague, 0x0014FE42u, 0x14u },
    { "dutchLeague", (void *)dutchLeague, 0x0014FE56u, 0x3Au },
    { "dutchLeagues1", (void *)dutchLeagues1, 0x0014FE90u, 0x30u },
    { "dutchPlayoffs", (void *)dutchPlayoffs, 0x0014FEC0u, 0x20u },
    { "hungarianLeague", (void *)hungarianLeague, 0x0014FEE0u, 0x14u },
    { "icelandicLeague", (void *)icelandicLeague, 0x0014FEF4u, 0x14u },
    { "israeliLeague", (void *)israeliLeague, 0x0014FF08u, 0x14u },
    { "italianLeague", (void *)italianLeague, 0x0014FF1Cu, 0x2Au },
    { "latvianLeague", (void *)latvianLeague, 0x0014FF46u, 0x14u },
    { "lithuanianLeague", (void *)lithuanianLeague, 0x0014FF5Au, 0x14u },
    { "luxembourgLeague", (void *)luxembourgLeague, 0x0014FF6Eu, 0x14u },
    { "malteseLeague", (void *)malteseLeague, 0x0014FF82u, 0x14u },
    { "northernIrishLeague", (void *)northernIrishLeague, 0x0014FF96u, 0x14u },
    { "norwegianLeague", (void *)norwegianLeague, 0x0014FFAAu, 0x14u },
    { "polishLeague", (void *)polishLeague, 0x0014FFBEu, 0x14u },
    { "portugeseLeague", (void *)portugeseLeague, 0x0014FFD2u, 0x14u },
    { "irishLeague", (void *)irishLeague, 0x0014FFE6u, 0x14u },
    { "romanianLeague", (void *)romanianLeague, 0x0014FFFAu, 0x14u },
    { "russianLeague", (void *)russianLeague, 0x0015000Eu, 0x14u },
    { "sanMarinoLeague", (void *)sanMarinoLeague, 0x00150022u, 0x14u },
    { "scottishLeague", (void *)scottishLeague, 0x00150036u, 0x36u },
    { "scottishLeague1", (void *)scottishLeague1, 0x0015006Cu, 0x18u },
    { "scottishPlayoffs", (void *)scottishPlayoffs, 0x00150084u, 0x1Cu },
    { "slovakianLeague", (void *)slovakianLeague, 0x001500A0u, 0x14u },
    { "slovenianLeague", (void *)slovenianLeague, 0x001500B4u, 0x14u },
    { "spanishLeague", (void *)spanishLeague, 0x001500C8u, 0x4Au },
    { "spanishLeague1", (void *)spanishLeague1, 0x00150112u, 0x30u },
    { "spanishPlayoffs", (void *)spanishPlayoffs, 0x00150142u, 0x20u },
    { "swedishLeague", (void *)swedishLeague, 0x00150162u, 0x14u },
    { "swissLeague", (void *)swissLeague, 0x00150176u, 0x14u },
    { "turkishLeague", (void *)turkishLeague, 0x0015018Au, 0x14u },
    { "ukrainianLeague", (void *)ukrainianLeague, 0x0015019Eu, 0x14u },
    { "welshLeague", (void *)welshLeague, 0x001501B2u, 0x14u },
    { "yugoslavianLeague", (void *)yugoslavianLeague, 0x001501C6u, 0x14u },
    { "algerianLeague", (void *)algerianLeague, 0x001501DAu, 0x14u },
    { "argentinianLeague", (void *)argentinianLeague, 0x001501EEu, 0x1Au },
    { "australianLeague", (void *)australianLeague, 0x00150208u, 0x46u },
    { "bolivianLeague", (void *)bolivianLeague, 0x0015024Eu, 0x14u },
    { "brasilianLeague", (void *)brasilianLeague, 0x00150262u, 0x14u },
    { "chileanLeague", (void *)chileanLeague, 0x00150276u, 0x14u },
    { "colombianLeague", (void *)colombianLeague, 0x0015028Au, 0x14u },
    { "ecuadorLeague", (void *)ecuadorLeague, 0x0015029Eu, 0x14u },
    { "elSalvadorLeague", (void *)elSalvadorLeague, 0x001502B2u, 0x26u },
    { "indianLeague", (void *)indianLeague, 0x001502D8u, 0x14u },
    { "japaneseLeague", (void *)japaneseLeague, 0x001502ECu, 0x2Au },
    { "mexicanLeague", (void *)mexicanLeague, 0x00150316u, 0x1Au },
    { "newZealandLeague", (void *)newZealandLeague, 0x00150330u, 0x38u },
    { "paraguayanLeague", (void *)paraguayanLeague, 0x00150368u, 0x14u },
    { "peruvianLeague", (void *)peruvianLeague, 0x0015037Cu, 0x14u },
    { "southAfricanLeague", (void *)southAfricanLeague, 0x00150390u, 0x14u },
    { "surinamLeague", (void *)surinamLeague, 0x001503A4u, 0x14u },
    { "taiwaneseLeague", (void *)taiwaneseLeague, 0x001503B8u, 0x14u },
    { "uruguayanLeague", (void *)uruguayanLeague, 0x001503CCu, 0x2Au },
    { "uruguayanLeague1", (void *)uruguayanLeague1, 0x001503F6u, 0x18u },
    { "uruguayanPlayoffs", (void *)uruguayanPlayoffs, 0x0015040Eu, 0x1Cu },
    { "americanLeague", (void *)americanLeague, 0x0015042Au, 0x14u },
    { "venezuelanLeague", (void *)venezuelanLeague, 0x0015043Eu, 0x14u },
    { "ghanaLeague", (void *)ghanaLeague, 0x00150452u, 0x14u },
    { "albanianCup", (void *)albanianCup, 0x00150466u, 0x12u },
    { "austrianCup", (void *)austrianCup, 0x00150478u, 0x12u },
    { "belarusianCup", (void *)belarusianCup, 0x0015048Au, 0x12u },
    { "belgianCup", (void *)belgianCup, 0x0015049Cu, 0x12u },
    { "bulgarianCup", (void *)bulgarianCup, 0x001504AEu, 0x12u },
    { "croatianCup", (void *)croatianCup, 0x001504C0u, 0x12u },
    { "cypriotCup", (void *)cypriotCup, 0x001504D2u, 0x12u },
    { "czechCup", (void *)czechCup, 0x001504E4u, 0x12u },
    { "danishCup", (void *)danishCup, 0x001504F6u, 0x12u },
    { "englishCup", (void *)englishCup, 0x00150508u, 0x1Cu },
    { "englishLeagueCup", (void *)englishLeagueCup, 0x00150524u, 0x1Cu },
    { "estonianCup", (void *)estonianCup, 0x00150578u, 0x12u },
    { "faroeIslesCup", (void *)faroeIslesCup, 0x0015058Au, 0x12u },
    { "finnishCup", (void *)finnishCup, 0x0015059Cu, 0x12u },
    { "frenchCup", (void *)frenchCup, 0x001505AEu, 0x1Cu },
    { "germanCup", (void *)germanCup, 0x001505CAu, 0x1Cu },
    { "greekCup", (void *)greekCup, 0x001505E6u, 0x12u },
    { "hungarianCup", (void *)hungarianCup, 0x001505F8u, 0x12u },
    { "icelandicCup", (void *)icelandicCup, 0x0015060Au, 0x12u },
    { "irishCup", (void *)irishCup, 0x0015061Cu, 0x12u },
    { "israeliCup", (void *)israeliCup, 0x0015062Eu, 0x12u },
    { "italianCup", (void *)italianCup, 0x00150640u, 0x1Cu },
    { "latvianCup", (void *)latvianCup, 0x0015065Cu, 0x12u },
    { "lithuanianCup", (void *)lithuanianCup, 0x0015066Eu, 0x12u },
    { "luxembourgCup", (void *)luxembourgCup, 0x00150680u, 0x12u },
    { "malteseCup", (void *)malteseCup, 0x00150692u, 0x12u },
    { "dutchCup", (void *)dutchCup, 0x001506A4u, 0x14u },
    { "northernIrishCup", (void *)northernIrishCup, 0x001506B8u, 0x12u },
    { "norwegianCup", (void *)norwegianCup, 0x001506CAu, 0x12u },
    { "polishCup", (void *)polishCup, 0x001506DCu, 0x12u },
    { "portugeseCup", (void *)portugeseCup, 0x001506EEu, 0x12u },
    { "romanianCup", (void *)romanianCup, 0x00150700u, 0x12u },
    { "russianCup", (void *)russianCup, 0x00150712u, 0x12u },
    { "sanMarinoCup", (void *)sanMarinoCup, 0x00150724u, 0x12u },
    { "scottishCup", (void *)scottishCup, 0x00150736u, 0x1Cu },
    { "scottishLeagueCup", (void *)scottishLeagueCup, 0x00150752u, 0x1Cu },
    { "slovakianCup", (void *)slovakianCup, 0x0015076Eu, 0x12u },
    { "slovenianCup", (void *)slovenianCup, 0x00150780u, 0x12u },
    { "spanishCup", (void *)spanishCup, 0x00150792u, 0x1Cu },
    { "swedishCup", (void *)swedishCup, 0x001507AEu, 0x12u },
    { "swissCup", (void *)swissCup, 0x001507C0u, 0x12u },
    { "turkishCup", (void *)turkishCup, 0x001507D2u, 0x12u },
    { "ukrainianCup", (void *)ukrainianCup, 0x001507E4u, 0x12u },
    { "welshCup", (void *)welshCup, 0x001507F6u, 0x12u },
    { "yugoslavianCup", (void *)yugoslavianCup, 0x00150808u, 0x12u },
    { "algerianCup", (void *)algerianCup, 0x0015081Au, 0x12u },
    { "australianCup", (void *)australianCup, 0x0015082Cu, 0x14u },
    { "japaneseCup", (void *)japaneseCup, 0x00150840u, 0x1Au },
    { "japaneseEmperorsCup", (void *)japaneseEmperorsCup, 0x0015085Au, 0x1Au },
    { "southAfricanCup", (void *)southAfricanCup, 0x00150874u, 0x12u },
    { "taiwaneseCup", (void *)taiwaneseCup, 0x00150886u, 0x12u },
    { "competitionsTable", (void *)competitionsTable, 0x001508DCu, 0x400u },
    { "worldCupQualifyingGroups", (void *)worldCupQualifyingGroups, 0x00150CDCu, 0x26u },
    { "ecQualifyingGroups", (void *)ecQualifyingGroups, 0x00150D02u, 0x26u },
    { "customsTable", (void *)customsTable, 0x00150D28u, 0xCu },
    { "worldTable", (void *)worldTable, 0x00150D34u, 0x10u },
    { "europeTable", (void *)europeTable, 0x00150D44u, 0x44u },
    { "africaTable", (void *)africaTable, 0x00150D88u, 0x10u },
    { "southAmericaTable", (void *)southAmericaTable, 0x00150D98u, 0x18u },
    { "northAmericaTable", (void *)northAmericaTable, 0x00150DB0u, 0x10u },
    { "asiaTable", (void *)asiaTable, 0x00150DC0u, 0x10u },
    { "oceaniaTable", (void *)oceaniaTable, 0x00150DD0u, 0x10u },
    { "careerFileBuffer", (void *)careerFileBuffer, 0x00157408u, 0x2u },
    { "tmdFileBuffer", (void *)tmdFileBuffer, 0x0015740Au, 0xD5C0u },
    { "careerCupPickList", (void *)careerCupPickList, 0x00164B36u, 0x4u },
    { "currentCompetitionPtr", (void *)currentCompetitionPtr, 0x00166B14u, 0x4u },
    { "secondaryCompetitionPtr", (void *)secondaryCompetitionPtr, 0x00166B18u, 0x4u },
    { "tertiaryCompetitionPtr", (void *)tertiaryCompetitionPtr, 0x00166B1Cu, 0x4u },
    { "quaternaryCompetitionPtr", (void *)quaternaryCompetitionPtr, 0x00166B20u, 0x4u },
    { "diyCompetitionPtr", (void *)diyCompetitionPtr, 0x00166B24u, 0x4u },
    { "diyCompetitionTablePtr", (void *)diyCompetitionTablePtr, 0x00166B2Au, 0x4u },
    { "careerCompetitionSlotPtrTable", (void *)careerCompetitionSlotPtrTable, 0x00166B60u, 0x14u },
    { "competitionContext", (void *)competitionContext, 0x00166B74u, 0x4u },
    { "secondaryCompStringOffset", (void *)secondaryCompStringOffset, 0x00166B9Cu, 0x4u },
    { "tertiaryCompetitionContext", (void *)tertiaryCompetitionContext, 0x001676C8u, 0x4u },
    { "tertiaryCompStringOffset", (void *)tertiaryCompStringOffset, 0x001676F0u, 0x4u },
    { "diyCompetitionContext", (void *)diyCompetitionContext, 0x00167B0Cu, 0x3Eu },
    { "euroCupsCompetitionContext", (void *)euroCupsCompetitionContext, 0x00167F50u, 0x4u },
    { "quaternaryCompStringOffset", (void *)quaternaryCompStringOffset, 0x00167F78u, 0x4u },
    { "euroCupCopy", (void *)euroCupCopy, 0x00168B24u, 0x4Eu },
    { "cupWinnersCupCopy", (void *)cupWinnersCupCopy, 0x00168B72u, 0x5Cu },
    { "uefaCupCopy", (void *)uefaCupCopy, 0x00168BCEu, 0x5Cu },
    { "invitationTournamentCopy", (void *)invitationTournamentCopy, 0x00168C2Au, 0x24u },
    { "tourCopy", (void *)tourCopy, 0x00168C4Eu, 0x24u },
    { "friendlyCopy", (void *)friendlyCopy, 0x00168C72u, 0x28u },
    { "europeanChampionships2Copy", (void *)europeanChampionships2Copy, 0x00168C9Au, 0x4Eu },
    { "ecQualifyingGroup1Copy", (void *)ecQualifyingGroup1Copy, 0x00168CE8u, 0x28u },
    { "ecQualifyingGroup2Copy", (void *)ecQualifyingGroup2Copy, 0x00168D10u, 0x28u },
    { "ecQualifyingGroup3Copy", (void *)ecQualifyingGroup3Copy, 0x00168D38u, 0x28u },
    { "ecQualifyingGroup4Copy", (void *)ecQualifyingGroup4Copy, 0x00168D60u, 0x28u },
    { "ecQualifyingGroup5Copy", (void *)ecQualifyingGroup5Copy, 0x00168D88u, 0x28u },
    { "ecQualifyingGroup6Copy", (void *)ecQualifyingGroup6Copy, 0x00168DB0u, 0x28u },
    { "ecQualifyingGroup7Copy", (void *)ecQualifyingGroup7Copy, 0x00168DD8u, 0x28u },
    { "ecQualifyingGroup8Copy", (void *)ecQualifyingGroup8Copy, 0x00168E00u, 0x24u },
    { "copaAmericaCopy", (void *)copaAmericaCopy, 0x00168E24u, 0x46u },
    { "africanNationsCupCopy", (void *)africanNationsCupCopy, 0x00168E6Au, 0x30u },
    { "northAmericanCupCopy", (void *)northAmericanCupCopy, 0x00168E9Au, 0x2Eu },
    { "asianCupCopy", (void *)asianCupCopy, 0x00168EC8u, 0x30u },
    { "oceaniaCupCopy", (void *)oceaniaCupCopy, 0x00168EF8u, 0x1Cu },
    { "worldCup2Copy", (void *)worldCup2Copy, 0x00168F14u, 0x70u },
    { "worldCupQualifyingGroup1Copy", (void *)worldCupQualifyingGroup1Copy, 0x00168F84u, 0x28u },
    { "worldCupQualifyingGroup2Copy", (void *)worldCupQualifyingGroup2Copy, 0x00168FACu, 0x28u },
    { "worldCupQualifyingGroup3Copy", (void *)worldCupQualifyingGroup3Copy, 0x00168FD4u, 0x28u },
    { "worldCupQualifyingGroup4Copy", (void *)worldCupQualifyingGroup4Copy, 0x00168FFCu, 0x28u },
    { "worldCupQualifyingGroup5Copy", (void *)worldCupQualifyingGroup5Copy, 0x00169024u, 0x28u },
    { "worldCupQualifyingGroup6Copy", (void *)worldCupQualifyingGroup6Copy, 0x0016904Cu, 0x28u },
    { "worldCupQualifyingGroup7Copy", (void *)worldCupQualifyingGroup7Copy, 0x00169074u, 0x28u },
    { "worldCupQualifyingGroup8Copy", (void *)worldCupQualifyingGroup8Copy, 0x0016909Cu, 0x28u },
    { "northAmericaWorldCupQualificationCopy", (void *)northAmericaWorldCupQualificationCopy, 0x001690C4u, 0x48u },
    { "southAmericaWorldCupQualificationCopy", (void *)southAmericaWorldCupQualificationCopy, 0x0016910Cu, 0x40u },
    { "africaWorldCupQualificationCopy", (void *)africaWorldCupQualificationCopy, 0x0016914Cu, 0x70u },
    { "asiaWorldCupQualificationCopy", (void *)asiaWorldCupQualificationCopy, 0x001691BCu, 0x6Cu },
    { "oceaniaWorldCupQualificationCopy", (void *)oceaniaWorldCupQualificationCopy, 0x00169228u, 0x24u },
    { "competitionFileBuffer", (void *)competitionFileBuffer, 0x0016E084u, 0x2u },
    { "primaryCompStringOffset", (void *)primaryCompStringOffset, 0x0016E0ACu, 0x4u },
};

const size_t g_amiga_addr_map_count =
    sizeof(g_amiga_addr_map) / sizeof(g_amiga_addr_map[0]);
