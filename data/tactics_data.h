/*
 * data/tactics_data.h — Extern declarations for tactics data
 *
 * NOT part of the build yet — lives in data/ for preparation.
 * When ready, move to src/ or include/ and add to the Makefile.
 *
 * Struct definitions (TacticsPosition, TacticsSlotIndex, enums) are in
 * data/game_types.h. TeamTactics and PlayerPositions are in include/game_types.h.
 */
#ifndef DATA_TACTICS_DATA_H
#define DATA_TACTICS_DATA_H

#include "game_types.h"

/* Forward declarations — these types already exist in include/game_types.h */
typedef struct PlayerPositions PlayerPositions;
typedef struct TeamTactics TeamTactics;

/* ======================================================================
 * TacticsPosition arrays — one per formation
 * ====================================================================== */

extern TacticsPosition tacticsPosition442;
extern TacticsPosition tacticsPosition541;
extern TacticsPosition tacticsPosition451;
extern TacticsPosition tacticsPosition532;
extern TacticsPosition tacticsPosition352;
extern TacticsPosition tacticsPosition433;
extern TacticsPosition tacticsPosition424;
extern TacticsPosition tacticsPosition343;
extern TacticsPosition tacticsPositionSweep;
extern TacticsPosition tacticsPosition523;
extern TacticsPosition tacticsPositionAttack;
extern TacticsPosition tacticsPositionDefend;
extern TacticsPosition tacticsPositionUSERA;
extern TacticsPosition tacticsPositionUSERB;
extern TacticsPosition tacticsPositionUSERC;
extern TacticsPosition tacticsPositionUSERD;
extern TacticsPosition tacticsPositionUSERE;
extern TacticsPosition tacticsPositionUSERF;

/* Pointer table for all 18 formation position mappings */
extern TacticsPosition *playersPositionsByTactics[18];

/* ======================================================================
 * TeamTactics — built-in formation definitions (12 preset + 6 user)
 * ====================================================================== */

extern TeamTactics teamTactics442;
extern TeamTactics teamTactics541;
extern TeamTactics teamTactics451;
extern TeamTactics teamTactics532;
extern TeamTactics teamTactics352;
extern TeamTactics teamTactics433;
extern TeamTactics teamTactics424;
extern TeamTactics teamTactics343;
extern TeamTactics teamTacticsSweep;
extern TeamTactics teamTactics523;
extern TeamTactics teamTacticsAttack;
extern TeamTactics teamTacticsDefend;

/* User-defined tactics (BSS — zeroed at init) */
extern TeamTactics teamTacticsUSERA;
extern TeamTactics teamTacticsUSERB;
extern TeamTactics teamTacticsUSERC;
extern TeamTactics teamTacticsUSERD;
extern TeamTactics teamTacticsUSERE;
extern TeamTactics teamTacticsUSERF;

/* Working copies for tactics editor */
extern TeamTactics editTacticsCurrentTactics;
extern TeamTactics savedCurrentTactics;

/* Pointer table for all 19 tactics (18 formations + editor working copy) */
extern TeamTactics *teamTacticsTable[19];

/* ======================================================================
 * Skill and quadrant tables
 * ====================================================================== */

/* 7x35 skill values per quadrant for player evaluation */
extern uint8_t skillsPerQuadrant[245];

/* Skill adjustment deltas for quadrant-based processing */
extern int8_t skillQuadrantAdjustDeltas[37];

/* Distance factor lookup for quadrant calculations (35x35 matrix, 1226 bytes) */
extern uint8_t quadrantDistanceFactor[1226];

/* Skill ponder weights for price calculation */
extern uint32_t priceSkillPonders[35];

/* Quadrant coordinate-to-ball-quadrant mapping */
extern int8_t quadrantCoordinatesToBallQuadrant[240];

/* ======================================================================
 * Misc tactics-related variables
 * ====================================================================== */

/* Tactics filename buffer for import */
extern uint8_t importTacticsFilename[14];

/* Player position arrays (working buffers) */
extern uint8_t playerPositionsArray[32];
extern uint8_t oldPlayerPositionsArray[32];

/* Coach animation frames */
extern uint16_t ourCoachFrames[16];
extern uint16_t opponentCoachFrames[16];

/* Formation menu state */
extern uint16_t formationMenuX;
extern uint16_t selectedFormationEntry;

#endif /* DATA_TACTICS_DATA_H */
