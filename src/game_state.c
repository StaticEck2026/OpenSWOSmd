/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "game_state.h"

/* ---- External globals ---- */
extern uint16_t isGoalAttempt;
extern uint16_t showStats;
extern uint16_t overlayActiveFlag;
extern uint16_t statsTimer;
extern int16_t  resultTimer;
extern uint16_t showingUserStats;

extern uint16_t gameSeconds;
extern uint32_t gameTime;               /* BCD-encoded: byte3=ones, byte2=tens, byte1=hundreds */
extern uint16_t secondsSwitchAccumulator;
extern uint16_t copyTimeSprite;
extern Sprite   currentTimeSprite;

extern TeamGame topTeamInGame;
extern TeamGame bottomTeamInGame;

extern uint8_t  plOrdinalsTable[];      /* 32 bytes: indices 0-15 at offset 0, duplicated at offset 16 */
extern uint8_t  plNamesLengthTable[];   /* 32 bytes: 16 entries x 2 bytes */
extern uint8_t  plNamesLengthTableCopy[];

extern Sprite   goal1TopSprite;
extern Sprite   goal2BottomSprite;

/* ---- Result display sprite globals ---- */
extern Sprite   resultOnHalftimeSprite;
extern Sprite   resultAfterTheGameSprite;
extern Sprite   gridUpperRowLeftSprite;
extern Sprite   gridUpperRowRightSprite;
extern Sprite   gridMiddleRowLeftSprite;
extern Sprite   gridMiddleRowRightSprite;
extern Sprite   gridBottomRowLeftSprite;
extern Sprite   gridBottomRowRightSprite;
extern Sprite   team1NameSprite;
extern Sprite   team2NameSprite;
extern Sprite   team1GoalsDigit2Sprite;
extern Sprite   team1GoalsDigit1Sprite;
extern Sprite   team2GoalsDigit2Sprite;
extern Sprite   team2GoalsDigit1Sprite;
extern Sprite   team1TotalGoalsDigit2Sprite;
extern Sprite   team1TotalGoalsDigit1Sprite;
extern Sprite   team2TotalGoalsDigit2Sprite;
extern Sprite   team2TotalGoalsDigit1Sprite;
extern Sprite   dashSprite;
extern Sprite   team1ScorerSprite[8];
extern Sprite   team2ScorerSprite[8];

extern uint16_t team1ScorerSpritesWidths[];  /* 8 entries */
extern uint16_t team2ScorerSpritesWidths[];  /* 8 entries */
extern uint16_t team1TotalGoals;
extern uint16_t team2TotalGoals;
extern uint16_t team1GoalsDigit2;
extern uint16_t team2GoalsDigit2;
extern uint16_t team1GoalsDigit1;
extern uint16_t team2GoalsDigit1;
extern uint16_t team1NameLength;
extern uint16_t dontShowScorers;
extern uint16_t secondLeg;

extern int16_t  cameraX;
extern int16_t  cameraY;
extern uint16_t lastFrameTicks;
extern uint16_t gameState;

extern ScorerInfo team1Scorers[8];
extern ScorerInfo team2Scorers[8];

/* ---- UpdateTime globals ---- */
extern uint16_t timeVar;
extern uint16_t timeDelta;
extern uint16_t gameStatePl;
extern uint16_t playingPenalties;
extern int16_t  extraTimeState;
extern int16_t  penaltiesState;
extern uint16_t endGameCounter;
extern uint16_t playing2ndGame;
extern uint16_t team1GoalsFirstLeg;
extern uint16_t team2GoalsFirstLeg;
extern TeamGame *winningTeamPtr;         /* pointer to winning team */
extern uint16_t statsTeam1Goals;
extern uint16_t statsTeam2Goals;
extern uint16_t statsTeam1GoalsCopy;
extern uint16_t statsTeam2GoalsCopy;
extern TeamGeneralInfo *lastTeamPlayed;
extern Sprite   ballSprite;
extern uint16_t currentPlayerIndicatorPosition;
extern TeamGeneralInfo topTeamData;
extern TeamGeneralInfo bottomTeamData;

/* C-to-asm bridges */
extern void _InitDisplaySprites(void);

/* Match state transitions (now in match_setup.c) */
extern void _EndFirstHalf(void);
extern void _EndFirstExtraTime(void);
extern void _EndOfGame(void);
extern void _StartFirstExtraTime(void);
extern void _StartPenalties(void);

/* Already-ported functions used by UpdateTime */
extern void _PlayEndGameWhistleSample(void);
extern void _CopySprite(uint16_t srcIdx, uint16_t dstIdx, uint16_t xOff, uint16_t yOff);
extern void _BlitSpritePlanar_Long_StridedDest(uint16_t sprIdx);
extern void _BlitSpritePlanar_Word_StridedDest(uint16_t sprIdx);

/* ---- Sprite image index constants (NOT struct offsets) ---- */
#define SPR_BIG_0               173
#define SPR_SMALL_ZERO          8
#define SPR_SCROLL_ARROW_UP     183
#define SPR_8_MINS              214

#define SPR_LITTLE_PLAYER_GENERIC    214
#define SPR_PREMIERE_DIVISION_WINNER 215
#define SPR_INVITATION_TOURNAMENT    216
#define SPR_CUP_WINNER               217

/* ---- Game state constants ---- */
#define ST_GAME_IN_PROGRESS         100
#define ST_RESULT_ON_HALFTIME       25
#define ST_RESULT_AFTER_THE_GAME    26

/* gameTime byte accessors (big-endian 68k) */
#define GAMETIME_ONES     (((uint8_t *)&gameTime)[3])
#define GAMETIME_TENS     (((uint8_t *)&gameTime)[2])
#define GAMETIME_HUNDREDS (((uint8_t *)&gameTime)[1])

/* ================================================================
 * ResetGoalAttempt  (SWOS2.s line 18697)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ResetGoalAttempt(void)
{
    isGoalAttempt = 0;
}

/* ================================================================
 * AllowStatistics  (SWOS2.s line 18703)
 *
 * Enables statistics overlay display.
 * Negates resultTimer to make any visible result go away.
 * ================================================================ */
__attribute__((used, externally_visible))
void _AllowStatistics(void)
{
    showStats = 1;
    overlayActiveFlag = 1;
    statsTimer = 32000;
    resultTimer = (int16_t)-resultTimer;    /* neg.w */
}

/* ================================================================
 * StatisticsOff  (SWOS2.s line 18712)
 * ================================================================ */
__attribute__((used, externally_visible))
void _StatisticsOff(void)
{
    statsTimer = 0;
    showingUserStats = 0;
    overlayActiveFlag = 0;
}

/* ================================================================
 * ResetTime  (SWOS2.s line 13398)
 *
 * Clears game clock and sets the time sprite to the initial state.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ResetTime(void)
{
    gameSeconds = 0;
    gameTime = 0;
    secondsSwitchAccumulator = 0;
    copyTimeSprite = 1;
    currentTimeSprite.imageIndex = SPR_8_MINS;
    currentTimeSprite.visible = 0;
}

/* ================================================================
 * BumpPlayerLastPlayedHalfAtHalfStart  (SWOS2.s line 13685)
 *
 * Per-player: if not red-carded and not already at 2, set to 1.
 * ================================================================ */
static void bumpPlayerHalfStart(PlayerInfo *player)
{
    if ((uint8_t)player->numYellowCards >= 2)
        return;
    if (player->halfPlayed == 2)
        return;
    player->halfPlayed = 1;
}

/* ================================================================
 * BumpPlayersLastPlayedHalfAtHalfStart  (SWOS2.s line 13667)
 *
 * Iterates both teams' 11 players, marking half-start.
 * ================================================================ */
__attribute__((used, externally_visible))
void _BumpPlayersLastPlayedHalfAtHalfStart(void)
{
    int16_t i;

    for (i = 0; i < 11; i++)
    {
        bumpPlayerHalfStart(&topTeamInGame.players[i]);
        bumpPlayerHalfStart(&bottomTeamInGame.players[i]);
    }
}

/* ================================================================
 * BumpPlayerLastPlayedHalfAtHalfEnd  (SWOS2.s line 13714)
 *
 * Per-player: if not red-carded and half-played is 1, set to 2.
 * ================================================================ */
static void bumpPlayerHalfEnd(PlayerInfo *player)
{
    if ((uint8_t)player->numYellowCards >= 2)
        return;
    if (player->halfPlayed != 1)
        return;
    player->halfPlayed = 2;
}

/* ================================================================
 * BumpPlayersLastPlayedHalfAtHalfEnd  (SWOS2.s line 13696)
 *
 * Iterates both teams' 11 players, marking half-end.
 * ================================================================ */
__attribute__((used, externally_visible))
void _BumpPlayersLastPlayedHalfAtHalfEnd(void)
{
    int16_t i;

    for (i = 0; i < 11; i++)
    {
        bumpPlayerHalfEnd(&topTeamInGame.players[i]);
        bumpPlayerHalfEnd(&bottomTeamInGame.players[i]);
    }
}

/* ================================================================
 * CopyPlayerNameLengths  (SWOS2.s line 12308)
 *
 * Copies 16 entries (2 bytes each) from plNamesLengthTable to
 * plNamesLengthTableCopy. Also initializes plOrdinalsTable with
 * sequential indices 0-15 at both offset 0 and offset 16.
 * ================================================================ */
__attribute__((used, externally_visible))
void _CopyPlayerNameLengths(void)
{
    uint16_t d0;

    for (d0 = 0; d0 < 16; d0++)
    {
        plOrdinalsTable[d0 + 16] = (uint8_t)d0;
        plOrdinalsTable[d0] = (uint8_t)d0;
        plNamesLengthTableCopy[d0 * 2]     = plNamesLengthTable[d0 * 2];
        plNamesLengthTableCopy[d0 * 2 + 1] = plNamesLengthTable[d0 * 2 + 1];
    }
}

/* ================================================================
 * ClearSaveFlagForGoalSprites  (SWOS2.s line 13375)
 *
 * Sets saveSprite=1 for goal1TopSprite and goal2BottomSprite.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ClearSaveFlagForGoalSprites(void)
{
    goal1TopSprite.saveSprite = 1;
    goal2BottomSprite.saveSprite = 1;
}

/* ================================================================
 * InitCurrentGoalHighlight  (SWOS2.s line 13290)
 *
 * Clears 19000 bytes at goalBasePtr, resets currentHilPtr,
 * clears replayState, sets throwInDeadVar=1.
 * ================================================================ */
extern uint8_t *goalBasePtr;
extern uint8_t *currentHilPtr;
extern uint16_t replayState;
extern uint16_t throwInDeadVar;

__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _InitCurrentGoalHighlight(void)
{
    uint32_t *p = (uint32_t *)goalBasePtr;
    int16_t i;
    for (i = 4749; i >= 0; i--)
        *p++ = 0;
    currentHilPtr = goalBasePtr;
    replayState = 0;
    throwInDeadVar = 1;
}

/* ================================================================
 * ResetResultSprites  (SWOS2.s line 13700)
 *
 * Hides all result/score display sprites and clears scorer data.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ResetResultSprites(void)
{
    int16_t i;
    Sprite *s1, *s2;
    ScorerInfo *sc1, *sc2;
    uint16_t *w1, *w2;

    resultOnHalftimeSprite.visible = 0;
    resultAfterTheGameSprite.visible = 0;

    team1NameSprite.imageIndex = 0xC1;
    team2NameSprite.imageIndex = 0xC2;

    gridUpperRowLeftSprite.visible = 0;
    gridUpperRowRightSprite.visible = 0;
    gridMiddleRowLeftSprite.visible = 0;
    gridMiddleRowRightSprite.visible = 0;
    gridBottomRowLeftSprite.visible = 0;
    gridBottomRowRightSprite.visible = 0;
    team1NameSprite.visible = 0;
    team2NameSprite.visible = 0;
    team1GoalsDigit2Sprite.visible = 0;
    team1GoalsDigit1Sprite.visible = 0;
    team1TotalGoalsDigit2Sprite.visible = 0;
    team1TotalGoalsDigit1Sprite.visible = 0;
    team2GoalsDigit2Sprite.visible = 0;
    team2GoalsDigit1Sprite.visible = 0;
    team2TotalGoalsDigit2Sprite.visible = 0;
    team2TotalGoalsDigit1Sprite.visible = 0;
    team1NameSprite.visible = 0;    /* redundant in original asm */
    dashSprite.visible = 0;

    s1 = team1ScorerSprite;
    s2 = team2ScorerSprite;
    sc1 = team1Scorers;
    sc2 = team2Scorers;
    w1 = team1ScorerSpritesWidths;
    w2 = team2ScorerSpritesWidths;

    for (i = 7; i >= 0; i--)
    {
        s1->visible = 0;
        s2->visible = 0;
        sc1->shirtNum = 0;
        sc2->shirtNum = 0;
        *w1++ = 0;
        *w2++ = 0;
        s1++;
        s2++;
        sc1++;
        sc2++;
    }

    _InitDisplaySprites();
}

/* ================================================================
 * ShowResult  (SWOS2.s line 13749)
 *
 * Timer-driven result display. Shows/hides score sprites,
 * positions them based on camera coordinates, updates per-frame.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ShowResult(void)
{
    int16_t d0, d1, d2, d7;
    Sprite *s;

    /* resultTimer == 0 -> nothing to do */
    if (resultTimer == 0)
        return;

    /* resultTimer < 0 -> jump to hide phase */
    if (resultTimer < 0)
        goto hide_result;

    /* --- timer init phase: map magic values to countdown values --- */
    if (resultTimer == 30000)
    {
        resultTimer = 250;
        goto show_sprites;
    }
    if (resultTimer == 31000)
    {
        resultTimer = 150;
        goto show_sprites;
    }
    if (resultTimer == 32000)
    {
        resultTimer = 29000;
        goto show_sprites;  /* was: goto loc_10B964 which falls through to show_sprites */
    }

    /* already in countdown phase -> skip to position update (loc_10BAB0) */
    goto countdown;

show_sprites:
    /* Show halftime/fulltime banner depending on gameState */
    if (gameState == ST_RESULT_ON_HALFTIME)
        resultOnHalftimeSprite.visible = 1;

    if (gameState == ST_RESULT_AFTER_THE_GAME)
        resultAfterTheGameSprite.visible = 1;

    /* Show aggregate/total score digits if secondLeg or dontShowScorers */
    if (dontShowScorers || secondLeg)
    {
        team1TotalGoalsDigit2Sprite.visible = 1;
        d0 = (int16_t)team1TotalGoals;
        if ((uint16_t)((uint32_t)(uint16_t)d0 / 10u) != 0)
            team1TotalGoalsDigit1Sprite.visible = 1;

        team2TotalGoalsDigit2Sprite.visible = 1;
        d0 = (int16_t)team2TotalGoals;
        if ((uint16_t)((uint32_t)(uint16_t)d0 / 10u) != 0)
            team2TotalGoalsDigit1Sprite.visible = 1;
    }

    /* Show grid, team names, goal digits, dash, scorer sprites */
    gridUpperRowLeftSprite.visible = 1;
    gridUpperRowRightSprite.visible = 1;
    gridMiddleRowLeftSprite.visible = 1;
    gridMiddleRowRightSprite.visible = 1;
    gridBottomRowLeftSprite.visible = 1;
    gridBottomRowRightSprite.visible = 1;
    team1NameSprite.visible = 1;
    team2NameSprite.visible = 1;
    team1GoalsDigit2Sprite.visible = 1;

    if (team1GoalsDigit1 != 0)
        team1GoalsDigit1Sprite.visible = 1;

    team2GoalsDigit2Sprite.visible = 1;

    if (team2GoalsDigit1 != 0)
        team2GoalsDigit1Sprite.visible = 1;

    team1NameSprite.visible = 1;    /* redundant in original asm */
    dashSprite.visible = 1;

    /* Team 1 scorer sprites -- break on width==0 or dontShowScorers */
    s = team1ScorerSprite;
    for (d0 = 7; d0 >= 0; d0--)
    {
        if (team1ScorerSpritesWidths[7 - d0] == 0)
            break;
        if (dontShowScorers)
            break;
        s->visible = 1;
        s++;
    }

    /* Team 2 scorer sprites -- break on width==0 or dontShowScorers */
    s = team2ScorerSprite;
    for (d0 = 7; d0 >= 0; d0--)
    {
        if (team2ScorerSpritesWidths[7 - d0] == 0)
            break;
        if (dontShowScorers)
            break;
        s->visible = 1;
        s++;
    }

    _InitDisplaySprites();

countdown:
    /* Decrement timer by lastFrameTicks */
    resultTimer -= (int16_t)lastFrameTicks;
    if (resultTimer > 0)
        goto position_sprites;
    /* resultTimer <= 0: fall through to check if we should trigger stats */

hide_result:
    /* If halftime or fulltime result, trigger stats timer */
    if (gameState == ST_RESULT_ON_HALFTIME || gameState == ST_RESULT_AFTER_THE_GAME)
        statsTimer = 0x7D00;

    resultTimer = 0;

    /* If sprites aren't visible, nothing to hide */
    if (team1NameSprite.visible == 0)
        return;

    /* Hide all result sprites */
    resultOnHalftimeSprite.visible = 0;
    resultAfterTheGameSprite.visible = 0;
    gridUpperRowLeftSprite.visible = 0;
    gridUpperRowRightSprite.visible = 0;
    gridMiddleRowLeftSprite.visible = 0;
    gridMiddleRowRightSprite.visible = 0;
    gridBottomRowLeftSprite.visible = 0;
    gridBottomRowRightSprite.visible = 0;
    team1NameSprite.visible = 0;
    team2NameSprite.visible = 0;
    team1GoalsDigit2Sprite.visible = 0;
    team1GoalsDigit1Sprite.visible = 0;
    team1TotalGoalsDigit2Sprite.visible = 0;
    team1TotalGoalsDigit1Sprite.visible = 0;
    team2GoalsDigit2Sprite.visible = 0;
    team2GoalsDigit1Sprite.visible = 0;
    team2TotalGoalsDigit2Sprite.visible = 0;
    team2TotalGoalsDigit1Sprite.visible = 0;
    team1NameSprite.visible = 0;    /* redundant in original asm */
    dashSprite.visible = 0;

    /* Hide scorer sprites */
    {
        Sprite *sa = team1ScorerSprite;
        Sprite *sb = team2ScorerSprite;
        for (d0 = 7; d0 >= 0; d0--)
        {
            sa->visible = 0;
            sb->visible = 0;
            sa++;
            sb++;
        }
    }

    _InitDisplaySprites();
    return;

position_sprites:
    /* Calculate vertical offset based on how many scorers exist */
    d7 = 0;
    for (d1 = 7; d1 >= 0; d1--)
    {
        d0 = team1ScorerSpritesWidths[7 - d1] + team2ScorerSpritesWidths[7 - d1];
        if (d0 == 0)
            break;
        d7 -= 7;
    }
    if (d7 > (int16_t)0xFFF2)   /* -14 */
        d7 = (int16_t)0xFFF2;

    /* --- Position team1NameSprite --- */
    team1NameSprite.x.s.i = cameraX + 136 - (int16_t)team1NameLength;
    team1NameSprite.y.s.i = cameraY + 246 + d7 + 10000;
    team1NameSprite.z.s.i = 10000;

    /* d0 = base row Y for grid sprites */
    d0 = team1NameSprite.y.s.i - 1015;

    /* Grid upper row */
    gridUpperRowLeftSprite.x.s.i = cameraX;
    gridUpperRowLeftSprite.y.s.i = d0;
    gridUpperRowLeftSprite.z.s.i = 9000;
    gridUpperRowRightSprite.x.s.i = cameraX + 168;
    gridUpperRowRightSprite.y.s.i = d0;
    gridUpperRowRightSprite.z.s.i = 9000;

    d0 += 32;

    /* Grid middle row */
    gridMiddleRowLeftSprite.x.s.i = cameraX;
    gridMiddleRowLeftSprite.y.s.i = d0;
    gridMiddleRowLeftSprite.z.s.i = 9000;
    gridMiddleRowRightSprite.x.s.i = cameraX + 168;
    gridMiddleRowRightSprite.y.s.i = d0;
    gridMiddleRowRightSprite.z.s.i = 9000;

    d0 += 32;

    /* Grid bottom row */
    gridBottomRowLeftSprite.x.s.i = cameraX;
    gridBottomRowLeftSprite.y.s.i = d0;
    gridBottomRowLeftSprite.z.s.i = 9000;
    gridBottomRowRightSprite.x.s.i = cameraX + 168;
    gridBottomRowRightSprite.y.s.i = d0;
    gridBottomRowRightSprite.z.s.i = 9000;

    /* --- Position team2NameSprite --- */
    team2NameSprite.x.s.i = cameraX + 200;
    team2NameSprite.y.s.i = team1NameSprite.y.s.i;
    team2NameSprite.z.s.i = 10000;

    /* --- Team 1 goals digit 2 (units) --- */
    team1GoalsDigit2Sprite.x.s.i = cameraX + 151;
    team1GoalsDigit2Sprite.y.s.i = cameraY + 241 + d7 + 10000;
    team1GoalsDigit2Sprite.z.s.i = 10000;
    team1GoalsDigit2Sprite.imageIndex = SPR_BIG_0 + team1GoalsDigit2;

    /* --- Team 1 goals digit 1 (tens) --- */
    if (team1GoalsDigit1Sprite.visible)
    {
        team1GoalsDigit1Sprite.x.s.i = team1GoalsDigit2Sprite.x.s.i - 12;
        team1GoalsDigit1Sprite.y.s.i = team1GoalsDigit2Sprite.y.s.i;
        team1GoalsDigit1Sprite.z.s.i = team1GoalsDigit2Sprite.z.s.i;
        team1GoalsDigit1Sprite.imageIndex = SPR_BIG_0 + team1GoalsDigit1;
    }

    /* --- Team 1 total goals (aggregate) --- */
    if (team1TotalGoalsDigit2Sprite.visible)
    {
        d1 = (int16_t)team1TotalGoals;
        d2 = (int16_t)((uint32_t)(uint16_t)d1 / 10u);
        d1 = (int16_t)((uint32_t)(uint16_t)d1 % 10u);

        team1TotalGoalsDigit2Sprite.x.s.i = cameraX + 158;
        if (d1 == 1)
            team1TotalGoalsDigit2Sprite.x.s.i += 4;
        team1TotalGoalsDigit2Sprite.y.s.i = cameraY + 263 + d7 + 10002;
        team1TotalGoalsDigit2Sprite.z.s.i = 10002;
        team1TotalGoalsDigit2Sprite.imageIndex = SPR_SMALL_ZERO + (uint16_t)d1;

        if (team1TotalGoalsDigit1Sprite.visible)
        {
            team1TotalGoalsDigit1Sprite.x.s.i = team1TotalGoalsDigit2Sprite.x.s.i - 6;
            if (d2 == 1)
                team1TotalGoalsDigit1Sprite.x.s.i += 4;
            team1TotalGoalsDigit1Sprite.y.s.i = team1TotalGoalsDigit2Sprite.y.s.i - 1;
            team1TotalGoalsDigit1Sprite.z.s.i = team1TotalGoalsDigit2Sprite.z.s.i - 1;
            team1TotalGoalsDigit1Sprite.imageIndex = SPR_SMALL_ZERO + (uint16_t)d2;
        }
    }

    /* --- Team 2 goals digit 2 (units) --- */
    team2GoalsDigit2Sprite.x.s.i = cameraX + 173;
    team2GoalsDigit2Sprite.y.s.i = cameraY + 241 + d7 + 10000;
    team2GoalsDigit2Sprite.z.s.i = 10000;
    team2GoalsDigit2Sprite.imageIndex = SPR_SCROLL_ARROW_UP + team2GoalsDigit2;

    /* --- Team 2 goals digit 1 (tens) --- */
    if (team2GoalsDigit1Sprite.visible)
    {
        team2GoalsDigit1Sprite.x.s.i = team2GoalsDigit2Sprite.x.s.i;
        team2GoalsDigit2Sprite.x.s.i += 12;    /* original: addi.w #12,SPRITE_X(a0) on digit2 */
        team2GoalsDigit1Sprite.y.s.i = team2GoalsDigit2Sprite.y.s.i;
        team2GoalsDigit1Sprite.z.s.i = team2GoalsDigit2Sprite.z.s.i;
        team2GoalsDigit1Sprite.imageIndex = SPR_SCROLL_ARROW_UP + team2GoalsDigit1;
    }

    /* --- Team 2 total goals (aggregate) --- */
    if (team2TotalGoalsDigit2Sprite.visible)
    {
        d1 = (int16_t)team2TotalGoals;
        d2 = (int16_t)((uint32_t)(uint16_t)d1 / 10u);
        d1 = (int16_t)((uint32_t)(uint16_t)d1 % 10u);

        team2TotalGoalsDigit2Sprite.x.s.i = cameraX + 171;
        team2TotalGoalsDigit2Sprite.y.s.i = cameraY + 263 + d7 + 10002;
        team2TotalGoalsDigit2Sprite.z.s.i = 10002;
        team2TotalGoalsDigit2Sprite.imageIndex = SPR_SMALL_ZERO + (uint16_t)d1;

        if (team2TotalGoalsDigit1Sprite.visible)
        {
            /* asm: copy digit2.X -> digit1.X, then digit2.X += 6 */
            team2TotalGoalsDigit1Sprite.x.s.i = team2TotalGoalsDigit2Sprite.x.s.i;
            team2TotalGoalsDigit2Sprite.x.s.i += 6;
            if (d2 == 1)
                team2TotalGoalsDigit2Sprite.x.s.i -= 4;
            team2TotalGoalsDigit1Sprite.y.s.i = team2TotalGoalsDigit2Sprite.y.s.i;
            team2TotalGoalsDigit1Sprite.z.s.i = team2TotalGoalsDigit2Sprite.z.s.i;
            team2TotalGoalsDigit1Sprite.y.s.i -= 1;
            team2TotalGoalsDigit1Sprite.z.s.i -= 1;
            team2TotalGoalsDigit1Sprite.imageIndex = SPR_SMALL_ZERO + (uint16_t)d2;
        }
    }

    /* --- Dash sprite --- */
    dashSprite.x.s.i = cameraX + 165;
    dashSprite.y.s.i = cameraY + 249 + d7 + 10000;
    dashSprite.z.s.i = 10000;

    /* --- Team 1 scorer sprites --- */
    {
        Sprite *spr = team1ScorerSprite;
        uint16_t *widths = team1ScorerSpritesWidths;
        d1 = 263 + d7;
        for (d0 = 7; d0 >= 0; d0--)
        {
            if (*widths == 0)
                break;
            spr->x.s.i = cameraX + 136 - (int16_t)*(widths);
            spr->y.s.i = cameraY + d1 + 10000;
            spr->z.s.i = 10000;
            d1 += 7;
            widths++;
            spr++;
        }
    }

    /* --- Team 2 scorer sprites --- */
    {
        Sprite *spr = team2ScorerSprite;
        uint16_t *widths = team2ScorerSpritesWidths;
        d1 = 263 + d7;
        for (d0 = 7; d0 >= 0; d0--)
        {
            if (*widths == 0)
                break;
            spr->x.s.i = cameraX + 200;
            spr->y.s.i = cameraY + d1 + 10000;
            spr->z.s.i = 10000;
            d1 += 7;
            widths++;
            spr++;
        }
    }

    /* --- Position halftime/fulltime result banners --- */
    d0 = gridUpperRowLeftSprite.y.s.i + 990;
    resultOnHalftimeSprite.x.s.i = cameraX + 168;
    resultOnHalftimeSprite.y.s.i = d0;
    resultOnHalftimeSprite.z.s.i = 10000;
    resultAfterTheGameSprite.x.s.i = resultOnHalftimeSprite.x.s.i;
    resultAfterTheGameSprite.y.s.i = resultOnHalftimeSprite.y.s.i;
    resultAfterTheGameSprite.z.s.i = resultOnHalftimeSprite.z.s.i;
}


/* ================================================================
 * UpdateTime  (SWOS2.s line 13241)
 *
 * Core game time management:
 * - Tracks minutes/seconds using BCD gameTime
 * - Handles half-time, full-time, extra time, penalties
 * - Builds and positions the time display sprite
 * ================================================================ */
__attribute__((used, externally_visible))
void _UpdateTime(void)
{
    uint16_t d0u;

    /* --- Timer/visibility section --- */
    /* Original asm always sets timeVar=31000, then immediately converts to 150.
     * The intermediate timer countdown code (sub.w d0,timeVar; bmi/bne) is
     * unreachable due to an unconditional bra.w time_invisible after loading
     * lastFrameTicks. We replicate the reachable path only. */
    timeVar = 31000;

    /* timeVar is always 31000 here -> convert to 150 */
    timeVar = 150;

    /* Make time sprite visible if not already */
    if (currentTimeSprite.visible == 0)
    {
        currentTimeSprite.visible = 1;
        _InitDisplaySprites();
    }

    /* --- Time update section (time_invisible) --- */

    /* If gameSeconds is negative -> last minute countdown in progress */
    if ((int16_t)gameSeconds < 0)
        goto last_minute_in_progress;

    /* Only update time during normal gameplay (not penalties) */
    if (gameStatePl != ST_GAME_IN_PROGRESS)
        goto no_update_to_time;
    if (playingPenalties)
        goto no_update_to_time;

    /* Accumulate time delta */
    secondsSwitchAccumulator -= timeDelta;
    if ((int16_t)secondsSwitchAccumulator >= 0)
        goto no_update_to_time;

    secondsSwitchAccumulator += 49;
    gameSeconds++;

    if (gameSeconds != 60)
        goto no_update_to_time;

    /* Minute boundary: increment BCD gameTime */
    gameSeconds = 0;
    GAMETIME_ONES++;
    if (GAMETIME_ONES >= 10)
    {
        GAMETIME_ONES = 0;
        GAMETIME_TENS++;
        if (GAMETIME_TENS >= 10)
        {
            GAMETIME_TENS = 0;
            GAMETIME_HUNDREDS++;
        }
    }

    /* --- check_game_end --- */
check_game_end:
    /* Minute 1: mark half-start */
    if (gameTime == 0x00000001)
        _BumpPlayersLastPlayedHalfAtHalfStart();

    /* Minute 46: mark second-half start */
    if (gameTime == 0x00000406)
        _BumpPlayersLastPlayedHalfAtHalfStart();

    /* Minute 45: half-time */
    if (gameTime == 0x00000405)
    {
        if ((int16_t)gameSeconds >= 0)
            goto last_cycle;
        gameSeconds = 0;
        _PlayEndGameWhistleSample();
        _EndFirstHalf();
        _BumpPlayersLastPlayedHalfAtHalfEnd();
        goto not_end_of_second_extra;
    }

    /* Minute 90: full-time */
    if (gameTime == 0x00000900)
    {
        if ((int16_t)gameSeconds >= 0)
            goto last_cycle;
        gameSeconds = 0;
        _PlayEndGameWhistleSample();
        _BumpPlayersLastPlayedHalfAtHalfEnd();
        statsTeam1GoalsCopy = statsTeam1Goals;
        statsTeam2GoalsCopy = statsTeam2Goals;

        d0u = team1TotalGoals;
        if (d0u != team2TotalGoals)
            goto not_draw;

        /* Tied at 90 minutes */
        if (secondLeg && playing2ndGame == 1)
        {
            /* Away goals rule: 2*thisLeg + firstLeg */
            uint16_t team2adj = (uint16_t)(statsTeam2Goals * 2 + team2GoalsFirstLeg);
            uint16_t team1adj = (uint16_t)(statsTeam1Goals * 2 + team1GoalsFirstLeg);
            if (team1adj != team2adj)
            {
                /* Away goals decide */
                if (team1adj > team2adj)
                    goto team1_wins;
                else
                    goto team2_wins;
            }
            /* Still tied -> fall through to extra time check */
        }

        /* Check extra time */
        if (extraTimeState)
        {
            extraTimeState = (int16_t)0xFFFF;
            _StartFirstExtraTime();
            goto not_end_of_second_extra;
        }
        /* Check penalties */
        if (penaltiesState)
        {
            penaltiesState = (int16_t)0xFFFF;
            _StartPenalties();
            goto not_end_of_second_extra;
        }
        /* Draw with no tiebreaker -> clear winner, end game */
        winningTeamPtr = (TeamGame *)0;
        _EndOfGame();
        goto not_end_of_second_extra;
    }

    /* Minute 105: end of first extra time */
    if (gameTime == 0x00010005)
    {
        if ((int16_t)gameSeconds >= 0)
            goto last_cycle;
        gameSeconds = 0;
        _PlayEndGameWhistleSample();
        _EndFirstExtraTime();
        goto not_end_of_second_extra;
    }

    /* Minute 120: end of second extra time */
    if (gameTime == 0x00010200)
    {
        if ((int16_t)gameSeconds >= 0)
            goto last_cycle;
        gameSeconds = 0;
        _PlayEndGameWhistleSample();

        d0u = team1TotalGoals;
        if (d0u != team2TotalGoals)
            goto not_draw;

        /* Tied after extra time */
        if (secondLeg && playing2ndGame)
        {
            uint16_t team2adj = (uint16_t)(statsTeam2Goals * 2 + team2GoalsFirstLeg);
            uint16_t team1adj = (uint16_t)(statsTeam1Goals * 2 + team1GoalsFirstLeg);
            if (team1adj != team2adj)
            {
                if (team1adj > team2adj)
                    goto team1_wins;
                else
                    goto team2_wins;
            }
        }
        /* Still tied -> check penalties */
        if (penaltiesState)
        {
            penaltiesState = (int16_t)0xFFFF;
            _StartPenalties();
            goto not_end_of_second_extra;
        }
        /* No penalties -> draw, end game */
        winningTeamPtr = (TeamGame *)0;
        _EndOfGame();
        goto not_end_of_second_extra;
    }

    goto not_end_of_second_extra;

not_draw:
    /* d0u has team1TotalGoals, compare with team2TotalGoals */
    if (d0u > team2TotalGoals)
        goto team1_wins;
team2_wins:
    winningTeamPtr = &bottomTeamInGame;
    goto end_game;
team1_wins:
    winningTeamPtr = &topTeamInGame;
end_game:
    _EndOfGame();
    goto not_end_of_second_extra;

last_cycle:
    gameSeconds = 0xFFFF;       /* negative = last minute in progress */
    endGameCounter = 50;
    goto no_update_to_time;

last_minute_in_progress:
    endGameCounter -= lastFrameTicks;
    if ((int16_t)endGameCounter < 0)
        goto check_game_end;

    if (gameStatePl != ST_GAME_IN_PROGRESS)
        goto last_cycle;

    {
        int16_t ballY = ballSprite.y.s.i;

        /* Ball must be between penalty areas (216 < y < 682) */
        if (ballY <= 216)
            goto last_cycle;
        if (ballY >= 682)
            goto last_cycle;

        /* Check which half the ball is in relative to center (449) */
        if (ballY <= 449)
        {
            /* Upper half: if bottom team has ball, extend time */
            if (lastTeamPlayed == &bottomTeamData)
                goto last_cycle;
        }
        else
        {
            /* Lower half: if top team has ball, extend time */
            if (lastTeamPlayed == &topTeamData)
                goto last_cycle;
        }
    }

no_update_to_time:
    /* Check if time sprite needs rebuilding */
    if (!copyTimeSprite)
        goto set_time_sprite_x;

not_end_of_second_extra:
    {
        /* Build time sprite from gameTime digits */
        uint16_t xOff = 0;
        uint16_t yOff = 0;
        uint16_t sprIdx;
        uint16_t digitSprite;

        if (GAMETIME_HUNDREDS != 0)
        {
            /* 3-digit time (100+) */
            sprIdx = SPR_INVITATION_TOURNAMENT;
            xOff = 5;       /* width for the hundreds digit (always 1) */
        }
        else if (GAMETIME_TENS != 0)
        {
            /* 2-digit time */
            sprIdx = SPR_PREMIERE_DIVISION_WINNER;
        }
        else
        {
            /* 1-digit time */
            sprIdx = SPR_LITTLE_PLAYER_GENERIC;
            goto copy_last_digit;
        }

        /* Copy tens digit */
        digitSprite = SPR_CUP_WINNER + GAMETIME_TENS;
        _CopySprite(digitSprite, sprIdx, xOff, yOff);
        xOff += 6;

    copy_last_digit:
        /* Copy ones digit */
        digitSprite = SPR_CUP_WINNER + GAMETIME_ONES;
        _CopySprite(digitSprite, sprIdx, xOff, yOff);

        /* Set sprite image index */
        currentTimeSprite.imageIndex = sprIdx;

        /* Blit the assembled time sprite */
        if (sprIdx == SPR_INVITATION_TOURNAMENT)
            _BlitSpritePlanar_Long_StridedDest(sprIdx);
        else
            _BlitSpritePlanar_Word_StridedDest(sprIdx);

        copyTimeSprite = 0;
    }

set_time_sprite_x:
    {
        /* Position time sprite relative to camera */
        currentTimeSprite.x.s.i = cameraX + 0x14;
        currentTimeSprite.y.s.i = cameraY + 0x2721;

        /* If player indicator is at position 4, shift time down by 8 */
        if (currentPlayerIndicatorPosition == 4)
            currentTimeSprite.y.s.i += 8;

        currentTimeSprite.z.s.i = 10000;
    }
}
