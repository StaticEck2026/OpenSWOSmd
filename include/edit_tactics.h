/* edit_tactics.h — Edit tactics menu functions. */
#pragma once
#include "game_types.h"
#include "menu_system.h"

/* --- Choose Tactics --- */
void _ChooseTacticsInit(void);
void _ShowStandardChooseTacticsMenu(uint8_t *teamPtr);

/* --- Edit Tactics Init/Restore --- */
void _EditTacticsInit(void);
void _EditTacticsOnRestore(void);

/* --- Menu item helpers --- */
void _EnableOrDisablePlayerFacesItems(void);
void _CopyCurrentTacticName(void);
void _EditTactics_UpdateExtraPlayerMenuVisibility(void);
void _SetBigCheckSprite(void);

/* --- Tactics buffer management --- */
void _CopyCurrentTactics(void);
void _ApplyCurrentEditedTactics(void);
void _SaveCurrentTactics(void);
void _RestoreTactics(void);

/* --- Drawing / position management (Chunk 83) --- */
void _DrawPlayerNamesAndFaces(void);
void _EditTacticsMenuSavePlayerPositions(void);
void _EditTactics_CommitPlayerPositionsAndRecalcSkill(void);

/* --- Import tactics (Chunk 83) --- */
void _ImportTacticsInit(void);
void _ImportTacticsSelectTactics(MenuEntry *cur);
void _ExitImportTactics(void);

/* --- Tactics flags (Chunk 83) --- */
void _MarkSelectedTeamsTacticsChanged(void);
void _FillSkinColorConversionTable(uint16_t skinColor, uint16_t *convertTable);

/* --- Menu actions (Chunk 84) --- */
void _SelectCopyTactics(void);
void _UndoTactics(void);
void _AbortEditTactics(void);
void _FlipOnOff(void);
void _ImportTactics(void);
void _SaveAndExitEditTactics(void);
void _EditTacticsOnSelect28(void);
void _EditTacticsAfterDrawPlayerFace(MenuEntry *cur);
void _EditTactics28AfterDraw(void);
void _EditTactics2AfterDraw(void);

/* --- Pitch coordinate helpers (Chunk 85) --- */
void _GetPlayerQuadrant(uint16_t player, uint16_t ballLoc,
                        int16_t *outXQuad, int16_t *outYQuad);
void _SetPlayerQuadrant(uint16_t player, int16_t xQuad, int16_t yQuad,
                        uint16_t ballLoc);
void _GetBallQuadrant(uint16_t ballLoc, int16_t *outCol, int16_t *outRow);
void _GetBallXYPositionFromQuadrant(int16_t row, int16_t col,
                                     int16_t *outX, int16_t *outY);
void _GetPlayerXYPositionFromQuadrant(uint16_t player, uint16_t ballLoc,
                                       int16_t *outX, int16_t *outY);
void _EditTactics_GetDirectionDelta(int16_t *outDx, int16_t *outDy);
void _GetEditTacticsItemXYPosition(int16_t idx, int16_t *outX, int16_t *outY);
uint16_t _EditTactics_TryMoveQuadrantCursor(int16_t row, int16_t col,
                                             int16_t *outRow, int16_t *outCol,
                                             int16_t *outBallLoc);

/* --- Pitch state modification (Chunk 85) --- */
void _EditTactics_SetAllPlayersQuadrantForBallLoc(uint16_t ballLoc,
                                                   uint16_t areaIdx);
void _EditTactics_FlipMaybe_MirrorPlayersInArea(uint16_t areaIdx);
void _EditTactics_FlipMaybe_MirrorBallArea(uint16_t ballLoc);
void _EditTactics_CommitDraggedPlayerPosition(void);

/* --- Pitch drawing/sorting (Chunk 85) --- */
void _SortEditTacticsDrawOrderByY(void);
void _SortPlayerAreasByY(void);
void _EditTacticsDrawCursor(void);
void _DrawPlayerDirections(void);

/* --- Sprite init and rendering (Chunk 86) --- */
void _InitLittlePlayerSprites(void);
void _DrawLittlePlayersAndBall(void);
void _DrawEditTacticsHighlightedPlayerRoute(void);

/* --- State machine and input (Chunk 87) --- */
void _DrawEditTacticsPitchAndPlayers(void);
void _HandlePlayersOrBallInEditTactics(void);
void _EditTacticsReadControls(void);
void _EditTactics_HandlePitchInput(void);
void _EditTacticsStatus(uint16_t status);
void _EditTacicsMovePlayersOrBall(void);
void _CopyTactics(void);

/* --- Player swap, pair linking & position lookup (Chunk 88) --- */
uint8_t *_GetPlayerInfoBreakPosition(uint8_t *teamPtr, uint16_t requestedSlot,
                                      uint16_t *outSlot, uint16_t *outPlayerIndex);
uint16_t _EditTactics_FindFreePairId(void);
void _EditTactics_ClearPairId(uint16_t value);
void _EditTactics_SwapPlayersInSlots(uint16_t slot1, uint16_t slot2);
void _EditTacticsCommon(MenuEntry *cur);
void _EditTacticsPlayerNamesCommon(MenuEntry *cur);

/* --- Chunk 213: EditTactics, ChooseTeamForEditTactics, AfterDraw, Highlighting --- */
uint8_t *_ChooseTeamForEditTactics(uint8_t *menuTitle, uint16_t *teamNumbers,
                                    uint16_t numTeams);
void _EditTactics(void);
void _EditTactics_ApplyPlayerRowHighlighting(int16_t playerOrdinal, MenuEntry *cur);
void _EditTacticsAfterDrawPlayerNames(MenuEntry *cur);

/* --- Load tactics file (Chunk 89) --- */
void _LoadTactics(void);

/* --- Edit Teams (Chunk 117) --- */
void _EditTeamsFinish(void);

/* --- Edit Teams (Chunk 133) --- */
void _EditTeamsInit(void);
void _EditTeamsUndo(void);
void _EditTeamsOK(void);
void _EditTeamsChangeShirtsPrimary(void);
void _EditTeamsChangeShirtsSecondary(void);
void _EditTeamsChangeShortsPrimary(void);
void _EditTeamsChangeShortsSecondary(void);
void _EditTeamsChangeSocksPrimary(void);
void _EditTeamsChangeSocksSecondary(void);
void _EditTeamsSelectPrimaryShirtType(MenuEntry *entry);
void _EditTeamsSelectSecondaryShirtType(MenuEntry *entry);
void _EditTeamsChangePlayerName(MenuEntry *entry);
void _EditTeamsSelectTeamName(MenuEntry *entry);
void _EditTeamsSelectCoachName(MenuEntry *entry);
void _EditTeamsChangePlayerFace(MenuEntry *entry);
void _InitEditTeamsMenu(void);
void _EditTeamsPlayersBeforeDraw(MenuEntry *entry);

/* --- EditTeamsCallback (Chunk 148) --- */
void _EditTeamsCallback(uint8_t *team);

/* --- Chunk 203: EditCustomTeam + ImportTeamsMenu --- */
void _EditCustomTeam(void);
void _ImportTeamsMenu(void);
