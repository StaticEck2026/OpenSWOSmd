#include "statistics.h"
#include "game_types.h"
#include "menu_system.h"
#include "play_match_menu.h"

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


/* ================================================================
 * TOP GOAL SCORERS — Chunk 134
 *
 * highestScorersTable layout (92 bytes, in BSS):
 *   offset 0-29:  uint16_t sortIndices[15] — byte offsets into entries
 *   offset 30:    uint8_t  minScore
 *   offset 31:    uint8_t  count (replaceable slots at minScore)
 *   offset 32-91: 15 × 4-byte entries:
 *                   +0 (word): teamId (0xFFFF = empty)
 *                   +2 (byte): playerIdx
 *                   +3 (byte): score (goals)
 * ================================================================ */

/* ---- Additional external globals for top goal scorers ---- */
extern uint8_t  highestScorersTable[];   /* 92 bytes BSS */
extern uint32_t diyStatsName;            /* pointer stored as uint32_t */
extern uint16_t numSelectedTeams;
extern uint16_t selTeamNumber;
extern uint8_t  selTeamsPtr;             /* start of selected teams, stride=684 */
extern uint16_t gameType;
extern uint16_t careerLoadedFlag;
extern uint8_t  careerTeam;              /* start of career team (684 bytes) */
extern uint16_t careerNumPlayers;
extern uint32_t secondaryCompetitionPtr;
extern uint32_t tertiaryCompetitionPtr;
extern uint32_t diyNameOfCompetitionPtr;
extern uint16_t diyUnknownWord_2C;
extern MenuEntry menuEntries[];
extern uint8_t  aLparen[];              /* "(" */
extern uint8_t  aRparen[];              /* ")" */
extern uint8_t  aCup_10[];              /* "\x04C\x00U\x00P\x00 " */

/* Menu data (declared in SWOS2.s, used as a6 for ShowMenu) */
extern uint8_t  competitionTopGoalScorersMenu[];
extern uint8_t  topGoalScorersMenu[];

/* ---- External functions ---- */
extern void _ShowMenu(uint8_t *menu);
extern void _StringCopy(uint8_t *src, uint8_t **dest);
extern uint8_t *_GetTeamPointer(uint16_t teamNumber);
extern uint8_t *_GetPlayer(uint16_t teamNumber, uint16_t playerFileIndex);
extern uint8_t *_GetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal);
extern uint16_t _FindTeamIdInDiyTeamsList(uint16_t teamId);
extern uint8_t *_VerifyTeamLoaded(uint16_t teamNumber, uint16_t playerIdx);
extern uint8_t *_ExtractSurname(uint8_t *name, uint16_t maxLen);
extern void _DrawMultipleItems(uint16_t maxItems, MenuEntry *firstEntry);


/* ================================================================
 * InitHighestScorersTable  (SWOS2.s line 51429)
 *
 * Clears the highest scorers table: initializes sort indices,
 * sets minScore to 0, count to 15, and all entries to empty.
 * ================================================================ */
static void InitHighestScorersTable(void)
{
    uint8_t *base = highestScorersTable;
    uint16_t idx = 0;
    int16_t i;
    uint16_t *sortPtr = (uint16_t *)base;

    base[30] = 0;        /* minScore = 0 */
    base[31] = 15;       /* count = 15 */

    for (i = 14; i >= 0; i--)
    {
        *sortPtr++ = idx;                            /* sort index */
        *(uint16_t *)(base + idx + 32) = 0xFFFF;     /* teamId = -1 */
        *(base + idx + 34) = 0xFF;                    /* playerIdx = -1 */
        *(base + idx + 35) = 0;                       /* score = 0 */
        idx += 4;
    }
}


/* ================================================================
 * UpdateHighestScorersMinScoreAndCount  (SWOS2.s line 51529)
 *
 * Scans all 15 entries to find the new minimum score and count
 * of entries at that minimum. Updates minScore and count fields.
 * Returns the new minScore.
 * ================================================================ */
static uint8_t UpdateHighestScorersMinScoreAndCount(uint8_t *base)
{
    uint8_t *ptr = base;
    uint8_t minVal = 0xFF;
    uint8_t cnt = 0;
    int16_t i;

    for (i = 14; i >= 0; i--)
    {
        uint8_t score = *(ptr + 35);  /* entry score at offset 35 from current pos */
        if (minVal > score)
        {
            /* New minimum found */
            minVal = score;
            cnt = 1;
        }
        else if (minVal == score)
        {
            cnt++;
        }
        ptr += 4;
    }

    base[30] = minVal;   /* minScore */
    base[31] = cnt;      /* count */
    return minVal;
}


/* ================================================================
 * FindReplaceableLowestScorerSlot  (SWOS2.s line 51554)
 *
 * Finds an entry to replace in the table. Prefers:
 * 1. An entry at minScore that has teamId == -1 (empty)
 * 2. An entry at minScore whose team isInDIYTeam == 1
 * 3. Falls back to first entry at minScore
 *
 * Returns pointer to the slot (a3 in asm).
 * ================================================================ */
static uint8_t *FindReplaceableLowestScorerSlot(uint8_t *base, uint8_t minScore)
{
    uint8_t *ptr = base;
    int16_t i;

    /* First pass: find empty slot or DIY team at minScore */
    for (i = 14; i >= 0; i--)
    {
        if (*(ptr + 35) == minScore)  /* score matches minScore */
        {
            uint16_t teamId = *(uint16_t *)(ptr + 32);
            if (teamId == 0xFFFF)
                return ptr;  /* empty slot */

            /* Check if team's isInDIYTeam flag is set */
            {
                uint16_t d0 = teamId;
                uint8_t d1 = *(ptr + 34);  /* playerIdx */
                int16_t d1w = (int8_t)d1;  /* ext.w d1 */
                uint8_t *teamPtr = _VerifyTeamLoaded(d0, (uint16_t)d1w);
                if (*(teamPtr + 4) == 1)   /* isInDIYTeam == 1 */
                    return ptr;
            }
        }
        ptr += 4;
    }

    /* Second pass: find first entry at minScore */
    ptr = base;
    for (;;)
    {
        if (*(ptr + 35) == minScore)
            return ptr;
        ptr += 4;
    }
}


/* ================================================================
 * AddTeamScorersToHighestScorersTable  (SWOS2.s line 51449)
 *
 * Adds goal scorers from one team to the highest scorers table.
 * d0 = statIndex (competition index, or negative for all-comps sum)
 * a0 = pointer to team data (684 bytes: TeamFile + 16 PlayerFile)
 * ================================================================ */
static void AddTeamScorersToHighestScorersTable(int16_t statIndex,
                                                 uint8_t *teamPtr)
{
    uint8_t *base = highestScorersTable;
    uint8_t *playerBase;
    uint8_t *teamBase;
    uint16_t teamNumber;
    uint8_t minScore;
    int16_t numPlayers;
    int16_t playerIdx;

    /* For DIY game types with negative statIndex, verify team is in DIY list */
    if (statIndex < 0)
    {
        if (gameType == 3 || gameType == 4)
        {
            if (_FindTeamIdInDiyTeamsList(*(uint16_t *)teamPtr) != 0)
                return;  /* team not in DIY list, skip */
        }
    }

    teamBase = teamPtr;
    playerBase = teamPtr;
    teamNumber = *(uint16_t *)teamBase;
    minScore = base[30];  /* current minScore */

    /* Determine number of players to iterate */
    numPlayers = 15;  /* default: 16 players (dbf loop 15→0) */
    if (teamNumber == selTeamNumber)
    {
        numPlayers = (int16_t)careerNumPlayers - 1;
    }

    playerIdx = 0;

    for (; numPlayers >= 0; numPlayers--)
    {
        uint16_t score;

        /* For career team, use GetPlayerAtIndex to remap */
        if (teamNumber == selTeamNumber)
        {
            playerBase = _GetPlayerAtIndex(teamNumber, (uint16_t)numPlayers);
            /* GetPlayerAtIndex stores fileIndex in _cGetPlayerAtIndexFileIdx */
            /* In asm, d1 returns the file index → stored in d7 (playerIdx) */
            extern uint16_t _cGetPlayerAtIndexFileIdx;
            playerIdx = (int16_t)_cGetPlayerAtIndexFileIdx;
        }

        /* Get score for this player */
        if (statIndex < 0)
        {
            /* Sum all 4 competition goal stats */
            uint16_t total = 0;
            total += *(playerBase + 0x6D);   /* goalStats[0] */
            total += *(playerBase + 0x6E);   /* goalStats[1] */
            total += *(playerBase + 0x6F);   /* goalStats[2] */
            total += *(playerBase + 0x70);   /* goalStats[3] */
            if (total > 255)
                total = 255;
            score = total;
            if (score == 0)
                goto next_player;
        }
        else
        {
            /* Specific competition index */
            score = *(playerBase + 0x6D + statIndex);
            if (score == 0)
                goto next_player;
        }

        /* Check if score qualifies for the table */
        if ((uint8_t)score < minScore)
            goto next_player;

        if ((uint8_t)score == minScore)
        {
            /* Score equals minimum — only skip if team isInDIYTeam */
            if (*(teamBase + 4) == 1)  /* isInDIYTeam */
                goto next_player;
        }

        /* Find a slot to replace */
        {
            uint8_t *slot = FindReplaceableLowestScorerSlot(base, minScore);
            slot[35] = (uint8_t)score;       /* store score */
            slot[34] = (uint8_t)playerIdx;   /* store playerIdx */
            *(uint16_t *)(slot + 32) = teamNumber;  /* store teamId */

            if ((uint8_t)score != minScore)
            {
                /* We replaced a slot with a higher score — decrement replaceable count */
                base[31]--;
                if (base[31] == 0)
                {
                    /* No more replaceable slots — recalculate min and count */
                    minScore = UpdateHighestScorersMinScoreAndCount(base);
                }
            }
        }

next_player:
        if (teamNumber != selTeamNumber)
        {
            /* Non-career: advance playerBase by 38 (PlayerFile size) */
            playerBase += 0x26;
        }
        playerIdx++;
    }
}


/* ================================================================
 * PopulateHighestScorersTableFromSelectedTeams  (SWOS2.s line 51402)
 *
 * Iterates selected teams, adding their scorers to the table.
 * Skips the career team (selTeamNumber), but if game type is
 * Career and careerLoadedFlag==0, also adds career team scorers.
 * d0 = statIndex passed through to AddTeamScorersToHighestScorersTable
 * ================================================================ */
static void PopulateHighestScorersTableFromSelectedTeams(int16_t statIndex)
{
    uint8_t *teamsPtr = &selTeamsPtr;
    uint16_t numTeams = numSelectedTeams;
    uint16_t careerTeamNum;

    if (numTeams == 0)
        return;

    careerTeamNum = selTeamNumber;
    numTeams--;

    for (; (int16_t)numTeams >= 0; numTeams--)
    {
        if (*(uint16_t *)teamsPtr != careerTeamNum)
        {
            AddTeamScorersToHighestScorersTable(statIndex, teamsPtr);
        }
        teamsPtr += 684;
    }

    /* In Career mode with careerLoadedFlag==0, add career team too */
    if (gameType == GAME_TYPE_CAREER)
    {
        if (careerLoadedFlag != 0)
            return;
        AddTeamScorersToHighestScorersTable(statIndex, &careerTeam);
    }
}


/* ================================================================
 * SortHighestScorerTable  (SWOS2.s line 51584)
 *
 * Bubble-sorts the sort indices in descending order of score.
 * Ties are broken alphabetically by player surname.
 * ================================================================ */

/* GetScorerSurname: returns pointer to last word (surname) in player name */
static uint8_t *GetScorerSurname(uint8_t *entryBase)
{
    uint16_t teamId = *(uint16_t *)(entryBase + 32);
    uint8_t playerIdx = *(entryBase + 34);
    int16_t idxSigned = (int8_t)playerIdx;
    uint8_t *playerPtr;
    uint8_t *nameStart;
    uint8_t *surname;

    playerPtr = _GetPlayer(teamId, (uint16_t)idxSigned);
    /* Player name at offset 0x4C + PLAYERFILE_PLAYERNAME(3) = 0x4F from GetPlayer result */
    nameStart = playerPtr + 0x4F;
    surname = nameStart;

    while (*nameStart)
    {
        if (*nameStart == ' ')
        {
            nameStart++;
            surname = nameStart;
        }
        else
        {
            nameStart++;
        }
    }

    return surname;
}

static void SortHighestScorerTable(void)
{
    uint8_t *base = highestScorersTable;
    uint16_t *sortArray = (uint16_t *)base;
    int16_t outer, inner;

    for (outer = 13; outer >= 0; outer--)
    {
        uint16_t *cmp = sortArray;
        for (inner = outer; inner >= 0; inner--)
        {
            uint8_t *entryA = base + cmp[0];  /* current */
            uint8_t *entryB = base + cmp[1];  /* next */

            uint16_t teamIdA = *(uint16_t *)(entryA + 32);
            uint16_t teamIdB = *(uint16_t *)(entryB + 32);

            /* Empty entries (teamId == -1) sort to end */
            if (teamIdA == 0xFFFF)
                goto do_swap;

            if (teamIdB == 0xFFFF)
                goto no_swap;

            /* Compare scores (higher first) */
            {
                uint8_t scoreA = *(entryA + 35);
                uint8_t scoreB = *(entryB + 35);

                if (scoreA > scoreB)
                    goto no_swap;
                if (scoreA < scoreB)
                    goto do_swap;
            }

            /* Equal scores — compare surnames alphabetically */
            {
                uint8_t *surnameA = GetScorerSurname(entryA);
                uint8_t *surnameB = GetScorerSurname(entryB);
                uint8_t chB;

                for (;;)
                {
                    chB = *surnameB++;
                    if (chB == 0)
                        goto no_swap;  /* B is shorter or equal → keep order */
                    if (chB < *surnameA)
                        goto no_swap;
                    if (chB > *surnameA)
                        goto do_swap;
                    surnameA++;
                }
            }

do_swap:
            {
                uint16_t tmp = cmp[0];
                cmp[0] = cmp[1];
                cmp[1] = tmp;
            }
no_swap:
            cmp++;
        }
    }
}


/* ================================================================
 * ShowCompetitionTopScorers  (SWOS2.s line 51366, loc_1382F6)
 *
 * Initializes, populates, sorts the highest scorers table, then
 * displays the competition top goal scorers menu.
 * d0 = statIndex, a0 = name pointer for menu title
 * ================================================================ */
static void ShowCompetitionTopScorers(int16_t statIndex, uint8_t *namePtr)
{
    if (statIndex == 4)
        statIndex = 0;

    InitHighestScorersTable();
    PopulateHighestScorersTableFromSelectedTeams(statIndex);
    diyStatsName = (uint32_t)namePtr;
    SortHighestScorerTable();
    _ShowMenu(competitionTopGoalScorersMenu);
}


/* ================================================================
 * TopGoalScorersMenu_0  (SWOS2.s line 51389)
 *
 * Entry point for career/overall top goal scorers display.
 * Uses statIndex = -1 (all competitions summed).
 * ================================================================ */
__attribute__((used, externally_visible))
void _TopGoalScorersMenu_0(void)
{
    InitHighestScorersTable();
    PopulateHighestScorersTableFromSelectedTeams(-1);
    SortHighestScorerTable();
    _ShowMenu(topGoalScorersMenu);
}


/* ================================================================
 * StatsHighestScorerList  (SWOS2.s line 55634)
 *
 * Entry point from stats menu — loads statIndex and name
 * from globals, then calls ShowCompetitionTopScorers.
 * ================================================================ */
__attribute__((used, externally_visible))
void _StatsHighestScorerList(void)
{
    uint8_t *namePtr = (uint8_t *)diyNameOfCompetitionPtr;
    int16_t statIdx = (int16_t)diyUnknownWord_2C;
    ShowCompetitionTopScorers(statIdx, namePtr);
}


/* ================================================================
 * CompetitionTopGoalScorersInit  (SWOS2.s line 52406)
 *
 * Menu init callback for competition top goal scorers.
 * Populates menu entries with scorer name, (team), and score.
 * ================================================================ */
__attribute__((used, externally_visible))
void _CompetitionTopGoalScorersInit(void)
{
    MenuEntry *a5 = &menuEntries[0];
    uint8_t *base = highestScorersTable;
    uint16_t *sortArray = (uint16_t *)base;
    int16_t i;

    /* First entry: set title text pointer from diyStatsName */
    a5->fg = diyStatsName;

    /* Skip to entry at offset 0xA8/56 = 3 */
    a5 = &menuEntries[3];

    for (i = 14; i >= 0; i--)
    {
        uint8_t *entry;
        uint16_t sortIdx = *sortArray++;
        entry = base + sortIdx;

        if (*(uint16_t *)(entry + 32) == 0xFFFF)
        {
            /* Empty entry — hide 4 sub-entries */
            a5->isInvisible = 1;
            a5 += 1;  /* 0x38/56 = 1 entry */
            a5->isInvisible = 1;
            a5 += 1;
            a5->isInvisible = 1;
            a5 += 1;
            a5->isInvisible = 1;
            a5 += 1;
            continue;
        }

        /* Valid entry */
        {
            uint16_t teamId = *(uint16_t *)(entry + 32);
            uint8_t playerIdxByte = *(entry + 34);
            int16_t playerIdxSigned = (int8_t)playerIdxByte;
            uint8_t *teamData;
            uint8_t *teamName;
            uint8_t *playerPtr;
            uint8_t *dest;

            /* Get team name */
            teamData = _GetTeamPointer(teamId);
            teamName = teamData + 5;  /* team name at offset 5 */

            /* Get player name */
            playerPtr = _GetPlayer(teamId, (uint16_t)playerIdxSigned);

            /* Entry 1: player name */
            a5 += 1;
            dest = (uint8_t *)a5->fg;  /* string buffer pointer */
            {
                uint8_t *src = playerPtr + 0x4F;  /* 0x4C + 3 = playerName */
                uint8_t *d = dest;
                _StringCopy(src, &d);
            }

            /* Entry 2: "(teamname)" */
            a5 += 1;
            dest = (uint8_t *)a5->fg;
            {
                uint8_t *d = dest;
                _StringCopy(aLparen, &d);
                _StringCopy(teamName, &d);
                _StringCopy(aRparen, &d);
                *d = 0;
            }

            /* Entry 3: score number */
            a5 += 1;
            {
                uint8_t score = *(entry + 35);
                a5->fg = (a5->fg & 0xFFFF0000u) | (uint16_t)score;
            }

            a5 += 1;
        }
    }
}


/* ================================================================
 * TopGoalScorersInit  (SWOS2.s line 52825)
 *
 * Menu init for career top goal scorers (multi-competition view).
 * Adjusts entry layout based on which competitions are active.
 * ================================================================ */
__attribute__((used, externally_visible))
void _TopGoalScorersInit(void)
{
    /* If no tertiary competition, shift entries down */
    if (tertiaryCompetitionPtr == 0)
    {
        /* Shift column headers: move entry[23]→entry[22], entry[24]→entry[23] */
        MenuEntry *e22 = &menuEntries[0x508 / 56];  /* entry 22 (at +$508) */
        MenuEntry *e23 = &menuEntries[0x540 / 56];  /* entry 23 (at +$540) */
        MenuEntry *e24 = &menuEntries[0x578 / 56];  /* entry 24 (at +$578) */
        e22->x = e23->x;
        e23->x = e24->x;

        /* Shift row label entries similarly */
        MenuEntry *e1 = &menuEntries[0x38 / 56];   /* entry 1 (at +$38) */
        MenuEntry *e2 = &menuEntries[0x70 / 56];   /* entry 2 (at +$70) */
        MenuEntry *e3 = &menuEntries[0xA8 / 56];   /* entry 3 (at +$A8) */
        e1->x = e2->x;
        e2->x = e3->x;

        /* Hide entry 3 */
        menuEntries[0xA8 / 56].isInvisible = 1;

        /* Set CUP label on entry 2 */
        MenuEntry *cupEntry = &menuEntries[0x70 / 56];
        cupEntry->fg = (uint32_t)aCup_10;
    }

    /* If no secondary competition, shift further */
    if (secondaryCompetitionPtr == 0)
    {
        MenuEntry *e22 = &menuEntries[0x508 / 56];
        MenuEntry *e23 = &menuEntries[0x540 / 56];
        e22->x = e23->x;

        MenuEntry *e1 = &menuEntries[0x38 / 56];
        MenuEntry *e2 = &menuEntries[0x70 / 56];
        e1->x = e2->x;

        /* Hide entries 2 and 1 */
        menuEntries[0x70 / 56].isInvisible = 1;
        menuEntries[0x38 / 56].isInvisible = 1;
    }
}


/* ================================================================
 * TopGoalScorersBeforeDrawCommon  (SWOS2.s line 52857)
 *
 * BeforeDraw callback for each scorer row in career top scorers.
 * entry (a5) ordinal determines which scorer to display.
 * Sets up 6 overlay menu entries with: surname, (team), and
 * per-competition goal counts + total.
 * ================================================================ */
__attribute__((used, externally_visible))
void _TopGoalScorersBeforeDrawCommon(MenuEntry *entry)
{
    uint8_t *base = highestScorersTable;
    uint16_t ordinal = entry->ordinal;
    int16_t sortPos = (int16_t)(ordinal - 6);
    int16_t sortOffset = sortPos * 2;  /* word offset into sort array */
    uint16_t yCoord = entry->y;
    MenuEntry *a4 = &menuEntries[0x498 / 56];  /* entry 21 */
    uint16_t sortIdx;
    uint8_t *scorerEntry;

    uint8_t minScoreVal = base[30];

    sortIdx = *(uint16_t *)(base + sortOffset);
    scorerEntry = base + sortIdx;

    /* Check if entry is valid */
    if (*(uint16_t *)(scorerEntry + 32) == 0xFFFF)
    {
        /* Invalid entry — hide all 6 overlay entries and the main entry */
        entry->isInvisible = 1;
        a4->y = yCoord;
        a4->isInvisible = 1;
        a4 += 1;
        a4->y = yCoord;
        a4->isInvisible = 1;
        a4 += 1;
        a4->y = yCoord;
        a4->isInvisible = 1;
        a4 += 1;
        a4->y = yCoord;
        a4->isInvisible = 1;
        a4 += 1;
        a4->y = yCoord;
        a4->isInvisible = 1;
        a4 += 1;
        a4->y = yCoord;
        a4->isInvisible = 1;
        a4 += 1;
        return;
    }

    /* Valid scorer entry */
    {
        uint16_t teamId = *(uint16_t *)(scorerEntry + 32);
        uint8_t playerIdxByte = *(scorerEntry + 34);
        int16_t playerIdxSigned = (int8_t)playerIdxByte;
        uint8_t *teamData;
        uint8_t *teamName;
        uint8_t *playerPtr;  /* result of GetPlayer, a5 in asm */
        uint8_t *surnameBuf;
        uint8_t *dest;
        uint16_t d0val;

        teamData = _GetTeamPointer(teamId);
        teamName = teamData + 5;

        playerPtr = _GetPlayer(teamId, (uint16_t)playerIdxSigned);

        /* Extract surname using ExtractSurname (name at offset 0x4F, maxLen 0x15=21) */
        surnameBuf = _ExtractSurname(playerPtr + 0x4F, 0x15);

        /* Entry 0: player surname */
        a4->fg = (uint32_t)surnameBuf;
        a4->y = yCoord;
        a4->isInvisible = 0;
        a4 += 1;

        /* Entry 1: "(teamname)" */
        dest = (uint8_t *)a4->fg;
        {
            uint8_t *d = dest;
            _StringCopy(aLparen, &d);
            _StringCopy(teamName, &d);
            _StringCopy(aRparen, &d);
            *d = 0;
        }
        a4->y = yCoord;
        a4->isInvisible = 0;
        a4 += 1;

        /* Entry 2: league goals (from secondaryCompetitionPtr) */
        d0val = 0;
        a4->isInvisible = 1;
        if (secondaryCompetitionPtr != 0)
        {
            d0val = *(playerPtr + 0x6D);     /* goalStats[0] */
            a4->fg = (a4->fg & 0xFFFF0000u) | d0val;
            a4->y = yCoord;
            a4->isInvisible = 0;
        }
        a4 += 1;

        /* Entry 3: cup goals (from secondaryCompetitionPtr) */
        a4->isInvisible = 1;
        if (secondaryCompetitionPtr != 0)
        {
            d0val = *(playerPtr + 0x6E);     /* goalStats[1] */
            a4->fg = (a4->fg & 0xFFFF0000u) | d0val;
            a4->y = yCoord;
            a4->isInvisible = 0;
        }
        a4 += 1;

        /* Entry 4: tertiary competition goals */
        a4->isInvisible = 1;
        if (tertiaryCompetitionPtr != 0)
        {
            d0val = *(playerPtr + 0x6F);     /* goalStats[2] */
            a4->fg = (a4->fg & 0xFFFF0000u) | d0val;
            a4->y = yCoord;
            a4->isInvisible = 0;
        }
        a4 += 1;

        /* Entry 5: total score from scorer table entry */
        {
            uint8_t totalScore = *(scorerEntry + 35);
            d0val = totalScore;
            a4->fg = (a4->fg & 0xFFFF0000u) | d0val;
            a4->y = yCoord;
            a4->isInvisible = 0;
        }
        a4 += 1;

        /* Draw all 6 overlay entries starting from entry 21 */
        {
            MenuEntry *drawStart = &menuEntries[0x498 / 56];
            _DrawMultipleItems(6, drawStart);
        }
    }
}
