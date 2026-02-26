#include "statistics.h"
#include "game_types.h"

/* ---- External globals ---- */
extern uint16_t showStats;
extern uint16_t overlayActiveFlag;
extern uint16_t statsTimer;
extern uint16_t showingUserStats;
extern uint16_t isGoalAttempt;

extern uint16_t gameStatePl;
extern uint16_t playingPenalties;
extern uint16_t lastFrameTicks;
extern uint16_t fireBlocked;
extern uint16_t trainingGame;

extern uint8_t  pl1Fire;
extern uint8_t  pl2Fire;

extern TeamGeneralInfo *lastTeamPlayed;
extern TeamGeneralInfo  topTeamData;
extern TeamGeneralInfo  bottomTeamData;
extern TeamGame         topTeamInGame;
extern TeamGame         bottomTeamInGame;

extern Sprite      ballSprite;
extern uint16_t ballInGoalkeeperArea;
extern uint16_t strikeDestX;

extern uint16_t statsTeam1Goals;
extern uint16_t statsTeam2Goals;
extern uint8_t  statsNumber[];      /* 6-byte buffer in asm data section */

extern uint8_t  bigCharsTable[];

/* ---- External string constants ---- */
extern uint8_t  aMATCHSTATS[];      /* "M A T C H   S T A T S" */
extern uint8_t  aGoals[];           /* "GOALS" */
extern uint8_t  aPossession[];      /* "POSSESSION" */
extern uint8_t  aMinus[];           /* "-" */
extern uint8_t  aGoalAttempts[];    /* "GOAL ATTEMPTS" */
extern uint8_t  aOnTarget[];        /* "ON TARGET" */
extern uint8_t  aCornersWon[];      /* "CORNERS WON" */
extern uint8_t  aFoulsConceded[];   /* "FOULS CONCEDED" */
extern uint8_t  aBookings[];        /* "BOOKINGS" */
extern uint8_t  aSendingsOff[];     /* "SENDINGS OFF" */

/* ---- External functions ---- */
extern void _Player1StatusProc(void);
extern void _Player2StatusProc(void);
extern void _StatisticsOff(void);
extern uint16_t _DrawMenuTextCentered(uint16_t x, uint16_t y, uint16_t color,
                                      uint8_t *text, uint8_t *charsTable);
extern void _Int2Ascii(uint16_t value, uint8_t *outBuf);
extern void _CDarkenRectangle(uint16_t sprIdx, uint16_t x, uint16_t y);

/* ---- Constants ---- */
#define SPR_SQUARE_GRID     1093


/* ================================================================
 * DrawGrid  (SWOS2.s line 18750)
 *
 * Draws darkened grid background using 16x16 tiles.
 * d1=x, d2=y, d3=width, d4=height
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawGrid(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    uint16_t cols = (width >> 4) - 1;
    uint16_t rows = (height >> 4) - 1;
    uint16_t origX = x;
    int16_t r, c;
    uint16_t cx, cy;

    cy = y;
    for (r = (int16_t)rows; r >= 0; r--)
    {
        cx = origX;
        for (c = (int16_t)cols; c >= 0; c--)
        {
            _CDarkenRectangle(SPR_SQUARE_GRID, cx, cy);
            cx += 16;
        }
        cy += 16;
    }
}


/* ================================================================
 * WriteStatsNumber  (SWOS2.s line 18702)
 *
 * Converts number to ASCII and draws it centered at (x, y).
 * Uses statsNumber as scratch buffer.
 * ================================================================ */
static void writeStatsNumber(uint16_t number, uint16_t x, uint16_t y,
                             uint16_t color, uint8_t *charTable)
{
    _Int2Ascii(number, statsNumber);
    _DrawMenuTextCentered(x, y, color, statsNumber, charTable);
}


/* ================================================================
 * WriteStatsPercent  (SWOS2.s line 18723)
 *
 * Converts number to ASCII, appends '%', draws centered.
 * ================================================================ */
static void writeStatsPercent(uint16_t number, uint16_t x, uint16_t y,
                              uint16_t color, uint8_t *charTable)
{
    uint8_t *p;

    _Int2Ascii(number, statsNumber);

    /* Find end of string (Int2Ascii leaves pointer at terminator via a1) */
    p = statsNumber;
    while (*p)
        p++;
    *p++ = '%';
    *p = 0;

    _DrawMenuTextCentered(x, y, color, statsNumber, charTable);
}


/* ================================================================
 * DrawStatistics  (SWOS2.s line 18483)
 *
 * Renders the full match statistics overlay:
 * - Grid backgrounds
 * - Team names
 * - Goals, possession, goal attempts, on target,
 *   corners won, fouls conceded, bookings, sendings off
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawStatistics(void)
{
    TeamGeneralInfo *topInfo;    /* team stats pointer for top/first team */
    TeamGeneralInfo *botInfo;    /* team stats pointer for bottom/second team */
    TeamStatisticsData *a5;      /* stats for top team */
    TeamStatisticsData *a6;      /* stats for bottom team */
    uint8_t *charTable = bigCharsTable;
    uint16_t color = 2;
    uint16_t y;
    uint16_t d7;

    /* Draw title grid (80, 35, 176, 16) */
    _DrawGrid(80, 35, 176, 16);

    /* Draw 10 data rows starting at y=55, each 304 wide, 16 tall, spaced 18 apart */
    y = 55;
    _DrawGrid(16, y, 304, 16);
    y += 18;
    _DrawGrid(16, y, 304, 16);
    y += 18;
    _DrawGrid(16, y, 304, 16);
    y += 18;
    _DrawGrid(16, y, 304, 16);
    y += 18;
    _DrawGrid(16, y, 304, 16);
    y += 18;
    _DrawGrid(16, y, 304, 16);
    y += 18;
    _DrawGrid(16, y, 304, 16);
    y += 18;
    _DrawGrid(16, y, 304, 16);
    y += 18;
    _DrawGrid(16, y, 304, 16);
    y += 18;
    _DrawGrid(16, y, 304, 16);

    /* Determine team order: topInfo = team whose inGameTeamPtr matches topTeamInGame */
    topInfo = &topTeamData;
    botInfo = &bottomTeamData;
    if (topInfo->inGameTeamPtr != &topTeamInGame)
    {
        /* Swap */
        TeamGeneralInfo *tmp = topInfo;
        topInfo = botInfo;
        botInfo = tmp;
    }
    a5 = topInfo->teamStatsPtr;
    a6 = botInfo->teamStatsPtr;

    /* Title: "M A T C H   S T A T S" at y=39 */
    y = 39;
    _DrawMenuTextCentered(168, y, color, aMATCHSTATS, charTable);

    /* Team names at y=59 */
    y += 20;
    {
        uint16_t topX = 80;
        if (trainingGame)
            topX += 5;
        _DrawMenuTextCentered(topX, y, color, (uint8_t *)topTeamInGame.teamName, charTable);
    }
    {
        uint16_t botX = 256;
        if (trainingGame)
            botX -= 5;
        _DrawMenuTextCentered(botX, y, color, (uint8_t *)bottomTeamInGame.teamName, charTable);
    }

    /* GOALS at y=77 */
    y += 18;
    _DrawMenuTextCentered(168, y, color, aGoals, charTable);
    writeStatsNumber(statsTeam1Goals, 80, y, color, charTable);
    writeStatsNumber(statsTeam2Goals, 256, y, color, charTable);

    /* POSSESSION at y=95 */
    y += 18;
    _DrawMenuTextCentered(168, y, color, aPossession, charTable);
    d7 = (uint16_t)a5->ballPossession + (uint16_t)a6->ballPossession;
    if (d7 == 0)
    {
        _DrawMenuTextCentered(80, y, color, aMinus, charTable);
        _DrawMenuTextCentered(256, y, color, aMinus, charTable);
    }
    else
    {
        uint16_t pct = (uint16_t)(((uint32_t)(uint16_t)a5->ballPossession * 100u) / d7);
        writeStatsPercent(pct, 80, y, color, charTable);
        writeStatsPercent((uint16_t)(100 - pct), 256, y, color, charTable);
    }

    /* GOAL ATTEMPTS at y=113 */
    y += 18;
    _DrawMenuTextCentered(168, y, color, aGoalAttempts, charTable);
    writeStatsNumber((uint16_t)a5->goalAttempts, 80, y, color, charTable);
    writeStatsNumber((uint16_t)a6->goalAttempts, 256, y, color, charTable);

    /* ON TARGET at y=131 */
    y += 18;
    _DrawMenuTextCentered(168, y, color, aOnTarget, charTable);
    writeStatsNumber((uint16_t)a5->onTarget, 80, y, color, charTable);
    writeStatsNumber((uint16_t)a6->onTarget, 256, y, color, charTable);

    /* CORNERS WON at y=149 */
    y += 18;
    _DrawMenuTextCentered(168, y, color, aCornersWon, charTable);
    writeStatsNumber((uint16_t)a5->cornersWon, 80, y, color, charTable);
    writeStatsNumber((uint16_t)a6->cornersWon, 256, y, color, charTable);

    /* FOULS CONCEDED at y=167 */
    y += 18;
    _DrawMenuTextCentered(168, y, color, aFoulsConceded, charTable);
    writeStatsNumber((uint16_t)a5->foulsConceded, 80, y, color, charTable);
    writeStatsNumber((uint16_t)a6->foulsConceded, 256, y, color, charTable);

    /* BOOKINGS at y=185 */
    y += 18;
    _DrawMenuTextCentered(168, y, color, aBookings, charTable);
    writeStatsNumber((uint16_t)a5->bookings, 80, y, color, charTable);
    writeStatsNumber((uint16_t)a6->bookings, 256, y, color, charTable);

    /* SENDINGS OFF at y=203 */
    y += 18;
    _DrawMenuTextCentered(168, y, color, aSendingsOff, charTable);
    writeStatsNumber((uint16_t)a5->sendingsOff, 80, y, color, charTable);
    writeStatsNumber((uint16_t)a6->sendingsOff, 256, y, color, charTable);
}


/* ================================================================
 * UpdateAndDrawStatistics  (SWOS2.s line 18356)
 *
 * Main statistics update loop:
 * - Tracks shot attempts and on-target shots
 * - Manages stats overlay timer
 * - Draws stats when active
 * ================================================================ */
__attribute__((used, externally_visible))
void _UpdateAndDrawStatistics(void)
{
    /* If showStats just set, trigger user stats display */
    if (showStats)
    {
        showStats = 0;
        showingUserStats = 1;
        statsTimer = 32000;
        _DrawStatistics();
    }

    /* While showing user stats, poll inputs */
    if (showingUserStats)
    {
        _Player1StatusProc();
        _Player2StatusProc();
        if (pl1Fire)
        {
            fireBlocked = 1;
            _StatisticsOff();
            return;
        }
        if (pl2Fire)
        {
            fireBlocked = 1;
            _StatisticsOff();
            return;
        }
        return;
    }

    /* -- Shot tracking logic (only when not in penalties) -- */
    if (!playingPenalties)
    {
        /* Increment possession counter for last team that touched ball */
        if (gameStatePl == ST_GAME_IN_PROGRESS)
        {
            TeamGeneralInfo *team = lastTeamPlayed;
            if (team == &topTeamData || team == &bottomTeamData)
            {
                TeamStatisticsData *statsPtr = team->teamStatsPtr;
                statsPtr->ballPossession++;
            }
        }

        /* Goal attempt tracking */
        if (gameStatePl == ST_GAME_IN_PROGRESS)
        {
            if (isGoalAttempt)
            {
                /* isGoalAttempt is set — check if ball is moving away from goal */
                TeamGeneralInfo *team = &topTeamData;
                int32_t deltaY = ballSprite.deltaY.raw;
                if (ballSprite.y.s.i >= 449)
                {
                    team = &bottomTeamData;
                    deltaY = -deltaY;
                }
                if (team == lastTeamPlayed || deltaY >= 0)
                {
                    /* Ball moving away or same team → clear attempt */
                    isGoalAttempt = 0;
                    goto check_show_stats_flag;
                }
                goto check_show_stats_flag;
            }

            /* Check if ball is in goalkeeper area */
            if (ballInGoalkeeperArea)
            {
                TeamGeneralInfo *team = &topTeamData;
                int32_t deltaY = ballSprite.deltaY.raw;
                if (ballSprite.y.s.i > 449)
                {
                    /* Ball in lower half — keep topTeamData */
                }
                else
                {
                    /* Ball in upper half */
                    team = &bottomTeamData;
                    deltaY = -deltaY;
                }

                /* deltaY > 0 means ball moving toward goal */
                if (deltaY > 0
                    && team == lastTeamPlayed
                    && team->playerHasBall == 0
                    && strikeDestX >= 240
                    && strikeDestX <= 431)
                {
                    /* Check if on target (between goalposts at 303-367) */
                    if (strikeDestX >= 303 && strikeDestX <= 367)
                    {
                        /* On target — increment on-target stat for attacking team */
                        TeamStatisticsData *statsPtr;
                        if (ballSprite.y.s.i > 449)
                            statsPtr = topTeamData.teamStatsPtr;
                        else
                            statsPtr = bottomTeamData.teamStatsPtr;
                        statsPtr->onTarget++;
                    }

                    /* Increment goal attempts for attacking team */
                    {
                        TeamStatisticsData *statsPtr;
                        if (ballSprite.y.s.i > 449)
                            statsPtr = topTeamData.teamStatsPtr;
                        else
                            statsPtr = bottomTeamData.teamStatsPtr;
                        statsPtr->goalAttempts++;
                    }

                    isGoalAttempt = 1;
                }
            }
        }
        else
        {
            /* Not in game → clear goal attempt */
            isGoalAttempt = 0;
        }
    }

check_show_stats_flag:
    /* Check if showStats was set during this frame */
    if (showStats)
    {
        showStats = 0;
        showingUserStats = 1;
    }

    /* Stats timer management */
    if (statsTimer == 0)
        return;

    if ((int16_t)statsTimer < 0)
    {
        _StatisticsOff();
        return;
    }

    if (statsTimer == 32000)
    {
        statsTimer = 30000;
        overlayActiveFlag = 1;
    }
    else
    {
        statsTimer -= lastFrameTicks;
        if ((int16_t)statsTimer <= 0)
        {
            _StatisticsOff();
            return;
        }
    }

    _DrawStatistics();
}
