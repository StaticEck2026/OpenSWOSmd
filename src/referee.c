/*
 * referee.c — Referee system: activation, movement, animation, booking display
 *
 * Ported from SWOS2.s: ActivateReferee, SetRefereeToLeavingState,
 * InitRefereeAnimationTable, UpdateReferee, UpdateBookedPlayerNumberSprite
 */

#include "referee.h"
#include "game_types.h"

/* ---- Referee state machine ---- */
#define REF_INCOMING            1
#define REF_WAITING_PLAYER      2
#define REF_ABOUT_TO_GIVE_CARD  3
#define REF_BOOKING             4
#define REF_LEAVING             5

/* ---- Card types ---- */
#define CARD_YELLOW             1
#define CARD_RED                2
#define CARD_SECOND_YELLOW      3

/* ---- Sprite indices ---- */
#define SPR_YELLOW_CARD         1178
#define SPR_RED_CARD            1177
#define SPR_CORNER_FLAG_FRAME_3 1073

/* ---- Extern globals ---- */
extern Sprite   refereeSprite;
extern Sprite   bookedPlayerCardOrNumberSprite;
extern uint16_t refState;
extern uint16_t foulXCoordinate;
extern uint16_t foulYCoordinate;
extern uint16_t cameraY;
extern uint16_t kRefereeSpeed;
extern uint16_t whichCard;
extern Sprite          *bookedPlayer;
extern TeamGeneralInfo *lastTeamBooked;
extern uint16_t refTimer;
extern uint16_t lastFrameTicks;
extern uint16_t gameStatePl;
extern uint16_t panicReasonCode;

/* AnimTable data (in asm code section) */
extern uint8_t refComingAnimTable[];
extern uint8_t refWaitingAnimTable[];
extern uint8_t refYellowCardAnimTable[];
extern uint8_t refRedCardAnimTable[];
extern uint8_t refSecondYellowAnimTable[];

/* Card display lookup tables (in asm code section, after UpdateBookedPlayerNumberSprite) */
extern int8_t yellowCardTable[];    /* 30 entries */
extern int8_t redCardTable[];       /* 30 entries */
extern int8_t secondYellowTable[];  /* 46 entries */

/* ---- External functions ---- */
extern uint16_t _Rand(void);
extern void _InitDisplaySprites(void);
extern void _PanicHaltLoop(void);

/* CalculateDeltaXAndY — now ported to C in math68k.c */
extern uint16_t _CalculateDeltaXAndY(int16_t destX, int16_t destY,
                                      int16_t srcX, int16_t srcY,
                                      int16_t speed,
                                      int32_t *outDeltaX, int32_t *outDeltaY);

/* SendPlayerAway ported to C in tackle_foul.c (still uses uint8_t* params) */
extern void _SendPlayerAway(uint8_t *playerSprite, uint8_t *teamData);

/* ================================================================
 * InitRefereeAnimationTable  (SWOS2.s line 10612)
 *
 * Sets up referee sprite animation from an AnimTable pointer.
 * AnimTable layout: uint16_t frameDelay, then 8 x uint32_t frame-indices-table ptrs
 *
 * in: animTable = pointer to AnimTable (was A1 in asm)
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitRefereeAnimationTable(uint8_t *animTable)
{
    Sprite *a0 = &refereeSprite;
    uint16_t dir = (uint16_t)a0->direction;
    uint16_t frameDelay = *(uint16_t *)animTable;
    /* Frame table pointer at offset 2 + dir*4 */
    uint32_t frameTablePtr = *(uint32_t *)(animTable + 2 + dir * 4);

    a0->frameDelay        = (int16_t)frameDelay;
    a0->frameIndicesTable = frameTablePtr;

    if (frameTablePtr == 0) {
        panicReasonCode = 0x1112;
        _PanicHaltLoop();
        return;
    }

    a0->frameSwitchCounter = (int16_t)0xFFFF;
    a0->frameIndex         = (int16_t)0xFFFF;
    a0->cycleFramesTimer   = 1;
}

/* ================================================================
 * ActivateReferee  (SWOS2.s line 10553)
 *
 * Positions referee sprite at foul location with random offset,
 * starts incoming animation.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ActivateReferee(void)
{
    Sprite *a0 = &refereeSprite;
    int16_t d1 = (int16_t)foulXCoordinate + 28;
    int16_t d2 = (int16_t)foulYCoordinate + 5;

    a0->destX = d1;
    a0->destY = d2;

    uint16_t rval = _Rand();
    int16_t d0 = (int16_t)(rval >> 3);

    if (foulXCoordinate >= 336) {
        d0 = -d0;
    }

    /* Position above or below camera based on foul Y */
    if (foulYCoordinate <= 449) {
        /* Foul in upper half: referee enters from bottom */
        d2 = (int16_t)cameraY + 287;
    } else {
        /* Foul in lower half: referee enters from top */
        d2 = (int16_t)cameraY - 20;
    }

    d1 = d1 + d0;
    a0->x.s.i    = d1;
    a0->y.s.i    = d2;
    /* Clear fractional parts */
    a0->x.s.frac = 0;
    a0->y.s.frac = 0;

    a0->speed   = (int16_t)kRefereeSpeed;
    a0->visible = 1;

    _InitDisplaySprites();
    _InitRefereeAnimationTable(refComingAnimTable);
    refState = REF_INCOMING;
}

/* ================================================================
 * SetRefereeToLeavingState  (SWOS2.s line 10590)
 *
 * Sets referee destination to off-screen and starts leaving anim.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetRefereeToLeavingState(void)
{
    Sprite *a0 = &refereeSprite;
    uint16_t rval = _Rand();
    int16_t d0 = (int16_t)(rval >> 2) - 32;   /* range -32..31 */

    int16_t d2;
    if (foulYCoordinate <= 449) {
        d2 = 770;
    } else {
        d2 = 129;
    }

    d0 = d0 + a0->x.s.i;
    a0->destX = d0;
    a0->destY = d2;

    _InitRefereeAnimationTable(refComingAnimTable);
    refState = REF_LEAVING;
}

/* ================================================================
 * UpdateReferee  (SWOS2.s line 10630)
 *
 * Per-frame referee update: animation cycling, movement toward
 * destination, state machine (incoming→waiting→card→booking→leaving).
 * ================================================================ */
__attribute__((used, externally_visible))
void _UpdateReferee(void)
{
    if (refState == 0)
        return;

    Sprite *a0 = &refereeSprite;

    if (a0->visible == 0)
        return;

    /* If off-screen, handle position update separately */
    if (a0->onScreen == 0)
        goto off_screen_update;

    /* ---- Animation frame cycling ---- */
    a0->cycleFramesTimer = (int16_t)((uint16_t)a0->cycleFramesTimer - 1);
    if ((uint16_t)a0->cycleFramesTimer == 0) {
        a0->frameIndex += 1;
        a0->cycleFramesTimer = a0->frameDelay;

        int16_t *frameTable = (int16_t *)a0->frameIndicesTable;

    decode_frame:;
        int16_t frameIdx = a0->frameIndex;
        int16_t frameVal = frameTable[frameIdx];

        if (frameVal >= 0) {
            /* Normal frame: set image index, bump switch counter */
            a0->frameSwitchCounter += 1;
            a0->imageIndex = frameVal;
        } else {
            /* Negative: animation command */
            if (frameVal == (int16_t)0xFC19) {
                /* -999: restart from frame 0 */
                a0->frameIndex = 0;
                goto decode_frame;
            } else if (frameVal == (int16_t)0xFF9B) {
                /* -101: hold (step back one frame) */
                a0->frameIndex -= 1;
                /* fall through to state dispatch */
            } else if (frameVal <= (int16_t)0xFF9C) {
                /* <= -100: relative jump (add 100+frameVal to index) */
                a0->frameIndex += (int16_t)(100 + frameVal);
                goto decode_frame;
            } else {
                /* -1..-99: new frame delay */
                int16_t newDelay = (int16_t)(-frameVal);
                a0->frameDelay       = newDelay;
                a0->cycleFramesTimer = newDelay;
                a0->frameIndex      += 1;
                goto decode_frame;
            }
        }
    }

    /* ---- State dispatch ---- */
state_dispatch:
    if (refState == REF_ABOUT_TO_GIVE_CARD) {
        /* Transition to REF_BOOKING with appropriate card animation */
        refState = REF_BOOKING;
        if (whichCard == CARD_RED) {
            _InitRefereeAnimationTable(refRedCardAnimTable);
        } else if (whichCard == CARD_SECOND_YELLOW) {
            _InitRefereeAnimationTable(refSecondYellowAnimTable);
        } else {
            _InitRefereeAnimationTable(refYellowCardAnimTable);
        }
        return;
    }

    if (refState == REF_INCOMING) {
        goto move_referee;
    }

    if (refState != REF_LEAVING) {
        return;
    }

    /* REF_LEAVING: if off-screen, hide and deactivate */
    if (a0->onScreen == 0) {
        refState   = 0;
        a0->visible = 0;
        _InitDisplaySprites();
        return;
    }

    /* ---- Move referee toward destination ---- */
move_referee:;
    a0->speed = (int16_t)kRefereeSpeed;
    uint16_t savedDir = (uint16_t)a0->direction;

    int16_t destX = a0->destX;
    int16_t destY = a0->destY;
    int16_t curX  = a0->x.s.i;
    int16_t curY  = a0->y.s.i;
    int16_t speed = a0->speed;

    int32_t outDeltaX, outDeltaY;
    uint16_t outDir = _CalculateDeltaXAndY(destX, destY, curX, curY, speed,
                                            &outDeltaX, &outDeltaY);

    a0->deltaX.raw = outDeltaX;
    a0->deltaY.raw = outDeltaY;

    /* Convert direction: (d0 + 16) & 0xFF >> 5 → 0..7 */
    uint16_t newDir = ((outDir + 16) & 0xFF) >> 5;
    a0->direction = (int16_t)newDir;

    if (newDir != savedDir) {
        _InitRefereeAnimationTable(refComingAnimTable);
    }

    /* ---- Clamp X toward destX ---- */
    {
        int32_t dx = a0->deltaX.raw;
        if (dx != 0) {
            a0->x.raw += dx;     /* add.l d0,SPRITE_X(a0) — adds to full 32-bit (int:frac) */
            if (dx < 0) {
                if (a0->x.s.i <= destX) {
                    a0->x.s.i      = destX;
                    a0->deltaX.raw = 0;
                }
            } else {
                if (a0->x.s.i >= destX) {
                    a0->x.s.i      = destX;
                    a0->deltaX.raw = 0;
                }
            }
        }
    }

    /* ---- Clamp Y toward destY ---- */
    {
        int32_t dy = a0->deltaY.raw;
        if (dy != 0) {
            a0->y.raw += dy;
            if (dy < 0) {
                if (a0->y.s.i <= destY) {
                    a0->y.s.i      = destY;
                    a0->deltaY.raw = 0;
                }
            } else {
                if (a0->y.s.i >= destY) {
                    a0->y.s.i      = destY;
                    a0->deltaY.raw = 0;
                }
            }
        }
    }

    /* ---- Check if destination reached → transition to REF_WAITING_PLAYER ---- */
    if (a0->deltaX.raw != 0 || a0->deltaY.raw != 0)
        return;

    refState      = REF_WAITING_PLAYER;
    a0->direction = 6;
    _InitRefereeAnimationTable(refWaitingAnimTable);
    return;

    /* ---- Off-screen position update (keep moving even when off-screen) ---- */
off_screen_update:
    if (gameStatePl != ST_GAME_IN_PROGRESS)
        goto state_dispatch;

    /* Clamp X */
    {
        int16_t dstX = a0->destX;
        int32_t dx = a0->deltaX.raw;
        if (dx != 0) {
            a0->x.raw += dx;
            if (dx < 0) {
                if (a0->x.s.i <= dstX) {
                    a0->x.s.i      = dstX;
                    a0->deltaX.raw = 0;
                }
            } else {
                if (a0->x.s.i >= dstX) {
                    a0->x.s.i      = dstX;
                    a0->deltaX.raw = 0;
                }
            }
        }
    }

    /* Clamp Y */
    {
        int16_t dstY = a0->destY;
        int32_t dy = a0->deltaY.raw;
        if (dy != 0) {
            a0->y.raw += dy;
            if (dy < 0) {
                if (a0->y.s.i <= dstY) {
                    a0->y.s.i      = dstY;
                    a0->deltaY.raw = 0;
                }
            } else {
                if (a0->y.s.i >= dstY) {
                    a0->y.s.i      = dstY;
                    a0->deltaY.raw = 0;
                }
            }
        }
    }
    return;
}

/* ================================================================
 * UpdateBookedPlayerNumberSprite  (SWOS2.s line 11127)
 *
 * Shows card or shirt number sprite above booked player during
 * booking animation. Uses time-sliced lookup tables to alternate
 * between card display and shirt number display.
 *
 * Table values: 0=hide, CARD_YELLOW(1)=yellow card, CARD_RED(2)=red card,
 *               other positive=show shirt number, -1=sequence end
 * ================================================================ */
__attribute__((used, externally_visible))
void _UpdateBookedPlayerNumberSprite(void)
{
    Sprite *a0 = &bookedPlayerCardOrNumberSprite;
    a0->imageIndex = (int16_t)0xFFFF;  /* default: hide */

    if (whichCard == 0)
        return;
    if (refState != REF_BOOKING)
        return;

    Sprite *playerSprite = bookedPlayer;
    if (playerSprite->playerState != PL_BOOKED)
        return;

    /* Advance timer */
    refTimer += lastFrameTicks;
    uint16_t tableIdx = refTimer >> 3;

    /* Select card display table */
    int8_t *table;
    if (whichCard == CARD_YELLOW) {
        table = yellowCardTable;
    } else if (whichCard == CARD_RED) {
        table = redCardTable;
    } else {
        table = secondYellowTable;
    }

    int8_t entry = table[tableIdx];

    if (entry == CARD_YELLOW) {
        /* Show yellow card image */
        a0->imageIndex = SPR_YELLOW_CARD;
        playerSprite = bookedPlayer;
    } else if (entry == CARD_RED) {
        /* Show red card image */
        a0->imageIndex = SPR_RED_CARD;
        playerSprite = bookedPlayer;
    } else if (entry < 0) {
        /* -1 sentinel: sequence done, referee leaves */
        _SetRefereeToLeavingState();

        /* bit 1 of whichCard = red card → send player off */
        if (whichCard & 2) {
            _SendPlayerAway((uint8_t *)bookedPlayer, (uint8_t *)lastTeamBooked);
        }

        whichCard    = 0;
        bookedPlayer = 0;
        return;
    } else if (entry == 0) {
        /* 0 = show nothing */
        return;
    } else {
        /* Positive non-card value: show shirt number */
        playerSprite = bookedPlayer;
        TeamGeneralInfo *teamInfo  = lastTeamBooked;
        TeamGame        *inGameTeam = teamInfo->inGameTeamPtr;
        uint16_t ordinal = (uint16_t)playerSprite->playerOrdinal;
        int8_t shirtNum = inGameTeam->players[ordinal - 1].shirtNumber;
        int16_t sprIdx  = SPR_CORNER_FLAG_FRAME_3 + (int16_t)shirtNum;
        a0->imageIndex  = sprIdx;
    }

    /* Position card/number sprite above booked player */
    a0->x.s.i  = playerSprite->x.s.i;
    a0->z.s.i  = 20;
    a0->y.s.i  = playerSprite->y.s.i;
}
