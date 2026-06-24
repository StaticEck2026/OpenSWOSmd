/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "team_control.h"

/* ==================================================================
 * Chunk 54 — Team Controls + Player Selection
 *
 * Ported functions:
 *   UpdateControlledPlayer      (A6=team)
 *   UpdatePlayerBeingPassedTo   (A6=team)
 *   UpdateAndApplyTeamControls  (no params)
 * ================================================================== */

/* ---- External globals ---- */
extern Sprite     ballSprite;
extern TeamGeneralInfo topTeamData;
extern TeamGeneralInfo bottomTeamData;

extern int16_t    gameStatePl;
extern int16_t    playingPenalties;
extern Sprite    *penaltyShooterSprite;
extern int16_t    fireBlocked;
extern int16_t    inSubstitutesMenu;
extern uint16_t   frameCount;

/* Input state (in SWOS2.s code section) */
extern int8_t     pl1Fire;
extern int8_t     pl1ShortFire;
extern int16_t    pl1FireCounter;
extern int16_t    pl1Direction;
extern int8_t     pl2Fire;
extern int8_t     pl2ShortFire;
extern int16_t    pl2FireCounter;
extern int16_t    pl2Direction;

/* ---- External functions ---- */
extern void Player1StatusProc(void);
extern void Player2StatusProc(void);
extern void UpdateGameTimersAndCameraBreakMode(void);

/* UpdatePlayers is now ported to C */
#include "update_players.h"

/* ==================================================================
 * UpdateControlledPlayer — find nearest player to ball for team
 *
 * Scans all 11 players, computes squared distance to ball,
 * stores in SPRITE_BALLDISTANCE. Selects nearest eligible player
 * as controlledPlayer. If new controlled player differs from current,
 * stops old one (sets dest = current pos) and updates pointer.
 * Also clears passToPlayerPtr if new controlled player was the
 * pass target.
 * ================================================================== */
__attribute__((used, externally_visible))
void UpdateControlledPlayer(TeamGeneralInfo *team) {
    Sprite *ball = &ballSprite;
    int16_t ballX = ball->x.s.i;
    int16_t ballY = ball->y.s.i;
    Sprite **sprTable = team->spritesTable;
    Sprite *nearest = (Sprite *)0;
    int32_t minDist = -1;  /* 0xFFFFFFFF as unsigned = max */
    int16_t i;

    for (i = 10; i >= 0; i--) {
        Sprite *pl = sprTable[i];
        int16_t dx = pl->x.s.i - ballX;
        int16_t dy = pl->y.s.i - ballY;
        int32_t dist = (int32_t)dx * dx + (int32_t)dy * dy;
        pl->ballDistance = dist;

        /* Skip filtering when game not in progress */
        if (gameStatePl == ST_GAME_IN_PROGRESS) {
            if (!pl->onScreen)
                continue;
        }

        /* Skip sent-away players */
        if (pl->sentAway)
            continue;

        /* Skip goalkeeper if not playing */
        if (!team->goaliePlayingOrOut) {
            if (pl->playerOrdinal == 1)
                continue;
        }

        /* Skip passing/kicking player */
        if (pl == team->passingKickingPlayer)
            continue;

        /* Skip players in certain states */
        if (pl->playerState == PL_TACKLING ||
            pl->playerState == PL_TACKLED ||
            pl->playerState == PL_JUMP_HEADING ||
            pl->playerState == PL_STATIC_HEADING ||
            pl->playerState == PL_ROLLING_INJURED)
            continue;

        /* Skip pass target */
        if (pl == team->passToPlayerPtr)
            continue;

        /* Check if closer than current best (unsigned compare) */
        if ((uint32_t)dist >= (uint32_t)minDist)
            continue;

        minDist = dist;
        nearest = pl;
    }

    /* During penalties + game in progress: skip controlled player update */
    if (playingPenalties) {
        if (gameStatePl == ST_GAME_IN_PROGRESS)
            return;
    }

    if (nearest == (Sprite *)0)
        return;

    /* Only update if ball is out of play (player switch allowed) */
    if (!team->ballOutOfPlay)
        return;

    if (nearest != team->controlledPlayer) {
        /* Stop old controlled player */
        Sprite *old = team->controlledPlayer;
        if (old != (Sprite *)0) {
            if (old->playerState == PL_NORMAL) {
                old->destX = old->x.s.i;
                old->destY = old->y.s.i;
            }
        }
    }

    team->controlledPlayer = nearest;

    /* If new controlled player was the pass target, clear pass target */
    if (nearest == team->passToPlayerPtr) {
        Sprite *oldPass = team->passToPlayerPtr;
        if (oldPass != (Sprite *)0) {
            if (oldPass->playerState == PL_NORMAL) {
                oldPass->destX = oldPass->x.s.i;
                oldPass->destY = oldPass->y.s.i;
            }
        }
        team->passToPlayerPtr = (Sprite *)0;
    }
}

/* ==================================================================
 * UpdatePlayerBeingPassedTo — find closest secondary player
 *
 * Two code paths: game in progress vs game stopped.
 * Game in progress: checks onScreen, various exclusions.
 * Game stopped: skips onScreen check but otherwise similar.
 * ================================================================== */
__attribute__((used, externally_visible))
void UpdatePlayerBeingPassedTo(TeamGeneralInfo *team) {
    Sprite *ball = &ballSprite;

    if (gameStatePl != ST_GAME_IN_PROGRESS) {
        /* ----- Stopped path ----- */
        team->ballX = ball->x.s.i;
        team->ballY = ball->y.s.i;

        if (!team->ballInPlay)
            return;
        if (team->playerSwitchTimer)
            return;

        if (team->passingToPlayer) {
            if (team->passToPlayerPtr)
                return;
        }

        /* Penalties special case */
        if (playingPenalties) {
            if (gameStatePl != ST_PENALTIES)
                return;
            Sprite *shooter = penaltyShooterSprite;
            team->ballInPlay = 0;
            team->passToPlayerPtr = shooter;
            return;
        }

        /* Scan team for nearest eligible (stopped) */
        {
            Sprite **sprTable = team->spritesTable;
            int16_t bx = ball->x.s.i;
            int16_t by = ball->y.s.i;
            Sprite *nearest = (Sprite *)0;
            int32_t minDist = -1;
            int16_t i;

            for (i = 10; i >= 0; i--) {
                Sprite *pl = sprTable[i];

                if (pl->sentAway)
                    continue;

                if (!team->ballOutOfPlayOrKeeper) {
                    if (pl->playerOrdinal == 1)
                        continue;
                }

                if (pl == team->controlledPlayer)
                    continue;
                if (pl == team->passingKickingPlayer)
                    continue;
                if (pl->playerState != PL_NORMAL)
                    continue;

                {
                    int32_t dist = pl->ballDistance;
                    if ((uint32_t)dist >= (uint32_t)minDist)
                        continue;
                    minDist = dist;
                    nearest = pl;
                }
            }

            if (nearest == (Sprite *)0)
                return;

            team->passToPlayerPtr = nearest;
        }
        return;
    }

    /* ----- Game in progress path ----- */
    team->ballX = ball->x.s.i;
    team->ballY = ball->y.s.i;

    if (!team->ballInPlay)
        return;
    if (team->playerSwitchTimer)
        return;

    if (team->passingToPlayer) {
        if (team->passToPlayerPtr)
            return;
    }

    /* Penalties special case */
    if (playingPenalties) {
        if (gameStatePl != ST_PENALTIES)
            return;
        Sprite *shooter = penaltyShooterSprite;
        team->ballInPlay = 0;
        team->passToPlayerPtr = shooter;
        return;
    }

    /* Scan team for nearest eligible (in progress) */
    {
        Sprite **sprTable = team->spritesTable;
        Sprite *nearest = (Sprite *)0;
        int32_t minDist = -1;
        int16_t i;

        for (i = 10; i >= 0; i--) {
            Sprite *pl = sprTable[i];

            if (!pl->onScreen)
                continue;
            if (pl->sentAway)
                continue;

            if (!team->ballOutOfPlayOrKeeper) {
                if (pl->playerOrdinal == 1)
                    continue;
            }

            if (pl == team->controlledPlayer)
                continue;
            if (pl == team->passingKickingPlayer)
                continue;
            if (pl->playerState != PL_NORMAL)
                continue;

            {
                int32_t dist = pl->ballDistance;
                if ((uint32_t)dist >= (uint32_t)minDist)
                    continue;
                minDist = dist;
                nearest = pl;
            }
        }

        if (nearest == (Sprite *)0)
            return;

        /* Update passToPlayerPtr, stopping old if needed */
        if (nearest != team->passToPlayerPtr) {
            Sprite *old = team->passToPlayerPtr;
            if (old != (Sprite *)0) {
                if (old->playerState == PL_NORMAL) {
                    old->destX = old->x.s.i;
                    old->destY = old->y.s.i;
                }
            }
        }

        team->passToPlayerPtr = nearest;
    }
}

/* ==================================================================
 * processInputForPlayer — common input dispatch logic
 *
 * Shared by player 1 and player 2 paths. Reads joystick state,
 * writes direction/fire fields on team struct, handles fire counter
 * classification (quickFire vs normalFire), subs menu override.
 * Returns: 1 if should exit early, 0 otherwise.
 * ================================================================== */
static int16_t processInputForPlayer(TeamGeneralInfo *team,
                                     int16_t direction, int8_t shortFire,
                                     int8_t fire, int16_t *fireCounterPtr) {
    int16_t fc;

    team->currentAllowedDirection = direction;
    team->direction = direction;
    team->fireThisFrame = shortFire;
    team->firePressed = fire;

    if (fire) {
        team->fireCounter++;
    } else {
        team->fireCounter = 0;
    }

    /* Classify fire counter into quickFire / normalFire */
    fc = *fireCounterPtr;
    if (fc == 0) {
        /* No fire press pending */
        team->quickFire = 0;
        team->normalFire = 0;
    } else if (fc < 0) {
        /* Negative fire counter (release-bounce) */
        team->quickFire = 0;
        if (team->normalFire) {
            team->normalFire = 0;
        }
        team->quickFire = 0;
        /* Check threshold: fc < -4 → normalFire */
        if (fc < -4) {
            team->normalFire = -1; /* 0xFF = true (slt sets to -1 when true) */
        } else {
            team->normalFire = 0;
        }
        if (fc >= -4) {
            /* fc in [-4..-1]: no action, keep as-is */
        } else {
            *fireCounterPtr = 0;
        }
    } else {
        /* Positive fire counter (press registered) */
        team->quickFire = -1;  /* sne: non-zero → 0xFF */
        team->normalFire = -1;
        /* fc <= 4 → quickFire, fc > 4 → normalFire */
        if (fc <= 4) {
            team->quickFire = -1;
            team->normalFire = 0;
        } else {
            team->quickFire = 0;
            team->normalFire = -1;
        }
        *fireCounterPtr = 0;
    }

    /* Check resetControls */
    if (team->resetControls)
        return 1;

    /* If in substitutes menu, clear all controls */
    if (inSubstitutesMenu) {
        team->currentAllowedDirection = -1;
        team->quickFire = 0;
        team->normalFire = 0;
        team->firePressed = 0;
        team->fireThisFrame = 0;
        team->fireCounter = 0;
    }

    /* Call UpdatePlayers */
    UpdatePlayers(team);

    /* Check headerOrTackle */
    if (team->headerOrTackle) {
        team->headerOrTackle = 0;
        *fireCounterPtr = 0;
        return 1;
    }

    return 0;
}

/* ==================================================================
 * UpdateAndApplyTeamControls — per-frame input dispatch
 *
 * Called from GameLoop. Alternates between top and bottom team
 * each frame (frameCount bit 0). Polls joystick, sets direction/fire
 * on TeamGeneralInfo, calls UpdatePlayers.
 * ================================================================== */
__attribute__((used, externally_visible))
void UpdateAndApplyTeamControls(void) {
    TeamGeneralInfo *team;

    UpdateGameTimersAndCameraBreakMode();

    /* Fire blocking: wait until both fire buttons released */
    if (fireBlocked) {
        Player1StatusProc();
        if (pl1Fire)
            return;
        Player2StatusProc();
        if (pl2Fire)
            return;
        fireBlocked = 0;
        pl1FireCounter = 0;
        pl2FireCounter = 0;
        return;
    }

    /* Alternate teams each frame */
    if (frameCount & 1) {
        team = &topTeamData;
    } else {
        team = &bottomTeamData;
    }

    /* Update controlled player and pass target */
    UpdateControlledPlayer(team);
    UpdatePlayerBeingPassedTo(team);

    /* Dispatch based on playerNumber */
    if (team->playerNumber == 0) {
        /* No human player for this team */
        /* resetControls check */
        if (team->resetControls)
            return;

        /* Subs menu override */
        if (inSubstitutesMenu) {
            team->currentAllowedDirection = -1;
            team->quickFire = 0;
            team->normalFire = 0;
            team->firePressed = 0;
            team->fireThisFrame = 0;
            team->fireCounter = 0;
        }

        UpdatePlayers(team);

        if (team->headerOrTackle) {
            team->headerOrTackle = 0;
            /* No fire counter to clear for AI team */
        }
        return;
    }

    if (team->playerNumber == 2) {
        /* Player 2 controls this team */
        Player2StatusProc();
        processInputForPlayer(team, pl2Direction, pl2ShortFire, pl2Fire,
                              &pl2FireCounter);
        return;
    }

    /* Player 1 controls this team (playerNumber == 1 or other) */
    Player1StatusProc();
    processInputForPlayer(team, pl1Direction, pl1ShortFire, pl1Fire,
                          &pl1FireCounter);
}
