/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* game_types.h — Shared game struct definitions for SWOS C port
 *
 * Adapted from data/SWOS2_IDA.h (IDA Pro export).
 * All IDA annotations stripped, types mapped to stdint-style typedefs.
 * Packing via SWOS_PACKED_ALIGN(2) to match 68k memory layout.
 */
#ifndef GAME_TYPES_H
#define GAME_TYPES_H

/* All fixed-width integer types (uint8_t..uint64_t, intN_t, uintptr_t,
 * size_t) come from swos_types.h. On hosted builds it pulls in
 * <stdint.h>/<stddef.h>; on Amiga it provides manual typedefs. */
#include "swos_types.h"

/* ---- Forward declarations ---- */
typedef struct Sprite Sprite;
typedef struct AnimTable AnimTable;
typedef struct TeamGeneralInfo TeamGeneralInfo;
typedef struct TeamStatisticsData TeamStatisticsData;
typedef struct TeamGame TeamGame;
typedef struct PlayerInfo PlayerInfo;
typedef struct GoalieSkill GoalieSkill;
typedef struct SpriteGraphics SpriteGraphics;
typedef struct GoalInfo GoalInfo;
typedef struct ScorerInfo ScorerInfo;

/* ==================================================================
 * FixedPoint — 16.16 fixed-point (M68k big-endian: high=integer, low=frac)
 * ================================================================== */
typedef union {
    int32_t raw;                    /* full 32-bit value */
    struct {
#if defined(AMIGA_FIXEDPOINT_LE) && AMIGA_FIXEDPOINT_LE
        uint16_t frac;              /* fractional part (low addr on LE) */
        int16_t  i;                 /* integer part (high addr on LE) */
#else
        int16_t  i;                 /* integer part (high word, BE) */
        uint16_t frac;              /* fractional part (low word, BE) */
#endif
    } s;
} FixedPoint;

#ifndef __APPLE__
typedef char _fixedpoint_size_check[(sizeof(FixedPoint) == 4) ? 1 : -1];
#endif

/* ==================================================================
 * Sprite — 110 bytes, pack(2)
 * ================================================================== */
struct SWOS_PACKED_ALIGN(2) Sprite {
    int16_t    teamNumber;          /* 0 */
    int16_t    playerOrdinal;       /* 2 */
    int16_t    frameOffset;         /* 4 */
    AnimTable *animationTable;       /* 6: pointer to AnimTable */
    int16_t    startingDirection;   /* 10 */
    uint8_t    playerState;         /* 12 */
    int8_t     playerDownTimer;     /* 13 */
    int16_t    field_E;             /* 14 */
    int16_t    field_10;            /* 16 */
    int16_t   *frameIndicesTable;   /* 18: ptr to int16_t[] */
    int16_t    frameIndex;          /* 22 */
    int16_t    frameDelay;          /* 24 */
    int16_t    cycleFramesTimer;    /* 26 */
    int16_t    frameSwitchCounter;  /* 28 */
    FixedPoint x;                   /* 30 */
    FixedPoint y;                   /* 34 */
    FixedPoint z;                   /* 38 */
    int16_t    direction;           /* 42 */
    int16_t    speed;               /* 44 */
    FixedPoint deltaX;              /* 46 */
    FixedPoint deltaY;              /* 50 */
    FixedPoint deltaZ;              /* 54 */
    int16_t    destX;               /* 58 */
    int16_t    destY;               /* 60 */
    uint8_t    field_3E[6];         /* 62 */
    int16_t    visible;             /* 68 */
    int16_t    imageIndex;          /* 70 */
    int16_t    saveSprite;          /* 72 */
    int32_t    ballDistance;        /* 74 */
    int16_t    field_4E;            /* 78 */
    int16_t    field_50;            /* 80 */
    int16_t    fullDirection;       /* 82 */
    int16_t    onScreen;            /* 84 */
    int16_t    field_56;            /* 86 */
    int16_t    field_58;            /* 88 */
    int16_t    field_5A;            /* 90 */
    int16_t    playerDirection;     /* 92 */
    int16_t    isMoving;            /* 94 */
    int16_t    tackleState;         /* 96 */
    int16_t    heading;             /* 98 */
    int16_t    destReachedState;    /* 100 */
    int16_t    cards;               /* 102 */
    int16_t    injuryLevel;         /* 104 */
    int16_t    tacklingTimer;       /* 106 */
    int16_t    sentAway;            /* 108 */
};

/* Sprite has 2 pointer fields (animationTable, frameIndicesTable);
 * each grows from 4→8 bytes on 64-bit. */
#if UINTPTR_MAX == 0xFFFFFFFFu
#ifndef __APPLE__
typedef char _sprite_size_check[(sizeof(Sprite) == 110) ? 1 : -1];
#endif
#else
#ifndef __APPLE__
typedef char _sprite_size_check[(sizeof(Sprite) == 118) ? 1 : -1];
#endif
#endif

#define SPRITE_SIZE 110

/* ==================================================================
 * AnimTable — animation frame table variants
 * Standard has 32 frame pointers, variants have fewer.
 * ================================================================== */
struct SWOS_PACKED_ALIGN(2) AnimTable {
    int16_t  frameDelay;
    int16_t *animFrames[32];
};

typedef struct SWOS_PACKED_ALIGN(2) {
    int16_t  frameDelay;
    int16_t *animFrames[20];
} AnimTable_20;

typedef struct SWOS_PACKED_ALIGN(2) {
    int16_t  frameDelay;
    int16_t *animFrames[12];
} AnimTable_12;

typedef struct SWOS_PACKED_ALIGN(2) {
    int16_t  frameDelay;
    int16_t *animFrames[8];
} AnimTable_8;

/* Compile-time size checks for AnimTable variants */
/* AnimTable* have `int16_t *animFrames[N]` where N varies per variant;
 * each pointer grows from 4→8 bytes on 64-bit. 32-bit/64-bit sizes:
 *   AnimTable    : N=32 → 2 + 32*4 = 130 / 2 + 32*8 = 258
 *   AnimTable_20 : N=20 → 2 + 20*4 = 82  / 2 + 20*8 = 162
 *   AnimTable_12 : N=12 → 2 + 12*4 = 50  / 2 + 12*8 = 98
 *   AnimTable_8  : N=8  → 2 +  8*4 = 34  / 2 +  8*8 = 66 */
#if UINTPTR_MAX == 0xFFFFFFFFu
#ifndef __APPLE__
typedef char _animtable_size_check[(sizeof(AnimTable) == 130) ? 1 : -1];
#endif
#ifndef __APPLE__
typedef char _animtable20_size_check[(sizeof(AnimTable_20) == 82) ? 1 : -1];
#endif
#ifndef __APPLE__
typedef char _animtable12_size_check[(sizeof(AnimTable_12) == 50) ? 1 : -1];
#endif
#ifndef __APPLE__
typedef char _animtable8_size_check[(sizeof(AnimTable_8) == 34) ? 1 : -1];
#endif
#else
#ifndef __APPLE__
typedef char _animtable_size_check[(sizeof(AnimTable) == 258) ? 1 : -1];
#endif
#ifndef __APPLE__
typedef char _animtable20_size_check[(sizeof(AnimTable_20) == 162) ? 1 : -1];
#endif
#ifndef __APPLE__
typedef char _animtable12_size_check[(sizeof(AnimTable_12) == 98) ? 1 : -1];
#endif
#ifndef __APPLE__
typedef char _animtable8_size_check[(sizeof(AnimTable_8) == 66) ? 1 : -1];
#endif
#endif

/* ==================================================================
 * TeamGeneralInfo — 144 bytes, pack(1)
 * ================================================================== */
struct SWOS_PACKED_ALIGN(2) TeamGeneralInfo {
    TeamGeneralInfo   *opponentsTeam;              /* 0 */
    int16_t            playerNumber;               /* 4 */
    int16_t            playerCoachNumber;           /* 6 */
    int16_t            isPlCoach;                   /* 8 */
    TeamGame          *inGameTeamPtr;               /* 10 */
    TeamStatisticsData *teamStatsPtr;               /* 14 */
    int16_t            teamNumber;                  /* 18 */
    Sprite           **spritesTable;                /* 20 */
    GoalieSkill       *shotChanceTable;             /* 24 */
    int16_t            tactics;                     /* 28 */
    int16_t            updatePlayerIndex;           /* 30 */
    Sprite            *controlledPlayer;            /* 32 */
    Sprite            *passToPlayerPtr;             /* 36 */
    int16_t            playerHasBall;               /* 40 */
    int16_t            allowedDirections;           /* 42 */
    int16_t            currentAllowedDirection;     /* 44 */
    int16_t            direction;                   /* 46 */
    int8_t             quickFire;                   /* 48 */
    int8_t             normalFire;                  /* 49 */
    int8_t             firePressed;                 /* 50 */
    int8_t             fireThisFrame;               /* 51 */
    int16_t            headerOrTackle;              /* 52 */
    int16_t            fireCounter;                 /* 54 */
    int16_t            controlledPlDirection;       /* 56 */
    int16_t            shooting;                    /* 58 */
    int8_t             field_3C;                    /* 60 */
    int8_t             plVeryCloseToBall;           /* 61 */
    int8_t             plCloseToBall;               /* 62 */
    int8_t             plNotFarFromBall;            /* 63 */
    int8_t             ballLessEqual4;              /* 64 */
    int8_t             ball4To8;                    /* 65 */
    int8_t             ball8To12;                   /* 66 */
    int8_t             ball12To17;                  /* 67 */
    int8_t             ballAbove17;                 /* 68 */
    int8_t             prevPlVeryCloseToBall;       /* 69 */
    int16_t            field_46;                    /* 70 */
    Sprite            *lastHeadingTacklingPlayer;   /* 72 */
    int16_t            goalkeeperSavedCommentTimer; /* 76 */
    int16_t            field_4E;                    /* 78 */
    int16_t            goalkeeperDivingRight;       /* 80 */
    int16_t            goalkeeperDivingLeft;        /* 82 */
    int16_t            ballOutOfPlayOrKeeper;       /* 84 */
    int16_t            goaliePlayingOrOut;          /* 86 */
    int16_t            passingBall;                 /* 88 */
    int16_t            passingToPlayer;             /* 90 */
    int16_t            playerSwitchTimer;           /* 92 */
    int16_t            ballInPlay;                  /* 94 */
    int16_t            ballOutOfPlay;               /* 96 */
    int16_t            ballX;                       /* 98 */
    int16_t            ballY;                       /* 100 */
    int16_t            passKickTimer;               /* 102 */
    Sprite            *passingKickingPlayer;        /* 104 */
    int16_t            unkBallTimer;                /* 108 */
    int16_t            ballCanBeControlled;         /* 110 */
    int16_t            ballControllingPlayerDirection; /* 112 */
    int16_t            field_72;                    /* 114 */
    int16_t            field_74;                    /* 116 */
    int16_t            spinTimer;                   /* 118 */
    int16_t            leftSpin;                    /* 120 */
    int16_t            rightSpin;                   /* 122 */
    int16_t            longPass;                    /* 124 */
    int16_t            longSpinPass;                /* 126 */
    int16_t            passInProgress;              /* 128 */
    int16_t            AI_timer;                    /* 130 */
    int16_t            field_84;                    /* 132 */
    int16_t            AI_afterTouchStrength;       /* 134 */
    int16_t            AI_ballSpinDirection;        /* 136 */
    int16_t            wonTheBallTimer;             /* 138 */
    int16_t            goalkeeperPlaying;           /* 140 */
    int16_t            resetControls;               /* 142 */
};

/* TeamGeneralInfo has 9 pointer fields (opponentsTeam, inGameTeamPtr,
 * teamStatsPtr, spritesTable, shotChanceTable, controlledPlayer,
 * passToPlayerPtr, lastHeadingTacklingPlayer, passingKickingPlayer);
 * +36 bytes on 64-bit. */
#if UINTPTR_MAX == 0xFFFFFFFFu
#ifndef __APPLE__
typedef char _tgi_size_check[(sizeof(TeamGeneralInfo) == 144) ? 1 : -1];
#endif
#else
#ifndef __APPLE__
typedef char _tgi_size_check[(sizeof(TeamGeneralInfo) == 180) ? 1 : -1];
#endif
#endif

/* ==================================================================
 * PlayerInfo — 61 bytes (embedded in TeamGame)
 * ================================================================== */
struct PlayerInfo {
    int8_t  skip;               /* 0 */
    int8_t  playerIndex;        /* 1 */
    int8_t  goalsScored;        /* 2 */
    int8_t  shirtNumber;        /* 3 */
    int8_t  position;           /* 4 */
    int8_t  face;               /* 5 */
    int8_t  isInjured;          /* 6 */
    int8_t  field_7;            /* 7 */
    int8_t  field_8;            /* 8 */
    int8_t  previousCards;      /* 9 */
    int8_t  numYellowCards;     /* 10 */
    int8_t  field_B;            /* 11 */
    int8_t  shortName[15];      /* 12 */
    int8_t  passing;            /* 27 */
    int8_t  shooting;           /* 28 */
    int8_t  heading;            /* 29 */
    int8_t  tackling;           /* 30 */
    int8_t  ballControl;        /* 31 */
    int8_t  speed;              /* 32 */
    int8_t  finishing;          /* 33 */
    int8_t  goalieSkill;        /* 34 */
    int8_t  injuriesBitfield;   /* 35 */
    int8_t  halfPlayed;         /* 36 */
    int8_t  face2;              /* 37 */
    int8_t  fullName[23];       /* 38 */
};

#ifndef __APPLE__
typedef char _playerinfo_size_check[(sizeof(PlayerInfo) == 61) ? 1 : -1];
#endif

#define PLAYERINFO_SIZE 61

/* ==================================================================
 * TeamGame — 1018 bytes (0x3FA)
 * ================================================================== */
struct TeamGame {
    int16_t    prShirtType;     /* 0 */
    int16_t    prShirtCol;      /* 2 */
    int16_t    prStripesCol;    /* 4 */
    int16_t    prShortsCol;     /* 6 */
    int16_t    prSocksCol;      /* 8 */
    int16_t    secShirtType;    /* 10 */
    int16_t    secShirtCol;     /* 12 */
    int16_t    secStripesCol;   /* 14 */
    int16_t    secShortsCol;    /* 16 */
    int16_t    secSocksCol;     /* 18 */
    int16_t    markedPlayer;    /* 20 */
    int8_t     teamName[17];    /* 22 */
    int8_t     unk1;            /* 39 */
    int8_t     numOwnGoals;     /* 40 */
    int8_t     unk3;            /* 41 */
    PlayerInfo players[16];     /* 42 */
};

#ifndef __APPLE__
typedef char _teamgame_size_check[(sizeof(TeamGame) == 1018) ? 1 : -1];
#endif

#define TEAMGAME_SIZE 1018
#define TEAMGAME_PLAYERS_OFFSET 42  /* byte offset of players[] within TeamGame */

/* ==================================================================
 * TeamStatisticsData — 14 bytes
 * ================================================================== */
struct TeamStatisticsData {
    int16_t ballPossession;     /* 0 */
    int16_t cornersWon;         /* 2 */
    int16_t foulsConceded;      /* 4 */
    int16_t bookings;           /* 6 */
    int16_t sendingsOff;        /* 8 */
    int16_t goalAttempts;       /* 10 */
    int16_t onTarget;           /* 12 */
};

#ifndef __APPLE__
typedef char _teamstats_size_check[(sizeof(TeamStatisticsData) == 14) ? 1 : -1];
#endif

/* ==================================================================
 * GoalieSkill — 60 bytes
 * ================================================================== */
struct GoalieSkill {
    int16_t field_0;            /* 0 */
    int16_t field_2;            /* 2 */
    int16_t field_4;            /* 4 */
    int16_t speed_unk;          /* 6 */
    int16_t speed_unk2;         /* 8 */
    int16_t index[16];          /* 10 */
    int16_t field_2A;           /* 42 */
    int16_t field_2C;           /* 44 */
    int16_t field_2E;           /* 46 */
    int16_t field_30;           /* 48 */
    int16_t field_32;           /* 50 */
    int16_t field_34;           /* 52 */
    int16_t field_36;           /* 54 */
    int16_t field_38;           /* 56 */
    int16_t field_3A;           /* 58 */
};

#ifndef __APPLE__
typedef char _goalieskill_size_check[(sizeof(GoalieSkill) == 60) ? 1 : -1];
#endif

/* ==================================================================
 * SpriteGraphics — 24 bytes
 * ================================================================== */
struct SpriteGraphics {
    uint8_t *sprData;           /* 0: ptr to pixel data */
    uint8_t *maskPtr;           /* 4: ptr to mask data (NULL = no mask) */
    uint16_t type;              /* 8: 0=separate planes, nonzero=linked/interleaved */
    int16_t  pixWidth;          /* 10 */
    int16_t  nlines;            /* 12 */
    int16_t  wquads;            /* 14: width in 16-pixel quads */
    int16_t  centerX;           /* 16 */
    int16_t  centerY;           /* 18 */
    uint16_t bltsize;            /* 20: pre-computed blitter size value */
    int16_t  ordinal;           /* 22 */
};

/* SpriteGraphics has 2 pointer fields (sprData, maskPtr); +8 on 64-bit. */
#if UINTPTR_MAX == 0xFFFFFFFFu
#ifndef __APPLE__
typedef char _spritegfx_size_check[(sizeof(SpriteGraphics) == 24) ? 1 : -1];
#endif
#else
#ifndef __APPLE__
typedef char _spritegfx_size_check[(sizeof(SpriteGraphics) == 32) ? 1 : -1];
#endif
#endif

#define SPRITEGRAPHICS_SIZE 24

/* ==================================================================
 * CharsTableHeader — font/character rendering table header (14 bytes)
 * The conversion table data (uint8_t[]) follows immediately after.
 * ================================================================== */
typedef struct {
    uint8_t  reserved[8];       /* 0-7: unknown/unused header data */
    int16_t  charSpacing;       /* 8 */
    int16_t  spaceWidth;        /* 10 */
    int16_t  spriteIndexOffset; /* 12 */
    /* conversion table uint8_t[] follows at offset 14 */
} CharsTableHeader;

#ifndef __APPLE__
typedef char _charstblhdr_size_check[(sizeof(CharsTableHeader) == 14) ? 1 : -1];
#endif

/* ==================================================================
 * Player state constants (from PlayerStates enum in IDA)
 * ================================================================== */
#define PL_NORMAL                0
#define PL_TACKLING              1
#define PL_TACKLED               3
#define PL_GOALIE_CATCHING_BALL  4
#define PL_THROW_IN              5
#define PL_GOALIE_DIVING_HIGH    6
#define PL_GOALIE_DIVING_LOW     7
#define PL_STATIC_HEADING        8
#define PL_JUMP_HEADING          9
#define PL_DOWN                  10
#define PL_GOALIE_CLAIMED        11
#define PL_BOOKED                12
#define PL_ROLLING_INJURED       13
#define PL_SAD                   14
#define PL_HAPPY                 15

/* ==================================================================
 * Game state constants (from GameState enum in IDA)
 * ================================================================== */
#define ST_PLAYERS_TO_INITIAL_POSITIONS  0
#define ST_GOAL_OUT_LEFT                 1
#define ST_GOAL_OUT_RIGHT                2
#define ST_KEEPER_HOLDS_BALL             3
#define ST_CORNER_LEFT                   4
#define ST_CORNER_RIGHT                  5
#define ST_FREE_KICK_LEFT1               6
#define ST_FREE_KICK_LEFT2               7
#define ST_FREE_KICK_LEFT3               8
#define ST_FREE_KICK_CENTER              9
#define ST_FREE_KICK_RIGHT1              10
#define ST_FREE_KICK_RIGHT2              11
#define ST_FREE_KICK_RIGHT3              12
#define ST_FOUL                          13
#define ST_PENALTY                       14
#define ST_THROW_IN_FORWARD_RIGHT        15
#define ST_THROW_IN_CENTER_RIGHT         16
#define ST_THROW_IN_BACK_RIGHT           17
#define ST_THROW_IN_FORWARD_LEFT         18
#define ST_THROW_IN_CENTER_LEFT          19
#define ST_THROW_IN_BACK_LEFT            20
#define ST_STARTING_GAME                 21
#define ST_CAMERA_GOING_TO_SHOWERS       22
#define ST_GOING_TO_HALFTIME             23
#define ST_PLAYERS_GOING_TO_SHOWER       24
#define ST_RESULT_ON_HALFTIME            25
#define ST_RESULT_AFTER_THE_GAME         26
#define ST_FIRST_EXTRA_STARTING          27
#define ST_FIRST_EXTRA_ENDED             28
#define ST_FIRST_HALF_ENDED              29
#define ST_GAME_ENDED                    30
#define ST_PENALTIES                      31
#define ST_GAME_IN_PROGRESS              100
#define ST_STOPPED                       101
#define ST_WAITING_ON_PLAYER             102

/* ==================================================================
 * TeamFile — on-disk team data format (header portion, 76 bytes)
 * Full entry is 684 bytes including player data that follows.
 * ================================================================== */
struct SWOS_PACKED_ALIGN(2) TeamFile {
    uint8_t  countryNumber;      /* 0 */
    uint8_t  teamOrdinal;        /* 1 */
    int16_t  globalTeamNumber;   /* 2 */
    uint8_t  teamControls;       /* 4 */
    int8_t   teamName[17];       /* 5 */
    uint8_t  writesZeroHere;     /* 22 */
    uint8_t  andWith0xFE;        /* 23 */
    uint8_t  tactics;            /* 24 */
    uint8_t  league;             /* 25 */
    uint8_t  prShirtType;        /* 26 */
    uint8_t  prStripesColor;     /* 27 */
    uint8_t  prBasicColor;       /* 28 */
    uint8_t  prShortsColor;      /* 29 */
    uint8_t  prSocksColor;       /* 30 */
    uint8_t  secShirtType;       /* 31 */
    uint8_t  secStripesColor;    /* 32 */
    uint8_t  secBasicColor;      /* 33 */
    uint8_t  secShortsColor;     /* 34 */
    uint8_t  secSocksColor;      /* 35 */
    int8_t   coachName[23];      /* 36 */
    uint8_t  someFlag;           /* 59 */
    uint8_t  playerNumbers[16];  /* 60 */
    /* player data follows at offset 76 */
};

#ifndef __APPLE__
typedef char _teamfile_size_check[(sizeof(struct TeamFile) == 76) ? 1 : -1];
#endif

typedef struct TeamFile TeamFile;

/* ==================================================================
 * GoalInfo — 6 bytes
 * goalType: 0=normal, 1=penalty, 2=own goal
 * time: 4 bytes (ASCII digits, patched in from gameTime[])
 * ================================================================== */
struct GoalInfo {
    int16_t goalType;           /* 0 */
    int8_t  time[4];            /* 2: four ASCII digit bytes */
};

#ifndef __APPLE__
typedef char _goalinfo_size_check[(sizeof(GoalInfo) == 6) ? 1 : -1];
#endif

/* ==================================================================
 * ScorerInfo — 66 bytes, 8 slots per team (528 bytes total)
 * ================================================================== */
struct ScorerInfo {
    int16_t  shirtNum;          /* 0: shirt number (or +100 for own goal) */
    int16_t  numSpritesTaken;   /* 2: number of sprite rows used */
    int16_t  numGoals;          /* 4: number of goals scored */
    GoalInfo goals[10];         /* 6: goal records (10 * 6 = 60 bytes) */
};

#ifndef __APPLE__
typedef char _scorerinfo_size_check[(sizeof(ScorerInfo) == 66) ? 1 : -1];
#endif

#define SCORERINFO_SIZE 66

/* Goal type constants */
#define GT_NORMAL   0
#define GT_PENALTY  1
#define GT_OWN_GOAL 2

/* ==================================================================
 * PlayerFile — 38 bytes per player within a team file
 * ================================================================== */
typedef struct SWOS_PACKED_ALIGN(2) PlayerFile {
    int8_t   nationality;          /* 0 */
    int8_t   field_1;              /* 1 */
    int8_t   shirtNumber;          /* 2 */
    int8_t   playerName[22];       /* 3-24 */
    int8_t   field_19;             /* 25 */
    uint8_t  positionAndFace;      /* 26 */
    uint8_t  cardsInjuries;        /* 27 */
    int8_t   passing;              /* 28 */
    int8_t   shootingHeading;      /* 29 */
    int8_t   tacklingBallControl;  /* 30 */
    int8_t   speedFinishing;       /* 31 */
    int8_t   price;                /* 32 */
    uint8_t  goalStats[4];         /* 33-36 */
    int8_t   someFlag;             /* 37 */
} PlayerFile;

#ifndef __APPLE__
typedef char _playerfile_size_check[(sizeof(PlayerFile) == 38) ? 1 : -1];
#endif

#define PLAYERFILE_SIZE 38

/* ==================================================================
 * TeamFileEx — TeamFile header (76 bytes) + 16 PlayerFile entries
 * Total: 684 bytes. Used for careerTeam, currentTeam, etc.
 * ================================================================== */
typedef struct SWOS_PACKED_ALIGN(2) TeamFileEx {
    TeamFile   team;               /* 0-75  (76 bytes) */
    PlayerFile players[16];        /* 76-683 (608 bytes) */
} TeamFileEx;

#ifndef __APPLE__
typedef char _teamfileex_size_check[(sizeof(TeamFileEx) == 684) ? 1 : -1];
#endif

/* ==================================================================
 * DIYCompetition — 96 bytes (0x60), base competition descriptor.
 * Used for diyFileBufferCopy and as the first 0x60 bytes of
 * diyCompetitionStart (which is actually 0x5C8 bytes total).
 *
 * Original IDA struct had opaque field_2[48] and field_3A[36];
 * sub-fields are broken out here based on actual access patterns.
 * ================================================================== */
typedef struct DIYCompetition {
    int16_t  competitionType;          /* 0x00 */
    /* --- field_2 sub-fields (48 bytes, 0x02-0x31) --- */
    uint16_t isActive;                 /* 0x02: 0=inactive, 1=in use */
    int8_t   competitionName[36];      /* 0x04: competition name string buffer */
    uint8_t *namePtr;                  /* 0x28: ptr to competition name string */
    uint8_t  compId;                   /* 0x2C: competition ID byte */
    uint8_t  slotId;                   /* 0x2D: competition slot index for cache matching */
    uint8_t  field_2E;                 /* 0x2E: team configuration byte */
    uint8_t  viewRandomPhase;          /* 0x2F: current month/phase for scheduling */
    uint8_t  avgSkill;                 /* 0x30: average team skill level */
    uint8_t  strengthTier;             /* 0x31: strength bracket tier */
    /* --- end field_2 --- */
    int16_t  numTeams;                 /* 0x32 */
    int16_t  pitchTypeOrSeason;        /* 0x34 */
    int16_t  pitchType;                /* 0x36 */
    int8_t   startMonth;               /* 0x38 */
    int8_t   endMonth;                 /* 0x39 */
    /* --- field_3A sub-fields (36 bytes, 0x3A-0x5D) --- */
    int16_t  resultCode;               /* 0x3A: match/competition result code, init 0xFFFF */
    int16_t  substitutesCount;         /* 0x3C: number of substitutes allowed */
    int16_t  slotStateA;               /* 0x3E: competition slot state A / menu ordinal, init 3 */
    int16_t  slotStateB;               /* 0x40: competition slot state B, init 2 */
    int16_t  slotStateC;               /* 0x42: competition slot state C / menu ordinal, init 8 */
    int16_t  matchStarted;             /* 0x44: 1 when match processing begins */
    int16_t  careerTeamInvolved;       /* 0x46: 1 when career team participates */
    int16_t  hasGroups;                /* 0x48: 0=no, 1=groups, 2=groups with final */
    int16_t  menuState;                /* 0x4A: cup menu state (0-6) */
    int16_t  roundIterator;            /* 0x4C: round iteration counter */
    int16_t  numTeamsInStage;          /* 0x4E: teams in current stage */
    int16_t  matchPairsPerRound;       /* 0x50: match pairs per round (half teams) */
    int16_t  totalMatchesInRound;      /* 0x52: total matches/fixtures in round */
    int16_t  roundComplete;            /* 0x54: 0=in progress, 1=round just completed */
    int16_t  competitionFinished;      /* 0x56: 1 when competition is fully complete */
    int16_t  currentRoundIndex;        /* 0x58: incremented after each completed round */
    int16_t  numRounds;                /* 0x5A: total rounds in competition */
    int16_t  subRoundCounter;          /* 0x5C: sub-round counter for multi-fixture rounds */
    /* --- end field_3A --- */
    int16_t  playEachTeam;             /* 0x5E */
} DIYCompetition;

/* DIYCompetition has 1 pointer field (namePtr); +4 from pointer growth
 * + 4 from 8-byte tail alignment (struct is NOT packed, so its
 * alignment matches the largest member = pointer = 8 bytes on 64-bit)
 * → +8 total. */
#if UINTPTR_MAX == 0xFFFFFFFFu
#ifndef __APPLE__
typedef char _diycompetition_size_check[(sizeof(DIYCompetition) == 0x60) ? 1 : -1];
#endif
#else
#ifndef __APPLE__
typedef char _diycompetition_size_check[(sizeof(DIYCompetition) == 0x68) ? 1 : -1];
#endif
#endif

/* ==================================================================
 * DIYTeamStats / DIYLeagueTableRow — 18 bytes
 * Per-team league standings entry written by clearLeagueTableStats
 * starting at diyCompetitionStart + 0x2B4. Each row is one teamSelOffset
 * (NOT cleared by the loop) plus 7 stats fields cleared per row.
 * 64 rows total; the table extends to offset 0x734 — past the end of
 * the 0x5C8-byte struct, into audioInstrumentBank's address range.
 * Intentional shared-memory overlay; see DIYCOMPETITIONEX_AUDIT.md.
 * ================================================================== */
typedef struct SWOS_PACKED_ALIGN(2) DIYTeamStats {
    int32_t  teamSelOffset;         /* 0x00: byte offset from selTeamsPtr to team record (NOT a pointer despite the historical "Ptr" name). Signed: careerTeam lives BELOW selTeamsPtr in BSS, so its offset is negative and must sign-extend on 64-bit when added back to &selTeamsPtr. */
    uint16_t gamesPlayed;           /* 0x04 */
    uint16_t wins;                  /* 0x06 */
    uint16_t draws;                 /* 0x08 */
    uint16_t losses;                /* 0x0A */
    uint16_t goalsFor;              /* 0x0C */
    uint16_t goalsAgainst;          /* 0x0E */
    uint16_t points;                /* 0x10 */
} DIYTeamStats;

typedef DIYTeamStats DIYLeagueTableRow;
#ifndef __APPLE__
typedef char _diyteamstats_size_check[(sizeof(DIYTeamStats) == 18) ? 1 : -1];
#endif

/* ==================================================================
 * DIYTournamentSlot — 118 bytes (0x76)
 *
 * Tournament-format slot record. SWOS2.s does not name this type, but
 * the C code in src/diy_competition.c iterates over a 6-element array
 * of these records embedded inside `diyTournamentTeamIds`, starting
 * at offset 0xC0 (= diyFileBufferCopy + 0x15A) with a stride of 0x76.
 *
 * Accessed via `slot = (uint8_t *)diyFileBufferCopy + slotIdx*0x76`
 * with field offsets +0x15A..+0x172 (24-byte field range), +0x174
 * (matchesPerSlot), +0x176 (progression month bytes tail).
 *
 * Field offsets within this struct begin at 0; the +0x15A bias from
 * the diyFileBufferCopy base is not represented here.
 *
 * See DIYCOMPETITIONEX_AUDIT.md for the per-field investigation.
 * ================================================================== */
typedef struct SWOS_PACKED_ALIGN(2) DIYTournamentSlot {
    uint8_t  _pad_00[2];            /* 0x00 (slot+0x15A): cleared to 0 in init, purpose unknown */
    uint16_t doubleCount;           /* 0x02 (slot+0x15C): cup-bracket doubled count flag */
    uint16_t isLeagueFormat;        /* 0x04 (slot+0x15E): nonzero=league/grouped, 0=cup */
    uint16_t seedingType;           /* 0x06 (slot+0x160) */
    uint16_t numTeams;              /* 0x08 (slot+0x162) */
    uint16_t divisionFactor;        /* 0x0A (slot+0x164): "next slot" / division factor */
    uint16_t playEachTeam;          /* 0x0C (slot+0x166) */
    uint16_t initConst168;          /* 0x0E (slot+0x168): init=2 */
    uint16_t groups;                /* 0x10 (slot+0x16A) */
    uint16_t teamsPerGroup;         /* 0x12 (slot+0x16C) */
    uint16_t formatFlag16E;         /* 0x14 (slot+0x16E): 1=normal, 2=skip */
    uint16_t initConst170;          /* 0x16 (slot+0x170): init=1 */
    uint16_t formatFlag172;         /* 0x18 (slot+0x172): 1=halve */
    uint16_t matchesPerSlot;        /* 0x1A (slot+0x174): computed matches-per-team this slot */
    uint8_t  progressionTable[90];  /* 0x1C (slot+0x176): month bytes — fills to 118 (0x76) */
} DIYTournamentSlot;

#ifndef __APPLE__
typedef char _diyslot_size_check[(sizeof(DIYTournamentSlot) == 0x76) ? 1 : -1];
#endif

/* ==================================================================
 * DIYCompetitionEx_CupView — 0xEA bytes (matchData cup interpretation)
 * Region at parent 0x64..0x14D when the competition is a cup
 * (competitionType == 1 or compId in {1, 2, 4}).
 * ================================================================== */
typedef struct SWOS_PACKED_ALIGN(2) DIYCompetitionEx_CupView {
    uint16_t currentMatchIndex;         /* 0x00 (parent 0x64) */
    uint16_t roundLegs[6];              /* 0x02 (parent 0x66): cup roundLegs[0..5] */
    uint16_t roundExtraTime[6];         /* 0x0E (parent 0x72): cup roundExtraTime[0..5] */
    uint16_t roundPenalties[6];         /* 0x1A (parent 0x7E): cup roundPenalties[0..5] */
    uint8_t  cupRoundResults[100];      /* 0x26 (parent 0x8A): diyCupRoundResults */
    uint8_t  _shuffleIndicesAlias[2];   /* 0x8A (parent 0xEE): aliases diyTeamShuffleIndices */
    uint8_t  _bss_2E01E[62];            /* 0x8C (parent 0xF0): BSS_00_2E01E (purpose unclear) */
    uint16_t leagueTeamIdsAlias[14];    /* 0xCA (parent 0x12E): aliases diyLeagueTeamIds */
    uint16_t competitionFormat;         /* 0xE6 (parent 0x14A): aliases diyCompetitionFormat */
    uint16_t roundFormat;               /* 0xE8 (parent 0x14C): aliases diyRoundFormat */
} DIYCompetitionEx_CupView;

#ifndef __APPLE__
typedef char _diycupview_size_check[(sizeof(DIYCompetitionEx_CupView) == 0xEA) ? 1 : -1];
#endif

/* ==================================================================
 * DIYCompetitionEx_LeagueView — 0xEA bytes
 * League competition interpretation of the match data region (0x64-0x14E).
 * Used when competitionType == 0 (league) or compId == 0.
 * ================================================================== */
typedef struct SWOS_PACKED_ALIGN(2) DIYCompetitionEx_LeagueView {
    uint16_t leagueMinSubstitutes;      /* 0x00 (parent 0x64) */
    uint16_t leagueMaxSubstitutes;      /* 0x02 (parent 0x66) */
    uint16_t hasGroupsDiy;              /* 0x04 (parent 0x68) */
    uint16_t totalMatchesPerSection;     /* 0x06 (parent 0x6A): BSS_00_2DF5E reset value */
    uint16_t totalMatchesPerSectionLive; /* 0x08 (parent 0x6C): BSS_00_2DF5E live counter, decrements per match */
    uint16_t teamEntryOffsets[14];      /* 0x0A (parent 0x6E): aliases diyTeamEntryOffsetsByIndex; sorted by standings, value/18 = team index */
    uint8_t  cupRoundResults[100];      /* 0x26 (parent 0x8A): same memory as cup view */
    uint8_t  shuffleAndPad[64];         /* 0x8A (parent 0xEE): diyTeamShuffleIndices(2) + BSS_00_2E01E(62) */
    uint16_t leagueTeamIds[14];         /* 0xCA (parent 0x12E): aliases diyLeagueTeamIds */
    uint16_t competitionFormat;         /* 0xE6 (parent 0x14A): aliases diyCompetitionFormat */
    uint16_t roundFormat;               /* 0xE8 (parent 0x14C): aliases diyRoundFormat */
} DIYCompetitionEx_LeagueView;

#ifndef __APPLE__
typedef char _diyleagueview_size_check[(sizeof(DIYCompetitionEx_LeagueView) == 0xEA) ? 1 : -1];
#endif

/* ==================================================================
 * DIYCompetitionEx_MatchData — 0xEA bytes (union)
 * Region at offset 0x64 within DIYCompetitionEx.
 * Same memory interpreted differently for cup vs league competitions.
 * ================================================================== */
typedef union SWOS_PACKED_ALIGN(2) DIYCompetitionEx_MatchData {
    DIYCompetitionEx_CupView cup;
    DIYCompetitionEx_LeagueView league;
    uint8_t raw[0xEA];
} DIYCompetitionEx_MatchData;

#ifndef __APPLE__
typedef char _diymatchdata_size_check[(sizeof(DIYCompetitionEx_MatchData) == 0xEA) ? 1 : -1];
#endif

/* ==================================================================
 * DIYCompetitionEx_LeagueSectionState — 136 bytes (0x88)
 * BSS_00_2E13E broken out: per-section progression counters and the
 * phase-start-month table used by the league progression path in
 * _DIY_RecomputeCompetitionDerivedFields and friends.
 * ================================================================== */
typedef struct SWOS_PACKED_ALIGN(2) DIYCompetitionEx_LeagueSectionState {
    uint16_t halfGroupCount;            /* 0x00 (parent 0x1C6) */
    uint16_t matchesPerHalf;            /* 0x02 (parent 0x1C8) */
    uint16_t halfGroupsCounter;         /* 0x04 (parent 0x1CA): == matchesPerHalf triggers boundary */
    uint16_t sectionMatchesProgress;    /* 0x06 (parent 0x1CC) */
    uint8_t  phaseStartMonths[128];     /* 0x08 (parent 0x1CE..0x24D): viewRandomPhase[phaseIdx] source */
} DIYCompetitionEx_LeagueSectionState;

#ifndef __APPLE__
typedef char _diysecstate_size_check[
#endif
    (sizeof(DIYCompetitionEx_LeagueSectionState) == 0x88) ? 1 : -1];

/* ==================================================================
 * DIYCompetitionEx — 0x5C8 bytes (1480), the extended runtime DIY
 * competition record at &diyCompetitionStart.
 *
 * Layout matches the SWOS2.s `ds.b` walk starting at line 2840
 * (diyCompetitionStart). The backing buffer (`diyCompetitionStartBuf`)
 * is 4096 bytes; this struct is a typed view of the first 0x5C8 bytes.
 *
 * NB: the league standings table written by clearLeagueTableStats
 * extends 1152 bytes from offset 0x2B4 to offset 0x734 — past the end
 * of this struct, into audioInstrumentBank's address range. That is
 * intentional shared-memory overlay (audio bank not loaded during DIY
 * init) and is not represented as a struct field; it is reached only
 * via the raw `clearLeagueTableStats` byte arithmetic.
 *
 * `diyFileBufferCopy` is a SEPARATE buffer that shares only the first
 * 0x60 bytes of layout (DIYCompetition base). Its post-base region has
 * a different adjacent-globals layout (BSS_00_2DB6A, then
 * diyTournamentTeamIds), so this struct does NOT model it past byte
 * 0x60. The 6-element DIYTournamentSlot array embedded inside
 * diyTournamentTeamIds is reached via raw pointer arithmetic, not
 * through this struct.
 * ================================================================== */
typedef struct SWOS_PACKED_ALIGN(2) DIYCompetitionEx {
    DIYCompetition base;                /* 0x00..0x5F: competition header */

    /* --- 0x60..0x63: SWOS2.s named diyMinSubstitutes / diyPointsForAWin --- */
    uint16_t minSubstitutes;            /* 0x60: diyMinSubstitutes (also overridden = playEachTeam in derived-fields recompute) */
    uint16_t pointsForAWin;             /* 0x62: diyPointsForAWin */

    /* --- 0x64..0x14D: cup/league shared region (aliased view) --- */
    DIYCompetitionEx_MatchData matchData;

    /* --- 0x14E..0x1BD: BSS_00_2E0AE (112 bytes, purpose unclear; sparse uses) --- */
    uint8_t _bss_2E0AE[112];            /* 0x14E */

    /* --- 0x1BE..0x1C5: SWOS2.s named diy{MatchesPlayed,LeagueCurrentSlot,
     *                                   LeagueMatchProgress,LeagueTotalMatches} --- */
    uint16_t matchesPlayedCount;        /* 0x1BE: diyMatchesPlayedCount */
    uint16_t leagueCurrentSlot;         /* 0x1C0: diyLeagueCurrentSlot (also section-progression flag) */
    uint16_t leagueMatchProgress;       /* 0x1C2: diyLeagueMatchProgress (also phase index) */
    uint16_t leagueTotalMatches;        /* 0x1C4: diyLeagueTotalMatches (also matchesPerSeason) */

    /* --- 0x1C6..0x24D: BSS_00_2E13E broken out (league section state) --- */
    DIYCompetitionEx_LeagueSectionState leagueSectionState;

    /* --- 0x24E..0x2CD: SWOS2.s named diyCupTeamIds[64] --- */
    uint16_t cupTeamIds[64];            /* 0x24E */

    /* --- 0x2CE..0x30D: SWOS2.s named diyTeamGridPairings[64] --- */
    uint8_t teamGridPairings[64];       /* 0x2CE */

    /* --- 0x30E..0x5C7: SWOS2.s named diyTeamShuffleOrder[698].
     *
     * The first 0x20 bytes hold raw shuffled team-byte indices, but the
     * rest of the array is reused as a scratch region for bracket-state
     * counters, the cup-progression month table, and the per-team
     * selTeamsPtr offset cache used by ApplyHomeAwaySwap and friends.
     * Anonymous union so existing `teamShuffleOrder[i]` callers keep
     * working while typed scratch-field access is also available via
     * `bracketScratch.<field>`. */
    union {
        uint8_t teamShuffleOrder[698];                   /* 0x30E */
        struct SWOS_PACKED_ALIGN(2) {
            uint8_t  shuffleHead[0x20];                  /* 0x30E..0x32D: raw shuffle bytes */
            uint16_t secondLegCounter;                    /* 0x32E */
            uint16_t groupSectionFlag;                    /* 0x330 */
            uint16_t matchCount;                          /* 0x332 */
            uint16_t totalMatches;                        /* 0x334 */
            uint16_t curMonthIdx;                         /* 0x336 */
            uint8_t  progTable[0xC];                      /* 0x338..0x343: month-progression bytes */
            int32_t  teamOffsetTable[64];                 /* 0x344..0x443: per-team byte offsets from selTeamsPtr (NOT pointers despite the historical "PtrTable" name). Signed so `&selTeamsPtr + offset` sign-extends on 64-bit when teamPtr is in BSS *below* selTeamsPtr (e.g. careerTeam). */
            uint8_t  scratchTail[0x184];                  /* 0x444..0x5C7: bracket scratch tail */
        } bracketScratch;
    };
} DIYCompetitionEx;

/* DIYCompetitionEx embeds DIYCompetition (which has 1 pointer);
 * +4 on 64-bit via that. The DIYCompetitionEx_* view variants are
 * pointer-free. */
#if UINTPTR_MAX == 0xFFFFFFFFu
#ifndef __APPLE__
typedef char _diycompetitionex_size_check[(sizeof(DIYCompetitionEx) == 0x5C8) ? 1 : -1];
#endif
#else
#ifndef __APPLE__
typedef char _diycompetitionex_size_check[(sizeof(DIYCompetitionEx) == 0x5D0) ? 1 : -1];
#endif
#endif

/* ==================================================================
 * SeasonInformations — 106 bytes per row, pack(2)
 *
 * Per-season summary used on the season-end recap screen. The
 * seasonInformationsArray global holds 20 of these (one per season
 * the player has played). Pointer fields (leagueStrPtr / strPtr1-3)
 * are runtime-derived comp-name strings — the JSON save emits them
 * as bare string values via emit_str_pool_field, re-interned into
 * the string pool on load.
 * ================================================================== */
typedef struct SWOS_PACKED_ALIGN(2) SeasonInformations {
    uint16_t clubNo;                  /* 0x00 */
    int8_t   clubName[18];            /* 0x02 */
    uint16_t trophyFlags;             /* 0x14 */
    uint8_t *leagueStrPtr;            /* 0x16: ptr to league name */
    uint8_t *strPtr1;                 /* 0x1A: ptr to cup name */
    uint8_t *strPtr2;                 /* 0x1E: ptr to cup name */
    uint8_t *strPtr3;                 /* 0x22: ptr to cup name */
    uint16_t bestScorerNumGoals;      /* 0x26 */
    int8_t   bestScorerName[23];      /* 0x28 */
    int8_t   secondScorerName[23];    /* 0x3F */
    int8_t   comp1Result;             /* 0x56 */
    int8_t   comp2Result;             /* 0x57 */
    int8_t   comp3Result;             /* 0x58 */
    int8_t   comp4Result;             /* 0x59 */
    uint16_t totalGoalsScored;        /* 0x5A */
    uint16_t totalGoalsTaken;         /* 0x5C */
    uint8_t  totalGames;              /* 0x5E */
    uint8_t  totalWins;               /* 0x5F */
    uint8_t  totalDrawn;              /* 0x60 */
    uint8_t  totalLost;               /* 0x61 */
    uint8_t  friendlyWins;            /* 0x62 */
    uint8_t  friendlyDraws;           /* 0x63 */
    uint8_t  friendlyLost;            /* 0x64 */
    uint8_t  tourWins;                /* 0x65 */
    uint8_t  tourDraws;               /* 0x66 */
    uint8_t  tourLost;                /* 0x67 */
    uint8_t  tournamentStatus;        /* 0x68 */
    int8_t   managerStatus;           /* 0x69 */
} SeasonInformations;

/* SeasonInformations has 4 pointer fields (leagueStrPtr, strPtr1/2/3);
 * +16 on 64-bit. */
#if UINTPTR_MAX == 0xFFFFFFFFu
#ifndef __APPLE__
typedef char _seasoninformations_size_check[(sizeof(SeasonInformations) == 106) ? 1 : -1];
#endif
#else
#ifndef __APPLE__
typedef char _seasoninformations_size_check[(sizeof(SeasonInformations) == 122) ? 1 : -1];
#endif
#endif

/* ==================================================================
 * JobOffer — 26 bytes, pack(2)
 * Queue entry for career mode job offers.
 * ================================================================== */
typedef struct SWOS_PACKED_ALIGN(2) JobOffer {
    uint16_t teamNo;               /* 0 */
    int8_t   teamName[18];         /* 2: 0xFF = unset */
    uint32_t amountOffered;        /* 20 */
    uint16_t unkn;                 /* 24 */
} JobOffer;

#ifndef __APPLE__
typedef char _joboffer_size_check[(sizeof(JobOffer) == 26) ? 1 : -1];
#endif

/* ==================================================================
 * NationalTeamRecord — 6 bytes, pack(1)
 * Entry in nationalTeams[]. Indexed by ordinal × 6.
 *
 * countryNumber/teamOrdinal are an Amiga BE byte pair: callers compare
 * the packed id against selTeamNumber via PACK_BE_U16, *not* a host
 * uint16 read.
 *
 * skillOrPrice is read host-native (`*(uint16_t *)(entry+4)`) by the
 * random-pick filters — see _PickRandomNationalTeamId / job_offers
 * searchNational. Keep that behaviour unchanged here; if it turns out
 * to be a latent endian bug, fix it separately and document.
 * ================================================================== */
typedef struct SWOS_PACKED NationalTeamRecord {
    uint8_t  countryNumber;      /* 0 */
    uint8_t  teamOrdinal;        /* 1 */
    uint8_t  nationalityCode;    /* 2 */
    uint8_t  teamFileId;          /* 3 */
    uint16_t skillOrPrice;        /* 4..5 — host-native, see note above */
} NationalTeamRecord;

#ifndef __APPLE__
typedef char _nationalteamrecord_size_check[(sizeof(NationalTeamRecord) == 6) ? 1 : -1];
#endif

/* ==================================================================
 * TeamSelectionKeyCache — variable-length entry stream in
 * `teamSelectionKeyCache` (BSS overlay over careerFileBufferBuf+73292).
 *
 * Each entry is a 10-byte header followed by N × 12-byte match slots,
 * with `entryStride = 10 + N*12`. Entries are sorted by `sortKey` and
 * walked by adding `entryStride` to the current entry pointer.
 *
 * The original asm `_TeamSelectionKeyCache_GetCurrentSelectedElementPtr`
 * helper returns `entry + diff*12` where `diff = totalElements -
 * remainingElements`. Writes go to elem[+0x0B..+0x15], so the asm
 * pointer is 10 bytes *before* the actual match slot start. The typed
 * helper `_TeamSelectionKeyCache_GetCurrentMatchSlot()` returns the
 * properly-aligned `TeamSelectionKeyCacheMatchSlot *`.
 *
 * Layout reverse-engineered from SWOS2.s lines 59549-59656 (entry
 * build), 60046-60054 (match-result write), 60723-60738
 * (GetCurrentSelectedElementPtr), and slot-build reads at
 * diy_competition.c:2776+ (compId/roundIdx lookup).
 * ================================================================== */
typedef struct SWOS_PACKED TeamSelectionKeyCacheMatchSlot {
    uint8_t  pairType;          /* +0  (elem[0x0A]) — 0/1/2 fixture type */
    uint8_t  matchFlags;        /* +1  (elem[0x0B]) — written post-match */
    uint16_t teamId1;           /* +2  (elem[0x0C]) — host-native */
    uint16_t teamId2;           /* +4  (elem[0x0E]) — host-native */
    uint16_t packedGoals;       /* +6  (elem[0x10]) — high=team1, low=team2 */
    uint16_t aggregate;         /* +8  (elem[0x12]) */
    uint16_t penaltyScore;      /* +10 (elem[0x14]) */
} TeamSelectionKeyCacheMatchSlot;

#ifndef __APPLE__
typedef char _tskc_slot_size_check[(sizeof(TeamSelectionKeyCacheMatchSlot) == 12) ? 1 : -1];
#endif

typedef struct SWOS_PACKED TeamSelectionKeyCacheEntry {
    uint8_t  compId;            /* +0 — competition id (matched in lookup) */
    uint8_t  sortKey;           /* +1 — InsertSpaceSorted insertion key */
    uint16_t entryStride;       /* +2 — host-native; = 10 + numTeams*12 */
    uint8_t  phaseFlag;         /* +4 — 0 == unprocessed */
    uint8_t  totalElements;     /* +5 — count of match slots */
    int8_t   remainingElements; /* +6 — slots left to play (< 0 means none) */
    uint8_t  roundIdx;          /* +7 — cup round index (cup mode) */
    uint8_t  legFlag;           /* +8 — 1/2/3 leg/format variant; never
                                 *      written via cacheEntry[8]= in the
                                 *      ported C; initialised somewhere in
                                 *      the cache-build chain that we
                                 *      haven't fully mapped. Reads at
                                 *      diy_competition.c:2804+. */
    uint8_t  cupRoundStringIdx; /* +9 — cup round string index */
    /* +10..: TeamSelectionKeyCacheMatchSlot slots[totalElements] */
} TeamSelectionKeyCacheEntry;

#ifndef __APPLE__
typedef char _tskc_entry_size_check[(sizeof(TeamSelectionKeyCacheEntry) == 10) ? 1 : -1];
#endif

/* ==================================================================
 * MarketPlayer — 42 bytes, pack(1)
 * Career foreign market player entry.
 * ================================================================== */
typedef struct MarketPlayer {
    uint16_t   teamId;             /* 0 */
    int8_t     playerOrdinal;      /* 2 */
    int8_t     padding;            /* 3 */
    PlayerFile player;             /* 4 (38 bytes) */
} MarketPlayer;

#ifndef __APPLE__
typedef char _marketplayer_size_check[(sizeof(MarketPlayer) == 42) ? 1 : -1];
#endif

/* ==================================================================
 * PoolPlayer — 58 bytes, pack(1)
 * Pool player database entry (poolplyr.dat).
 * ================================================================== */
typedef struct PoolPlayer {
    uint16_t   teamId;             /* 0 */
    int8_t     playerOrdinal;      /* 2 */
    int8_t     teamName[17];       /* 3 */
    PlayerFile player;             /* 20 (38 bytes) */
} PoolPlayer;

#ifndef __APPLE__
typedef char _poolplayer_size_check[(sizeof(PoolPlayer) == 58) ? 1 : -1];
#endif

/* ==================================================================
 * PlayerPositions — 35 bytes, pack(1)
 * 5 ball-location zones × 7 packed position bytes per zone.
 * ================================================================== */
typedef struct PlayerPositions {
    uint8_t positions[5][7];       /* packed x/y nibbles */
} PlayerPositions;

#ifndef __APPLE__
typedef char _playerpositions_size_check[(sizeof(PlayerPositions) == 35) ? 1 : -1];
#endif

/* ==================================================================
 * TeamTactics — 370 bytes, pack(1)
 * Formation/tactics definition: name + 10 outfield player positions
 * (5 ball zones × 7 quadrants each) + unk pairing table + redirect.
 * ================================================================== */
typedef struct TeamTactics {
    int8_t           name[9];              /* 0: tactic name */
    PlayerPositions  positions[10];        /* 9: 10 players × 35 bytes = 350 */
    uint8_t          unkTable[10];         /* 359: pairing table */
    uint8_t          ballOutOfPlayTactics; /* 369: redirect tactic index */
} TeamTactics;

#ifndef __APPLE__
typedef char _teamtactics_size_check[(sizeof(TeamTactics) == 370) ? 1 : -1];
#endif

/* ==================================================================
 * CountryNumbers — country identifier constants
 * ================================================================== */

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

/* ==================================================================
 * TacticsSlotIndex — player slot indices within a formation
 * ================================================================== */

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

/* ==================================================================
 * TacticsPosition — maps 11 player slots for a formation
 * ================================================================== */

typedef struct SWOS_PACKED {
    TacticsSlotIndex index[11];
} TacticsPosition;

/* ==================================================================
 * TacticsEnum — formation identifiers
 * ================================================================== */

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

/* ==================================================================
 * Positions enum — player position types
 * ================================================================== */

#define POS_GOALKEEPER    0x0
#define POS_RIGHT_BACK    0x1
#define POS_LEFT_BACK     0x2
#define POS_DEFENDER      0x3
#define POS_RIGHT_WING    0x4
#define POS_LEFT_WING     0x5
#define POS_MIDFIELDER    0x6
#define POS_ATTACKER      0x7

/* ==================================================================
 * Competition — 9 bytes, base competition descriptor
 * ================================================================== */

typedef struct SWOS_PACKED {
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

/* ==================================================================
 * CompetitionLeague — 13 bytes, league competition header
 * ================================================================== */

typedef struct SWOS_PACKED {
    Competition comp;
    int8_t divisionsCount;
    int8_t rounds;
    int8_t pointsForWin;
    int8_t subsReserves;
} CompetitionLeague;

/* ==================================================================
 * Division — 6 bytes, division descriptor within a league
 * ================================================================== */

typedef struct SWOS_PACKED {
    int8_t teams;
    int8_t promoted;
    int8_t promotedPo;
    int8_t relegated;
    int8_t relegatedPo;
    int8_t unk1;
} Division;

/* ==================================================================
 * CompetitionCup — 14 bytes, cup competition header
 * ================================================================== */

typedef struct SWOS_PACKED {
    Competition comp;
    int8_t unk1;
    int8_t teams;
    int8_t awayGoalsMode;
    int8_t subsReserves;
    int8_t firstRoundNo;
} CompetitionCup;

/* ==================================================================
 * Round — 3 bytes, round descriptor for CompetitionCup2
 * ================================================================== */

typedef struct SWOS_PACKED {
    int8_t teamCount;
    int8_t groupCount;
    int8_t teamGroupCount;
} Round;

/* ==================================================================
 * CompetitionCup2 — 34 bytes, cup competition with group rounds
 * ================================================================== */

typedef struct SWOS_PACKED {
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

/* ==================================================================
 * CompetitionLeague variants (with Division arrays)
 * ================================================================== */

typedef struct SWOS_PACKED {
    CompetitionLeague comp;
    Division divisions[1];
    int8_t unk1;
} CompetitionLeague_1;

typedef struct SWOS_PACKED {
    CompetitionLeague comp;
    Division divisions[2];
    int8_t unk1;
} CompetitionLeague_2;

typedef struct SWOS_PACKED {
    CompetitionLeague comp;
    Division divisions[3];
    int8_t unk1;
} CompetitionLeague_3;

typedef struct SWOS_PACKED {
    CompetitionLeague comp;
    Division divisions[4];
    int8_t unk1;
} CompetitionLeague_4;

/* ==================================================================
 * CompetitionCup variants (with rounds arrays)
 * ================================================================== */

typedef struct SWOS_PACKED {
    CompetitionCup comp;
    int8_t rounds[4];
} CompetitionCup_4;

typedef struct SWOS_PACKED {
    CompetitionCup comp;
    int8_t rounds[6];
} CompetitionCup_6;

typedef struct SWOS_PACKED {
    CompetitionCup comp;
    int8_t rounds[10];
} CompetitionCup_10;

/* ==================================================================
 * CompetitionCup2 variants (with roundSettings)
 * ================================================================== */

typedef struct SWOS_PACKED {
    CompetitionCup2 compCup;
    int8_t roundSettings[4];
} CompetitionCup2_4;

typedef struct SWOS_PACKED {
    CompetitionCup2 compCup;
    int8_t roundSettings[6];
} CompetitionCup2_6;

/* ==================================================================
 * CompetitionCup with text pointers
 * ================================================================== */

typedef struct SWOS_PACKED {
    CompetitionCup comp;
    int8_t rounds[6];
    char *text[2];
} CompetitionCup_6_2;

typedef struct SWOS_PACKED {
    CompetitionCup comp;
    int8_t rounds[4];
    char *text[2];
} CompetitionCup_4_2;

/* ==================================================================
 * CompetitionCup with text + trailing data
 * ================================================================== */

typedef struct SWOS_PACKED {
    CompetitionCup comp;
    int8_t rounds[6];
    char *text[2];
    int8_t unk1[8];
} CompetitionCup_6_2_8;

typedef struct SWOS_PACKED {
    CompetitionCup comp;
    int8_t rounds[6];
    char *text[2];
    int8_t unk1[12];
} CompetitionCup_6_2_12;

typedef struct SWOS_PACKED {
    CompetitionCup comp;
    int8_t rounds[6];
    char *text[2];
    int8_t unk1[64];
} CompetitionCup_6_2_64;

/* ==================================================================
 * CompetitionCup2 with text pointers variants
 * ================================================================== */

typedef struct SWOS_PACKED {
    CompetitionCup2_4 compCup;
    char *text[2];
} CompetitionCup2_4_2;

typedef struct SWOS_PACKED {
    CompetitionCup2_6 compCup;
    char *text[2];
} CompetitionCup2_6_2;

typedef struct SWOS_PACKED {
    CompetitionCup2_4 compCup;
    char *text[2];
    int8_t unk1[24];
} CompetitionCup2_4_2_24;

typedef struct SWOS_PACKED {
    CompetitionCup2_4 compCup;
    char *text[2];
    int8_t unk1[32];
} CompetitionCup2_4_2_32;

typedef struct SWOS_PACKED {
    CompetitionCup2_6 compCup;
    char *text[2];
    int8_t unk1[16];
} CompetitionCup2_6_2_16;

typedef struct SWOS_PACKED {
    CompetitionCup2_6 compCup;
    char *text[2];
    int8_t unk1[24];
} CompetitionCup2_6_2_24;

typedef struct SWOS_PACKED {
    CompetitionCup2_6 compCup;
    char *text[2];
    int8_t unk1[48];
} CompetitionCup2_6_2_48;

typedef struct SWOS_PACKED {
    CompetitionCup2_6 compCup;
    char *text[2];
    int8_t unk1[60];
} CompetitionCup2_6_2_60;

typedef struct SWOS_PACKED {
    CompetitionCup2_6 compCup;
    char *text[2];
    int8_t unk1[64];
} CompetitionCup2_6_2_64;

/* ==================================================================
 * CompetitionLeague with text pointers variants
 * ================================================================== */

typedef struct SWOS_PACKED {
    CompetitionLeague comp;
    Division divisions[3];
    int8_t unk1;
    char *text[6];
} CompetitionLeague_3_6;

typedef struct SWOS_PACKED {
    CompetitionLeague comp;
    Division divisions[2];
    int8_t unk1;
    char *text[4];
} CompetitionLeague_2_4;

typedef struct SWOS_PACKED {
    CompetitionLeague comp;
    Division divisions[4];
    int8_t unk1;
    char *text[8];
} CompetitionLeague_4_8;

typedef struct SWOS_PACKED {
    CompetitionLeague comp;
    Division divisions[4];
    int8_t unk1;
    char *text[10];
    int8_t unk2[24];
} CompetitionLeague_4_10_24;

typedef struct SWOS_PACKED {
    CompetitionLeague comp;
    Division divisions[2];
    int8_t unk1;
    char *text[2];
    int8_t unk2[24];
} CompetitionLeague_2_2_24;

typedef struct SWOS_PACKED {
    CompetitionLeague comp;
    Division divisions[4];
    int8_t unk1;
    char *text[2];
    int8_t unk2[8];
} CompetitionLeague_4_2_8;

typedef struct SWOS_PACKED {
    CompetitionLeague comp;
    Division divisions[2];
    int8_t unk1;
    char *text[4];
    char *unk2[2];
    int8_t unk3[24];
} CompetitionLeague_2_4_2_24;

typedef struct SWOS_PACKED {
    CompetitionLeague comp;
    Division divisions[2];
    int8_t unk1;
    char *unk2[2];
    int8_t unk3[8];
} CompetitionLeague_2_2_8;

typedef struct SWOS_PACKED {
    CompetitionLeague comp;
    Division divisions[4];
    int8_t unk1;
    char *text[8];
    char *unk2[2];
    int8_t unk3[24];
} CompetitionLeague_4_8_2_24;

/* ==================================================================
 * Playoff and Leagues helper structs
 * ================================================================== */

typedef struct SWOS_PACKED {
    int8_t unk1[16];
    char *text[2];
    int8_t unk2[8];
} Playoffs_8;

typedef struct SWOS_PACKED {
    int8_t unk1[16];
    char *text[2];
    int8_t unk2[4];
} Playoffs_4;

typedef struct SWOS_PACKED {
    CompetitionLeague *text1;
    int8_t unk1[8];
    CompetitionLeague *text2;
    int8_t unk2[8];
    CompetitionLeague *text3;
    int8_t unk3[8];
    CompetitionLeague *text4;
    int8_t unk4[8];
} Leagues_4;

typedef struct SWOS_PACKED {
    Leagues_4 leagues;
    Playoffs_8 *playoffs;
    Leagues_4 *leagues2;
    int8_t unk2[24];
} Leagues_4_2_24;

typedef struct SWOS_PACKED {
    CompetitionLeague *compLeague1;
    int8_t unk1[8];
    CompetitionLeague *compLeague2;
    int8_t unk2[8];
} Leagues_2;

/* ==================================================================
 * CompetitionCareer — career competition entry
 * ================================================================== */

typedef struct SWOS_PACKED {
    CompetitionCup *competition;
    CompetitionCup *competitionCopy;
    uint16_t length;
} CompetitionCareer;

/* ==================================================================
 * Qualifying group structs
 * ================================================================== */

typedef struct SWOS_PACKED {
    CompetitionCup *compCup[3];
} QualifyingGroup_3;

typedef struct SWOS_PACKED {
    CompetitionCup *compCup[9];
    CountryNumbers countries[2];
} QualifyingGroup_9_2;

typedef struct SWOS_PACKED {
    CompetitionCup *compCup[2];
    CountryNumbers countries[8];
} QualifyingGroup_2_8;

typedef struct SWOS_PACKED {
    CompetitionCup *compCup[6];
    CountryNumbers countries[44];
} QualifyingGroup_6_44;

typedef struct SWOS_PACKED {
    CompetitionCup *compCup[3];
    CountryNumbers countries[4];
} QualifyingGroup_3_4;

typedef struct SWOS_PACKED {
    CompetitionCup *compCup[3];
    CountryNumbers countries[12];
} QualifyingGroup_3_12;

/* ==================================================================
 * DIYStage — DIY competition stage descriptor
 * ================================================================== */

typedef struct SWOS_PACKED {
    uint16_t legs;
    uint16_t extraTime;
    uint16_t penalties;
    uint16_t playCount;
    uint16_t ptsForWin;
} DIYStage;

#endif /* GAME_TYPES_H */
