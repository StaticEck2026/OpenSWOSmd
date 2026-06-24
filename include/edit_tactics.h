/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* edit_tactics.h — Edit tactics menu functions. */
#pragma once
#include "game_types.h"
#include "menu_system.h"

/* --- Choose Tactics --- */
void ChooseTacticsInit(void);
void ShowStandardChooseTacticsMenu(TeamFileEx *teamPtr);

/* --- Edit Tactics Init/Restore --- */
void EditTacticsInit(void);
void EditTacticsOnRestore(void);

/* --- Menu item helpers --- */
void EnableOrDisablePlayerFacesItems(void);
void CopyCurrentTacticName(void);
void EditTactics_UpdateExtraPlayerMenuVisibility(void);
void SetBigCheckSprite(void);

/* --- Tactics buffer management --- */
void CopyCurrentTactics(void);
void ApplyCurrentEditedTactics(void);
void SaveCurrentTactics(void);
void RestoreTactics(void);

/* --- Drawing / position management (Chunk 83) --- */
void DrawPlayerNamesAndFaces(void);
void EditTacticsMenuSavePlayerPositions(void);
void EditTactics_CommitPlayerPositionsAndRecalcSkill(void);

/* --- Import tactics (Chunk 83) --- */
void ImportTacticsInit(void);
void ImportTacticsSelectTactics(MenuEntry *cur);
void ExitImportTactics(void);

/* --- Tactics flags (Chunk 83) --- */
void MarkSelectedTeamsTacticsChanged(void);
void FillSkinColorConversionTable(uint16_t skinColor, uint16_t *convertTable);

/* --- Menu actions (Chunk 84) --- */
void SelectCopyTactics(void);
void UndoTactics(void);
void AbortEditTactics(void);
void FlipOnOff(void);
void ImportTactics(void);
void SaveAndExitEditTactics(void);
void EditTacticsOnSelect28(void);
void EditTacticsAfterDrawPlayerFace(MenuEntry *cur);
void EditTactics28AfterDraw(void);
void EditTactics2AfterDraw(void);

/* --- Pitch coordinate helpers (Chunk 85) --- */
void GetPlayerQuadrant(uint16_t player, uint16_t ballLoc,
                        int16_t *outXQuad, int16_t *outYQuad);
void SetPlayerQuadrant(uint16_t player, int16_t xQuad, int16_t yQuad,
                        uint16_t ballLoc);
void GetBallQuadrant(uint16_t ballLoc, int16_t *outCol, int16_t *outRow);
void GetBallXYPositionFromQuadrant(int16_t row, int16_t col,
                                     int16_t *outX, int16_t *outY);
void GetPlayerXYPositionFromQuadrant(uint16_t player, uint16_t ballLoc,
                                       int16_t *outX, int16_t *outY);
void EditTactics_GetDirectionDelta(int16_t *outDx, int16_t *outDy);
void GetEditTacticsItemXYPosition(int16_t idx, int16_t *outX, int16_t *outY);
uint16_t EditTactics_TryMoveQuadrantCursor(int16_t row, int16_t col,
                                             int16_t *outRow, int16_t *outCol,
                                             int16_t *outBallLoc);

/* --- Pitch state modification (Chunk 85) --- */
void EditTactics_SetAllPlayersQuadrantForBallLoc(uint16_t ballLoc,
                                                   uint16_t areaIdx);
void EditTactics_FlipMaybe_MirrorPlayersInArea(uint16_t areaIdx);
void EditTactics_FlipMaybe_MirrorBallArea(uint16_t ballLoc);
void EditTactics_CommitDraggedPlayerPosition(void);

/* --- Pitch drawing/sorting (Chunk 85) --- */
void SortEditTacticsDrawOrderByY(void);
void SortPlayerAreasByY(void);
void EditTacticsDrawCursor(void);
void DrawPlayerDirections(void);

/* --- Sprite init and rendering (Chunk 86) --- */
void InitLittlePlayerSprites(void);
void DrawLittlePlayersAndBall(void);
void DrawEditTacticsHighlightedPlayerRoute(void);

/* --- State machine and input (Chunk 87) --- */
void DrawEditTacticsPitchAndPlayers(void);
void HandlePlayersOrBallInEditTactics(void);
void EditTacticsReadControls(void);
void EditTactics_HandlePitchInput(void);
void EditTacticsStatus(uint16_t status);
void EditTacicsMovePlayersOrBall(void);
void CopyTactics(void);

/* --- Player swap, pair linking & position lookup (Chunk 88) --- */
uint8_t *GetPlayerInfoBreakPosition(TeamFileEx *teamPtr, uint16_t requestedSlot,
                                      uint16_t *outSlot, uint16_t *outPlayerIndex);
uint16_t EditTactics_FindFreePairId(void);
void EditTactics_ClearPairId(uint16_t value);
void EditTactics_SwapPlayersInSlots(uint16_t slot1, uint16_t slot2);
void EditTacticsCommon(MenuEntry *cur);
void EditTacticsPlayerNamesCommon(MenuEntry *cur);

/* --- Chunk 213: EditTactics, ChooseTeamForEditTactics, AfterDraw, Highlighting --- */
TeamFileEx *ChooseTeamForEditTactics(uint8_t *_menuTitle, uint16_t *teamNumbers,
                                      uint16_t numTeams);
void EditTactics(void);
void EditTactics_ApplyPlayerRowHighlighting(int16_t playerOrdinal, MenuEntry *cur);
void EditTacticsAfterDrawPlayerNames(MenuEntry *cur);

/* --- Load tactics file (Chunk 89) --- */
void LoadTactics(void);

/* --- Edit Teams (Chunk 117) --- */
void EditTeamsFinish(void);

/* --- Edit Teams (Chunk 133) --- */
void EditTeamsInit(void);
void EditTeamsUndo(void);
void EditTeamsOK(void);
void EditTeamsChangeShirtsPrimary(void);
void EditTeamsChangeShirtsSecondary(void);
void EditTeamsChangeShortsPrimary(void);
void EditTeamsChangeShortsSecondary(void);
void EditTeamsChangeSocksPrimary(void);
void EditTeamsChangeSocksSecondary(void);
void EditTeamsSelectPrimaryShirtType(MenuEntry *entry);
void EditTeamsSelectSecondaryShirtType(MenuEntry *entry);
void EditTeamsChangePlayerName(MenuEntry *entry);
void EditTeamsSelectTeamName(MenuEntry *entry);
void EditTeamsSelectCoachName(MenuEntry *entry);
void EditTeamsChangePlayerFace(MenuEntry *entry);
void InitEditTeamsMenu(void);
void EditTeamsPlayersBeforeDraw(MenuEntry *entry);

/* --- EditTeamsCallback (Chunk 148) --- */
void EditTeamsCallback(TeamFileEx *team);

/* --- Chunk 203: EditCustomTeam + ImportTeamsMenu --- */
void EditCustomTeam(void);
void ImportTeamsMenu(void);
