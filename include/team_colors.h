#ifndef TEAM_COLORS_H
#define TEAM_COLORS_H

#include "game_types.h"

/* ---- Shirt types ---- */
#define SHIRT_ORDINARY          0
#define SHIRT_COLORED_SLEEVES   1
#define SHIRT_VERTICAL_STRIPES  2
#define SHIRT_HORIZONTAL_STRIPES 3

/* ---- Exported functions ---- */
uint16_t __attribute__((used, externally_visible)) _ConvertTeamColor(uint16_t color);
int16_t  __attribute__((used, externally_visible)) _AreBasicColorsConflicting(uint16_t color1, uint16_t color2);
int16_t  __attribute__((used, externally_visible)) _AreShortsAndStripeColorsConflicting(uint16_t color1, uint16_t color2);
int16_t  __attribute__((used, externally_visible)) _GetTeamKitsSimilarityFactor(
    uint16_t shirtType1, uint8_t stripesColor1, uint8_t basicColor1, uint8_t shortsColor1,
    uint16_t shirtType2, uint8_t stripesColor2, uint8_t basicColor2, uint8_t shortsColor2);
void     __attribute__((used, externally_visible)) _MeasureTeamKitsSimilarity(TeamFile *team1, TeamFile *team2);
void     __attribute__((used, externally_visible)) _ConvertTeamKitColors(
    uint16_t shirtType, uint8_t stripesColor, uint8_t basicColor, uint8_t shortsColor, uint8_t socksColor,
    TeamGame *team);
void     __attribute__((used, externally_visible)) _SetTeamSecondaryColors(
    uint16_t shirtType, uint8_t stripesColor, uint8_t basicColor, uint8_t shortsColor, uint8_t socksColor,
    TeamGame *team);
void     __attribute__((used, externally_visible)) _SetInGameTeamColors(uint16_t secondaryKitFlags, TeamFile *team1File, TeamFile *team2File);
void     __attribute__((used, externally_visible)) _SetInGameTeamsPrimaryColors(TeamFile *team1File, TeamFile *team2File);
uint16_t __attribute__((used, externally_visible)) _ConvertPlayerDressColor(uint16_t color);

#endif
