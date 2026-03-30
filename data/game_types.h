/*
 * data/game_types.h — Competition-related struct definitions for SWOS C port
 *
 * These types are derived from data/SWOS2_IDA.h and converted to use
 * standard C types (uint8_t, int8_t, etc.) instead of IDA-style types.
 *
 * This file lives in data/ and is NOT part of the build yet.
 * When ready, merge into include/game_types.h.
 */
#ifndef DATA_GAME_TYPES_H
#define DATA_GAME_TYPES_H

#include <stdint.h>

/* ======================================================================
 * Country Numbers enum
 * ====================================================================== */

typedef uint8_t CountryNumbers;

#define CN_ALBANIA          0
#define CN_AUSTRIA          1
#define CN_BELGIUM          2
#define CN_BULGARIA         3
#define CN_CROATIA          4
#define CN_CYPRUS           5
#define CN_CZECH_REPUBLIC   6
#define CN_DENMARK          7
#define CN_ENGLAND          8
#define CN_ESTONIA          10
#define CN_FAROE_ISLANDS    11
#define CN_FINLAND          12
#define CN_FRANCE           13
#define CN_GERMANY          14
#define CN_GREECE           15
#define CN_HUNGARY          16
#define CN_ICELAND          17
#define CN_REPUBLIC_IRELAND 18
#define CN_ISRAEL           19
#define CN_ITALY            20
#define CN_LATVIA           21
#define CN_LITHUANIA        22
#define CN_LUXEMBOURG       23
#define CN_MALTA            24
#define CN_HOLLAND          25
#define CN_NORTHERN_IRELAND 26
#define CN_NORWAY           27
#define CN_POLAND           28
#define CN_PORTUGAL         29
#define CN_ROMANIA          30
#define CN_RUSSIA           31
#define CN_SAN_MARINO       32
#define CN_SCOTLAND         33
#define CN_SLOVENIA         34
#define CN_SPAIN            35
#define CN_SWEDEN           36
#define CN_SWITZERLAND      37
#define CN_TURKEY           38
#define CN_UKRAINE          39
#define CN_WALES            40
#define CN_YUGOSLAVIA       41
#define CN_ALGERIA          42
#define CN_ARGENTINA        43
#define CN_AUSTRALIA        44
#define CN_BOLIVIA          45
#define CN_BRAZIL           46
#define CN_CHILE            48
#define CN_COLOMBIA         49
#define CN_ECUADOR          50
#define CN_EL_SALVADOR      51
#define CN_JAPAN            55
#define CN_MEXICO           60
#define CN_NEW_ZEALAND      62
#define CN_PARAGUAY         64
#define CN_PERU             65
#define CN_SURINAM          66
#define CN_TAIWAN           67
#define CN_SOUTH_AFRICA     69
#define CN_URUGUAY          71
#define CN_USA              73
#define CN_INDIA            75
#define CN_BELARUS          76
#define CN_VENEZUELA        77
#define CN_SLOVAKIA         78
#define CN_GHANA            79
#define CN_EUROPE           80
#define CN_AFRICA           81
#define CN_SOUTH_AMERICA    82
#define CN_NORTH_AMERICA    83
#define CN_ASIA             84
#define CN_OCEANIA          85

/* ======================================================================
 * Base Competition structs
 * ====================================================================== */

/* 9 bytes - base competition descriptor */
typedef struct __attribute__((packed)) {
    int8_t compNumber;
    int8_t type;
    int8_t teamNumber;
    int8_t beginMonth;
    int8_t endMonth;
    int8_t unk1;
    int8_t unk2;
    int8_t unk3;
    int8_t unk4;
} Competition;

/* 13 bytes - league competition header */
typedef struct __attribute__((packed)) {
    Competition comp;
    int8_t divisionsCount;
    int8_t rounds;
    int8_t pointsForWin;
    int8_t subsReserves;
} CompetitionLeague;

/* 6 bytes - division descriptor within a league */
typedef struct __attribute__((packed)) {
    int8_t teams;
    int8_t promoted;
    int8_t promotedPo;
    int8_t relegated;
    int8_t relegatedPo;
    int8_t unk1;
} Division;

/* 14 bytes - cup competition header */
typedef struct __attribute__((packed)) {
    Competition comp;
    int8_t unk1;
    int8_t teams;
    int8_t awayGoalsMode;
    int8_t subsReserves;
    int8_t firstRoundNo;
} CompetitionCup;

/* 3 bytes - round descriptor for CompetitionCup2 */
typedef struct __attribute__((packed)) {
    int8_t teamCount;
    int8_t groupCount;
    int8_t teamGroupCount;
} Round;

/* 34 bytes - cup competition with group rounds */
typedef struct __attribute__((packed)) {
    Competition comp;
    int8_t unk1;
    int8_t unk2;
    int8_t unk3;
    int8_t awayGoalsMode;
    int8_t subsReserves;
    int8_t firstRoundNo;
    Round rounds[6];
    int8_t unk4;
} CompetitionCup2;

/* ======================================================================
 * CompetitionLeague variants (with Division arrays)
 * ====================================================================== */

typedef struct __attribute__((packed)) {
    CompetitionLeague comp;
    Division divisions[1];
    int8_t unk1;
} CompetitionLeague_1;

typedef struct __attribute__((packed)) {
    CompetitionLeague comp;
    Division divisions[2];
    int8_t unk1;
} CompetitionLeague_2;

typedef struct __attribute__((packed)) {
    CompetitionLeague comp;
    Division divisions[3];
    int8_t unk1;
} CompetitionLeague_3;

typedef struct __attribute__((packed)) {
    CompetitionLeague comp;
    Division divisions[4];
    int8_t unk1;
} CompetitionLeague_4;

/* ======================================================================
 * CompetitionCup variants (with rounds arrays)
 * ====================================================================== */

typedef struct __attribute__((packed)) {
    CompetitionCup comp;
    int8_t rounds[4];
} CompetitionCup_4;

typedef struct __attribute__((packed)) {
    CompetitionCup comp;
    int8_t rounds[6];
} CompetitionCup_6;

typedef struct __attribute__((packed)) {
    CompetitionCup comp;
    int8_t rounds[10];
} CompetitionCup_10;

/* ======================================================================
 * CompetitionCup2 variants (with roundSettings)
 * ====================================================================== */

typedef struct __attribute__((packed)) {
    CompetitionCup2 compCup;
    int8_t roundSettings[4];
} CompetitionCup2_4;

typedef struct __attribute__((packed)) {
    CompetitionCup2 compCup;
    int8_t roundSettings[6];
} CompetitionCup2_6;

/* ======================================================================
 * CompetitionCup with text pointers
 * ====================================================================== */

typedef struct __attribute__((packed)) {
    CompetitionCup comp;
    int8_t rounds[6];
    char *text[2];
} CompetitionCup_6_2;

typedef struct __attribute__((packed)) {
    CompetitionCup comp;
    int8_t rounds[4];
    char *text[2];
} CompetitionCup_4_2;

/* ======================================================================
 * CompetitionCup with text + trailing data
 * ====================================================================== */

typedef struct __attribute__((packed)) {
    CompetitionCup comp;
    int8_t rounds[6];
    char *text[2];
    int8_t unk1[8];
} CompetitionCup_6_2_8;

typedef struct __attribute__((packed)) {
    CompetitionCup comp;
    int8_t rounds[6];
    char *text[2];
    int8_t unk1[12];
} CompetitionCup_6_2_12;

typedef struct __attribute__((packed)) {
    CompetitionCup comp;
    int8_t rounds[6];
    char *text[2];
    int8_t unk1[64];
} CompetitionCup_6_2_64;

/* ======================================================================
 * CompetitionCup2 with text pointers variants
 * ====================================================================== */

typedef struct __attribute__((packed)) {
    CompetitionCup2_4 compCup;
    char *text[2];
} CompetitionCup2_4_2;

typedef struct __attribute__((packed)) {
    CompetitionCup2_6 compCup;
    char *text[2];
} CompetitionCup2_6_2;

typedef struct __attribute__((packed)) {
    CompetitionCup2_4 compCup;
    char *text[2];
    int8_t unk1[24];
} CompetitionCup2_4_2_24;

typedef struct __attribute__((packed)) {
    CompetitionCup2_4 compCup;
    char *text[2];
    int8_t unk1[32];
} CompetitionCup2_4_2_32;

typedef struct __attribute__((packed)) {
    CompetitionCup2_6 compCup;
    char *text[2];
    int8_t unk1[16];
} CompetitionCup2_6_2_16;

typedef struct __attribute__((packed)) {
    CompetitionCup2_6 compCup;
    char *text[2];
    int8_t unk1[24];
} CompetitionCup2_6_2_24;

typedef struct __attribute__((packed)) {
    CompetitionCup2_6 compCup;
    char *text[2];
    int8_t unk1[48];
} CompetitionCup2_6_2_48;

typedef struct __attribute__((packed)) {
    CompetitionCup2_6 compCup;
    char *text[2];
    int8_t unk1[60];
} CompetitionCup2_6_2_60;

typedef struct __attribute__((packed)) {
    CompetitionCup2_6 compCup;
    char *text[2];
    int8_t unk1[64];
} CompetitionCup2_6_2_64;

/* ======================================================================
 * CompetitionLeague with text pointers variants
 * ====================================================================== */

typedef struct __attribute__((packed)) {
    CompetitionLeague comp;
    Division divisions[3];
    int8_t unk1;
    char *text[6];
} CompetitionLeague_3_6;

typedef struct __attribute__((packed)) {
    CompetitionLeague comp;
    Division divisions[2];
    int8_t unk1;
    char *text[4];
} CompetitionLeague_2_4;

typedef struct __attribute__((packed)) {
    CompetitionLeague comp;
    Division divisions[4];
    int8_t unk1;
    char *text[8];
} CompetitionLeague_4_8;

typedef struct __attribute__((packed)) {
    CompetitionLeague comp;
    Division divisions[4];
    int8_t unk1;
    char *text[10];
    int8_t unk2[24];
} CompetitionLeague_4_10_24;

typedef struct __attribute__((packed)) {
    CompetitionLeague comp;
    Division divisions[2];
    int8_t unk1;
    char *text[2];
    int8_t unk2[24];
} CompetitionLeague_2_2_24;

typedef struct __attribute__((packed)) {
    CompetitionLeague comp;
    Division divisions[4];
    int8_t unk1;
    char *text[2];
    int8_t unk2[8];
} CompetitionLeague_4_2_8;

typedef struct __attribute__((packed)) {
    CompetitionLeague comp;
    Division divisions[2];
    int8_t unk1;
    char *text[4];
    char *unk2[2];
    int8_t unk3[24];
} CompetitionLeague_2_4_2_24;

typedef struct __attribute__((packed)) {
    CompetitionLeague comp;
    Division divisions[2];
    int8_t unk1;
    char *unk2[2];
    int8_t unk3[8];
} CompetitionLeague_2_2_8;

typedef struct __attribute__((packed)) {
    CompetitionLeague comp;
    Division divisions[4];
    int8_t unk1;
    char *text[8];
    char *unk2[2];
    int8_t unk3[24];
} CompetitionLeague_4_8_2_24;

/* ======================================================================
 * Playoff and Leagues helper structs
 * ====================================================================== */

typedef struct __attribute__((packed)) {
    int8_t unk1[16];
    char *text[2];
    int8_t unk2[8];
} Playoffs_8;

typedef struct __attribute__((packed)) {
    int8_t unk1[16];
    char *text[2];
    int8_t unk2[4];
} Playoffs_4;

typedef struct __attribute__((packed)) {
    CompetitionLeague *text1;  /* actually a pointer, named text1 in IDA */
    int8_t unk1[8];
    CompetitionLeague *text2;
    int8_t unk2[8];
    CompetitionLeague *text3;
    int8_t unk3[8];
    CompetitionLeague *text4;
    int8_t unk4[8];
} Leagues_4;

typedef struct __attribute__((packed)) {
    Leagues_4 leagues;
    Playoffs_8 *playoffs;
    Leagues_4 *leagues2;
    int8_t unk2[24];
} Leagues_4_2_24;

typedef struct __attribute__((packed)) {
    CompetitionLeague *compLeague1;
    int8_t unk1[8];
    CompetitionLeague *compLeague2;
    int8_t unk2[8];
} Leagues_2;

/* ======================================================================
 * Career competition entry
 * ====================================================================== */

typedef struct __attribute__((packed)) {
    CompetitionCup *competition;
    CompetitionCup *competitionCopy;
    uint16_t length;
} CompetitionCareer;

/* ======================================================================
 * Qualifying group structs
 * ====================================================================== */

typedef struct __attribute__((packed)) {
    CompetitionCup *compCup[3];
} QualifyingGroup_3;

typedef struct __attribute__((packed)) {
    CompetitionCup *compCup[9];
    CountryNumbers countries[2];
} QualifyingGroup_9_2;

typedef struct __attribute__((packed)) {
    CompetitionCup *compCup[2];
    CountryNumbers countries[8];
} QualifyingGroup_2_8;

typedef struct __attribute__((packed)) {
    CompetitionCup *compCup[6];
    CountryNumbers countries[44];
} QualifyingGroup_6_44;

typedef struct __attribute__((packed)) {
    CompetitionCup *compCup[3];
    CountryNumbers countries[4];
} QualifyingGroup_3_4;

typedef struct __attribute__((packed)) {
    CompetitionCup *compCup[3];
    CountryNumbers countries[12];
} QualifyingGroup_3_12;

/* ======================================================================
 * DIY Stage descriptor
 * ====================================================================== */

typedef struct __attribute__((packed)) {
    uint16_t legs;
    uint16_t extraTime;
    uint16_t penalties;
    uint16_t playCount;
    uint16_t ptsForWin;
} DIYStage;

/* ======================================================================
 * TacticsSlotIndex — player slot indices within a formation
 * ====================================================================== */

typedef uint8_t TacticsSlotIndex;

#define TSI_GOALKEEPER  0x0
#define TSI_1           0x1
#define TSI_2           0x2
#define TSI_3           0x3
#define TSI_4           0x4
#define TSI_5           0x5
#define TSI_6           0x6
#define TSI_7           0x7
#define TSI_8           0x8
#define TSI_9           0x9
#define TSI_10          0xA

/* ======================================================================
 * TacticsPosition — maps 11 player slots for a formation
 * ====================================================================== */

typedef struct __attribute__((packed)) {
    TacticsSlotIndex index[11];
} TacticsPosition;

/* ======================================================================
 * TacticsEnum — formation identifiers
 * ====================================================================== */

#define TACTIC_DEFAULT    0
#define TACTIC_5_4_1      1
#define TACTIC_4_5_1      2
#define TACTIC_5_3_2      3
#define TACTIC_3_5_2      4
#define TACTIC_4_3_3      5
#define TACTIC_4_2_4      6
#define TACTIC_3_4_3      7
#define TACTIC_SWEEP      8
#define TACTIC_5_2_3      9
#define TACTIC_ATTACK     10
#define TACTIC_DEFEND     11
#define TACTIC_USER_A     12
#define TACTIC_USER_B     13
#define TACTIC_USER_C     14
#define TACTIC_USER_D     15
#define TACTIC_USER_E     16
#define TACTIC_USER_F     17
#define TACTIC_IMPORTED   1000

/* ======================================================================
 * Positions enum — player position types
 * ====================================================================== */

#define POS_GOALKEEPER    0x0
#define POS_RIGHT_BACK    0x1
#define POS_LEFT_BACK     0x2
#define POS_DEFENDER      0x3
#define POS_RIGHT_WING    0x4
#define POS_LEFT_WING     0x5
#define POS_MIDFIELDER    0x6
#define POS_ATTACKER      0x7

#endif /* DATA_GAME_TYPES_H */
