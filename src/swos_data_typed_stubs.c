/*
 * swos_data_typed_stubs.c — typed replacements for the remaining
 * `char foo[4096]` auto-stubs in swos_amiga_data_stubs.c.
 *
 * Every symbol here had a 4K zero-filled placeholder so the linker was
 * happy, but the type, size, and (for arrays) initial data all came
 * out wrong. This file gives each one a proper typed definition
 * transcribed from the Amiga C port's Data.c — matching every
 * extern declaration in the game code.
 *
 * Zero-initialised scalars and structs are just tentative definitions;
 * initialised arrays and the ball-out-of-play pointer tables carry
 * their real Amiga values. References Position struct from SWOS2.h
 * (word x; word y;) that's not yet in our game_types.h — declared
 * locally below.
 */

#include <stdint.h>
#include "game_types.h"
#include "menu_system.h"

/* Forward type declarations needed by this file only. Our headers
 * already define TeamFile/TeamFileEx/TeamGame/TeamGeneralInfo/
 * JobOffer/ScorerInfo/Sprite/MenuEntry/DIYCompetition/CompetitionCup;
 * Position and the GameTypes enum are not yet there. */
typedef struct {
    uint16_t x;
    uint16_t y;
} Position;

typedef uint16_t GameTypes;

/* CompetitionCup forward (pointer-only use here — full struct in
 * competition_data.h). */
/* CompetitionCup defined in game_types.h */


/* DIY_Competition alias — Data.c uses DIY_Competition; our headers
 * use DIYCompetition. Same 96-byte layout. */
typedef DIYCompetition DIY_Competition;

/* ---- 42 Position arrays (Amiga formation / set-piece player positions) + 2 pointer tables ---- */
Position bottomCornerLeftPositions[11] = { { 250, 5 }, { 100, 600 }, { 200, 277 }, { 300, 277 }, { 490, 600 }, { 150, 500 }, { 250, 600 }, { 350, 500 }, { 450, 555 }, { 210, 611 }, { 290, 611 } };
Position topCornerLeftPositions[11] = { { 250, 2 }, { 175, 38 }, { 225, 22 }, { 275, 22 }, { 325, 38 }, { 150, 111 }, { 200, 55 }, { 300, 83 }, { 400, 166 }, { 200, 300 }, { 300, 300 } };
Position bottomCornerRightPositions[11] = { { 250, 5 }, { 10, 600 }, { 200, 277 }, { 300, 277 }, { 400, 277 }, { 50, 555 }, { 150, 500 }, { 250, 600 }, { 350, 500 }, { 210, 611 }, { 290, 611 } };
Position topCornerRightPositions[11] = { { 250, 2 }, { 175, 38 }, { 225, 22 }, { 275, 22 }, { 325, 38 }, { 100, 166 }, { 200, 83 }, { 300, 55 }, { 350, 111 }, { 200, 300 }, { 300, 300 } };
Position bottomGoalOutLeftPositions[11] = { { 290, 10 }, { 100, 166 }, { 200, 133 }, { 300, 133 }, { 400, 166 }, { 100, 355 }, { 200, 277 }, { 300, 277 }, { 400, 355 }, { 200, 444 }, { 300, 444 } };
Position topGoalOutLeftPositions[11] = { { 250, 11 }, { 100, 144 }, { 200, 144 }, { 300, 144 }, { 400, 144 }, { 100, 244 }, { 200, 277 }, { 300, 277 }, { 400, 244 }, { 200, 444 }, { 300, 444 } };
Position bottomGoalOutRightPositions[11] = { { 210, 10 }, { 100, 166 }, { 200, 133 }, { 300, 133 }, { 400, 166 }, { 100, 355 }, { 200, 277 }, { 300, 277 }, { 400, 355 }, { 200, 444 }, { 300, 444 } };
Position topGoalOutRightPositions[11] = { { 250, 11 }, { 100, 144 }, { 200, 144 }, { 300, 144 }, { 400, 144 }, { 100, 244 }, { 200, 277 }, { 300, 277 }, { 400, 244 }, { 200, 444 }, { 300, 444 } };
Position bottomKeeperHoldsBallPositions[11] = { { 1000, 0 }, { 100, 166 }, { 200, 133 }, { 300, 133 }, { 400, 166 }, { 100, 355 }, { 200, 277 }, { 300, 277 }, { 400, 355 }, { 200, 444 }, { 300, 444 } };
Position topKeeperHoldsBallPositions[11] = { { 250, 11 }, { 100, 144 }, { 200, 144 }, { 300, 144 }, { 400, 144 }, { 100, 244 }, { 200, 277 }, { 300, 277 }, { 400, 244 }, { 200, 444 }, { 300, 444 } };
Position bottomStartingPositions[11] = { { 250, 1 }, { 100, 155 }, { 200, 133 }, { 300, 133 }, { 400, 155 }, { 60, 320 }, { 190, 244 }, { 310, 244 }, { 355, 320 }, { 220, 320 }, { 260, 316 } };
Position topStartingPositions[11] = { { 250, 11 }, { 120, 133 }, { 210, 133 }, { 290, 133 }, { 380, 133 }, { 90, 244 }, { 220, 200 }, { 280, 200 }, { 410, 244 }, { 200, 277 }, { 300, 277 } };
Position topFreeKickCenterPositions[11] = { { 250, 2 }, { -1015, -54 }, { -1005, -55 }, { 1005, -55 }, { 1015, -54 }, { -1080, 20 }, { -1050, -55 }, { 1060, -45 }, { 1070, 40 }, { 22222, 22222 }, { 22222, 22222 } };
Position bottomFreeKickCenterPositions[11] = { { 250, 5 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { -1090, 20 }, { -1035, -35 }, { 1020, -60 }, { 1120, 30 }, { -1020, -10 }, { 1040, -15 } };
Position topFreeKickRight3Positions[11] = { { 250, 2 }, { 290, 22 }, { 220, 27 }, { -1048, -29 }, { -1043, -37 }, { 160, 88 }, { 160, 50 }, { 250, 27 }, { 310, 22 }, { 22222, 22222 }, { 22222, 22222 } };
Position bottomFreeKickRight3Positions[11] = { { 250, 5 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { -1000, -15 }, { 1030, -25 }, { 200, 500 }, { 340, 522 }, { 220, 605 }, { 260, 611 } };
Position topFreeKickLeft1Positions[11] = { { 250, 2 }, { 1043, -37 }, { 1048, -29 }, { 280, 27 }, { 210, 22 }, { 190, 22 }, { 250, 27 }, { 340, 50 }, { 340, 88 }, { 22222, 22222 }, { 22222, 22222 } };
Position bottomFreeKickLeft1Positions[11] = { { 250, 5 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 160, 522 }, { 300, 500 }, { -1030, -25 }, { 1000, -15 }, { 240, 611 }, { 280, 605 } };
Position topFreeKickRight2Positions[11] = { { 250, 2 }, { -1048, -26 }, { -1043, -31 }, { -1037, -36 }, { -1030, -41 }, { 190, 11 }, { 220, 38 }, { 250, 33 }, { 300, 22 }, { 22222, 22222 }, { 22222, 22222 } };
Position bottomFreeKickRight2Positions[11] = { { 250, 5 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { -1000, -15 }, { 1030, -25 }, { 250, 411 }, { 340, 522 }, { 220, 611 }, { 260, 611 } };
Position topFreeKickLeft2Positions[11] = { { 250, 2 }, { 1030, -41 }, { 1037, -36 }, { 1043, -31 }, { 1048, -26 }, { 200, 22 }, { 250, 33 }, { 280, 38 }, { 310, 11 }, { 22222, 22222 }, { 22222, 22222 } };
Position bottomFreeKickLeft2Positions[11] = { { 250, 5 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 160, 522 }, { 250, 411 }, { -1030, -25 }, { 1000, -15 }, { 240, 611 }, { 280, 611 } };
Position topFreeKickRight1Positions[11] = { { 250, 2 }, { -1038, -44 }, { -1031, -47 }, { -1023, -50 }, { -1014, -53 }, { 190, 11 }, { 220, 38 }, { 250, 33 }, { 300, 22 }, { 22222, 22222 }, { 22222, 22222 } };
Position bottomFreeKickRight1Positions[11] = { { 250, 5 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { -1000, -15 }, { 1030, -25 }, { 250, 411 }, { 340, 522 }, { 220, 611 }, { 260, 611 } };
Position topFreeKickLeft3Positions[11] = { { 250, 2 }, { 1014, -53 }, { 1023, -50 }, { 1031, -47 }, { 1038, -44 }, { 200, 22 }, { 250, 33 }, { 280, 38 }, { 310, 11 }, { 22222, 22222 }, { 22222, 22222 } };
Position bottomFreeKickLeft3Positions[11] = { { 250, 5 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 160, 522 }, { 250, 411 }, { -1030, -25 }, { 1000, -15 }, { 240, 611 }, { 280, 611 } };
Position bottomPenaltyPositions[11] = { { 250, 5 }, { 100, 277 }, { 200, 277 }, { 300, 277 }, { 400, 277 }, { 100, 500 }, { 200, 500 }, { 300, 500 }, { 400, 500 }, { 220, 555 }, { 250, 522 } };
Position topPenaltyPositions[11] = { { 250, 2 }, { 175, 88 }, { 225, 122 }, { 275, 122 }, { 325, 88 }, { 70, 155 }, { 200, 166 }, { 300, 166 }, { 430, 155 }, { 150, 300 }, { 300, 300 } };
Position bottomThrowInForwardRightPositions[11] = { { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { -1075, 0 }, { -1025, 0 }, { 22222, 22222 }, { 22222, 22222 }, { -1075, 50 }, { -1050, 75 }, { 22222, 22222 }, { 22222, 22222 } };
Position bottomThrowInCenterRightPositions[11] = { { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { -1100, 25 }, { -1075, -20 }, { -1025, -25 }, { 22222, 22222 }, { -1075, 75 } };
Position bottomThrowInBackRightPositions[11] = { { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { -1075, -50 }, { -1075, 0 }, { -1025, 0 }, { 22222, 22222 }, { 22222, 22222 } };
Position bottomThrowInForwardLeftPositions[11] = { { 22222, 22222 }, { 1025, 0 }, { 1075, 0 }, { 22222, 22222 }, { 22222, 22222 }, { 1075, 50 }, { 1050, 75 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 } };
Position bottomThrowInCenterLeftPositions[12] = { { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 1025, -25 }, { 1075, -20 }, { 1100, 25 }, { 22222, 22222 }, { 22222, 22222 }, { 1075, 75 }, { 22222, 22222 } };
Position bottomThrowInBackLeftPositions[11] = { { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 }, { 1025, 0 }, { 1075, 0 }, { 1075, -50 }, { 22222, 22222 }, { 22222, 22222 }, { 22222, 22222 } };
Position bottomStartingGamePositions[11] = { { 380, 255 }, { 365, 261 }, { 350, 266 }, { 335, 272 }, { 320, 277 }, { 305, 283 }, { 290, 288 }, { 275, 294 }, { 260, 300 }, { 245, 305 }, { 230, 311 } };
Position topStartingGamePositions[11] = { { 120, 255 }, { 135, 261 }, { 150, 266 }, { 165, 272 }, { 180, 277 }, { 195, 283 }, { 210, 288 }, { 225, 294 }, { 240, 300 }, { 255, 305 }, { 270, 311 } };
Position bottomLeavingPitchPositions[11] = { { 550, 277 }, { 550, 277 }, { 550, 277 }, { 550, 277 }, { 550, 277 }, { 550, 277 }, { 550, 277 }, { 550, 277 }, { 550, 277 }, { 550, 277 }, { 550, 277 } };
Position topLeavingPitchPositions[11] = { { -50, 377 }, { -50, 377 }, { -50, 377 }, { -50, 377 }, { -50, 377 }, { -50, 377 }, { -50, 377 }, { -50, 377 }, { -50, 377 }, { -50, 377 }, { -50, 377 } };
Position bottomFirstExtraStartingPositions[11] = { { 250, 1 }, { 100, 155 }, { 200, 133 }, { 300, 133 }, { 400, 155 }, { 60, 320 }, { 190, 244 }, { 310, 244 }, { 355, 320 }, { 220, 320 }, { 260, 316 } };
Position topFirstExtraStartingPositions[11] = { { 250, 11 }, { 120, 133 }, { 210, 133 }, { 290, 133 }, { 380, 133 }, { 90, 244 }, { 220, 200 }, { 280, 200 }, { 410, 244 }, { 200, 277 }, { 300, 277 } };
Position bottomPenaltiesPositions[11] = { { 200, 522 }, { 260, 344 }, { 280, 333 }, { 300, 333 }, { 320, 344 }, { 260, 366 }, { 280, 355 }, { 300, 355 }, { 320, 366 }, { 280, 377 }, { 300, 377 } };
Position topPenaltiesPositions[11] = { { 250, 2 }, { 260, 277 }, { 280, 266 }, { 300, 266 }, { 320, 277 }, { 260, 300 }, { 280, 288 }, { 300, 288 }, { 320, 300 }, { 280, 311 }, { 300, 311 } };
Position * bottomBallOutOfPlayPositions[32] = { bottomStartingPositions, bottomGoalOutLeftPositions, bottomGoalOutRightPositions, bottomKeeperHoldsBallPositions, bottomCornerLeftPositions, bottomCornerRightPositions, bottomFreeKickLeft1Positions, bottomFreeKickLeft2Positions, bottomFreeKickLeft3Positions, bottomFreeKickCenterPositions, bottomFreeKickRight1Positions, bottomFreeKickRight2Positions, bottomFreeKickRight3Positions, 0, bottomPenaltyPositions, bottomThrowInForwardRightPositions, bottomThrowInCenterRightPositions, bottomThrowInBackRightPositions, bottomThrowInForwardLeftPositions, bottomThrowInCenterLeftPositions, bottomThrowInBackLeftPositions, bottomStartingGamePositions, bottomStartingGamePositions, bottomLeavingPitchPositions, bottomLeavingPitchPositions, bottomLeavingPitchPositions, bottomLeavingPitchPositions, bottomFirstExtraStartingPositions, bottomFirstExtraStartingPositions, 0, 0, bottomPenaltiesPositions };
Position * topBallOutOfPlayPositions[32] = { topStartingPositions, topGoalOutLeftPositions, topGoalOutRightPositions, topKeeperHoldsBallPositions, topCornerLeftPositions, topCornerRightPositions, topFreeKickLeft1Positions, topFreeKickLeft2Positions, topFreeKickLeft3Positions, topFreeKickCenterPositions, topFreeKickRight1Positions, topFreeKickRight2Positions, topFreeKickRight3Positions, 0, topPenaltyPositions, 0, 0, 0, 0, 0, 0, topStartingGamePositions, topStartingGamePositions, topLeavingPitchPositions, topLeavingPitchPositions, topLeavingPitchPositions, topLeavingPitchPositions, topFirstExtraStartingPositions, topFirstExtraStartingPositions, 0, 0, topPenaltiesPositions };

/* ---- 446 formerly-stubbed globals: typed storage + the real values that had them ---- */
TeamFile tertiaryQualifiedTeamGrid;
TeamFile euroCupTitleHolderGrid;
TeamFile cupWinnersCupTitleHolderGrid;
TeamFile uefaCupTitleHolderGrid;
uint32_t teamFileDataSize;
uint16_t teamFileTeamCount;
uint8_t lastLoadedExtNumber;
TeamGame topTeamSaved;
TeamGame bottomTeamSaved;
TeamGame topTeamInGame;
TeamGame bottomTeamInGame;
uint32_t magicValue;
uint8_t gameRound;
uint8_t diskImageContext;
uint16_t nextGoal;
uint16_t bootTemp0;
uint16_t bootTemp1;
uint16_t bootTemp2;
uint16_t bootTemp3;
uint16_t bootTemp4;
uint16_t bootTemp5;
uint16_t bootTemp6;
uint16_t kSpriteBlitQueueSentinel_A;
uint16_t kSpriteBlitQueueSentinel_B;
uint16_t ballSpeed;
uint16_t ballSpeedAfterSave;
uint16_t gateReceiptsFactor;
uint16_t foreignMarketRandomPickRetries;
uint16_t homeMarketRandomPickRetries;
uint16_t offerSearchRetryCount;
uint16_t goalkeeperDiveAttemptCount;
/* diyFileBufferCopy / diyCompetitionStart / competitionFileBuffer:
 * backing storage moved to swos_globals.c, sized big enough for the
 * 2900-byte word-copy in _SaveDIYTournament (was clobbering magicValue
 * when only 1508 bytes were allocated). The named symbols are .equ
 * aliases pointing at the backing buffers so all the
 *   `extern DIYCompetitionEx diyFileBufferCopy;` / `.base.field`
 * struct-style access still works unchanged. */
uint16_t diyMinSubstitutes;
uint16_t diyPointsForAWin;
uint16_t diyLeagueMinSubstitutes;
uint16_t diyLeagueMaxSubstitutes;
uint16_t diyHasGroups;
/* diyTeamEntryOffsetsByIndex / diyTeamShuffleIndices: backing storage
 * moved to swos_globals.c. On Amiga BSS these labels sit inside the
 * `diyCompetitionStart` run — diyTeamEntryOffsetsByIndex at offset
 * 0x6E (overlapping the league teamEntryOffsets field that
 * _FindTeamInDIY reads as `comp + 0x6E`), and diyTeamShuffleIndices at
 * 0xEE. The writes spill across the run on both labels, so the global
 * stores AND `comp + offset` reads must hit the same memory. */
uint16_t diyLeagueCurrentSlot;
uint16_t diyLeagueMatchProgress;
uint16_t diyLeagueTotalMatches;
TeamFile tempTeamBuffer;
/* numberOfFiles: backing storage moved to swos_globals.c. On Amiga
 * the label overlaps the start of foundFilenamesBuffer's BSS run —
 * AssetSys_ResolveAssetList writes the file count at &numberOfFiles
 * and the 32-byte file entries from &numberOfFiles + 4 onwards (which
 * IS foundFilenamesBuffer on Amiga). The .equ alias mirrors that
 * overlap so reads via foundFilenamesBuffer see the entries the
 * writer just produced. */
uint8_t poolPlayerStrideRef;
uint32_t imageStreamBytesLeft;
uint16_t replayState;
uint16_t userRequestedReplay;
uint16_t throwInDeadVar;
uint16_t savedCameraX;
uint16_t savedCameraY;
uint16_t teamsLoaded;
uint16_t poolplyrLoaded;
uint8_t highestScorersTable;
uint32_t menuDataOffset;
uint32_t menuFreeSpace;
MenuEntry templateMenuEntry;
uint16_t exitMenu;
uint16_t numSpritesToRender;
uint16_t hilNumGoals;
uint16_t hilTeam1Goals;
uint16_t hilTeam2Goals;
uint16_t hilPitchNumber;
uint16_t hilMaxSubstitutes;
uint8_t audioDbgFlagA  = 0;
uint8_t team1GoalieFaces  = 0;
uint8_t team2GoalieSkinType  = 0;
uint8_t subsFire  = 0;
uint8_t pl1WasLeft  = 0;
uint8_t pl1WasRight  = 0;
uint8_t pl1WasUp  = 0;
uint8_t pl1WasDown  = 0;
uint8_t pl1LastFired  = 0;
uint8_t pl1Left  = 0;
uint8_t pl1Right  = 0;
uint8_t pl1Up  = 0;
uint8_t pl1Down  = 0;
uint8_t pl1Fire  = 0;
uint8_t pl1UpRight  = 0;
uint8_t pl1UpLeft  = 0;
uint8_t pl1DownRight  = 0;
uint8_t pl1DownLeft  = 0;
uint8_t pl1LeftPress  = 0;
uint8_t pl1RightPress  = 0;
uint8_t pl1UpPress  = 0;
uint8_t pl1DownPress  = 0;
uint8_t pl1ShortFire  = 0;
uint8_t pl2WasLeft  = 0;
uint8_t pl2WasRight  = 0;
uint8_t pl2WasUp  = 0;
uint8_t pl2WasDown  = 0;
uint8_t pl2LastFired  = 0;
uint8_t pl2Left  = 0;
uint8_t pl2Right  = 0;
uint8_t pl2Up  = 0;
uint8_t pl2Down  = 0;
uint8_t pl2Fire  = 0;
uint8_t pl2UpRight  = 0;
uint8_t pl2UpLeft  = 0;
uint8_t pl2DownRight  = 0;
uint8_t pl2DownLeft  = 0;
uint8_t pl2LeftPress  = 0;
uint8_t pl2RightPress  = 0;
uint8_t pl2UpPress  = 0;
uint8_t pl2DownPress  = 0;
uint8_t pl2ShortFire  = 0;
uint8_t keyIn  = 0;
uint8_t seed  = 0;
uint8_t randXorIndex  = 0;
uint8_t randXorKey  = 0;
TeamGeneralInfo topTeamData = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
TeamGeneralInfo bottomTeamData = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
uint8_t playerTurnFlags  = 0;
uint8_t up  = 0;
uint8_t down  = 0;
uint8_t left  = 0;
uint8_t right  = 0;
uint8_t shirtType1  = 0;
uint8_t shirtType2  = 0;
uint8_t currentCompetitionId  = 0;
/* diyLeaguePitchTypeStrTable: real packed-struct definition in
 * swos_amiga_ptr_imports.c. The auto-extractor mis-classified it as a
 * scalar with the SWOS2.s file offset (188198) as its value. */
/* diyCupPitchTypeStringTable, diyCupAwayGoalsStringTable,
 * diyPitchTypeStringTable: real packed-struct definitions in
 * swos_amiga_ptr_imports.c. The auto-extractor mis-classified all
 * three as uint32_t scalars holding the SWOS2.s file offset value. */
/* All 31 of `diyAwayGoalsStrTab` and `diyStage[1..6]<Type>StrTab` were
 * mis-classified as uint32_t scalars holding SWOS2.s file-offset values.
 * Real packed-struct definitions live in swos_amiga_ptr_imports.c. */

/* diyTournamentStages: backing storage for the 6-stage cup-of-cups
 * config. The auto-extractor missed it because no caller references the
 * symbol by name; the StringTable indexPtrs reach it via
 * `diyTournamentStages + N*0xA + offset`. Each DIYStage = 10 bytes:
 * legs(0), extraTime(2), penalties(4), ptsForWin(6), playCount(8). */
DIYStage diyTournamentStages[6];

/* diyTournamentCompetition: a third DIY_Competition struct living at
 * Amiga BSS offset 0x2DAD0 — same layout as `diyCompetitionStart` but
 * used by the TOURNAMENT designer (designDiyTournamentMenu). The Amiga
 * label was unnamed (referenced as `lea ($2DAD0).l,a0` in disasm) so
 * the auto-extractor never created a symbol; the menu data hardcodes
 * `dc.l $2DAD4` as the competitionName text pointer (offset 4 of the
 * struct), and that literal got faithfully copied into menu_data.c as
 * `(int8_t*)0x2DAD4` — pointing at unmapped memory in our build. Give
 * it a real DIYCompetitionEx allocation here and have menu_data.c
 * reference `&diyTournamentCompetition.base.competitionName` instead. */
DIYCompetitionEx diyTournamentCompetition;
/* diyCompetitionMenu: real packed-menu blob in menu_data.c (same
 * auto-extractor mis-classification as diskMenu). */
uint8_t nationalityOrdinal  = 0;
/* diskMenu: real packed-menu blob in menu_data.c. The auto-extractor
 * mis-classified it as a scalar with the SWOS2.s file offset (1312736)
 * as its value, so any _ShowMenu(diskMenu) crashed in _UnpackMenu
 * when it walked off the end of the 4-byte allocation. */
uint8_t trialistNameBufTerminator  = 0;
uint32_t loaderBankBitmap;
uint32_t loaderParam;
uint32_t loaderMemTop;
uint16_t menuCursorFrame;
uint16_t twoPlayers;
uint16_t player1ClearFlag;
uint16_t player2ClearFlag;
uint16_t deadVar01;
uint16_t deadVar02;
uint16_t internationalAutoAdvanceFlag;
uint16_t seasonUpdateInProgress;
uint16_t writeOnlyVar01;
uint16_t highlightedLeagueTeamId;
uint16_t oldPlayerSkill;
uint16_t newPlayerSkill;
uint16_t team1SelectedPlayerIndex;
uint16_t team2SelectedPlayerIndex;
/* careerFileBuffer: backing storage moved to swos_globals.c. On
 * Amiga the label is a 2-byte ds.b at the head of a ~98 KB BSS run
 * that the save-file header overlays — saveFileVersion, the three
 * 4-byte magic words, and saveFileAmigaType all sit at fixed offsets
 * within this run (see the .equ aliases). Sized big enough for a
 * full career save (header + 64 teams * 684 bytes). */
uint16_t currentSeasonShowing;
uint16_t stayingInOldClub;
uint16_t overdraftStreak;
uint16_t overdraftWarningState;
uint16_t performanceWarningState;
uint16_t overdraftWarningCooldown;
uint16_t performanceWarningCooldown;
uint16_t youreSacked;
uint16_t retired;
uint16_t competitionSlotUpdatedFlag;
uint16_t careerTeamLostMatch;
uint16_t clubBusinessActive;
uint32_t newBalance;
uint32_t old_balance;
uint32_t playerPurchases;
uint32_t playerSales;
uint32_t generalRunningCosts;
uint32_t playerWagesBill;
uint32_t gateReceipts;
uint32_t startingBalance;
uint32_t startingBalanceCopy;
uint32_t competitionBonuses;
uint32_t sensisoftPlcSponsorship;
uint16_t financeTotalsNeedResetFlag;
uint32_t weeklyRunningCosts;
uint32_t weeklyPlayerWages;
uint32_t totalTeamValue;
uint16_t sensisoftPlcSponsorshipFactor;
uint16_t additionalInvestments;
uint16_t teamValueDivisor;
uint16_t expectedLeagueRankIndex;
uint16_t boardConfidenceHistory0;
uint16_t boardConfidenceHistory1;
uint16_t boardConfidenceHistory2;
uint16_t boardConfidenceHistory3;
uint16_t boardConfidenceScore;
uint16_t previousBoardConfidence;
uint16_t playWinCupAnimation;
uint16_t minorTrophyBonusActive;
uint16_t leagueTitleBonusActive;
uint16_t majorTrophyBonusActive;
uint16_t cupBonus1Active;
uint16_t cupBonus2Active;
uint16_t wonWorldCup;
uint16_t wonContinentalCup;
uint16_t wonInvitationTournament;
uint16_t qualifyingStage1PendingFlag0;
uint16_t qualifyingStage1PendingFlag1;
uint16_t qualifyingStage2PendingFlag0;
uint16_t qualifyingStage2PendingFlag1;
uint16_t isCareer;
uint16_t careerMatchSimulatedFlag;
uint16_t gamesPlayed;
uint16_t numWins;
uint16_t numDrawn;
uint16_t numLost;
uint16_t goalsScored;
uint16_t goalsTaken;
uint16_t boardConfidenceMod0;
uint16_t boardConfidenceMod1;
uint16_t boardConfidenceMod2;
uint16_t boardConfidenceMod3;
uint16_t boardConfidenceValue0;
uint16_t boardConfidenceValue1;
uint16_t boardConfidenceMod4;
uint16_t boardConfidenceMod5;
uint16_t boardConfidenceMod6;
uint16_t boardConfidenceMod7;
uint16_t boardConfidenceMod8;
uint16_t boardConfidenceMod9;
uint16_t contractNegotiationActive;
uint16_t wageScalingDivisor;
uint16_t teamNoOfferAccepted;
uint32_t teamFundsAccepted;
uint16_t isNationalSelector;
uint8_t someTeamFlags;
uint16_t lastNationalityCall;
uint8_t nationalTeamNationality;
uint8_t nationalTeamFileId;
uint16_t sameNationalCallFlag;
uint16_t sackedReputationScore;
uint16_t nationalSelectorReputationPoints;
uint8_t nationalTeamFileCode;
uint16_t seasonReview;
uint32_t careerCupPickList;
uint16_t worldCupHostTeam;
uint16_t worldCupQualifyingTeam;
uint16_t euroChampionshipQualTeam;
uint16_t selTeamNumber;
uint16_t coachOrPlayer;
uint16_t careerTeamLeague;
uint16_t currentDivisionIndex;
uint16_t leagueFirstRoundNumber;
uint16_t playPromotedAnimation;
uint16_t playRelegatedAnimation;
uint16_t mrOrMs;
uint32_t managerSexBuffer;
uint8_t plNationality;
uint16_t winCupBonusCount;
uint16_t minorTrophyBonusCount;
uint16_t majorTrophyBonusCount;
uint16_t careerReputationPoints;
uint16_t removedOfferPlayersFromSelectedTeamCount;
uint16_t removedOfferPlayersFromOtherTeamsCount;
uint16_t homeMarketUpdateBacklog;
uint16_t foreignMarketUpdateBacklog;
uint16_t offerExpiryIterationsRemaining;
uint16_t numForeignMarketPlayers;
uint16_t numHomeMarketPlayers;
uint16_t numOffers;
uint16_t careerSeasonsCompleted;
uint16_t numForeignMarketPlayers2;
uint16_t pendingForeignPlayerCount;
uint16_t seasonTransferQuota;
uint16_t transferPoolSlotLimit;
uint16_t offerSwapSlotLimit;
uint16_t transferPoolSlotsUsed;
uint16_t offerSwapSlotsUsed;
uint16_t numNewJobOffers;
JobOffer newJobOffers[5];
uint16_t numJobOffers;
JobOffer jobOffers[5];
uint16_t timeToNegotiate;
uint16_t numPlayersOnTransferMarket;
uint16_t lastOfferPrimaryPlayerOrdinal;
uint16_t lastOfferExtraPlayersWanted;
uint16_t lastOfferSwapPlayerOrdinal;
uint16_t lastOfferExtraPlayerOrdinal;
uint16_t offerNegotiationAttemptCount;
uint16_t transferMarketRandOffset0;
uint16_t transferMarketRandOffset1;
uint16_t transferMarketRandOffset2;
uint16_t transferMarketPhase;
uint8_t careerUnavailablePlayerLookupTable;
uint8_t careerUnavailablePlayerNames;
/* careerTeam: backing storage moved to swos_globals.c. On Amiga the
 * label is `ds.b $50C` (1292 bytes — TeamFile + 32 PlayerFile slots),
 * but our TeamFileEx struct only declares 16 players (684 bytes). The
 * `_InitTeams` -> `teamInit(&careerTeam, 31)` path walks 32 player
 * slots, and on the SDL build the upper 16 slots overflow into adjacent
 * globals (most visibly stomping selTeamNumber). The .equ alias points
 * `careerTeam` at a properly sized 1292-byte buffer; struct field
 * accesses still resolve through the TeamFileEx layout, but the
 * trailing bytes are now backed by real memory. */
uint16_t careerNumPlayers;
uint16_t selTeamSelectedPlayerPosition;
uint16_t removedPlayerStatAccum;
uint8_t lastFinancialWeekIndex;
TeamFileEx currentTeam;
uint16_t numPlayers;
uint16_t currentTeamNumber;
uint16_t currentTeamSelectedPlayerPosition;
uint16_t trainingMatchSavedPlayerPos;
uint8_t trainingMatchSavedTactics;
uint16_t lastMatchSavedPlayerPos;
uint16_t lastMatchSavedCoachOrPlayer;
uint8_t currentTactics;
uint8_t lastMatchSavedTeamControls;
uint16_t seasonFinished;
uint16_t careerFinished;
uint16_t euroCupSlotIndex;
uint16_t seasonNumberOfSquads;
/* competitionContext: 4-byte stub; backing storage in swos_globals.c.
 * On Amiga it heads a ~2.8 KB adjacent BSS run, and
 * `_Career_StoreCompetitionSlotStateToBuffer(slotState)` writes up to
 * 1844 bytes here for slot 1. Size accordingly to avoid stomping
 * neighboring globals (most visibly careerNumPlayers). */
uint32_t secondaryCompStringOffset;
uint8_t minorTrophyBonusValue;
uint16_t secondaryCompWinnerTeam;
uint16_t secondaryCompSlotStateA;
uint16_t secondaryCompSlotStateB;
uint16_t secondaryCompSlotStateC;
uint16_t secondaryCompFinished;
/* tertiaryCompetitionContext: backing storage in swos_globals.c. */
uint32_t tertiaryCompStringOffset;
uint8_t leagueTitleBonusValue;
uint16_t tertiaryCompWinnerTeam;
uint16_t tertiaryCompSlotStateA;
uint16_t tertiaryCompSlotStateB;
uint16_t tertiaryCompSlotStateC;
uint16_t tertiaryCompFinished;
uint16_t diyCompSlotStateA;
uint16_t diyCompSlotStateB;
uint16_t diyCompSlotStateC;
/* euroCupsCompetitionContext: backing storage in swos_globals.c. */
uint32_t quaternaryCompStringOffset;
uint8_t majorTrophyOrCupBonusValue;
uint16_t quaternaryCompWinnerTeam;
uint16_t quaternaryCompSlotStateA;
uint16_t quaternaryCompSlotStateB;
uint16_t quaternaryCompSlotStateC;
uint8_t teamBudgetRankTable;
uint16_t teamSelectionKeyCacheIndex;
uint16_t teamSelectionKeyCacheOffset;
uint16_t teamSelectionKeyCacheCount;
uint16_t careerSlotProcessingInProgress;
/* teamSelectionKeyCache / teamSelectionKeyCacheEnd: backing buffer
 * defined in swos_globals.c. On Amiga BSS the cache spans ~16 KB
 * between these labels (with several other globals overlapping in
 * the middle), and InsertSpaceSorted bounds-checks against the End
 * label. SDL must mirror that single contiguous buffer or 34-entry
 * builds for a typical league overflow into the wrong globals. */
uint8_t careerJobOfferPhaseBias;
uint16_t competitionOver;
uint32_t primaryCompStringOffset;
uint8_t majorTrophyBonusValue;
uint16_t numDiyTeams;
uint16_t primaryCompWinnerTeam;
uint16_t primaryCompSquadViewParam;
uint16_t primaryCompSlotStateA;
uint16_t primaryCompSlotStateB;
uint16_t primaryCompSlotStateC;
uint16_t diyTeamMinDivisionIndex;
uint16_t diyTeamMaxDivisionOffset;
uint16_t leaguePromotionCount;
uint16_t leagueRelegationCount;
uint16_t leagueWarningsEnabled;
uint8_t viewRandomPhase;
uint8_t seasonPlaying;
uint16_t inFriendlyMenu;
uint16_t isNationalTeam;
GameTypes gameType;
uint16_t contestStarted;
uint16_t careerLoadedFlag;
int16_t gameLength;
int16_t autoReplays;
int16_t menuMusic;
int16_t autoSaveHighlights;
int16_t allPlayerTeamsEqual;
int16_t pitchType;
uint8_t seed2;
uint8_t randXorIndex2;
uint8_t randXorKey2;
uint8_t newRandomSeedCopy;
uint16_t randomSeed;
/* saveFileVersion / saveFileMagic1..3: aliased to fixed offsets
 * within careerFileBuffer in swos_globals.c. The Amiga writes them
 * as named globals during _SaveCompetitionFile and reads them after
 * _LoadFileEx populates careerFileBuffer; both paths require the
 * globals to live INSIDE the file buffer at the right offset. */
/* numSelectedTeams: aliased to careerFileBuffer + 98476 in
 * swos_globals.c. _SaveCareerFile computes the on-disk file size as
 * `(&numSelectedTeams - &careerFileBuffer) + numSelectedTeams*684 + 2`,
 * which only produces a sensible value if numSelectedTeams sits at
 * the Amiga's fixed offset within the career-file buffer. _LoadFileEx
 * then loads that count back into the same byte. */
/* selTeamNameBase: alias into selTeamsBackingBuf + 5 (see swos_globals.c) */
uint16_t saveGameLength;
uint32_t irqCookieObfuscated;
uint8_t chipRamBitmapEnd;
Sprite * sortedSprites[78];
/* ---- Chairman sell/exchange response string tables ----
 *
 * Both arrays index chairman-negotiation lines by an offer-outcome
 * code. The `+1` trick on the uint8_t strings skips the leading 0x00
 * byte that acts as a "no punctuation" marker at the front of
 * aAsThesePlayers and aAndWeWillAccep — the chairman engine prepends
 * byte 0 as a status glyph, and the +1 entry points past it to the
 * plain ASCII run. Transcribed from Data.c:2931/2936. */
extern const char aAsThisPlayerIs[];
extern uint8_t    aAsThesePlayers[35];
extern uint8_t    aAndWeWillAccep[38];
extern const char aThisPlayerFrom[];
extern const char aAndWeWillAccep_0[];
extern const char aThesePlayersFr[];
extern const char aButWeWouldBeHa[];
extern const char aFavourableOffe[];

char *exchangeFmtLine2b[9] = {
    0,
    (char *)aAsThisPlayerIs,
    (char *)aAsThesePlayers,
    (char *)(aAsThesePlayers + 1),
    (char *)aAndWeWillAccep,
    (char *)(aAndWeWillAccep + 1),
    (char *)aThisPlayerFrom,
    (char *)aAndWeWillAccep_0,
    (char *)aThisPlayerFrom,
};

char *sellExchangeResponseStrings[12] = {
    0, 0,
    (char *)aAsThisPlayerIs,
    (char *)aAsThesePlayers,
    (char *)(aAsThesePlayers + 1),
    (char *)aAndWeWillAccep,
    (char *)(aAndWeWillAccep + 1),
    (char *)aThisPlayerFrom,
    (char *)aAndWeWillAccep_0,
    (char *)aThesePlayersFr,
    (char *)aButWeWouldBeHa,
    (char *)aFavourableOffe,
};
CompetitionCup * currentCompetitionPtr;
CompetitionCup * secondaryCompetitionPtr;
CompetitionCup * tertiaryCompetitionPtr;
CompetitionCup * quaternaryCompetitionPtr;
void * diyCompetitionPtr;
void * diyCompetitionTablePtr;
void * nearestTeammatePtr;
/* saveFileAmigaType: aliased to careerFileBuffer + 98472 in swos_globals.c. */
uint32_t * currentGoalPtr;
uint32_t * currentHilPtr;
uint8_t * renderStackPtr;
uint8_t * plane01RenderStackPtr;
uint8_t * plane23RenderStackPtr;
uint8_t * menuImageStreamBaseEnd;
uint8_t * imageStreamCursor;
uint8_t * stringDataEnd;
char    * errorMessagePtr;
/* selTeamsPtr: alias into selTeamsBackingBuf + 0 (see swos_globals.c).
 * Previously defined here as `TeamFile *` — the 4-byte definition
 * didn't match the ~45 KB buffer the game actually writes through it. */
ScorerInfo team1Scorers[8];
ScorerInfo team2Scorers[8];
/* careerDefaultInternationalSquadSlotsTable: 11 shirt-numbers assigned
 * to the starting XI of a freshly-initialised international team. IDA's
 * sparse-ASM exporter mis-classified the table as executable code (the
 * `prototype: void …(void)` comment in SWOS2.s line 104747 is an export
 * artefact) and reassembled the 12 bytes back as 6 × move-word-sized
 * instructions: `btst d0,d2 / btst d2,d6 / movep.w $407(a1),d1 /
 * movep.w $800(a2),d5` — opcode bytes 01 42 05 06 03 49 04 07 0B 4A 08
 * 00. The game reads the first 11 of those bytes as shirt numbers in
 * `_Career_InitInternationalSquadAndLookupTablesForNewSeason`
 * (career_teams.c:2220). Using the decoded bytes preserves what the
 * Amiga binary actually shipped with. */
uint8_t careerDefaultInternationalSquadSlotsTable[11] = {
    0x01, 0x42, 0x05, 0x06, 0x03, 0x49, 0x04, 0x07, 0x0B, 0x4A, 0x08
};
void * spriteBlitQueue;

/* editTeams* sprite-copy blocks and their ColTable counterparts live
 * in swos_globals.c as six 36-byte adjacent-globals backing buffers —
 * the MENU_CMD_SPRITE_COPY engine reads the src/dst sprite indices at
 * +0/+2 and the 16-word colour table at +4..+35 from a single pointer,
 * same shape as stadiumMenuLittlePlayer + littlePlayerSpriteGeneric +
 * littlePlayersColorTable. See `project_adjacent_globals_bss.md`. */
