#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#include "game_types.h"

/* ---- SetPitchTypeAndNumber ----
 * Selects pitch type based on probabilities (seasonal or standard)
 * and pitch number from team data or random. Clears numScenesInHilFile.
 */
void _SetPitchTypeAndNumber(void);

/* ---- CopyTeamsAndHeader ----
 * Copies left/right teams from hil file buffer to in-game team structs,
 * plus game name/round strings.
 */
void _CopyTeamsAndHeader(void);

/* ---- SetHilFileHeader ----
 * Packs in-game team data, game name/round, goals, pitch number into
 * the hil file buffer header.
 */
void _SetHilFileHeader(void);

/* ---- SaveHighlightsPointers ----
 * Saves highlight buffer pointers and enables highlight playback loop.
 */
void _SaveHighlightsPointers(void);

/* ---- RestoreHighlightPointers ----
 * Restores saved highlight buffer pointers, disabling highlight loop.
 */
void _RestoreHighlightPointers(void);

/* ---- FadeAndSaveHighlightScene ----
 * FadeOut, save scene, FadeIn wrapper.
 */
void _FadeAndSaveHighlightScene(void);

/* ---- SaveHighlightScene ----
 * Saves current replay ring buffer to the highlight goals buffer.
 * Maximum 10 scenes; overwrites oldest when full.
 */
void _SaveHighlightScene(void);

/* ---- ThrowInDeadProc ----
 * Sets throwInDeadVar = 1.
 */
void _ThrowInDeadProc(void);

/* ---- SaveCoordinatesForHighlights ----
 * Encodes sprite coordinates (or frame markers) into the ring buffer.
 * spriteIndexOrMarker: >= 0 sprite index, -2 = end marker, < 0 = frame start
 * x, y: coordinates (sprite or camera)
 */
void _SaveCoordinatesForHighlights(int16_t spriteIndexOrMarker, uint16_t x, uint16_t y);

/* ---- DrawReplayFrame ----
 * Draws one frame of instant replay playback.
 * Manages state machine: init camera, decode sprite/frame data, draw score overlay.
 */
void _DrawReplayFrame(void);

/* ---- ReplayUpdateCameraAndSwapBuffers ----
 * Sets camera coordinates, inits camera view, swaps buffers, inits again.
 * targetX, targetY: camera coordinates to set.
 */
void _ReplayUpdateCameraAndSwapBuffers(int16_t targetX, int16_t targetY);

/* ---- HandleInstantReplayStateSwitch ----
 * State machine: transitions replayState between 0 (search for frame) -> 1 -> 2 <-> 3.
 */
void _HandleInstantReplayStateSwitch(void);

/* ---- RestoreCameraPosition ----
 * Restores camera position saved before replay, resets replay state.
 */
void _RestoreCameraPosition(void);

#endif
