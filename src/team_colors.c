/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "team_colors.h"
#include "game_types.h"

/* ---- External globals (in SWOS2.s code section) ---- */
extern uint8_t shirtColorTable[];          /* 10-byte color index LUT */
extern uint16_t conflictingBasicTeamColorsTable[];   /* 10 words, bit=conflict */
extern uint16_t conflictingShortsTeamColorsTable[];  /* 10 words, bit=conflict */
extern uint8_t kitsSimilarityLookupTable[];          /* terminated by 0xFF, 3-byte entries */
extern uint8_t shirtType1;                 /* scratch: shirt type of team 1 */
extern uint8_t shirtType2;                 /* scratch: shirt type of team 2 */
extern uint16_t similarityFactor;          /* scratch: similarity score */
extern uint8_t teamKitsSimilarity[];       /* 4-byte result: similarity factors for 4 kit combos */
extern uint8_t teamKitTieBreakColors[];    /* 10 colors + 0xFF terminator */

extern TeamGame topTeamInGame;
extern TeamGame bottomTeamInGame;


/* ============================================================
 * ConvertTeamColor
 * ============================================================
 * Converts a team color index (0..9) to the in-game palette color.
 * Original: ConvertTeamColor in SWOS2.s (line 45843)
 */
uint16_t __attribute__((used, externally_visible))
_ConvertTeamColor(uint16_t color)
{
    return (uint16_t)shirtColorTable[color & 0xFF];
}


/* ============================================================
 * AreBasicColorsConflicting
 * ============================================================
 * Checks if two basic team colors conflict.
 * Returns: non-zero = conflict, zero = OK
 * Original: AreBasicColorsConflicting in SWOS2.s (line 43406)
 */
int16_t __attribute__((used, externally_visible))
_AreBasicColorsConflicting(uint16_t color1, uint16_t color2)
{
    uint16_t mask = conflictingBasicTeamColorsTable[color1 & 0xFF];
    return (mask >> (color2 & 0xFF)) & 1;
}


/* ============================================================
 * AreShortsAndStripeColorsConflicting
 * ============================================================
 * Checks if shorts/stripe colors conflict.
 * Returns: non-zero = conflict, zero = OK
 * Original: AreShortsAndStripeColorsConflicting in SWOS2.s (line 43417)
 */
int16_t __attribute__((used, externally_visible))
_AreShortsAndStripeColorsConflicting(uint16_t color1, uint16_t color2)
{
    uint16_t mask = conflictingShortsTeamColorsTable[color1 & 0xFF];
    return (mask >> (color2 & 0xFF)) & 1;
}


/* ============================================================
 * _GetTeamKitsSimilarityFactor
 * ============================================================
 * Computes how similar two team kits are.
 * Returns similarity factor in range [0..99]:
 *   99 = completely different (early exit)
 *   88/77/66 = special cases for sleeve/stripe combinations
 *   0..~35 = detailed scoring (lower = more different)
 *   <11 triggers lookup-table-based kit replacement loop
 * The result is clamped to >= 0.
 * Original: _GetTeamKitsSimilarityFactor in SWOS2.s (line 43145)
 *
 * Register mapping:
 *   d0 = shirtType1, d1 = stripesColor1, d2 = basicColor1, d3 = shortsColor1
 *   d4 = shirtType2, d5 = stripesColor2, d6 = basicColor2, d7 = shortsColor2(->result)
 */
int16_t __attribute__((used, externally_visible))
_GetTeamKitsSimilarityFactor(
    uint16_t d0_shirtType1, uint8_t d1_stripes1, uint8_t d2_basic1, uint8_t d3_shorts1,
    uint16_t d4_shirtType2, uint8_t d5_stripes2, uint8_t d6_basic2, uint8_t d7_shorts2)
{
    /* Local copies matching the register variables in asm */
    uint8_t st1, st2;    /* shirt types (byte) */
    uint8_t d1, d2, d3;  /* team1: stripes, basic, shorts */
    uint8_t d5, d6, d7;  /* team2: stripes, basic, shorts */
    int16_t sim;

restart:
    st1 = (uint8_t)d0_shirtType1;
    st2 = (uint8_t)d4_shirtType2;
    d1 = d1_stripes1;
    d2 = d2_basic1;
    d3 = d3_shorts1;
    d5 = d5_stripes2;
    d6 = d6_basic2;
    d7 = d7_shorts2;

    shirtType1 = st1;
    shirtType2 = st2;

    /* If ORDINARY, copy stripes color over basic (they're the same) */
    if (st1 == SHIRT_ORDINARY)
        d2 = d1;
    if (st2 == SHIRT_ORDINARY)
        d6 = d5;

    /* ---- Phase 1: quick conflict checks -> return 99 if not conflicting ---- */
    similarityFactor = 99;

    /* Test d1 vs d5 (stripes1 vs stripes2) */
    if (_AreBasicColorsConflicting(d1, d5)) {
        /* also test d2 vs d6 (basic1 vs basic2) */
        if (_AreBasicColorsConflicting(d2, d6))
            goto done;  /* both basic pairs conflict -> kits are different enough */
    }

    /* Test d1 vs d6 (stripes1 vs basic2) */
    if (_AreBasicColorsConflicting(d1, d6)) {
        /* also test d2 vs d5 (basic1 vs stripes2) */
        if (_AreBasicColorsConflicting(d2, d5))
            goto done;
    }

    /* ---- Phase 2: sleeve/ordinary cross-checks ---- */
    /* If one team has COLORED_SLEEVES and the other ORDINARY (or vice-versa):
     * check shorts-vs-stripe combinations */
    if ((st1 == SHIRT_COLORED_SLEEVES && st2 == SHIRT_ORDINARY) ||
        (st1 == SHIRT_ORDINARY && st2 == SHIRT_COLORED_SLEEVES)) {
        if (_AreShortsAndStripeColorsConflicting(d1, d7) &&
            _AreShortsAndStripeColorsConflicting(d3, d5)) {
            similarityFactor = 88;
            goto done;
        }
    }

    /* ---- Phase 3: both teams have stripes (vertical/horizontal) ---- */
    if ((st1 == SHIRT_VERTICAL_STRIPES || st1 == SHIRT_HORIZONTAL_STRIPES) &&
        (st2 == SHIRT_VERTICAL_STRIPES || st2 == SHIRT_HORIZONTAL_STRIPES)) {
        /* test shorts1 vs stripes2 OR shorts1 vs basic2 */
        if (_AreBasicColorsConflicting(d3, d5) || _AreBasicColorsConflicting(d3, d6)) {
            /* test shorts2 vs stripes1 OR shorts2 vs basic1 */
            if (_AreBasicColorsConflicting(d7, d1) || _AreBasicColorsConflicting(d7, d2)) {
                similarityFactor = 77;
                goto done;
            }
        }
    }

    /* ---- Phase 4: both teams have COLORED_SLEEVES ---- */
    if (st1 == SHIRT_COLORED_SLEEVES && st2 == SHIRT_COLORED_SLEEVES) {
        if (_AreBasicColorsConflicting(d1, d5)) {
            similarityFactor = 66;
            goto done;
        }
    }

    /* ---- Phase 5: detailed scoring ---- */
    similarityFactor = 0;

    /* Different shirt types: -4 */
    if (st1 != st2)
        similarityFactor -= 4;

    /* Team1 ordinary, extra bonus logic */
    if (st1 == SHIRT_ORDINARY) {
        similarityFactor -= 2;
        if (st2 == SHIRT_COLORED_SLEEVES) {
            if (d1 == d2 && d1 == d3)
                similarityFactor -= 6;
        }
    }

    /* Team2 ordinary, extra bonus logic */
    if (st2 == SHIRT_ORDINARY) {
        similarityFactor -= 2;
        if (st1 == SHIRT_COLORED_SLEEVES) {
            if (d5 == d6 && d5 == d7)
                similarityFactor -= 6;
        }
    }

    /* Ordinary/colored_sleeves cross: +1 */
    if ((st1 == SHIRT_ORDINARY && st2 == SHIRT_COLORED_SLEEVES) ||
        (st1 == SHIRT_COLORED_SLEEVES && st2 == SHIRT_ORDINARY)) {
        similarityFactor += 1;
    }

    /* Cross-product: compare all color pairs */
    /* d1 vs d5 */
    if (_AreBasicColorsConflicting(d1, d5))
        similarityFactor += 5;
    /* d1 vs d6 */
    if (_AreBasicColorsConflicting(d1, d6))
        similarityFactor += 5;
    /* d1 vs d7 */
    if (_AreBasicColorsConflicting(d1, d7))
        similarityFactor += 2;
    /* d2 vs d5 */
    if (_AreBasicColorsConflicting(d2, d5))
        similarityFactor += 5;
    /* d2 vs d6 */
    if (_AreBasicColorsConflicting(d2, d6))
        similarityFactor += 5;
    /* d2 vs d7 */
    if (_AreBasicColorsConflicting(d2, d7))
        similarityFactor += 2;
    /* d3 vs d5 */
    if (_AreBasicColorsConflicting(d3, d5))
        similarityFactor += 2;
    /* d3 vs d6 */
    if (_AreBasicColorsConflicting(d3, d6))
        similarityFactor += 2;
    /* d3 vs d7 */
    if (_AreBasicColorsConflicting(d3, d7))
        similarityFactor += 5;

done:
    /* ---- Phase 6: if still too similar (<11), try lookup table replacements ---- */
    if ((int16_t)similarityFactor < 11) {
        uint8_t *tbl = kitsSimilarityLookupTable;
        uint16_t replaced1 = 0;
        uint16_t replaced2 = 0;

        while (*tbl != 0xFF) {
            /* Try replacing team1 stripes if striped */
            if (!replaced1) {
                if (st1 == SHIRT_VERTICAL_STRIPES || st1 == SHIRT_HORIZONTAL_STRIPES) {
                    if ((d1_stripes1 == tbl[0] && d2_basic1 == tbl[1]) ||
                        (d1_stripes1 == tbl[1] && d2_basic1 == tbl[0])) {
                        replaced1 = 1;
                        d0_shirtType1 = SHIRT_ORDINARY;
                        d1_stripes1 = tbl[2];
                        d2_basic1 = tbl[2];
                    }
                }
            }

            /* Try replacing team2 stripes if striped */
            if (!replaced2) {
                if (st2 == SHIRT_VERTICAL_STRIPES || st2 == SHIRT_HORIZONTAL_STRIPES) {
                    if ((d5_stripes2 == tbl[0] && d6_basic2 == tbl[1]) ||
                        (d5_stripes2 == tbl[1] && d6_basic2 == tbl[0])) {
                        replaced2 = 1;
                        d4_shirtType2 = SHIRT_ORDINARY;
                        d5_stripes2 = tbl[2];
                        d6_basic2 = tbl[2];
                    }
                }
            }

            tbl += 3;
        }

        /* If a replacement was found, restart the whole comparison */
        if (replaced1 || replaced2)
            goto restart;
    }

    /* Clamp result to >= 0 */
    sim = (int16_t)similarityFactor;
    if (sim < 0)
        sim = 0;
    return sim;
}


/* ============================================================
 * MeasureTeamKitsSimilarity
 * ============================================================
 * Measures all 4 combinations of primary/secondary kits between
 * two teams and stores results in teamKitsSimilarity[4].
 * Returns pointer to teamKitsSimilarity (but C callers don't need it).
 * Original: MeasureTeamKitsSimilarity in SWOS2.s (line 43569)
 */
void __attribute__((used, externally_visible))
_MeasureTeamKitsSimilarity(TeamFile *team1, TeamFile *team2)
{
    /* team1 primary vs team2 primary */
    teamKitsSimilarity[0] = (uint8_t)_GetTeamKitsSimilarityFactor(
        team1->prShirtType,
        team1->prStripesColor,
        team1->prBasicColor,
        team1->prShortsColor,
        team2->prShirtType,
        team2->prStripesColor,
        team2->prBasicColor,
        team2->prShortsColor);

    /* team1 primary vs team2 secondary */
    teamKitsSimilarity[1] = (uint8_t)_GetTeamKitsSimilarityFactor(
        team1->prShirtType,
        team1->prStripesColor,
        team1->prBasicColor,
        team1->prShortsColor,
        team2->secShirtType,
        team2->secStripesColor,
        team2->secBasicColor,
        team2->secShortsColor);

    /* team1 secondary vs team2 primary */
    teamKitsSimilarity[2] = (uint8_t)_GetTeamKitsSimilarityFactor(
        team1->secShirtType,
        team1->secStripesColor,
        team1->secBasicColor,
        team1->secShortsColor,
        team2->prShirtType,
        team2->prStripesColor,
        team2->prBasicColor,
        team2->prShortsColor);

    /* team1 secondary vs team2 secondary */
    teamKitsSimilarity[3] = (uint8_t)_GetTeamKitsSimilarityFactor(
        team1->secShirtType,
        team1->secStripesColor,
        team1->secBasicColor,
        team1->secShortsColor,
        team2->secShirtType,
        team2->secStripesColor,
        team2->secBasicColor,
        team2->secShortsColor);
}


/* ============================================================
 * ConvertTeamColor + ConvertTeamKitColors
 * ============================================================
 * Writes converted kit colors into the in-game team struct.
 * For ORDINARY shirt type, stripes color == basic color.
 * Original: ConvertTeamKitColors in SWOS2.s (line 45790)
 */
void __attribute__((used, externally_visible))
_ConvertTeamKitColors(
    uint16_t shirtType, uint8_t stripesColor, uint8_t basicColor,
    uint8_t shortsColor, uint8_t socksColor, TeamGame *team)
{
    int16_t st = (int16_t)(int8_t)(uint8_t)shirtType;  /* ext.w d0 (sign-extend byte to word) */
    team->prShirtType = st;

    uint16_t col = _ConvertTeamColor(stripesColor);
    team->prShirtCol = col;

    if (st != SHIRT_ORDINARY) {
        col = _ConvertTeamColor(basicColor);
    }
    team->prStripesCol = col;

    team->prShortsCol = _ConvertTeamColor(shortsColor);
    team->prSocksCol  = _ConvertTeamColor(socksColor);
}


/* ============================================================
 * SetTeamSecondaryColors
 * ============================================================
 * Writes converted secondary kit colors into the in-game team struct.
 * Original: SetTeamSecondaryColors in SWOS2.s (line 45819)
 */
void __attribute__((used, externally_visible))
_SetTeamSecondaryColors(
    uint16_t shirtType, uint8_t stripesColor, uint8_t basicColor,
    uint8_t shortsColor, uint8_t socksColor, TeamGame *team)
{
    int16_t st = (int16_t)(int8_t)(uint8_t)shirtType;  /* ext.w d0 */
    team->secShirtType = st;

    uint16_t col = _ConvertTeamColor(stripesColor);
    team->secShirtCol = col;

    if (team->secShirtType != SHIRT_ORDINARY) {
        col = _ConvertTeamColor(basicColor);
    }
    team->secStripesCol = col;

    team->secShortsCol = _ConvertTeamColor(shortsColor);
    team->secSocksCol  = _ConvertTeamColor(socksColor);
}


/* ============================================================
 * IsSingleColorKitSimilar (static)
 * ============================================================
 * Tests whether a single-color kit (ORDINARY with all same color)
 * is similar to team2's primary kit.
 * Returns: 1 if NOT similar (carry set = not similar in asm), 0 if similar.
 * Original: IsSingleColorKitSimilar in SWOS2.s (line 45646)
 */
static int16_t
isSingleColorKitSimilar(uint8_t color, TeamFile *team2File)
{
    uint8_t d4 = team2File->prShirtType;
    uint8_t d5 = team2File->prStripesColor;
    uint8_t d6 = team2File->prBasicColor;
    uint8_t d7 = team2File->prShortsColor;

    int16_t sim = _GetTeamKitsSimilarityFactor(
        SHIRT_ORDINARY, color, color, color,
        d4, d5, d6, d7);

    /* asm: cmpi.b #11,d7; carry set if d7 < 11 -> kits are different enough
     * carry set -> bcs branches -> use this color
     * Return 1 when kits are different (sim < 11), 0 when too similar */
    return sim < 11;
}


/* ============================================================
 * ConvertTeamKitColorsWithFixedColors (static)
 * ============================================================
 * Sets bottom team to a fixed single-color kit and top team
 * to its primary kit.
 * Original: ConvertTeamKitColorsWithFixedColors in SWOS2.s (line 45668)
 */
static void
convertTeamKitColorsWithFixedColors(uint8_t shirtType, uint8_t color, uint8_t basicColor,
                                     uint8_t shortsColor, TeamFile *topTeamFile)
{
    /* Bottom team gets the single-color kit (d4=socksColor=d1=color) */
    _ConvertTeamKitColors(shirtType, color, basicColor, shortsColor, color, &bottomTeamInGame);

    /* Top team gets its primary colors */
    _ConvertTeamKitColors(
        topTeamFile->prShirtType,
        topTeamFile->prStripesColor,
        topTeamFile->prBasicColor,
        topTeamFile->prShortsColor,
        topTeamFile->prSocksColor,
        &topTeamInGame);
}


/* ============================================================
 * ResolveTeamKitColorConflict (static)
 * ============================================================
 * When no primary/secondary combination works, try a series of
 * tie-break single colors from teamKitTieBreakColors[].
 * If nothing works, fallback: top=red(4), bottom=blue(5).
 * Original: ResolveTeamKitColorConflict in SWOS2.s (line 45685)
 */
static void
resolveTeamKitColorConflict(TeamFile *topTeamFile)
{
    uint8_t *tbl = teamKitTieBreakColors;

    while (*tbl != 0xFF) {
        uint8_t color = *tbl++;
        int16_t sim = _GetTeamKitsSimilarityFactor(
            SHIRT_ORDINARY,
            color, color, color,
            topTeamFile->prShirtType,
            topTeamFile->prStripesColor,
            topTeamFile->prBasicColor,
            topTeamFile->prShortsColor);

        if (sim < 11) {
            /* Found a non-conflicting color */
            _ConvertTeamKitColors(SHIRT_ORDINARY, color, color, color, color, &bottomTeamInGame);
            _ConvertTeamKitColors(
                topTeamFile->prShirtType,
                topTeamFile->prStripesColor,
                topTeamFile->prBasicColor,
                topTeamFile->prShortsColor,
                topTeamFile->prSocksColor,
                &topTeamInGame);
            return;
        }
    }

    /* Fallback: top = red(4), bottom = blue(5) */
    _ConvertTeamKitColors(SHIRT_ORDINARY, 4, 4, 1, 4, &topTeamInGame);
    _ConvertTeamKitColors(SHIRT_ORDINARY, 5, 5, 1, 5, &bottomTeamInGame);
}


/* ============================================================
 * SetInGameTeamColors
 * ============================================================
 * Sets both teams' in-game colors based on secondary kit flags.
 *   d7 bit 0 = use secondary for bottom team (team2/a3)
 *   d7 bit 1 = use secondary for top team (team1/a2)
 * Original: SetInGameTeamColors in SWOS2.s (line 45739)
 *
 * Note: the bit assignments look swapped (bit 1 for top, bit 0 for bottom)
 * because the asm uses a2=team1(top), a3=team2(bottom), and the
 * secondary kit flags were set from MeasureTeamKitsSimilarity results:
 *   result[0]=pp, result[1]=ps, result[2]=sp, result[3]=ss
 *   d7=0: both primary
 *   d7=1: top secondary, bottom primary -> bit 0 for bottom
 *   d7=2: top primary, bottom secondary -> bit 1 for top
 *   d7=3: both secondary
 * Actually re-reading asm: bit 1 tested first for a2(top), bit 0 for a3(bottom).
 */
void __attribute__((used, externally_visible))
_SetInGameTeamColors(uint16_t secondaryKitFlags, TeamFile *team1File, TeamFile *team2File)
{
    uint8_t d0, d1_s, d2_s, d3_s, d4_s;

    /* Top team (a2 = team1File) */
    if (secondaryKitFlags & 2) {
        /* Use secondary kit for top team */
        d0 = team1File->secShirtType;
        d1_s = team1File->secStripesColor;
        d2_s = team1File->secBasicColor;
        d3_s = team1File->secShortsColor;
        d4_s = team1File->secSocksColor;
    } else {
        d0 = team1File->prShirtType;
        d1_s = team1File->prStripesColor;
        d2_s = team1File->prBasicColor;
        d3_s = team1File->prShortsColor;
        d4_s = team1File->prSocksColor;
    }
    _ConvertTeamKitColors(d0, d1_s, d2_s, d3_s, d4_s, &topTeamInGame);

    /* Bottom team (a3 = team2File) */
    if (secondaryKitFlags & 1) {
        d0 = team2File->secShirtType;
        d1_s = team2File->secStripesColor;
        d2_s = team2File->secBasicColor;
        d3_s = team2File->secShortsColor;
        d4_s = team2File->secSocksColor;
    } else {
        d0 = team2File->prShirtType;
        d1_s = team2File->prStripesColor;
        d2_s = team2File->prBasicColor;
        d3_s = team2File->prShortsColor;
        d4_s = team2File->prSocksColor;
    }
    _ConvertTeamKitColors(d0, d1_s, d2_s, d3_s, d4_s, &bottomTeamInGame);
}


/* ============================================================
 * SetInGameTeamsPrimaryColors
 * ============================================================
 * Master color resolution function. Measures all 4 kit combinations,
 * picks the best non-conflicting one. If none works, tries single-color
 * alternatives or tie-break table.
 * Original: SetInGameTeamsPrimaryColors in SWOS2.s (line 45595)
 *
 * Flow:
 *   1. Measure 4 kit combinations -> teamKitsSimilarity[0..3]
 *   2. Check each combo (pp, ps, sp, ss) -- first with similarity < 11 wins
 *   3. If none, try single-color alternatives for the bottom team
 *   4. If still nothing, use ResolveTeamKitColorConflict (tie-break table)
 */
void __attribute__((used, externally_visible))
_SetInGameTeamsPrimaryColors(TeamFile *team1File, TeamFile *team2File)
{
    /* a2=team1File(top), a3=team2File(bottom) -- matching asm register assignment */
    _MeasureTeamKitsSimilarity(team1File, team2File);

    /* a6 points into teamKitsSimilarity, incremented after each check */
    /* Check combo 0: both primary (d7=0) */
    if (teamKitsSimilarity[0] < 11) {
        _SetInGameTeamColors(0, team1File, team2File);
        return;
    }
    /* Check combo 1: team1 primary, team2 secondary (d7=1) */
    if (teamKitsSimilarity[1] < 11) {
        _SetInGameTeamColors(1, team1File, team2File);
        return;
    }
    /* Check combo 2: team1 secondary, team2 primary (d7=2) */
    if (teamKitsSimilarity[2] < 11) {
        _SetInGameTeamColors(2, team1File, team2File);
        return;
    }
    /* Check combo 3: both secondary (d7=3) */
    if (teamKitsSimilarity[3] < 11) {
        _SetInGameTeamColors(3, team1File, team2File);
        return;
    }

    /* No combo works -- try single-color alternatives for bottom team (a3) */
    /* Try bottom team's primary stripes color */
    if (isSingleColorKitSimilar(team2File->prStripesColor, team1File)) {
        convertTeamKitColorsWithFixedColors(
            SHIRT_ORDINARY,
            team2File->prStripesColor,
            team2File->prStripesColor,
            team2File->prStripesColor,
            team1File);
        return;
    }

    /* Try bottom team's primary basic color (only if stripes != 0, i.e. not already tried) */
    if (team2File->prStripesColor != 0) {
        if (isSingleColorKitSimilar(team2File->prBasicColor, team1File)) {
            convertTeamKitColorsWithFixedColors(
                SHIRT_ORDINARY,
                team2File->prBasicColor,
                team2File->prBasicColor,
                team2File->prBasicColor,
                team1File);
            return;
        }
    }

    /* Try bottom team's primary shorts color */
    if (isSingleColorKitSimilar(team2File->prShortsColor, team1File)) {
        convertTeamKitColorsWithFixedColors(
            SHIRT_ORDINARY,
            team2File->prShortsColor,
            team2File->prShortsColor,
            team2File->prShortsColor,
            team1File);
        return;
    }

    /* Try bottom team's secondary stripes color */
    if (isSingleColorKitSimilar(team2File->secStripesColor, team1File)) {
        convertTeamKitColorsWithFixedColors(
            SHIRT_ORDINARY,
            team2File->secStripesColor,
            team2File->secStripesColor,
            team2File->secStripesColor,
            team1File);
        return;
    }

    /* Try bottom team's secondary basic color (only if sec stripes != 0) */
    if (team2File->secStripesColor != 0) {
        if (isSingleColorKitSimilar(team2File->secBasicColor, team1File)) {
            convertTeamKitColorsWithFixedColors(
                SHIRT_ORDINARY,
                team2File->secBasicColor,
                team2File->secBasicColor,
                team2File->secBasicColor,
                team1File);
            return;
        }
    }

    /* Try bottom team's secondary shorts color */
    if (isSingleColorKitSimilar(team2File->secShortsColor, team1File)) {
        convertTeamKitColorsWithFixedColors(
            SHIRT_ORDINARY,
            team2File->secShortsColor,
            team2File->secShortsColor,
            team2File->secShortsColor,
            team1File);
        return;
    }

    /* Last resort: tie-break table */
    resolveTeamKitColorConflict(team1File);
}


/* ============================================================
 * ConvertPlayerDressColor
 * ============================================================
 * Converts a player dress color byte via shirtColorTable lookup.
 * Same as ConvertTeamColor but uses ext.w (sign-extend byte).
 * Original: ConvertPlayerDressColor in SWOS2.s (line 88608)
 */
uint16_t __attribute__((used, externally_visible))
_ConvertPlayerDressColor(uint16_t color)
{
    /* ext.w d0: sign-extend low byte to word */
    int16_t idx = (int16_t)(int8_t)(uint8_t)color;
    return (uint16_t)(shirtColorTable[idx] & 0xFF);
}
