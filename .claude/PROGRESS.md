# SWOS_C Porting Progress Log

> Moved from CLAUDE.md to prevent "Prompt too long" errors.
> This file is NOT auto-loaded into context. Read it with the Read tool when resuming a session.

---

## Progress Log (keep updated)

Update this section every time you port a chunk. Keep it chronological.

### Baseline
- Date: 2026-02-17
- Entrypoint: `main.c` → calls `_start` in `SWOS.c`
- Assembly: most subroutines still in `SWOS2.s`

### Chunk 0 — Boot / Startup
- `_start` (from `start`) ported. Project compiles/links and starts from `main`.

### Chunk 1 — Simple Utilities + Fade System
- Date: 2026-02-17
- 7 functions → `src/game_utils.c`, `src/fade.c`
- `_AssetLoaderResetState`, `_WaitForVSync`, `_InitSkinAndDefaultPointers`, `_BuildPaletteFromColors`, `_FadeOutToBlack`, `_FadeIn`, `_FadeOutAndIn`

### Chunk 1.5 — Display & Copper System
- Date: 2026-02-17
- 6 functions → `src/display.c`
- `_InitDisplayAndCopper`, `_UpdatePlanePtrsAndBlitParams`, `_SwapBuffersAndCameraViewState`, `_SetCameraCoordinates`, `_DoubleBufferSwapSequence`, `_BlitterCopyPanelToScreen2`

### Chunk 2 — Asset Loader, Bootstrap, Frontend (mainAsm)
- Date: 2026-02-17
- 10 functions → `src/asset_loader.c`, `src/loader_bootstrap.c`, `src/frontend.c`
- `_AssetLoaderRegisterRegion`, `_AssetLoaderInitAltPathAndBanks`, `_AssetPrecacheCore`, `_AssetPrecacheOneCore`, `_AssetPrecacheMenuPack`, `_AssetPrecacheOneMenu`, `_InstallAndInitLoaderBootstrap`, `_InstallAndRunLoaderBootstrap_Op3`, `_FrontEndMainLoop`, `_mainAsm`
- Bug fix: `loaderBankBitmap` was `uint16_t` → fixed to `uint32_t`

### Chunk 3 — Loader Intro, Timer, Frontend Helpers
- Date: 2026-02-17
- 6 functions + 4 static helpers → `src/game_utils.c`, `src/frontend.c`, `src/loader_intro.c`
- `_ReadTimerDelta`, `_LoadOverlayToFrontbuffer`, `_FrontEndPreLoopSetup`, `_ShowLoaderIntroSequence`
- Code-as-data fix: `ShowLoaderIntroSequence` label → `team1Scorers`/`team2Scorers` data buffers
- Symbol conflict: `_LoadFile` in Resload.s → renamed C bridge to `_CLoadFile`

### Chunk 4 — Audio Init, Input Polling, Menu Init
- Date: 2026-02-17
- 5 functions → `src/audio.c`, `src/input.c`, `src/menus.c`
- `_InitMenuMusic`, `_TryPlayMusic`, `_Player1StatusProc`, `_PrepareMenuBackground`, `_InitMainMenuStuff`

### Chunk 5 — Menu/Game Init Helpers + Utilities
- Date: 2026-02-17
- 7 functions + 1 static → `src/game_utils.c`, `src/menus.c`
- `_Randomize2`, `_ZeroOutStars`, `_SetDefaultOptions`, `_SetDefaultNameAndSurname`, `_InitCareerVariables`, `_InitUserTactics`, `_InitializeTacticsPositions`

### Chunk 6 — Audio Beep, Loader Intro Helpers, Asset Service Wrappers
- Date: 2026-02-17
- 7 functions → `src/audio.c`, `src/loader_intro.c` (static), `src/asset_loader.c`
- `_AudioBeepAllShort`, `loaderIntroEnsureMenuMusicPlaying`, `loaderIntroStopMusicAndResetAudio`, `loaderIntroLoadHeroAudioBank`, `_AssetSvcLoaderOp`, `_AssetSvcLoaderOp2`, `_AssetSvcLoaderOp3`

### Chunk 7 — Asset Loading with Retry + RNC Decompression
- Date: 2026-02-17
- 3 functions → `src/asset_loader.c`
- `_PlaySong`, `_AssetEnsureReadyWithRetry`, `_LoadRNCToBufferNoBlit`

### Chunk 8 — Player2 Input + Audio Utilities
- Date: 2026-02-17
- 7 functions → `src/input.c`, `src/audio.c`
- `_Player2StatusProc`, `_ClearAudioActiveFlag`, `_StopAudioAndClearFlags`, `_LoadSfxResources`, `_EnsureSfxLoadedAndStart`, `_StopMusic`, `_TryRestartingMusic`

### Chunk 9 — Asset Cache System
- Date: 2026-02-18
- 5 functions + 1 static → `src/asset_loader.c`
- `_AssetCacheFindByName`, `_AssetCacheCopyByName`, `_AssetCacheLoadFromAssetSys`, `_AssetCacheStoreFromMemory`, `assetCacheAllocInternal`

### Chunk 10 — PanicHaltLoop + ProcessImageCommandStream + BlitDispatch
- Date: 2026-02-18
- 3 functions + 2 static → `src/panic.c`, `src/image_cmd.c`
- `_PanicHaltLoop`, `_ProcessImageCommandStream`, `blitDispatch`

### Chunk 11 — Menu System Core
- Date: 2026-02-17
- 11 functions → `src/menu_system.c`
- `_CalcMenuEntryAddress`, `_SetExitMenuFlag`, `_DrawMenuPanelBackground`, `_DrawSelectedMenuItemFrame`, `_DrawMenu`, `_UpdateBlinkingMenuText`, `_MenuCheckControls`, `_MenuProc`, `_ActivateMenu`, `_ShowMenu`, `_RestorePreviousMenu`
- Structs: `Menu` (22 bytes), `MenuEntry` (56 bytes)

### Chunk 12 — UnpackMenu, DrawMenuItem, and Menu Rendering Helpers
- Date: 2026-02-18
- 17 functions → `src/menu_system.c`
- `_UnpackMenu`, `_DrawMenuItem`, `_CallDrawMenuItem`, `_DrawMenuItemInnerFrame`, `_DrawMenuItemOuterFrame`, `_DrawMenuItemBackground`, `_DrawMenuItemBackgroundMasked`, `_DrawMenuBackWithCheck`, `_DrawBackAllItems`, `_GetKey`, `_BlitterCopyOverlayToScreen`, `_Int2Ascii`, `_SetCurrentEntry`, `initMenuEntry`
- Bug fix: All asm-to-C stubs changed `move.w` → `move.l` for uint16_t params (GCC expects 32-bit stack slots)

### Chunk 13 — String Utilities, DrawMenuItems Family, File I/O
- Date: 2026-02-18
- 14 functions → `src/string_utils.c`, `src/file_io.c`, `src/menu_system.c`
- `_GetStringLength`, `_StringEquals`, `_StringCopy`, `_StringCopyUntilChar`, `_LoadFileEx`, `_WriteFile`, `_BlitMaskedRectAltToFront`, `_DrawMenuItemsFromList`, `_DrawMultipleItems`, `_DrawMenuItemsWaitForVSync`, `_CallDrawMultipleItems`, `_CallDrawMultipleItemsRect`
- Symbol: `_LoadFileEx` (not `_LoadFile`) to avoid Resload.s conflict

### Chunk 14 — Text Rendering + Sprite Operations
- Date: 2026-02-18
- 8 functions + 1 static → `src/text_render.c`, `src/sprites.c`
- `_GetTextWidth`, `_DrawMenuText`, `_DrawMenuTextRightAligned`, `_DrawMenuTextCentered`, `blitStringToFrontBuffer`, `_CopySprite`, `_CopyWholeSprite`, `_ConvertSpriteColors`

### Chunk 15 — File I/O Helpers + Competition File System
- Date: 2026-02-18
- 13 functions → `src/menus.c`, `src/audio.c`, `src/file_io.c`
- `_SaveOptions`, `_RestoreOptions`, `_ShowErrorMenu`, `_CycleMenuMusic`, `_ShowFsErrorByCode`, `_SaveCompetitionFile`, `_LoadCompetitionFile`, `_LoadCareerFile`, `_SaveCareerFile`, `_LoadPresetFile`, `_SavePresetCompetitionFile`, `_LoadDIYFile`, `_SaveDIYFile`

### Chunk 16 — File System Helpers, Team Loading, Continue/Abort Menu
- Date: 2026-02-18
- 15 functions + 2 static → `src/file_io.c`, `src/teams.c`, `src/menus.c`
- `_FindFilesExtensionPresent`, `_ReadTacticsFile`, `_WriteTacticsFile`, `_LoadHilFile`, `_DoSaveHighlights`, `_LoadTeamFile`, `_SetTeamGlobalNumbers`, `_DoContinueAbortMenu`, `_ContinueAbortInit`, `_ContinueSelected`, `_AbortSelected`, `_ContinueInit`, `_ContinueMenuContinueSelected`

### Chunk 17 — File Dialog System
- Date: 2026-02-18
- 8 functions + 1 no-op → `src/file_io.c`
- `_DiskMenuRet`, `_ShowInsertDisk2Dialog`, `_Asset_PromptInsertSaveDiskDf0`, `_FormatSaveDiskMenu`, `_DeleteFileMenu`, `_ExitDisk`, `_SelectFileToSaveDialog`, `_GetFilenameAndExtension`

### Chunk 18 — Blitter Operations + Text2Sprite
- Date: 2026-02-18
- ✅ Ported (7 functions + 4 static helpers):
  - `_FillRectangle` (from `FillRectangle`) → `src/blitter.c` — fills rectangle with camera offset + vertical wrap
  - `_RectRowMaskedApplyScreenOffset` (from `RectRowMaskedApplyScreenOffset`) → `src/blitter.c` — thin wrapper adding camera offsets
  - `_BlitSpritePlanar_Word` (from `BlitSpritePlanar_Word`) → `src/blitter.c` — OR-combines 4 planes into mask, word access
  - `_BlitSpritePlanar_Word_StridedDest` (from `BlitSpritePlanar_Word_StridedDest`) → `src/blitter.c` — strided dest variant
  - `_BlitSpritePlanar_Long_StridedDest` (from `BlitSpritePlanar_Long_StridedDest`) → `src/blitter.c` — longword access variant
  - `_BlitGlyphToSpriteBitmap` (from `BlitGlyphToSpriteBitmap`) → `src/blitter.c` — 4-pass blitter glyph renderer
  - `_Text2Sprite` (from `Text2Sprite`) → `src/blitter.c` — text-to-sprite renderer
  - `blitterSpanCoreMasked` (static) — single-plane horizontal span fill
  - `blitterSpan4Planes` (static) — 4-plane span fill
  - `blitterRectRowMasked` (static) — masked rectangular blit
  - `waitBlit` (static) — blitter busy-wait helper
- New files: `include/blitter.h`, `src/blitter.c`
- Assembly stubs in SWOS2.s:
  - `FillRectangle:` register-to-stack stub (d0-d4 → 5 stack params, jsr _FillRectangle)
  - `RectRowMaskedApplyScreenOffset: rts` (dead code, 0 asm callers — C callers use _RectRowMaskedApplyScreenOffset directly)
  - `BlitSpritePlanar_Word:` register-to-stack stub (d0 → stack, jsr _BlitSpritePlanar_Word)
  - `BlitSpritePlanar_Word_StridedDest:` register-to-stack stub (d0 → stack)
  - `BlitSpritePlanar_Long_StridedDest:` register-to-stack stub (d0 → stack)
  - `BlitGlyphToSpriteBitmap:` register-to-stack stub (d0-d4 → 5 stack params, preserves embedded data labels)
  - `Text2Sprite:` register-to-stack stub (d1,d2,d3,d4,a0,a1 → 7 stack params + outTotalWidth, returns d0 + d1=totalWidth, Z flag)
- C-to-asm bridges removed from swos2_unit.s (2 bridges):
  - `_CFillRectangle`, `_CRectRowMaskedApplyScreenOffset`
- C-to-asm bridge KEPT (DrawSprite16Pixels NOT ported):
  - `_CDrawSprite16Pixels` (3 params: sprIdx, x, y → d0, d1, d2)
- New xref entries in swos2_unit.s: `_FillRectangle`, `_BlitSpritePlanar_Word`, `_BlitSpritePlanar_Word_StridedDest`, `_BlitSpritePlanar_Long_StridedDest`, `_BlitGlyphToSpriteBitmap`, `_Text2Sprite`
- NOT ported (kept as asm):
  - `DrawSprite16Pixels` — 4-line shim into `DrawSprite_entry2` (large unported sprite-to-screen blitter)
  - `DrawSpriteCentered` — also falls through to `DrawSprite_entry2`
  - `BlitterSpan4Planes` / `BlitterSpanCoreMasked` — asm versions neutered with `rts` (C has static replacements)
  - `blitter_rectrow_masked` — dead code (only caller was RectRowMaskedApplyScreenOffset, now `rts`)
- Notes:
  - BlitGlyphToSpriteBitmap has embedded PC-relative data labels (`bltconShiftBase_glyph`, `glyphBlitPlaneMask`, `glyphHeightLinesCache`, `blitSrcA_MaskPtr_glyph`, `blitSrcB_GlyphPtr_glyph`, `dstRowBytesPerPlane`, `dstRowAdvanceBytes`) used by Loader_ReadFileChunks/Loader_WriteFileChunks via PC-relative access — these data labels are preserved after the stub
  - `panic_bad_glyph_header` and `panic_missing_bitplanes` labels kept as dead `rts` stubs
  - Text2Sprite asm stub uses d3 as implicit planeMask parameter (not in IDA prototype but passed through to BlitGlyphToSpriteBitmap)
  - C callers in menu_system.c already used `_FillRectangle` and `_RectRowMaskedApplyScreenOffset` directly (no bridge name updates needed)
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning

### Chunk 19 — SFX Triggers + Crowd Chant System
- Date: 2026-02-18
- 30 functions → `src/sfx_crowd.c`, `include/sfx_crowd.h`
- **Crowd management (7):**
  - `_CrowdResetMatchState`, `_TurnCrowdChantsOn`, `_TurnCrowdChantsOff`, `_PlayCrowdChants`, `_MuteAllAudioChannelsWithDelay`, `_AudioFadeCrowdDownThenPulseMusic`, `_AudioMuteAllWithFrameRamp`
- **SFX triggers (23):**
  - `_PlayEnqueuedSamples`, `_PlayPenaltySample`, `_PlaySample7IfNotTraining`, `_PlaySfxSample16`, `_PlayGoodPassSample`, `_PlayKickSample`, `_GateKickSampleAndCrowd`, `_PlayRefereeWhistleSample`, `_PlayFoulWhistleSample`, `_PlayEndGameWhistleSample`, `_PlaySample1IfNotTraining`, `_PlaySample1`, `_PlaySample2IfNotTraining`, `_PlaySample4IfNotTraining`, `_PlaySample3IfNotTraining`, `_PlayMissGoalSample`, `_PlayPenalty`, `_PlayGameStateWhistleSample`, `_PlayGoalReplayBottomSample`, `_PlayGoalKickOffSample`, `_PlayCornerKickSample`, `_HandleKickSampleAndCrowd`, `_PlaySample`
- New C-to-asm bridges in `swos2_unit.s`: `_CRand` (calls Rand, masks to byte), `_CCrowdTickJump` (d0=channel, d1=waveId)
- Data labels kept in SWOS2.s, exported via xdef: `pendingSampleId3_4`, `crowdChantsOn`, `lastKickSampleTick`, `audioDbgFlagA`, `crowdSamples[4]`, `currentCrowdSample`, `fadeInChantTimer`, `nextChantTimer`, `fadeOutChantTimer`, `crowdChantVolume`
- BSS globals exported: `endOfHalfWriteOnly`, `goalCounter`, `sfxQueueWriteIdx`, `crowdEventFlags`, `sfxQueueCount`, `trainingGame`, `gameState`, `patternsGoalCounter`, `showFansCounter`
- Assembly stubs:
  - Most functions: simple `jmp _FuncName` (no params)
  - `GateKickSampleAndCrowd`: register-to-stack (d0, d1 → stack)
  - `HandleKickSampleAndCrowd`: register-to-stack + `tst.w d0` after jsr (sets Z flag from C return: 0=played, non-zero=blocked)
  - `PlaySample`: register-to-stack (d0 → stack)
- Notes:
  - PlayCrowdChants is a state machine: idle→fadein→playing→fadeout→cooldown
  - audioDbgFlagA also written by unported asm (lines 5326/5330) via absolute addressing — works with xdef
  - _CRand includes `andi.l #$FF,d0` to mask Rand's byte return value
  - nullsub_6 left as-is (dead code, just rts)
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning

### Chunk 20 — Fade Internals, Display/Blit Helpers, Pitch Loading
- Date: 2026-02-18
- 11 functions → `src/fade.c`, `src/display.c`, `src/pitch.c` (new), `include/pitch.h` (new)
- **Fade internals (3):**
  - `_ApplyFadeStep` (from `ApplyFadeStep` + `ApplyFadeLoaderPath`) → `src/fade.c` — applies fade level to copper palette (normal + loader path with blue channel clamping)
  - `_FadeOutTick` (from `FadeOutTick`) → `src/fade.c` — ISR-called fade-out step
  - `_FadeInTick` (from `FadeInTick`) → `src/fade.c` — ISR-called fade-in step
- **Display helpers (4):**
  - `_ProcessSpriteBlitQueue` (from `ProcessSpriteBlitQueue`) → `src/display.c` — drains blitter A→D copy queue
  - `_BlitterCopyPanelToScreen` (from `BlitterCopyPanelToScreen`) → `src/display.c` — copies overlay panel to frontbuffer
  - `_HWSprite_LoadFromSpriteGraphics` (from `HWSprite_LoadFromSpriteGraphics`) → `src/display.c` — clears HW sprite via blitter D-only fill
  - `_FlashColor0Delay` (from `FlashColor0Delay`) → `src/display.c` — debug color flash with delay loop
- **Pitch loading (3+1 static):**
  - `_LoadPitchMap` (from `LoadPitchMap`) → `src/pitch.c` — loads pitch map file by index
  - `_FixupAndColorPitchFile` (from `FixupAndColorPitchFile`) → `src/pitch.c` — relocates pitch pointers + recolors fan sprites for both teams
  - `_AdjustFanColor` (from `AdjustFanColor`) → `src/pitch.c` — converts planar 4-plane sprite colors via lookup table
  - `pitchFansConvertTable` (static) — 16-entry color remap table, indices 10/11 set to team kit colors
- New files: `include/pitch.h`, `src/pitch.c`
- Assembly stubs in SWOS2.s:
  - `ApplyFadeStep:` register-to-stack stub (d2 → stack, jsr _ApplyFadeStep)
  - `FadeOutTick: jmp _FadeOutTick`
  - `FadeInTick: jmp _FadeInTick`
  - `ProcessSpriteBlitQueue: jmp _ProcessSpriteBlitQueue`
  - `BlitterCopyPanelToScreen: jmp _BlitterCopyPanelToScreen`
  - `HWSprite_LoadFromSpriteGraphics:` register-to-stack stub (d0 → stack)
  - `FlashColor0Delay:` register-to-stack stub (d0, d1 → stack)
  - `LoadPitchMap:` register-to-stack stub (d0 → stack)
  - `FixupAndColorPitchFile: jmp _FixupAndColorPitchFile`
  - `AdjustFanColor:` register-to-stack stub (a1, a0 → stack as dest, convTable)
- New xdef entries in swos2_unit.s: `paletteTable`, `swcPitchMapFiles`, `fansSidesSwapped`, `cameraCoordinatesValid`, `topTeamInGame`, `bottomTeamInGame`
- New xref entries in swos2_unit.s: `_ApplyFadeStep`, `_FadeOutTick`, `_FadeInTick`, `_ProcessSpriteBlitQueue`, `_BlitterCopyPanelToScreen`, `_HWSprite_LoadFromSpriteGraphics`, `_FlashColor0Delay`, `_LoadPitchMap`, `_FixupAndColorPitchFile`, `_AdjustFanColor`
- Data: kFadeShiftMaskTable and fadeShiftMaskTable moved to static const arrays in fade.c; fadeStep replaced with #define FADE_STEP; pitchFansConvertTable moved to static array in pitch.c
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning

### Chunk 21 — Random Number System
- Date: 2026-02-18
- 5 functions → `src/random.c`, `include/random.h` (new files)
- **Random functions (5):**
  - `_Rand` (from `Rand`) → `src/random.c` — primary PRNG, XOR-table based, returns 0-255
  - `_Rand2` (from `Rand2`) → `src/random.c` — secondary PRNG, identical algorithm, independent state
  - `_GetRandomWord` (from `GetRandomWord`) → `src/random.c` — combines two Rand2 calls into 16-bit result
  - `_SaveRand2State` (from `SaveRand2State`) → `src/random.c` — pushes Rand2 state (3 bytes) onto save buffer
  - `_RestoreRand2State` (from `RestoreRand2State`) → `src/random.c` — pops Rand2 state from save buffer
- **Also stubbed:** `Randomize2` (already ported in Chunk 5 as `_Randomize2` in game_utils.c, but asm body was never replaced — now stubbed with register-to-stack bridge)
- New files: `include/random.h`, `src/random.c`
- Assembly stubs in SWOS2.s:
  - `Rand:` preserves d1/a0 (matching original), jsr _Rand
  - `Rand2:` preserves d1/a0 (matching original), jsr _Rand2
  - `GetRandomWord:` preserves d1, jsr _GetRandomWord
  - `SaveRand2State:` preserves a0, jsr _SaveRand2State
  - `RestoreRand2State:` preserves a0, jsr _RestoreRand2State
  - `Randomize2:` preserves d0-d1/a0, pushes d0 as param, jsr _Randomize2
- Data labels kept in SWOS2.s: `seed`, `randXorIndex`, `randXorKey` (in code section, must remain adjacent for word-write from 5 asm callers), `savedRandStatePtr`, `savedRandState` (15-byte buffer), `kRandomTable` (257 bytes, already exported)
- New xdef entries in swos2_unit.s: `seed`, `randXorIndex`, `randXorKey`, `savedRandStatePtr`, `savedRandState`
- New xref entries in swos2_unit.s: `_Rand`, `_Rand2`, `_GetRandomWord`, `_SaveRand2State`, `_RestoreRand2State`
- **Cleanup:** Removed `_CRand` bridge from swos2_unit.s (no longer needed). Updated `sfx_crowd.c` to call `_Rand()` directly instead of `_CRand()`.
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning

### Chunk 22 — Simple Game State Functions
- Date: 2026-02-18
- 7 functions + 2 static → `src/game_state.c`, `include/game_state.h` (new files)
- **Statistics (3):**
  - `_ResetGoalAttempt` (from `ResetGoalAttempt`) → clears isGoalAttempt flag
  - `_AllowStatistics` (from `AllowStatistics`) → enables stats overlay, sets timer, negates resultTimer
  - `_StatisticsOff` (from `StatisticsOff`) → clears stats timer and overlay flags
- **Time (1):**
  - `_ResetTime` (from `ResetTime`) → clears game clock, resets time sprite to SPR_8_MINS
- **Player half-tracking (2+2s):**
  - `_BumpPlayersLastPlayedHalfAtHalfStart` (from `BumpPlayersLastPlayedHalfAtHalfStart`) → loops 11 players per team, marks half-start
  - `_BumpPlayersLastPlayedHalfAtHalfEnd` (from `BumpPlayersLastPlayedHalfAtHalfEnd`) → loops 11 players per team, marks half-end
  - `bumpPlayerHalfStart` (static) → per-player: if not red-carded and not already at 2, set HALFPLAYED to 1
  - `bumpPlayerHalfEnd` (static) → per-player: if not red-carded and HALFPLAYED is 1, set to 2
- **Team setup (1):**
  - `_CopyPlayerNameLengths` (from `CopyPlayerNameLengths`) → copies 16 name-length entries, initializes ordinal table
- New files: `include/game_state.h`, `src/game_state.c`
- Assembly stubs in SWOS2.s:
  - `ResetGoalAttempt: jmp _ResetGoalAttempt`
  - `AllowStatistics: jmp _AllowStatistics`
  - `StatisticsOff: jmp _StatisticsOff`
  - `ResetTime: jmp _ResetTime`
  - `BumpPlayersLastPlayedHalfAtHalfStart: jmp _BumpPlayersLastPlayedHalfAtHalfStart`
  - `BumpPlayersLastPlayedHalfAtHalfEnd: jmp _BumpPlayersLastPlayedHalfAtHalfEnd`
  - `BumpPlayerLastPlayedHalfAtHalfStart: rts` (dead code, inlined in C)
  - `BumpPlayerLastPlayedHalfAtHalfEnd: rts` (dead code, inlined in C)
  - `CopyPlayerNameLengths: jmp _CopyPlayerNameLengths`
- Data labels kept in SWOS2.s: `copyTimeSprite`, `secondsSwitchAccumulator`, `gameSeconds`, `gameTime` (all in code section, used by UpdateTime which remains in asm)
- New xdef entries in swos2_unit.s: `isGoalAttempt`, `showStats`, `overlayActiveFlag`, `statsTimer`, `resultTimer`, `showingUserStats`, `gameSeconds`, `gameTime`, `secondsSwitchAccumulator`, `copyTimeSprite`, `currentTimeSprite`, `plOrdinalsTable`, `plNamesLengthTable`, `plNamesLengthTableCopy`
- New xref entries in swos2_unit.s: `_ResetGoalAttempt`, `_AllowStatistics`, `_StatisticsOff`, `_ResetTime`, `_BumpPlayersLastPlayedHalfAtHalfStart`, `_BumpPlayersLastPlayedHalfAtHalfEnd`, `_CopyPlayerNameLengths`
- Notes: PLAYERINFO_STRIDE = 61 with HALFPLAYED at offset 78 (extends past nominal player size — boundary-spill pattern, intentional in original code)
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning

### Chunk 23 — Replay Menu + ClearSaveFlagForGoalSprites
- Date: 2026-02-18
- 5 functions → `src/replay_menu.c`, `include/replay_menu.h` (new), `src/game_state.c`
- **Replay menu (4):**
  - `_DrawMenuBack150x20` (from `DrawMenuBack150x20`) → `src/replay_menu.c` — draws 150x20 menu background + outer frame
  - `_DrawReplayContinueShiningFrame` (from `DrawReplayContinueShiningFrame`) → `src/replay_menu.c` — animated shining frame around selected option
  - `_DrawReplayOrExitMenu` (from `DrawReplayOrExitMenu`) → `src/replay_menu.c` — draws REPLAY/EXIT boxes twice (double-buffered)
  - `_ReplayExitMenuAfterFriendly` (from `ReplayExitMenuAfterFriendly`) → `src/replay_menu.c` — post-friendly-match REPLAY/EXIT menu with event loop
- **Goal sprites (1):**
  - `_ClearSaveFlagForGoalSprites` (from `ClearSaveFlagForGoalSprites`) → `src/game_state.c` — sets saveSprite=1 for both goal sprites
- New files: `include/replay_menu.h`, `src/replay_menu.c`
- Assembly stubs in SWOS2.s:
  - `ReplayExitMenuAfterFriendly: jmp _ReplayExitMenuAfterFriendly`
  - `DrawReplayOrExitMenu: jmp _DrawReplayOrExitMenu`
  - `DrawMenuBack150x20:` register-to-stack stub (d0=color, d1=x, d2=y → 3 stack params)
  - `DrawReplayContinueShiningFrame:` register-to-stack stub (d0=x, d1=y → 2 stack params)
  - `ClearSaveFlagForGoalSprites: jmp _ClearSaveFlagForGoalSprites`
- Dead code after DrawMenuBack150x20 rts (lines 18370-18412) and DrawReplayContinueShiningFrame rts (lines 18444-18455) left as-is
- New xdef entries in swos2_unit.s: `goal1TopSprite`, `goal2BottomSprite`, `replaySelected`, `aReplay`, `aExit`, `aGrafsMenubg2Ra`
- New xref entries in swos2_unit.s: `_ClearSaveFlagForGoalSprites`, `_DrawMenuBack150x20`, `_DrawReplayContinueShiningFrame`, `_DrawReplayOrExitMenu`, `_ReplayExitMenuAfterFriendly`
- Notes:
  - ReplayExitMenuAfterFriendly has two loop entry points (full redraw vs wait-only); C uses `needsRedraw` flag to replicate
  - DrawReplayOrExitMenu draws both frames with VBlank sync (double-buffered); d7 counts down from 1 to 0
  - When no direction pressed and on EXIT, snaps back to REPLAY (original behavior)
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning

### Chunk 24 — Result Display (ResetResultSprites + ShowResult)
- Date: 2026-02-18
- 2 functions → `src/game_state.c`
- **Result display (2):**
  - `_ResetResultSprites` (from `ResetResultSprites`) → clears visibility on all result-related sprites, clears scorer data arrays
  - `_ShowResult` (from `ShowResult`) → timer-driven result display: init phase maps magic timer values (30000/31000/32000) to countdown values, shows/hides sprites based on gameState, positions all score/team/scorer/grid sprites relative to camera, handles aggregate/total score digits for 2-leg games
- New C-to-asm bridge in swos2_unit.s: `_CInitDisplaySprites` (calls InitDisplaySprites)
- Assembly stubs in SWOS2.s:
  - `ResetResultSprites: jmp _ResetResultSprites`
  - `ShowResult: jmp _ShowResult` (dead code from old body left in place after jmp)
- New xdef entries in swos2_unit.s (32): all result/score sprite globals, team goals digits, scorer widths, secondLeg, dontShowScorers, team1NameLength, team1/2TotalGoals
- New xref entries in swos2_unit.s: `_ResetResultSprites`, `_ShowResult`
- Sprite field accessor macros added: `SPR_X()`, `SPR_Y()`, `SPR_Z()`, `SPR_VIS()`, `SPR_IMG()` for cleaner code
- Notes:
  - ShowResult is ~385 lines of asm but mostly repetitive sprite positioning
  - Team2 goals/total digit positioning has asymmetric X-offset logic (digit1.X = digit2.X copy, then digit2.X += offset)
  - Scorer visibility loop breaks on either width==0 OR dontShowScorers (both jump to same exit label)
  - Vertical offset d7 computed from scorer count, clamped to min -14 (0xFFF2)
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning

### Chunk 25 — Audio System Core
- Date: 2026-02-18
- 8 functions + 1 static → `src/audio.c`, `include/audio.h` (updated)
- **Audio setup (8):**
  - `_StopAllAudioChannels` (from `StopAllAudioChannels`) → disables all audio DMA, clears masterVolume/modeFlags
  - `_AudioSetChannelVolume` (from `AudioSetChannelVolume`) → sets volume for one or all channels (bit 7 = all)
  - `_AudioSetMode` (from `AudioSetMode`) → sets audio mode flags (bits 2-3 enable primary/secondary processing)
  - `_InitAudioState` (from `InitAudioState`) → initializes audio system from bank data, relocates descriptors, inits 4 channels
  - `_AudioApplyPresetToChannels` (from `AudioApplyPresetToChannels`) → applies 4-byte preset to all channels
  - `_AudioChannelNoteOnOff` (from `AudioChannelNoteOnOff`) → starts/stops a note on a channel (0=stop)
  - `_AudioDisableDMAForChannel` (from `AudioDisableDMAForChannel`) → disables DMA for a single channel
  - `_AudioChannelQueueNextWaveIfEnabled` (from `AudioChannelQueueNextWaveIfEnabled`) → queues wave if secondary processing enabled
- **Static helper (1):**
  - `clearAudioChannelStruct` — zeros 176 bytes (0xB0) of channel struct
  - Also exported as `_ClearAudioChannelStruct` for potential asm callers
- **AudioState struct** defined in `audio.h` (46 bytes, packed, matches asm layout):
  - songPtr, bankSectionPtrs[7], masterVolume, modeFlags, irqSubtick, toggleFlag, primaryChanEnable, secondaryChanEnable, channelVolume[4], channelChangeFlags, currentChannelIdx
- **AudioChannel** accessed via offset macros (ACHAN_*) and CHAN_U32/U16/U8 accessors
- **Access() inlined** as `accessRelocate()` — adds bankSectionPtrs[5] and masks to 24-bit
- Assembly stubs in SWOS2.s:
  - `InitAudioState:` register-to-stack stub (a0=bankBase, a1=songPtr → 2 stack params), saves all regs
  - `ClearAudioChannelStruct:` register-to-stack stub (a0 → stack)
  - `StopAllAudioChannels:` saves d0-d1/a0-a1, jsr _StopAllAudioChannels
  - `AudioSetChannelVolume:` register-to-stack stub (d0=channel, d1=volume → 2 stack params)
  - `AudioSetMode:` register-to-stack stub (d0=mode → stack)
  - `AudioApplyPresetToChannels:` register-to-stack stub (d0=presetId → stack)
  - `AudioChannelNoteOnOff:` register-to-stack stub (d0=channel, d1=noteId → 2 stack params)
  - `AudioChannelStop:` dead code (`rts`)
  - `AudioChannelQueueNextWaveIfEnabled:` register-to-stack stub (d0=channel, d1=waveId)
- `AudioDisableDMAForChannel` asm body KEPT (used by IRQ handlers still in asm)
- **Bridges removed from swos2_unit.s:**
  - `_StopAudio` → replaced by direct `_StopAllAudioChannels()` calls in C
  - `_StartMenuMusic` → replaced by direct `_InitAudioState()` calls in C
  - `_SetVolumeOrStopChants` → replaced by direct `_AudioSetChannelVolume()` calls in C
  - `_AudioApplyPresetToChannels` bridge → replaced by C function of same name
  - `_CCrowdTickJump` → replaced by direct `_AudioChannelQueueNextWaveIfEnabled()` calls in C
- **C callers updated:**
  - `audio.c`: _StopAudio→_StopAllAudioChannels, _StartMenuMusic→_InitAudioState, _SetVolumeOrStopChants→_AudioSetChannelVolume
  - `loader_intro.c`: same replacements
  - `sfx_crowd.c`: _SetVolumeOrStopChants→_AudioSetChannelVolume, _CCrowdTickJump→_AudioChannelQueueNextWaveIfEnabled
- New xdef entries in swos2_unit.s: `audioState`, `audioChannel0`, `audioChannel1`, `audioChannel2`, `audioChannel3`
- New xref entries in swos2_unit.s: `_StopAllAudioChannels`, `_AudioSetChannelVolume`, `_AudioSetMode`, `_ClearAudioChannelStruct`, `_InitAudioState`, `_AudioApplyPresetToChannels`, `_AudioChannelNoteOnOff`, `_AudioDisableDMAForChannel`, `_AudioChannelQueueNextWaveIfEnabled`
- Notes:
  - AudioState structure is PC-relative data in SWOS2.s code section — stays in asm, exported via xdef
  - IRQ handlers (AudioIRQUpdateChannels, AudioIRQServiceOneChannel, etc.) remain in asm and continue using PC-relative access
  - Thin wrappers (StartMenuMusic→InitAudioState, StopAudio→StopAllAudioChannels, etc.) chain through stubs, adding one extra branch
  - `clearAudioChannelStruct` uses `__attribute__((optimize("no-tree-loop-distribute-patterns")))` to prevent GCC converting loop to memset
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning

### Chunk 26 — Time & Statistics System
- Date: 2026-02-18
- 6 functions + 2 static → `src/game_state.c` (UpdateTime), `src/statistics.c` (new), `include/statistics.h` (new)
- **Time management (1):**
  - `_UpdateTime` (from `UpdateTime`) → `src/game_state.c` — core game time: BCD minute tracking, half-time/full-time/extra-time/penalty triggers, away-goals rule, time sprite building and positioning
- **Statistics display (3+2s):**
  - `_UpdateAndDrawStatistics` (from `UpdateAndDrawStatistics`) → `src/statistics.c` — stats overlay lifecycle: shot tracking, goal attempt detection, possession counting, user input dismissal, timer management
  - `_DrawStatistics` (from `DrawStatistics`) → `src/statistics.c` — renders full stats UI: 11 grid rows, team names, goals/possession/attempts/on-target/corners/fouls/bookings/sendings-off
  - `_DrawGrid` (from `DrawGrid`) → `src/statistics.c` — fills grid background using 16x16 DarkenRectangle tiles
  - `writeStatsNumber` (static) — converts number to ASCII and draws centered
  - `writeStatsPercent` (static) — converts number to ASCII with '%' suffix and draws centered
- New files: `include/statistics.h`, `src/statistics.c`
- Assembly stubs in SWOS2.s:
  - `UpdateTime: jmp _UpdateTime` (dead code from old body left in place after jmp)
  - `UpdateAndDrawStatistics: jmp _UpdateAndDrawStatistics` (dead code left in place — contains anti-tamper label targets)
  - `DrawStatistics: jmp _DrawStatistics` (dead code left in place — contains anti-tamper label targets)
  - `WriteStatsNumber: rts` (dead code, only caller was DrawStatistics now in C)
  - `WriteStatsPercent: rts` (dead code, only caller was DrawStatistics now in C)
  - `DrawGrid: rts` (dead code, only caller was DrawStatistics now in C)
- New C-to-asm bridges in swos2_unit.s:
  - `_CEndFirstHalf` (calls EndFirstHalf)
  - `_CEndFirstExtraTime` (calls EndFirstExtraTime)
  - `_CEndOfGame` (calls EndOfGame)
  - `_CStartFirstExtraTime` (calls StartFirstExtraTime)
  - `_CStartPenalties` (calls StartPenalties)
  - `_CDarkenRectangle` (d0=sprIdx, d1=x, d2=y → calls DarkenRectangle)
- New xdef entries in swos2_unit.s (39): timeVar, timeDelta, gameStatePl, extraTimeState, penaltiesState, endGameCounter, playing2ndGame, team1GoalsFirstLeg, team2GoalsFirstLeg, winningTeamPtr, statsTeam1GoalsCopy, statsTeam2GoalsCopy, statsTeam1Goals, statsTeam2Goals, playingPenalties, lastTeamPlayed, ballInGoalkeeperArea, strikeDestX, ballSprite, topTeamData, bottomTeamData, currentPlayerIndicatorPosition, fireBlocked, aMATCHSTATS, aPossession, aGoalAttempts, aOnTarget, aCornersWon, aFoulsConceded, aBookings, aSendingsOff, aMinus, aGoals, statsNumber, + 6 bridge xdefs
- New xref entries in swos2_unit.s: `_UpdateTime`, `_UpdateAndDrawStatistics`, `_DrawStatistics`, `_DrawGrid`
- Notes:
  - UpdateTime's initial timeVar=31000 block has an unconditional `bra.w time_invisible` that makes the timer countdown dead code — translated faithfully (always sets timeVar=150, makes sprite visible)
  - Anti-tamper check labels (`swap_to_bottom_team_goalkeeper+4`, `select_team_order_for_stats+4`, `draw_fouls_conceded_team2`) in dead code after jmp stubs must be preserved — external irqCookieObfuscated checks branch to them (never taken in normal execution)
  - Away goals rule at minute 90 and 120: `2*thisLegGoals + firstLegGoals` compared for both teams
  - gameTime is big-endian BCD: byte[1]=hundreds, byte[2]=tens, byte[3]=ones; compared as uint32_t for minute checks
  - DrawStatistics uses TEAMGENERALINFO_INGAMETEAMPTR to determine team display order (topTeamData may be swapped)
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning

### Chunk 27 — Advertisement System, Game Display Sprites, Camera Movement
- Date: 2026-02-18
- 10 functions → `src/adverts.c`, `include/adverts.h` (new), `src/camera.c`, `include/camera.h` (new)
- **Advertisement system (5):**
  - `_CopyAdvertisementsData` (from `CopyAdvertisementsData`) → copies sprite graphics data pointers for 3 ad strips from offsets 0x1C4/0x1E4/0x204
  - `_InitAdvertisements` (from `InitAdvertisements`) → clears ad state, calls SetAdvertisementsInvisible
  - `_SetAdvertisementsVisible` (from `SetAdvertisementsVisible`) → sets visible=1 on 8 advert sprites, calls InitDisplaySprites
  - `_SetAdvertisementsInvisible` (from `SetAdvertisementsInvisible`) → clears visible on 8 advert sprites, calls InitDisplaySprites
  - `_ManageAdvertisements` (from `ManageAdvertisements`) → state machine for animated pitch-side ads: random selection, frame-gated animation, slide pause, teardown
- **Game display sprites (2):**
  - `_UpdateCornerFlagSprite` (from `UpdateCornerFlagSprite`) → positions corner flag sprite based on camera, animates via 32-entry lookup table
  - `_SpinBig_S_Sprite` (from `SpinBig_S_Sprite`) → updates spinning S indicator: spin index advances by lastFrameTicks, pauses during subs/stats, positioned relative to camera
- **Camera movement (3):**
  - `_ScrollToCurrent` (from `ScrollToCurrent`) → sets cameraCoordinatesValid=1, calls ScrollXUpdateColumnsAndBlit + ScrollYUpdateRowsAndBlit
  - `_MoveView` (from `MoveView`) → iteratively moves cameraX/cameraXMod320 toward target (wrap at 352), then cameraY/cameraYMod320 (wrap at 272), calling ScrollToCurrent each step
  - `_InitCameraMoveView` (from `InitCameraMoveView`) → initializes camera by scrolling to (targetX,272) then (cameraX,0), then MoveView to final target
- New files: `include/adverts.h`, `src/adverts.c`, `include/camera.h`, `src/camera.c`
- Assembly stubs in SWOS2.s:
  - `CopyAdvertisementsData: jmp _CopyAdvertisementsData`
  - `InitAdvertisements: jmp _InitAdvertisements`
  - `SetAdvertisementsVisible: jmp _SetAdvertisementsVisible`
  - `SetAdvertisementsInvisible: jmp _SetAdvertisementsInvisible`
  - `ManageAdvertisements: jmp _ManageAdvertisements`
  - `UpdateCornerFlagSprite: jmp _UpdateCornerFlagSprite`
  - `SpinBig_S_Sprite: jmp _SpinBig_S_Sprite`
  - `ScrollToCurrent: jmp _ScrollToCurrent`
  - `InitCameraMoveView:` register-to-stack stub (d1=targetX, d2=targetY → 2 stack params)
  - `MoveView:` register-to-stack stub (d1=newScreenX, d2=newScreenY → 2 stack params)
- New C-to-asm bridges in swos2_unit.s:
  - `_CScrollXUpdateColumnsAndBlit` (calls ScrollXUpdateColumnsAndBlit, saves/restores all regs)
  - `_CScrollYUpdateRowsAndBlit` (calls ScrollYUpdateRowsAndBlit, saves/restores all regs)
- New xdef entries in swos2_unit.s (16): advertIndex, advertSlideTimer, currentAnimatedAdLine, advert1Pointers, advert2Pointers, advert3Pointers, blockAdvertisements, advertInGameSprites, cornerFlagSprite, cornerFlagPictureIndices, big_S_Sprite, spinBigS, bigSIndex, goalCameraMode, foulXCoordinate, frameCount, inSubstitutesMenu, subsState
- New xref entries in swos2_unit.s (10): _CopyAdvertisementsData, _InitAdvertisements, _SetAdvertisementsVisible, _SetAdvertisementsInvisible, _ManageAdvertisements, _UpdateCornerFlagSprite, _SpinBig_S_Sprite, _InitCameraMoveView, _MoveView, _ScrollToCurrent
- Data labels kept in SWOS2.s: advertIndex, advertSlideTimer, currentAnimatedAdLine, advert1/2/3Pointers (8 longs each), blockAdvertisements, cornerFlagPictureIndices (32 bytes), spinBigS, bigSIndex, numberBuffer (16 bytes)
- Notes:
  - ManageAdvertisements is a state machine: picks random ad strip (1-3 based on foulXCoordinate), animates 6 lines, pauses 40 frames, animates 7 more lines, then tears down
  - spriteGraphicsPtr type fixed from uint32_t to uint8_t* to match other modules
  - ScrollXUpdateColumnsAndBlit/ScrollYUpdateRowsAndBlit remain in asm (heavy blitter ops, ~170/190 lines each)
  - MoveView calls ScrollToCurrent on every pixel step — faithfully preserving the original's behavior
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning

### Chunk 28 — Face/Color Helpers, ExtractSurname, Player Name Sprites
- Date: 2026-02-18
- 7 functions (5 exported + 1 static + 1 dead stub) → `src/player_sprites.c`, `include/player_sprites.h` (new)
- **String processing (1):**
  - `_ExtractSurname` (from `ExtractSurname`) → `src/player_sprites.c` — extracts surname from player name with 3 modes: full name if fits, "F. surname" initial mode, skip-firstname mode; uses surnameBuf
- **Face/color helpers (4):**
  - `_CountFaces` (from `CountFaces`) → `src/player_sprites.c` — counts face frequency across 16 players into facesFrequency[3]
  - `_ConvertColorTableToSkinColor` (from `ConvertColorTableToSkinColor`) → `src/player_sprites.c` — applies skin/hair color conversion via pair-list lookup
  - `_DetermineGoalieFaces` (from `DetermineGoalieFaces`) → `src/player_sprites.c` — scans team for GK face assignments, fills gkFaceToIndex/gkFaceIndices
  - `_InitializePlayerSpriteFrameIndices` (from `InitializePlayerSpriteFrameIndices`) → `src/player_sprites.c` — sets SPRITE_FRAMEOFFSET for 11 players per team based on skin color (GK: ×0x3A, outfield: ×0x65)
- **Player name sprites (2):**
  - `makePlayerNameSprite` (static, from `MakePlayerNameSprite`) → `src/player_sprites.c` — builds "NN surname" sprite via Int2Ascii + ExtractSurname + Text2Sprite
  - `_MakeSpritesFromPlayerNames` (from `MakeSpritesFromPlayerNames`) → `src/player_sprites.c` — loops 16 players × 2 teams calling makePlayerNameSprite
- New files: `include/player_sprites.h`, `src/player_sprites.c`
- Assembly stubs in SWOS2.s:
  - `ExtractSurname:` register-to-stack stub (a0, d0 → stack; result: movea.l d0,a0)
  - `CountFaces:` register-to-stack stub (a1 → stack)
  - `ConvertColorTableToSkinColor:` register-to-stack stub (d0, a1 → stack)
  - `DetermineGoalieFaces:` register-to-stack stub (a0, a1, a2 → stack)
  - `InitializePlayerSpriteFrameIndices: jmp _InitializePlayerSpriteFrameIndices`
  - `MakePlayerNameSprite: rts` (dead code, only caller also ported)
  - `MakeSpritesFromPlayerNames:` register-to-stack stub (a5, a6 → stack)
- New xdef entries in swos2_unit.s (13): facesFrequency, skinAndHairColorConversionTable, team1GoalieFaceToPosition, team2GoalieFaceToPosition, team1GoalieFaces, team2GoalieFaces, team2GoalieSkinType, team2ReserveGoalieSkinType, team1SpritesTable, team2SpritesTable, numberBuffer, topTeamPtr, bottomTeamPtr
- New xref entries in swos2_unit.s (6): _ExtractSurname, _CountFaces, _ConvertColorTableToSkinColor, _DetermineGoalieFaces, _InitializePlayerSpriteFrameIndices, _MakeSpritesFromPlayerNames
- Data labels preserved in SWOS2.s: numberBuffer (16 bytes), facesFrequency (4 bytes), team1/2PlayersColorTable, colorTableReserves, bigNumbersColorConversionTable, skinAndHairColorConversionTable, team1/2GoalieFaceToPosition, team1/2GoalieFaces, team2GoalieSkinType, team2ReserveGoalieSkinType
- Notes:
  - ExtractSurname has 3 modes controlled by bits 15:14 of maxLenAndFlags: no flags (try full, abbreviate if too long), bit15=force initial "F. surname", bit14=skip firstname
  - MakePlayerNameSprite asm stub is dead (rts) since only caller MakeSpritesFromPlayerNames is also ported
  - ConvertColorTableToSkinColor sign-extends color byte before indexing skinAndHairColorConversionTable (3 pointer entries → pair lists)
  - InitializePlayerSpriteFrameIndices uses gkFaceToPosition lookup for goalkeeper (slot 0=main, 1=reserve), multiplier 0x3A (58); outfield uses face×0x65 (101)
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning

### Chunk 29 — ConvertColorsAndTextSprites + Game HUD
- Date: 2026-02-18
- 4 functions (4 exported + 2 static) → `src/color_sprites.c`, `include/color_sprites.h` (new), `src/game_hud.c`, `include/game_hud.h` (new)
- **Color/sprite initialization (1+2s):**
  - `_ConvertColorsAndTextSprites` (from `ConvertColorsAndTextSprites`) → `src/color_sprites.c` — master sprite color init: sets team filenames from shirt type, runs ProcessImageCommandStream, builds team name sprites via Text2Sprite, converts big number/result/reserve/player/goalkeeper sprites with team and skin color tables (12 sprite conversion loops)
  - `convertSpriteRange` (static) — loops sprite indices calling _ConvertSpriteColors
  - `findMostFrequentFaceAndApplyLUT` (static) — calls _CountFaces, finds most frequent face, applies skin/hair LUT from skinAndHairColorConversionTable
- **Game HUD (3):**
  - `_UpdateControlledPlayerNumbers` (from `UpdateControlledPlayerNumbers`) → `src/game_hud.c` — shows shirt number sprite above controlled player per team, blink when controlled==marked via currentGameTick bit 4
  - `_MarkPlayer` (from `MarkPlayer`) → `src/game_hud.c` — shows marker sprite above marked player, alternates between top/bottom team via currentGameTick bit 4
  - `_SetCurrentPlayerName` (from `SetCurrentPlayerName`) → `src/game_hud.c` — state machine for player name display with priority: scorer > booked > lastPlayerBeforeGoalkeeper > lastPlayerPlayed; positions in 4 screen corners based on currentPlayerIndicatorPosition; nobodysBallTimer/nobodysBallLastFrame delay logic
- New files: `include/color_sprites.h`, `src/color_sprites.c`, `include/game_hud.h`, `src/game_hud.c`
- Assembly stubs in SWOS2.s:
  - `ConvertColorsAndTextSprites:` register-to-stack stub (a5=team1, a6=team2 → 2 stack params, saves d2-d7/a2-a6)
  - `UpdateControlledPlayerNumbers: jmp _UpdateControlledPlayerNumbers`
  - `MarkPlayer: jmp _MarkPlayer`
  - `SetCurrentPlayerName: jmp _SetCurrentPlayerName`
- New xdef entries in swos2_unit.s (20): team1MostFrequentFace, team2MostFrequentFace, bigNumbersColorConversionTable, colorTableReserves, team1PlayersColorTable, team2PlayersColorTable, teamFilenamesTable, nobodysBallTimer, nobodysBallLastFrame, currentScorer, lastTeamScored, lastPlayerBeforeGoalkeeper, team1CurPlayerNumSprite, team2CurPlayerNumSprite, playerMarkSprite, currentPlayerNameSprite, whichCard, bookedPlayer, lastTeamBooked, lastPlayerPlayed, team2NameLength, team1Filename, team2Filename, menuColorAndTextScript
- New xref entries in swos2_unit.s (4): _ConvertColorsAndTextSprites, _UpdateControlledPlayerNumbers, _MarkPlayer, _SetCurrentPlayerName
- Notes:
  - ConvertColorsAndTextSprites has 12 distinct sprite conversion loops for team1/2 × skin0/1/2 + GK main/reserve + reserves
  - Skin 0 and GK main sprites always converted; skin 1/2 and GK reserve only when useIndividualPlayerSkinColor != 0
  - GK face data layout: team1GoalieFaces[0]=main, team1GoalieFaces[1]=reserve (asm label "team2GoalieFaces"); team2GoalieSkinType[0]=main, [1]=reserve
  - SetCurrentPlayerName uses goto-based control flow faithfully matching the asm branch structure
  - Sprite field access via byte-offset macros (SPR_W, SPR_UW, SPR_L) consistent with asm field offsets
  - menuImageStreamBaseEnd stored as uint32_t (pointer-as-integer), cast to uint8_t* at call site
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning

### Chunk 30 — Game Init + Sprite Sort + Goal Highlight
- Date: 2026-02-18
- 5 functions (5 exported) → `src/game_init.c`, `include/game_init.h` (new), `src/game_state.c`, `src/sprites.c`
- **Goal highlight (1):**
  - `_InitCurrentGoalHighlight` (from `InitCurrentGoalHighlight`) → `src/game_state.c` — clears 19000 bytes at goalBasePtr, resets currentHilPtr/replayState/throwInDeadVar
- **Sprite management (2):**
  - `_SortSprites` (from `SortSprites`) → `src/sprites.c` — improved bubble sort on sortedSprites[] by SPRITE_Y, early exit on no-swaps
  - `_InitDisplaySprites` (from `InitDisplaySprites`) → `src/sprites.c` — collects visible sprites from allSpritesArray into sortedSprites, counts into numSpritesToRender, calls SortSprites
- **Game init (2):**
  - `_InitTeamsData` (from `InitTeamsData`) → `src/game_init.c` — clears ~20 per-match state globals, sets gameState=ST_GAME_IN_PROGRESS, initializes topTeamData/bottomTeamData TeamGeneralInfo structs (2-iteration loop with teamPlayingUp swap)
  - `_InitGameSaveTeams` (from `InitGameSaveTeams`) → `src/game_init.c` — copies both team in-game structs (1018 bytes each) to snapshot buffers, loads pitch map, fixup+color, ConvertColorsAndTextSprites, waits for fire release, fades out, paletteIndex=2
- New files: `include/game_init.h`, `src/game_init.c`
- Assembly stubs in SWOS2.s:
  - `InitCurrentGoalHighlight: jmp _InitCurrentGoalHighlight`
  - `SortSprites: jmp _SortSprites`
  - `InitDisplaySprites: jmp _InitDisplaySprites`
  - `InitTeamsData: jmp _InitTeamsData`
  - `InitGameSaveTeams:` register-to-stack stub (a5, a6 → stack), saves d2-d7/a2-a4, falls through to InitGameRestoreTeams (preserving original bra.w *+4 fall-through behavior)
- New xdef entries in swos2_unit.s (34): currentHilPtr, replayState, throwInDeadVar, sortedSprites, numSpritesToRender, allSpritesArray, goalScored, runSlower, playerHadBall, lastKeeperPlayed, penalty, goalOut, gameNotInProgressCounterWriteOnly, stoppageTimerTotal, stoppageTimerActive, stoppageEventTimer, inGameCounter, breakState, breakCameraMode, teamPlayingUp, topTeamPlayerNo, bottomTeamPlayerNo, topTeamCoachNo, bottomTeamCoachNo, pl1Coach, pl2Coach, team1StatsData, team2StatsData, pl1Tactics, pl2Tactics, topTeamSaved, bottomTeamSaved, pitchNumberAndType, lastTeamPlayedBeforeBreak
- New xref entries in swos2_unit.s (5): _InitCurrentGoalHighlight, _SortSprites, _InitDisplaySprites, _InitTeamsData, _InitGameSaveTeams
- Type consistency fixes: goalBasePtr/currentHilPtr declared as `uint8_t *` (matching game_utils.c), topTeamPtr/bottomTeamPtr as `uint8_t *` (matching player_sprites.c)
- Notes:
  - InitGameSaveTeams asm stub intentionally has no `rts` — falls through to InitGameRestoreTeams (which remains in asm)
  - InitTeamsData uses WR32/WR16/WR8 macros for struct field writes (matching byte-offset layout of TeamGeneralInfo)
  - InitCurrentGoalHighlight needs `optimize("no-tree-loop-distribute-patterns")` to prevent GCC converting 19000-byte clear loop to memset
  - InitGameSaveTeams also uses `optimize("no-tree-loop-distribute-patterns")` for the 1018-byte copy loops
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning

### Chunk 31 — Match State Transitions + InitGameRestoreTeams
- Date: 2026-02-18
- 21 functions (15 exported + 6 static) → `src/match_setup.c`, `include/match_setup.h` (new)
- **Ball/player resets (5):**
  - `_SetBallPosition(int16_t ballX, int16_t ballY)` (from `SetBallPosition`) → stops ball, sets position to (x,y), z=0
  - `resetPlayerPassingKicking(uint8_t *team)` (static, from `ResetPlayerPassingKicking`) → clears TGI passing/kicking fields
  - `_ResetBothTeamsPlayerPassingKicking` (from `ResetBothTeamsPlayerPassingKicking`) → saves camera direction, resets both teams
  - `stopPlayers(uint8_t *team)` (static, from `StopPlayers`) → for PL_NORMAL/HAPPY/SAD players not sent away, set dest=current pos
  - `_StopAllPlayers` (from `StopAllPlayers`) → saves camera direction, stops + resets both teams
- **Referee/patterns (2):**
  - `_RemoveReferee` (from `RemoveReferee`) → clears refState, resets referee sprite, calls _CInitRefereeAnimationTable
  - `_InitAnimatedPatterns` (from `InitAnimatedPatterns`) → fills 8-entry array with 0xFFFF, clears counters
- **Player entry (1):**
  - `_InitPlayersBeforeEnteringPitch` (from `InitPlayersBeforeEnteringPitch`) → positions 22 players using kTeamsStartingCoordinates, adds random jitter, resets sprite state, calls _CSetPlayerAnimationTable
- **Match lifecycle (10):**
  - `_StartingMatch` (from `StartingMatch`) → halfNumber=1, ball at (1672,449), InitTeamsData, StopAllPlayers, InitPlayersBeforeEnteringPitch
  - `_SetCameraMovingToShowerState` (from `SetCameraMovingToShowerState`) → halfNumber=2, inverts teamPlayingUp/teamStarting
  - `_EndFirstHalf` (from `EndFirstHalf`) → ST_FIRST_HALF_ENDED, ResetBothTeamsPlayerPassingKicking
  - `_FirstHalfJustEnded` (from `FirstHalfJustEnded`) → ST_GOING_TO_HALFTIME, StopAllPlayers
  - `_EndOfGame` (from `EndOfGame`) → ST_GAME_ENDED, ResetBothTeamsPlayerPassingKicking
  - `_PlayersLeavingPitch` (from `PlayersLeavingPitch`) → ST_PLAYERS_GOING_TO_SHOWER, StopAllPlayers
  - `_GoToHalftime` (from `GoToHalftime`) → sets ball, resultTimer=30000, timeVar=32000, ST_RESULT_ON_HALFTIME
  - `_GameOver` (from `GameOver`) → FadeOutToBlack, ReplayUpdateCameraAndSwapBuffers, sets result display state
  - `_StartFirstExtraTime` (from `StartFirstExtraTime`) → random teamPlayingUp/teamStarting, ST_FIRST_EXTRA_STARTING
  - `_EndFirstExtraTime` (from `EndFirstExtraTime`) → inverts teams, ST_FIRST_EXTRA_ENDED
- **Kickoff/penalties (2):**
  - `_PrepareForInitialKick` (from `PrepareForInitialKick`) → ball at (336,449), sets cameraDirection/playerTurnFlags based on kickoff team
  - `_StartPenalties` (from `StartPenalties`) → saves goals, clears penalty state, random teams, clears cards/sentaway, calls _CNextPenalty
- **Master init (1):**
  - `_InitGameRestoreTeams` (from `InitGameRestoreTeams`) → restores team data from snapshots, applies tactics, sets palette/pitch factors, clears per-match state, calls StartingMatch
- **Static helpers (2):**
  - `setBreakState()` — common pattern for game state changes (sets gameState, breakCameraMode=0xFFFF, clears timers)
  - `clearCameraVelocity()` — zeros cameraXVelocity/cameraYVelocity
- Static data: `kTeamsStartingCoordinates[44]` — 22 starting positions (x,y) for player entry
- New files: `include/match_setup.h`, `src/match_setup.c`
- Assembly stubs in SWOS2.s:
  - `SetBallPosition:` register-to-stack stub (d1,d2 → stack params)
  - `ResetPlayerPassingKicking: rts` (dead code, inlined in C callers)
  - `StopPlayers: rts` (dead code, inlined in C callers)
  - All others: `jmp _FuncName`
- New C-to-asm bridges in swos2_unit.s:
  - `_CSetPlayerAnimationTable` (a0=animTable, a1=spritePtr)
  - `_CInitRefereeAnimationTable` (a1=animTable)
  - `_CApplyTeamTactics` (a4=playerInfos)
  - `_CNextPenalty` (no params)
  - `_CReplayUpdateCameraAndSwapBuffers` (d1=x, d2=y)
- Removed 5 obsolete bridges: _CEndFirstHalf, _CEndFirstExtraTime, _CEndOfGame, _CStartFirstExtraTime, _CStartPenalties (functions now ported to C, called directly from game_state.c)
- New xdef entries in swos2_unit.s (~55): halfNumber, teamStarting, hideBall, cameraDirection, playerTurnFlags, resultTimer, ballZVelocity, ballXVelocity, ballYVelocity, refState, animatedPatternsArray, numAnimatedPatterns, etc.
- New xref entries in swos2_unit.s (23): all ported C functions
- game_state.c updated: replaced _CEndFirstHalf/_CEndFirstExtraTime/_CEndOfGame/_CStartFirstExtraTime/_CStartPenalties calls with direct _EndFirstHalf/_EndFirstExtraTime/_EndOfGame/_StartFirstExtraTime/_StartPenalties
- Notes:
  - setBreakState/clearCameraVelocity extract common match transition pattern (reduces ~100 lines of duplication)
  - InitPlayersBeforeEnteringPitch uses kTeamsStartingCoordinates for staggered entry positions (22 x,y pairs)
  - InitGameRestoreTeams is the master init called from InitGameSaveTeams fall-through — restores team snapshots, applies pitch physics, calls StartingMatch
  - `bne.w *+4` in InitGameRestoreTeams asm is a no-op conditional branch (always falls through) — translated faithfully
  - _Rand return type fixed from uint8_t to uint16_t to match random.c declaration
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning
- Functions ported this chunk: 21 (15 exported + 6 static)
- **Running total: 302 functions ported (281 + 21)**

### Chunk 32 — Audio IRQ Subsystem (34 functions → `src/audio_irq.c`)
- **Scope:** Complete audio IRQ subsystem — VBlank → AudioIRQUpdateChannels → per-channel processing with dual-voice (primary "beep" + secondary "mod") architecture
- **Exported:** `_AudioIRQUpdateChannels` (returns channelChangeFlags in d0)
- **Static functions (33):** 6 inline HW helpers (audxBase, writeAudxPtr/Len/Per/Vol, writeDMACON) + 27 regular:
  - Primary voice: beepVolumeEnvStop, beepVolumeEnvLoadSegment, beepVolumeEnvStep, beepVolumeLfoStep, applyBeepVolumeAndWriteAUDxVOL, beepLFOAndPitchSlide, initVolumeEnvForNote, noteOnInitChannel, selectVariant, loadAUDxPtrLenIfReloadNeeded, enableDMAIfStartFlag, runChannelScriptStep, updateBeepVoiceParams
  - Secondary voice: volumeEnvStop, volumeEnvLoadSegment, volumeEnvStep, volumeLfoStep, applyVolumeAndWriteAUDxVOL, serviceVolume, lfoAndPitchSlide, initPitchEnv, initVolumeEnv, selectInstrument, loadAUDxPtrLenIfReloadNeeded2, enableDMAIfStartFlag2, runModCommandStream
  - Top-level: serviceOneChannel
- **Assembly stubs:** AudioIRQUpdateChannels forwards to `_AudioIRQUpdateChannels`; all sub-functions stubbed to `rts` (only called from within the C IRQ handler now). `AudioDisableDMAForChannel` kept intact in asm (still used by other asm callers).
- **kAmigaNotePeriodTable** (36-entry Amiga period lookup) moved to `static const` in C — only referenced within IRQ code.
- **Key porting notes:**
  - 68k `muls.w` + `asr.w #6` pattern: must truncate to int16_t before shift (68k shift operates on 16-bit, not 32-bit result)
  - masterVolume per-channel trick: ISR copies channelVolume[i] to low byte of masterVolume word (`move.b $28(a5),$21(a5)`) — big-endian low-byte write
  - Volume clamping: negative values clamp to 0, positive >0x40 clamp to 0x40 (matching 68k `bls`/`bgt` branch pattern)
  - Envelope state machine: states 0=inactive, 2=segment loaded, 4=attack/init, 0xFF=stopping; dispatched via `jsr locret(pc,d0.w)` jump table → C switch statements
  - `st` instruction = set byte to 0xFF
  - `bclr #0` = test bit 0 then clear only bit 0 (not zero the whole byte)
  - Added missing `int8_t` typedef to `audio.h` (was absent from the type definitions)
- **~675 lines of dead code removed** from SWOS2.s between stubs and kAmigaNotePeriodTable
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 34 (1 exported + 33 static)
- **Running total: 336 functions ported (302 + 34)**

### Chunk 33 — Player Movement + Animation (5 functions → `src/player_anim.c`)
- **Scope:** Per-frame player sprite animation advancement, position movement with fixed-point destination clamping, and animation table setup/reset
- **Exported (3):** `_MovePlayers`, `_SetPlayerAnimationTable`, `_SetPlayerAnimationTableAndPictureIndex`
- **Static (2 + 1 helper):** `setNextPlayerFrame`, `movePlayer`, `clampAxis` (shared axis-clamping helper), `computeAnimSlot` (shared slot computation helper)
- **Assembly stubs:**
  - `MovePlayers` → `jmp _MovePlayers` (no params)
  - `SetNextPlayerFrame` → `rts` (dead code, only caller was MovePlayers)
  - `MovePlayer` → `rts` (dead code, only caller was MovePlayers)
  - `SetPlayerAnimationTable` → register-to-stack stub (save d0, d2-d7/a2-a6, push a1/a0, call C, restore) — 38 asm callers
  - `SetPlayerAnimationTableAndPictureIndex` → same stub pattern — 4 asm callers
- **Bridge cleanup:** Removed `_CSetPlayerAnimationTable` bridge from swos2_unit.s. Updated match_setup.c to call `_SetPlayerAnimationTable` directly.
- **New xdef in swos2_unit.s:** `lastTeamScoredNumber`, `lastPlayerScored`
- **New xref in swos2_unit.s:** `_MovePlayers`, `_SetPlayerAnimationTable`, `_SetPlayerAnimationTableAndPictureIndex`
- **Key porting notes:**
  - Animation command decoding: negative frame words encode loop (-999→reset), hold (-101→step back), relative jump (≤-100), delay change (-99..-1)
  - Goal celebration: +24 frame offset for scorer's team players facing dir 0/4 (not goalkeeper), with staggered flash pattern
  - Fixed-point position: SPRITE_X/Y are int32_t accumulators, high word = integer part (big-endian). Clamping compares/writes integer part only.
  - clampAxis helper avoids duplicated/error-prone clamping logic across X/Y and on-screen/off-screen paths
  - AnimTable slot formula: `((teamIdx + gkAdj) * 8 + direction) * 4` byte offset into frame pointers at animTable+2
  - `_SetPlayerAnimationTable` resets frame state (frameIndex=-1, timer=1, counter=0xFFFF); `_SetPlayerAnimationTableAndPictureIndex` preserves frame state and immediately updates imageIndex
  - **Known latent bug:** match_setup.c has wrong sprite field offsets (SPRITE_X=0 should be 30, etc.) — deferred to separate fix
- Build status: **verified** — clean build, player_anim.o linked at 0x15855, all 3 exports in map
- Functions ported this chunk: 5 (3 exported + 2 static + 2 inline helpers)
- **Running total: 341 functions ported (336 + 5)**

### Chunk 34 — Team Kit Color Resolution System (12 functions → `src/team_colors.c`)
- **Scope:** Kit color similarity scoring, conflict detection, and in-game team color setup
- **Exported (9):** `_ConvertTeamColor`, `_AreBasicColorsConflicting`, `_AreShortsAndStripeColorsConflicting`, `_GetTeamKitsSimilarityFactor`, `_MeasureTeamKitsSimilarity`, `_ConvertTeamKitColors`, `_SetTeamSecondaryColors`, `_SetInGameTeamColors`, `_SetInGameTeamsPrimaryColors`
- **Static (3):** `isSingleColorKitSimilar`, `convertTeamKitColorsWithFixedColors`, `resolveTeamKitColorConflict`
- **Assembly stubs:**
  - `GetTeamKitsSimilarityFactor` → register-to-stack bridge (d0-d7 → 8 long params, andi.l #$FF each)
  - `MeasureTeamKitsSimilarity` → register-to-stack bridge (a0, a1 → 2 pointer params, restores a6=teamKitsSimilarity)
  - `SetInGameTeamsPrimaryColors` → register-to-stack bridge (a2, a3 → 2 pointer params)
  - `SetTeamSecondaryColors` → register-to-stack bridge (d0-d4, a1 → 6 params)
  - 8 others → `rts` (dead code, only called from within the ported group)
- **New xdef in swos2_unit.s:** `conflictingBasicTeamColorsTable`, `conflictingShortsTeamColorsTable`, `kitsSimilarityLookupTable`, `shirtType1`, `shirtType2`, `similarityFactor`, `teamKitsSimilarity`, `teamKitTieBreakColors`, `shirtColorTable`
- **New xref in swos2_unit.s:** `_GetTeamKitsSimilarityFactor`, `_MeasureTeamKitsSimilarity`, `_SetInGameTeamsPrimaryColors`, `_SetTeamSecondaryColors`
- **Key porting notes:**
  - Similarity factor scale: HIGH (99,88,77,66) = kits are similar/conflicting; LOW (<11) = different enough to use
  - Color conflict bitmap tables: 10×10 matrices using btst for lookup (conflictingBasicTeamColorsTable, conflictingShortsTeamColorsTable)
  - Kit types: ORDINARY(0), COLORED_SLEEVES(1), VERTICAL_STRIPES(2), HORIZONTAL_STRIPES(3)
  - Stripe-to-solid replacement logic in kitsSimilarityLookupTable
  - Team file offsets (TEAMFILE_*) vs in-game offsets (TEAMGAME_*) for accessing team data
- Build status: **verified** — clean build, team_colors.o linked at 0x16af4, all symbols in map
- Functions ported this chunk: 12 (9 exported + 3 static)
- **Running total: 353 functions ported (341 + 12)**

### Chunk 35 — Referee System (5 functions → `src/referee.c`)
- **Scope:** Referee activation, movement, animation, booking card display
- **Exported (5):** `_InitRefereeAnimationTable`, `_ActivateReferee`, `_SetRefereeToLeavingState`, `_UpdateReferee`, `_UpdateBookedPlayerNumberSprite`
- **Assembly stubs:**
  - `ActivateReferee: jmp _ActivateReferee` (2 asm callers in TryBookingThePlayer/TrySendingOffThePlayer)
  - `SetRefereeToLeavingState: jmp _SetRefereeToLeavingState` (safety stub, no known asm callers after port)
  - `InitRefereeAnimationTable:` register-to-stack stub (A1 → stack, saves all regs) — still called by asm stubs of ActivateReferee/SetRefereeToLeavingState before they redirect
  - `UpdateReferee: jmp _UpdateReferee` (1 asm caller in GameLoop)
  - `UpdateBookedPlayerNumberSprite: jmp _UpdateBookedPlayerNumberSprite` (1 asm caller in GameLoop)
- **New C-to-asm bridges in swos2_unit.s:**
  - `_CCalculateDeltaXAndY` (d1=destX, d2=destY, d3=srcX, d4=srcY, d0=speed → outDeltaX, outDeltaY, outDirection via pointers)
  - `_CSendPlayerAway` (a1=playerSprite, a6=teamData)
- **Bridge removed:** `_CInitRefereeAnimationTable` — replaced by direct call to `_InitRefereeAnimationTable`
- **match_setup.c updated:** `_RemoveReferee` now calls `_InitRefereeAnimationTable` directly
- **New xdef entries in swos2_unit.s:** `bookedPlayerCardOrNumberSprite`, `refTimer`, `kRefereeSpeed`, `refComingAnimTable`, `refYellowCardAnimTable`, `refRedCardAnimTable`, `refSecondYellowAnimTable`, `yellowCardTable`, `redCardTable`, `secondYellowTable`
- **New xref entries in swos2_unit.s:** `_InitRefereeAnimationTable`, `_ActivateReferee`, `_SetRefereeToLeavingState`, `_UpdateReferee`, `_UpdateBookedPlayerNumberSprite`
- **Data tables kept in SWOS2.s:** `yellowCardTable` (30 bytes), `redCardTable` (30 bytes), `secondYellowTable` (46 bytes) — time-sliced card/number display LUTs; `refComingAnimTable`, `refWaitingAnimTable`, `refYellowCardAnimTable`, `refRedCardAnimTable`, `refSecondYellowAnimTable` — 8-direction animation tables
- **Key porting notes:**
  - UpdateReferee has 4 states: INCOMING (move to foul), WAITING_PLAYER (face direction 6), ABOUT_TO_GIVE_CARD→BOOKING (show card animation), LEAVING (move off-screen then hide)
  - Animation frame commands: -999=restart, -101=hold, ≤-100=relative jump, -1..-99=new delay
  - Off-screen update path: only moves position (no animation) when gameStatePl == ST_GAME_IN_PROGRESS
  - CalculateDeltaXAndY returns fixed-point deltaX/Y (int32_t) and direction (0..255); direction mapped to 0..7 via `(d0+16)&0xFF>>5`
  - UpdateBookedPlayerNumberSprite displays card/number alternating via LUT: entry=1→yellow card, entry=2→red card, entry>0→shirt number, entry=-1→sequence end (referee leaves), entry=0→hide
  - SendPlayerAway expects A6=team data; in original asm A6 was implicitly preserved from earlier in the game loop; C code uses lastTeamBooked explicitly
- Build status: **verified** — clean build, referee.c linked, all symbols in map
- Functions ported this chunk: 5 (5 exported)
- **Running total: 358 functions ported (353 + 5)**

### Chunk 36 — PrintFormatted + InputText Family (5 functions → `src/input_text.c`)
- Date: 2026-02-19
- **Scope:** Format string printer and interactive text input system
- **Exported (5):**
  - `_PrintFormatted` (from `PrintFormatted`) → format string with %0..%6 decimal + %a/%b/%c string substitution; goto-based fall-through for digit emission matching 68k behavior
  - `_InputText` (from `InputText`) → interactive text editor: cursor movement, insert/delete, commit/cancel; uses inMenuKeyboardBuffer as edit buffer
  - `_InsertCharacter` (from `InsertCharacter`) → shifts bytes right in inMenuKeyboardBuffer, inserts char at position
  - `_InputTextDeleteCharAtCursor` (from `InputTextDeleteCharAtCursor`) → shifts bytes left at cursor position
  - `_InputTextFilterKey` (from `InputTextFilterKey`) → filters currentKey against A-Z, 0-9, allowed control keys table; allowExtraCharsFlag rejects '.' and '/' when set
- New files: `include/input_text.h`, `src/input_text.c`
- Assembly stubs in SWOS2.s:
  - `PrintFormatted:` register-to-stack stub (d0-d6 zero-extended + a0-a4 → 12 stack params, saves d2-d7/a2-a6)
  - `InputText:` register-to-stack stub (d0 zero-extended, a5 → 2 stack params, saves d2-d7/a2-a6, tst.w d0 after return for Z flag)
  - `InsertCharacter: rts` (dead code, only callers were in InputText)
  - `InputTextDeleteCharAtCursor: rts` (dead code, only callers were in InputText)
  - `InputTextFilterKey: rts` (dead code, only caller was InputText)
- Data labels preserved in SWOS2.s: `inputAllowedControlKeys` (12 bytes: allowed control key table + 0xFF terminator), `allowExtraCharsFlag` (word)
- New xdef entries in swos2_unit.s: `inMenuKeyboardBuffer`, `deadInputTextVar`, `inputAllowedControlKeys`, `allowExtraCharsFlag`
- New xref entries in swos2_unit.s: `_PrintFormatted`, `_InputText`
- Key porting notes:
  - PrintFormatted numeric conversion uses goto-based fall-through matching 68k cascade: once entering at a digit level, all lower digits are always emitted (val=100 → "100", not "1")
  - GetStringLength stub returns a0 = buffer + len (at NUL), NOT buffer + len + 1 — critical for InsertCharacter which shifts from NUL position right
  - InsertCharacter/InputTextDeleteCharAtCursor need `optimize("no-tree-loop-distribute-patterns")` to prevent GCC converting byte-shift loops to memmove
  - InputText also needs the attribute since GCC may inline the helpers
  - InputText cursor starts at NUL (buffer + len), not past it — correct for cursor display (0xFF inserted before NUL)
  - allowExtraCharsFlag: despite name, setting it REJECTS '.' and '/' (intercepted before table scan where they'd normally match)
- Build status: **verified** — clean build, only expected currentMenu boundary-spill warning
- Functions ported this chunk: 5 (5 exported)
- **Running total: 363 functions ported (358 + 5)**

### Chunk 37 — Highlight/Replay Save System (16 functions → `src/highlight.c`)
- Date: 2026-02-19
- **Scope:** Highlight recording, replay playback, pitch selection, and replay state machine
- **Exported (13):**
  - `_SetPitchTypeAndNumber` (from `SetPitchTypeAndNumber`) → selects pitch type from probability tables (standard or seasonal), derives pitch number from team data or random
  - `_CopyTeamsAndHeader` (from `CopyTeamsAndHeader`) → copies teams from hil file buffer to in-game structs, plus game name/round
  - `_SetHilFileHeader` (from `SetHilFileHeader`) → packs in-game data into hil file buffer header
  - `_SaveHighlightsPointers` (from `SaveHighlightsPointers`) → saves highlight buffer pointers, enables playback loop
  - `_RestoreHighlightPointers` (from `RestoreHighlightPointers`) → restores saved pointers, disables playback
  - `_FadeAndSaveHighlightScene` (from `FadeAndSaveHighlightScene`) → FadeOut + SaveHighlightScene + FadeIn wrapper
  - `_SaveHighlightScene` (from `SaveHighlightScene`) → saves replay ring buffer segment to goals buffer, max 10 scenes, shifts oldest on overflow
  - `_ThrowInDeadProc` (from `ThrowInDeadProc`) → sets throwInDeadVar = 1
  - `_SaveCoordinatesForHighlights` (from `SaveCoordinatesForHighlights`) → encodes sprite coords / frame markers into ring buffer
  - `_DrawReplayFrame` (from `DrawReplayFrame`) → replay frame playback state machine: camera init, sprite/frame decode, score overlay, timing
  - `_ReplayUpdateCameraAndSwapBuffers` (from `ReplayUpdateCameraAndSwapBuffers`) → camera setup sequence for replay
  - `_HandleInstantReplayStateSwitch` (from `HandleInstantReplayStateSwitch`) → state transitions: 0→1 (search), 2↔3 (play/pause)
  - `_RestoreCameraPosition` (from `RestoreCameraPosition`) → restores camera after replay ends
- **Internal (3):** `wrapPointerFwd` (ValidateHilPointerAdd), `wrapPointerBack` (ValidateHilPointerSub), `wrapHilPtr` (IncrementHilBufferPointer) — static C helpers
- New files: `include/highlight.h`, `src/highlight.c`
- Assembly stubs in SWOS2.s:
  - Simple `jmp _FuncName` stubs: SetPitchTypeAndNumber, CopyTeamsAndHeader, SetHilFileHeader, SaveHighlightsPointers, RestoreHighlightPointers, FadeAndSaveHighlightScene, SaveHighlightScene, ThrowInDeadProc, DrawReplayFrame, HandleInstantReplayStateSwitch, RestoreCameraPosition
  - Register-to-stack bridges: SaveCoordinatesForHighlights (d0=int16_t via ext.l, d4/d5=uint16_t via andi.l), ReplayUpdateCameraAndSwapBuffers (d1/d2=int16_t via ext.l)
  - Dead `rts` stubs: IncrementHilBufferPointer, ValidateHilPointerAdd, ValidateHilPointerSub
- Data labels preserved: pitchNumberProbabilities (16 bytes), kHilFileSig (4 bytes), dead code after ThrowInDeadProc (lines 12135-12142)
- New C-to-asm bridges in swos2_unit.s: `_CResetAnimatedPatternsForBothTeams`, `_CDrawAnimatedPatterns`
- Local const data in C: pitchTypeSeasonalProbs (84 bytes), pitchTypeProbs (8 bytes), pitchNumProbs (16 bytes)
- New xdef entries (30+): numScenesInHilFile, hilSceneNumber, goIntoHighlightsLoop, savedCurrentHilPtr, savedCurrentGoalPtr, savedGoalBasePtr, savedNextGoalPtr, normalReplayRunning, savedCameraX, savedCameraY, hilLeftTeamGoals, hilRightTeamGoals, hilPitchNumber, hilTeam1Goals, hilTeam2Goals, hilMaxSubstitutes, hilGameName, hilGameRound, gameName, gameRound, gamePitchTypeOrSeason, gamePitchType, gameSeason, plgD0Param, gameMaxSubstitutes, paused, animPatternsState, currentGoalPtr, hilStartOfGoalsBuffer, inGameHilBuffer
- Key porting notes:
  - DrawReplayFrame uses goto-based control flow matching asm labels (sprite_decode=loc_10C494, check_next=loc_10C4D0, end_replay=loc_10C4C4, after_decode=loc_10C4E2)
  - Sprite packing: (index << 20) | (x << 10) | y, bit 31 clear; frame markers have bit 31 set
  - Sprite unpacking uses 68k-style lsl.l/asr.l sign extension: `(int32_t)(d0 << 12) >> 22` for 10-bit signed x
  - SetPitchTypeAndNumber has 3 dead asm instructions at pitch_type_selected (currentGameTick masking) that are immediately overwritten in both branches — omitted in C
  - SaveHighlightScene has dead beq branch (move.w #10,d0; beq — 10 is never 0) and dead bmi branch — omitted
  - Busy-wait in DrawReplayFrame state 3 uses volatile pointer to prevent optimization
  - CopyTeamsAndHeader/SetHilFileHeader need `optimize("no-tree-loop-distribute-patterns")` for byte-copy loops
  - Existing bridge `_CReplayUpdateCameraAndSwapBuffers` still works (double-hop through asm stub)
- Build status: **verified** — clean build
- Functions ported this chunk: 16 (13 exported + 3 internal)
- **Running total: 379 functions ported (363 + 16)**

### Chunk 38 — Animated Patterns + GoalScored (11 functions → `src/animated_patterns.c`, `src/goal_scored.c`)
- Date: 2026-02-19
- **Scope:** Crowd fan animated patterns (8-state machine, tile toggling) and goal scoring system (scorer list sprites)
- **Exported (3):**
  - `_DrawAnimatedPatterns` (from `DrawAnimatedPatterns`) → 8-state machine for fan celebration patterns; selects random pitch tiles, toggles between static/animated variants based on game state (goal celebration phases, ball position, etc.)
  - `_ResetAnimatedPatternsForBothTeams` (from `ResetAnimatedPatternsForBothTeams`) → resets animated pitch tiles back to static for top + bottom fan sections, clears animatedPatterns tracking array
  - `_GoalScored` (from `GoalScored`) → updates goal counters, determines goal type (normal/penalty/own goal), builds scorer name+time string sprites with multi-row splitting
- **Static (8):**
  - `swapTwoRandomIndicesFromAnimPatternsTable` — shuffles two random entries in 64-byte permutation table
  - `drawPatternFromIndex` — draws single animated tile: copies tile data to sprite 137, draws to frontbuffer + mask
  - `resetAnimatedPatterns` — inner loop: scans pitchDatBuffer entries, forces animated (even) tiles to static (odd)
  - `readWord`, `writeWord`, `readUWord`, `writeUWord` — struct field access helpers for GoalScored
  - Goal format template strings (static char arrays): 6 templates for normal/penalty/own-goal with and without trailing comma
- New files: `include/animated_patterns.h`, `src/animated_patterns.c`, `include/goal_scored.h`, `src/goal_scored.c`
- Assembly stubs in SWOS2.s:
  - `DrawAnimatedPatterns: jmp _DrawAnimatedPatterns` (1 asm caller in game loop)
  - `SwapTwoRandomIndicesFromAnimPatternsTable: rts` (dead, only caller was DrawAnimatedPatterns)
  - `ResetAnimatedPatternsForBothTeams: jmp _ResetAnimatedPatternsForBothTeams` (1 asm caller)
  - `ResetAnimatedPatterns: rts` (dead, only caller was ResetAnimatedPatternsForBothTeams)
  - `DrawPatternFromIndex: rts` (dead, only caller was DrawAnimatedPatterns)
  - `GoalScored:` register-to-stack bridge (d0 via ext.l, a0 → 2 stack params, saves d2-d7/a2-a6)
- Data labels preserved in SWOS2.s: animPatternsRandomTable (64 bytes), animatedPatterns through animPatternsState (data words), goal format strings (aAsc_0 through aAsc_2) and pointer tables (alreadyScoredTable, firstGoalScoredTable)
- **Bridge changes:**
  - Removed `_CResetAnimatedPatternsForBothTeams` and `_CDrawAnimatedPatterns` bridges from swos2_unit.s (functions now in C)
  - Updated highlight.c to call `_ResetAnimatedPatternsForBothTeams` and `_DrawAnimatedPatterns` directly
  - Reuses existing `_CDrawSprite16Pixels` bridge (from earlier chunk) for drawPatternFromIndex
- New xdef entries in swos2_unit.s: `teamScoredGamePtr`, `animPatternsRandomTable`, `patternStatic`, `otherTeamScored`, `ingameTeamPlayerOffsets`
- New xref entries in swos2_unit.s: `_DrawAnimatedPatterns`, `_ResetAnimatedPatternsForBothTeams`, `_GoalScored`
- Key porting notes:
  - Pitch tile pairs: odd index = static (fans sitting), even = animated (fans jumping). bit 7 of byte offset = bit 0 of tile index (since TILE_SIZE=128=0x80)
  - DrawAnimatedPatterns uses `patternStatic` as animate flag: non-zero→toggle to animated, zero→toggle to static
  - Camera Y determines fan section: <64 = top fans (sprite range +21), >=544 = bottom fans (+1071), 64-543 = no fans visible
  - otherTeamScored suppresses animation for away team's half when camera at top
  - GoalScored goal format templates are writable char arrays; first 4 bytes overwritten with time digits, byte 0 skipped, leading '0's trimmed
  - Scorer list supports up to 8 sprite rows with multi-row splitting when Text2Sprite overflows
  - Shift-down logic steals sprite slot from the bottom of the 8-sprite array when a scorer needs an extra row
  - PLAYERINFO_SIZE + PLAYERINFO_SKIP = 61 + 0 = 61 byte stride for player records in team file data
  - extern type fixes: `spriteGraphicsPtr` as `uint8_t *`, `topTeamPtr`/`bottomTeamPtr` as `uint8_t *`, `pitchDatBuffer` as `uint8_t[]` to match existing declarations
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 11 (3 exported + 8 static)
- **Running total: 390 functions ported (379 + 11)**

### Chunk 39 — Bench/Substitution Controls (22 functions → `src/bench_subs.c`)
- Date: 2026-02-19
- **Scope:** In-game bench menu system: substitution controls, formation selection, player marking, player swapping
- **Exported (1):**
  - `_BenchCheckControls` (from `BenchCheckControls`) → per-frame bench controls check called from GameLoop; detects double-tap direction to activate bench, dispatches to subsState handlers (0-4), handles training mode team swapping
- **Static (21):**
  - `leaveSubsMenu` — cleanup: clears bench state, calls _InitCurrentGoalHighlight and _CCheckIfGoalkeeperClaimedTheBall
  - `benchMenuHandleControls` — top-level dispatcher by subsState (0=top menu, 1=select substitute, 2=formation, 3=send-off confirm, 4=mark players)
  - `trainingSwapBenchTeams` — swaps bench/opponent teams for training mode
  - `isThisPlayerOkToSelect` — validates bench player selection (subs count, substituted, position, cards)
  - `firePressedInSubsMenu` — handles fire button: enters substitute selection (state 1) or marks menu (state 4)
  - `showFormationMenu` — enters formation selection (state 2)
  - `subsBenchOff` — returns to top-level bench view (state 0)
  - `selectPlayerToSubstituteMenuHandler` — state 1: confirms substitution, updates sub counters
  - `checkForThrowInAndGoalkeepersBall` — resets throw-in player state, calls CheckIfGoalkeeperClaimedTheBall
  - `substitutePlayer` — executes substitution: swaps 61 bytes player data + 110 bytes sprite data, reinitializes sprite
  - `state3HandleFirePress` — state 3: clears overlay on fire (dead code for send-off logic)
  - `markPlayersMenuHandler` — state 4: toggle marked player (long fire), swap two players (fire with selection)
  - `handleFormationMenuControls` — state 2: applies selected formation on fire
  - `updateSubstitutePlayerIndices` — scans ordinals forward/backward for valid substitution candidates
  - `updatePlayerToBeSubstitutedPosition` — converts ordinal to position via getBenchPlayerPointer
  - `isPlayerOkToSubstitute` — validates candidate (cards >= 2 = OK, state 3 checks disallowSubs)
  - `handleFormationMenuMovement` — left/right navigation through 18 formation entries (0x00-0x11)
  - `findInitialPlayerToBeSubstituted` — priority search: exact position match > group match (DEF/MID) > first available
  - `swapPlayerNameData` — swaps ordinals and name lengths between two player indices
  - `getBenchPlayerPointer` — looks up player in current formation/tactics ordering, returns pointer + ordinal
  - `checkForThrowInAndGoalkeepersBall` — resets throw-in player state
- New files: `include/bench_subs.h`, `src/bench_subs.c`
- Assembly stubs in SWOS2.s:
  - `BenchCheckControls: jmp _BenchCheckControls` (1 asm caller in game loop)
  - All 21 other functions: dead code (NO external callers confirmed)
- New C-to-asm bridges in swos2_unit.s:
  - `_CCheckIfGoalkeeperClaimedTheBall` (saves d2-d7/a2-a6, calls CheckIfGoalkeeperClaimedTheBall)
- New xdef entries in swos2_unit.s (~66): benchTeam, benchTeamGame, subsTeamsSwapped, subsTrainingTopTeam, drawTrainingTopHalfOfTheBench, initialSubsState, subsDirection, subsFire, subsFireTimer, benchPlayerIndex, subsBlockFire, subsBlockDirections, subsMovementDelayTimer, subsPrevDirection, subsPrevPl1Direction, subsPrevPl2Direction, subsPl1TapCount, subsPl2TapCount, pl1BenchTimeoutCounter, pl2BenchTimeoutCounter, pl1BenchBlockWhileHoldingDirection, pl2BenchBlockWhileHoldingDirection, benchOff, waitForPlayerToGoInTimer, cameraLeavingSubsTimer, substituteInProgress, injuriesForever, bench1Called, bench2Called, benchY, opponentBenchY, plComingYTmp, plComingY, benchWriteOnly01, benchXLimit, subsMenuX, formationMenuX, plLeavingX, plComingX, benchWriteOnly02, deadBenchVar, leavingSubsMenu, subsWriteOnly, goToSubsTimer, selectedFormationEntry, plToBeSubstitutedPos, plToBeSubstitutedOrd, plToEnterGameIndex, selectedPlayerInSubs, team1NumSubs, team2NumSubs, team1GoalkeeperReplaced, team2GoalkeeperReplaced, maxSubsPerTeam, substitutedPlSprite, teamThatSubstitutes, substitutedPlX, substitutedPlY, substitutedPlDestX, substitutedPlDestY, plSubstitutedX, plSubstitutedY, positionsTable, writeOnly01
- New xref entries in swos2_unit.s: `_BenchCheckControls`
- Key porting notes:
  - Double-tap detection: direction pressed within 15 frames of previous tap activates bench
  - Training mode inverts up/down controls for top team (subsTrainingTopTeam flag)
  - Team name bit 1 (topTeamInGame[TEAMGAME_TEAMNAME] & 2) determines bench side Y coordinates
  - substitutePlayer and markPlayersMenuHandler use byte-by-byte swap loops needing `optimize("no-tree-loop-distribute-patterns")`
  - benchTeam and benchTeamGame stored as uint32_t (pointer-as-integer pattern) matching asm lea/movea semantics
  - positionsTable (18 entries × 11 bytes) maps formation index to player ordering
  - Bug fix during port: asm line 12143 checks PLAYERNUMBER==2 (not PLAYERCOACHNUMBER) — corrected in C
  - Dead code: state 3 (send-off confirmation) handler exists but is never reached in normal gameplay
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 22 (1 exported + 21 static)
- **Running total: 412 functions ported (390 + 22)**

### Chunk 40 — Bench/Substitution Drawing (7 functions → `src/bench_subs.c`)
- Date: 2026-02-19
- **Scope:** Drawing functions for bench players, coach, substitutes menu, and formation menu
- **Exported (1):**
  - `_DrawBenchAndSubsMenu` (from `DrawBenchAndSubsMenu`) → main drawing entry: draws bench player sprites, coach animation, selection arrow, dispatches to DrawSubstitutesMenu/DrawFormationMenu based on subsState
- **Static (6):**
  - `drawOpponentsBench` — draws opponent's bench (saves/restores subsState, flips drawTrainingTopHalfOfTheBench)
  - `drawBenchPlayersAndCoach` — draws coach sprite (animated from frame tables) + up to 5 bench player sprites with kit colors, goalkeeper detection, selected/normal states
  - `drawSubstitutesMenu` — draws substitutes selection overlay: team color extraction, title box with "SUBSTITUTES" sprite, 11 player entries + "FORMATION" entry
  - `drawSubstitutesMenuEntry` — single entry: shadow/frame/background rectangles, face sprite (individual skin), shirt number, player name, position, card icon, injury indicator
  - `drawFormationMenu` — draws formation menu overlay: title box with "FORMATION" sprite, 18 formation entries
  - `drawFormationMenuEntry` — single formation entry: shadow/frame/background, centered formation name text
- Added to existing files: `src/bench_subs.c`, `include/bench_subs.h`
- Assembly stubs in SWOS2.s:
  - `DrawBenchAndSubsMenu: jmp _DrawBenchAndSubsMenu` (1 asm caller at line 14262 in game loop)
  - All 6 other functions: dead code (NO external callers)
- New C-to-asm bridges in swos2_unit.s:
  - `_CDrawSpriteInCameraView` (d0=sprite, d1=x, d2=y → calls DrawSpriteInCameraView)
  - `_CDrawSpriteHotspotInCameraView` (d0=sprite, d1=x, d2=y → calls DrawSpriteHotspotInCameraView)
- New xdef entries in swos2_unit.s: team1MostFrequentFace, team2MostFrequentFace, benchColors, subsMenuBackgroundColor, subsMenuHighlightColor, subsMenuEntryFrameColor, ourCoachFrames, opponentCoachFrames, playerFacesTable, playerNumberString, aFormation, playerPositionsStringTable, tacticsStringTable
- New xref entries in swos2_unit.s: `_DrawBenchAndSubsMenu`
- Key porting notes:
  - Boundary spill fix: asm uses `lea (team2GoalkeeperReplaced),a1; move.w (a1,teamNumber*2),d3` to read team1MostFrequentFace/team2MostFrequentFace — C uses explicit variables instead of pointer arithmetic
  - Bug fix: drawOpponentsBench condition was inverted (benchTeamGame != bottomTeamInGame selects bottomTeamInGame as opponent, not ==)
  - Coach animation: currentGameTick & 0x1E0 >> 4 indexes into 16-entry ourCoachFrames/opponentCoachFrames tables
  - Training top half of bench: loop iterates backward (stride -61) instead of forward (+61)
  - benchColors extraction: finds 3 distinct colors from kit (shirt/stripe/shorts/socks), skipping colors 2 and 15 (pitch background colors)
  - Face sprite blinks when player is marked: `frameCount & 4` == 0 suppresses face drawing
  - Injury severity encoded in bits 7:5 of injuriesbitfield, compared with frameCount & 7 for flicker effect
  - drawFormationMenuEntry uses tacticsStringTable for entries 0-11, teamTacticsTable for entries 12-17
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 7 (1 exported + 6 static)
- **Running total: 419 functions ported (412 + 7)**

### Chunk 41 — Game Loop + MainKeysCheck + Highlights Playback (10 functions → `src/game_loop.c`)
- Date: 2026-02-19
- **Scope:** Main game loop, key dispatch, highlight/replay playback loops, fake goal assignment on ESC
- **Exported (8):**
  - `_ViewMatchSequence` (from `ViewMatchSequence`) → loads topTeamInGame/bottomTeamInGame, calls StartMatchSequence
  - `_ViewHighlights` (from `ViewHighlights`) → loads topTeamInGame/bottomTeamInGame, calls ViewHighLightsMain
  - `_StartMatchSequence` (from `StartMatchSequence`) → loads SFX, sets playGame=1, calls InitGameSaveTeams then GameLoop
  - `_GameLoop` (from `GameLoop`) → main per-frame game loop: init random scroll, VBlank sync, pause handling, gameplay update (MoveCamera/UpdateBall/MovePlayers/etc.), replay/highlight dispatch, end-of-match flow with friendly replay restart
  - `_KeepStatisticsOnScreen` (from `KeepStatisticsOnScreen`) → spin-loop while stats overlay showing: tick wait, key dispatch, UpdateAndDrawStatistics
  - `_ViewHighLightsMain` (from `ViewHighLightsMain`) → highlights viewer: copies teams, inits game, frame loop with highlight playback dispatch
  - `_PlayInstantReplayLoop` (from `PlayInstantReplayLoop`) → instant replay playback: pause handling, fire-to-abort (per-player joystick detection based on teamScoredDataPtr), DrawReplayFrame, VBlank sync
  - `_PlayHighlightsLoop` (from `PlayHighlightsLoop`) → multi-scene highlight playback: advances through scenes, fire-to-abort, scene pointer management
- **Exported (2 additional):**
  - `_MainKeysCheck` (from `MainKeysCheck`) → key dispatch: R=instant replay, H=normal replay, 0x88=toggle bigS, 0x87=cycle indicator, 0x89=fade+crowd, 0x10/0x11=bench, S=stats, ESC=exit with fake goals, SPACE=capture highlight, P=pause
  - `_AssignFakeGoalsToScorers` (from `AssignFakeGoalsToScorers`) → weighted random goal assignment to player positions when ESC-aborting; uses kAbortLossScorerWeightTable + playersPositionsByTactics
- **Static (2):**
  - `waitVBlankSync` — VBlank sync helper: DMACON blitter wait, anti-tamper cookie check (always passes), waitingInterrupt spin-wait
  - `pauseSpinWaitOneTick` — waits for currentTick to change (matches dbf d1 with d1=0 pattern)
- New files: `include/game_loop.h`, `src/game_loop.c`
- Assembly stubs in SWOS2.s:
  - `ViewMatchSequence: jmp _ViewMatchSequence`
  - `ViewHighlights: jmp _ViewHighlights`
  - `StartMatchSequence:` register-to-stack stub (a5, a6 → stack, saves d2-d7/a2-a6)
  - `GameLoop: jmp _GameLoop`
  - `KeepStatisticsOnScreen: jmp _KeepStatisticsOnScreen`
  - `ViewHighLightsMain:` register-to-stack stub (a5, a6 → stack, saves d2-d7/a2-a6)
  - `PlayInstantReplayLoop: jmp _PlayInstantReplayLoop`
  - `PlayHighlightsLoop: jmp _PlayHighlightsLoop`
  - `MainKeysCheck:` register-to-stack stub (d0 zero-extended → stack, saves d2-d7/a2-a6)
  - `AssignFakeGoalsToScorers:` register-to-stack stub (d0 zero-extended, d5 sign-extended → stack, saves d2-d7/a2-a6)
- New C-to-asm bridges in swos2_unit.s:
  - `_CMoveCamera` (calls MoveCamera, saves/restores all regs)
  - `_CUpdateAndApplyTeamControls` (calls UpdateAndApplyTeamControls)
  - `_CUpdateBall` (calls UpdateBall)
  - `_CInjuryUpdateTickGuard` (calls InjuryUpdateTickGuard)
  - `_CDoGoalkeeperSprites` (calls DoGoalkeeperSprites)
  - `_CDrawSprites` (calls DrawSprites)
- Data labels preserved in SWOS2.s: normalReplayRunning, instantReplayFlag, saveHighlightScene, fadeAndSaveHighlightScene, spaceReplayTimer, subsWriteOnly, playGame, playHighlightFlag, paused, overlayFlagBackup, kAbortLossScorerWeightTable (12 bytes)
- New xdef entries in swos2_unit.s: gameCanceled, instantReplayFlag, saveHighlightScene, fadeAndSaveHighlightScene, spaceReplayTimer, overlayFlagBackup, userRequestedReplay, teamScoredDataPtr, isGameFriendly, team1NumAllowedInjuries, team2NumAllowedInjuries, kAbortLossScorerWeightTable, + 6 bridge xdefs
- New xref entries in swos2_unit.s: _ViewMatchSequence, _ViewHighlights, _StartMatchSequence, _GameLoop, _KeepStatisticsOnScreen, _ViewHighLightsMain, _PlayInstantReplayLoop, _PlayHighlightsLoop, _MainKeysCheck, _AssignFakeGoalsToScorers
- Key porting notes:
  - VBlank sync anti-tamper checks: each loop has unique XOR magic + expected value; branches to dead-code labels on failure (never taken). C implementation preserves the waitingInterrupt spin-wait but skips the XOR check (always passes)
  - GameLoop friendly replay restart: original uses `bra.w GameLoop`; C uses recursive `_GameLoop()` call
  - PlayInstantReplayLoop fire-to-abort: complex per-player detection based on teamScoredDataPtr→PLAYERNUMBER/PLAYERCOACHNUMBER to determine which joystick to poll
  - MainKeysCheck ESC path: determines winning team by TEAMGENERALINFO_TEAMNUMBER (offset 0x12=18, NOT playerNumber), assigns fake 5+ goals with increment loop, then calls AssignFakeGoalsToScorers
  - AssignFakeGoalsToScorers: weighted random with 11 outfield positions (weights 35,25,20,...) + rare own-goal (>=248) + very rare goalkeeper (second random)
  - TEAMGENERALINFO_PLAYERCOACHNUMBER is at offset 6 (not 8 as initially assumed)
  - nullsub_2/3/4/5/8/9/10 are all no-ops — omitted in C
  - ~700 lines of dead asm code removed from GameLoop/MainKeysCheck/replay functions
- Build status: **verified** — clean build, only pre-existing warnings

### Chunk 42 — CalculateDeltaXAndY + MoveCamera (2 functions → `src/math68k.c` + `src/move_camera.c`)
- Date: 2026-02-19
- **Scope:** Direction/delta vector calculation with sine/cosine lookup tables, and per-frame camera tracking logic
- **Exported (2):**
  - `_CalculateDeltaXAndY` (from `CalculateDeltaXAndY`) → given src/dest coords + speed, computes direction (0..255 or 0xFFFF), deltaX, deltaY using angle coefficient table + sine/cosine table. Iterative dx/dy normalization into [0..31], tangent lookup, quadrant mapping, fixed-point 16×16→32 multiplication.
  - `_MoveCamera` (from `MoveCamera`) → per-frame camera tracking with priority system: (1) fans → no-op, (2) booking → lock on player, (3) penalties → lock on spot, (4) subs timers → bench area, (5) subs menu → goal/bench state, (6) normal game → velocity-based ball tracking with acceleration ±2 toward ±40, (7) set pieces → narrow bounds, (8) special states → fixed targets. Uses 16.16 fixed-point for smooth sub-pixel movement. Clamps camera to pitch/screen bounds.
- New files: `include/math68k.h`, `src/math68k.c`, `src/move_camera.c`
- Static const data moved to C:
  - `kSineCosineTable` (257 int16_t entries — 256 sine/cosine values + pad) → static const in math68k.c
  - `kAngleCoeficients` (1025 int16_t entries — 32×32 angle lookup + pad) → static const in math68k.c
  - `nextCameraDirections` (16 int32_t entries — 8 direction pairs) → static const in move_camera.c
  - `maxCameraMovement` (0x00050000) → #define in move_camera.c
- Assembly stubs in SWOS2.s:
  - `CalculateDeltaXAndY:` register-to-stack stub — allocates 8-byte local for outDeltaX/outDeltaY, pushes 7 args (destX, destY, srcX, srcY, speed, &outDX, &outDY), calls `_CalculateDeltaXAndY`, loads results back to d1/d2/d0, sets Z flag
  - `MoveCamera: jmp _MoveCamera` (+ nop)
- Data labels preserved in SWOS2.s: cameraXVelocity, cameraYVelocity (exported, used by other C code)
- Removed bridges:
  - `_CCalculateDeltaXAndY` bridge removed from swos2_unit.s (was 16 lines)
  - `_CMoveCamera` bridge removed from swos2_unit.s (was 5 lines)
- Updated callers:
  - `referee.c`: changed from `_CCalculateDeltaXAndY` (void + output pointer for dir) to `_CalculateDeltaXAndY` (returns dir directly)
  - `game_loop.c`: changed from `_CMoveCamera()` to `_MoveCamera()`
- New xref entries: `_CalculateDeltaXAndY`, `_MoveCamera`
- Removed xdef entries: `_CCalculateDeltaXAndY`, `_CMoveCamera`
- Key porting notes:
  - cameraX/cameraY are 32-bit fixed-point (dc.l) in memory: high word = integer, low word = fraction. Declared as int16_t in most C files (reads integer part on big-endian 68k). move_camera.c uses `__asm__("cameraX")` aliases for int32_t access.
  - MoveCamera writes only the fractional part back, then calls SetCameraCoordinates for the integer part. The original uses `move.w d1,(cameraX+2).l` for fraction + `swap d1` for integer extraction.
  - ~550 lines of asm code removed (CalculateDeltaXAndY body + 2 data tables + MoveCamera body + inline data)
- **Running total: 431 functions ported (429 + 2)**
- Build status: **verified** — clean build, only pre-existing warnings + LTO type mismatch for cameraX/cameraY (int32_t vs int16_t, safe due to function call barriers)

### Chunk 43 — Ball Physics + Helpers (12 functions → `src/ball_physics.c`)
- Date: 2026-02-19
- **Scope:** Core ball physics simulation, goalkeeper sprite positioning, injury guard, player-ball interaction utilities
- **Exported (10):**
  - `_UpdateBall` (from `UpdateBall`) → ~400 lines, core ball physics: frame animation selection (static/moving), movement via CalculateDeltaXAndY, friction (ground/air constants), gravity integration, keeper-holds-ball special handling, bounce physics with configurable damping, goal area collision (upper/lower goal, crossbar, side posts), goalpost deflection with random scatter, out-of-play boundary checks (4 quadrant calls to CheckIfBallOutOfPlay), shadow sprite positioning, AI quadrant calculation for tactics
  - `_CalculateNextBallPosition` (from `CalculateNextBallPosition`) → trajectory prediction with scaled integration loops (scale 1/2/4/8 based on ball height), writes to ballNextX/ballNextY
  - `_DoGoalkeeperSprites` (from `DoGoalkeeperSprites`) → positions ball relative to diving goalkeeper using goalkeeperDiveBallHeights (28 entries) and kGoalKeeperClaimingBallHeight (7 entries) lookup tables
  - `_InjuryUpdateTickGuard` (from `InjuryUpdateTickGuard`) → checks injuriesForever flag (no-op guard for injury system)
  - `_ResetBothTeamSpinTimers` (from `ResetBothTeamSpinTimers`) → sets both team spin timers to 0xFFFF
  - `_GetPlayerPointerFromShirtNumber` (from `GetPlayerPointerFromShirtNumber`) → looks up player info pointer from shirt ordinal via ingameTeamPlayerOffsets table
  - `_UpdateControllingPlayer` (from `UpdateControllingPlayer`) → positions ball at player's feet using kBallPlOffsets direction table, halves deltaZ
  - `_UpdateBallWithControllingGoalkeeper` (from `UpdateBallWithControllingGoalkeeper`) → like above but forces deltaZ negative (ball always dropping when keeper has it)
  - `_UpdatePlayerWithBall` (from `UpdatePlayerWithBall`) → offsets player position relative to ball using kPlayerWithBallOffsets table
  - `_SetThrowInPlayerDestinationCoordinates` (from `SetThrowInPlayerDestinationCoordinates`) → positions throw-in player 3px from ball
- **Static helpers (2):**
  - `reverseDestXDirection` — reflects destX around saved X position
  - `reverseDestYDirection` — reflects destY around saved Y position
- New files: `include/ball_physics.h`, `src/ball_physics.c`
- Assembly stubs in SWOS2.s:
  - `UpdateBall: jmp _UpdateBall` (body removed, ~500 lines)
  - `ReverseDestXDirection: rts` (dead code, only caller was UpdateBall)
  - `ReverseDestYDirection: rts` (dead code, only caller was UpdateBall)
  - `DoGoalkeeperSprites: jmp _DoGoalkeeperSprites` (body removed, ~77 lines)
  - `InjuryUpdateTickGuard: jmp _InjuryUpdateTickGuard`
  - `CalculateNextBallPosition: jmp _CalculateNextBallPosition` (body removed, ~67 lines)
  - `ResetBothTeamSpinTimers: jmp _ResetBothTeamSpinTimers`
  - `GetPlayerPointerFromShirtNumber:` register bridge (A1,A6→stack, result D0→A4, preserves A1) — 15 active asm callers
  - `UpdateControllingPlayer:` register bridge (A1→stack, preserves A1) — 1 active asm caller
  - `UpdateBallWithControllingGoalkeeper:` register bridge (A1→stack, preserves A1)
  - `UpdatePlayerWithBall:` register bridge (A1→stack, preserves A1) — 3 active asm callers
  - `SetThrowInPlayerDestinationCoordinates:` register bridge (A1→stack, preserves A1) — 2 active asm callers
- Data labels preserved in SWOS2.s: ballNextX, ballNextY, kBallPlOffsets (16 entries), kPlayerWithBallOffsets (16 entries)
- New xdef entries (23): ballShadowSprite, goalie1Sprite, goalie2Sprite, ballStaticFrameIndices, ballMovingFrameIndices, kBallGroundConstant, kBallAirConstant, kGravityConstant, writeOnlyVar04, goalkeeperDiveBallHeights, kGoalKeeperClaimingBallHeight, ballNextX, ballNextY, ballXQuadrantLimits, ballYQuadrantLimits, ballQuadrantIndex, ballXQuadrantDead, ballYQuadrantDead, playerXQuadrantOffset, playerYQuadrantOffset, kBallPlOffsets, kPlayerWithBallOffsets, _CCheckIfBallOutOfPlay
- New xref entries (10): _UpdateBall, _DoGoalkeeperSprites, _InjuryUpdateTickGuard, _CalculateNextBallPosition, _ResetBothTeamSpinTimers, _UpdateControllingPlayer, _UpdateBallWithControllingGoalkeeper, _UpdatePlayerWithBall, _SetThrowInPlayerDestinationCoordinates, _GetPlayerPointerFromShirtNumber
- Removed bridges: _CUpdateBall, _CInjuryUpdateTickGuard, _CDoGoalkeeperSprites (from swos2_unit.s)
- New bridge: _CCheckIfBallOutOfPlay (no params, calls asm CheckIfBallOutOfPlay — still unported)
- Updated callers: game_loop.c changed from _CUpdateBall/_CInjuryUpdateTickGuard/_CDoGoalkeeperSprites to _UpdateBall/_InjuryUpdateTickGuard/_DoGoalkeeperSprites
- RE bug fixed: line 9517 in SWOS2.s `move.l #(frontbufferBaseAlt+$1CAA),d3` — IDA address-as-constant bug. Real value is `0x00010002` (positive counterpart of `0xFFFEFFFE`). See Bug Log in CLAUDE.md.
- ~650 lines of asm code removed
- **Running total: 443 functions ported (431 + 12)**
- Build status: **verified** — clean build, no new warnings, no unresolved symbols

### Chunk 44 — Interrupt Handlers + Hardware Init (6 functions → `src/interrupts.c`)
- Date: 2026-02-19
- **Scope:** Interrupt service routines, hardware initialization, and interrupt vector setup
- **Exported (4):**
  - `_InitHardwareAtFrameStart` (from `InitHardwareAtFrameStart`) → waits for raster at top of frame (VPOSR ≤ 0x0A), disables all interrupts (INTENA/INTREQ 0x7FFF), DMA (DMACON 0x7FFF), CIA interrupts (CIAA_ICR/CIAB_ICR 0x7F), clears BPLCON0
  - `_SetupInterrupts` (from `SetupInterrupts`) → saves old Level 3 vector to Level 1, installs Int3Handler at Level 3 (VBlank), Int5Handler at Level 5 (serial), enables INTENA 0xC824 (Master + RBF + VBlank + SOFTINT)
  - `_Int2KeyboardHandler` (from `Int2KeyboardHandler`) → Level 2 ISR body: checks INTREQR/CIAA_ICR, calls KBHook (returns raw CIAA_SDR in d0), keyboard handshake via CIAA_CRA bit 6 + VPOSR polling delay, NOT+ROR scan code conversion, key buffer insertion (max 9), key repeat setup (keyDelay=0x800C)
  - `_Int3TimerHandler` (from `Int3TimerHandler`) → Level 1 ISR body (deferred VBlank): display buffer swap path (UpdatePlanePtrsAndBlitParams → SwapBuffersAndCameraViewState → SetCameraCoordinates), fade ticks, audio IRQ dispatch, game tick counting (currentTick always, currentGameTick when not paused), key repeat countdown (keyDelay negative→decrement→zero triggers repeat), clears SOFTINT
- **Kept in asm (2):**
  - `Int3Handler` — VBlank dispatcher (6 instructions + rte): clears VBlank INTREQ, triggers SOFTINT, checks/clears int5SerialBusBufferFull
  - `Int5Handler` — Serial port ISR (3 instructions + rte): clears serial INTREQ, sets int5SerialBusBufferFull flag
- **Dead code stubbed (1):**
  - `AudioDisableDMAForChannel` → `rts` (already ported to C in audio.c, no remaining asm callers)
- New files: `include/interrupts.h`, `src/interrupts.c`
- Assembly stubs in SWOS2.s:
  - `Int2KeyboardHandler:` ISR wrapper (movem.l d0-d7/a0-a6 save/restore + jsr _Int2KeyboardHandler + rte)
  - `Int3TimerHandler:` ISR wrapper (same pattern)
  - `InitHardwareAtFrameStart: jmp _InitHardwareAtFrameStart`
  - `SetupInterrupts: jmp _SetupInterrupts`
  - `AudioDisableDMAForChannel: rts`
- Inline data preserved in SWOS2.s: keyDelay (dc.w), keyCount (dc.w), keyBuffer (12 bytes), writeOnly01 (dc.w), lastVBlankServiceTick (dc.w), int5SerialBusBufferFull (dc.w)
- New xdef entries in swos2_unit.s: keyDelay, keyIn, lastVBlankServiceTick, Int3Handler, Int5Handler, didSwapThisFrame, int5SerialBusBufferFull
- New xref entries in swos2_unit.s: _InitHardwareAtFrameStart, _SetupInterrupts, _Int2KeyboardHandler, _Int3TimerHandler
- Key porting notes:
  - KBHook (Resload.s) reads CIAA_SDR, returns raw byte in d0; declared as `extern uint8_t KBHook(void)` — d0 return maps naturally to m68k ABI
  - cameraX/cameraY are 32-bit fixed-point; `move.w (cameraX).l,d1` reads high word (integer part) on big-endian 68k; C uses `extern int16_t cameraX` which maps to the same word
  - debugMode LINE-A trap (dc.b 0xA0, 0x00) skipped in C port (dead code in non-debug builds)
  - Unreachable code after `bra.w return_interrupt` in Int3TimerHandler (COPER check) omitted
  - ~120 lines of asm code removed across 4 function bodies + 1 dead code stub
- **Running total: 449 functions ported (443 + 4 exported + 2 kept in asm)**
- Build status: **verified** — clean build, no new unresolved symbols, all symbols correctly placed in map

### Chunk 45 — RNC Decompressor Suite (9 functions → `src/rnc.c`)
- Date: 2026-02-19
- **Scope:** RNC ProPack v1 (Huffman) and v2 (LZ bitstream) decompression engines
- **Exported (2):**
  - `_RncDecompressV1` (from `RncDecompressV1`) → V1 entry with error check: calls v1DecodeBlock, panics on error codes 0/-1/-2
  - `_RncDecompressV2` (from `RncDecompressV2` + `RncDecodeV2Body`) → V2 LZ bitstream decompressor: 8-bit shift register with carry chain, match/literal state machine
- **Static (7):**
  - `v1DecodeBlock` (from `RncDecodeBlock`) → V1 Huffman core: reads RNC header, handles src/dst overlap relocation, initializes 16-bit LE bitstream, main loop (build 3 tables → decode literals+matches per block)
  - `v1ReadU32` (from `RncReadU32`) → reads 4 bytes big-endian
  - `v1GetBits` / `v1SkipBits` (from `RncGetBits`) → extract/consume bits from V1 bitstream
  - `v1Refill` (from `RncRefillBits`) → loads 2 bytes LE when bit buffer underflows
  - `v1DecodeSymbol` (from `RncDecodeSymbol`) → Huffman symbol decode with extra bits
  - `v1BuildTable` (from `RncBuildTable`) → builds Huffman decode table (mask/match + aux) from bitstream
  - `v2Bit` — V2 bit extraction: shift d2 left, refill on zero (addx.b carry chain)
- New files: `include/rnc.h`, `src/rnc.c`
- Assembly stubs in SWOS2.s:
  - `RncDecompressV1:` register-to-stack stub (a0=src, a1=dst → 2 stack params, saves d2-d7/a2-a6)
  - `RncDecompressV2:` register-to-stack stub (a0=src, a1=dst → 2 stack params, saves d2-d7/a2-a6)
  - `RncDecodeBlock:` dead code (`rts`) — only called by RncDecompressV1, now ported
  - `RncDecodeSymbol`, `RncGetBits`, `RncRefillBits`, `RncReadU32`, `RncBuildTable`, `RncDecodeV2Body`: all `rts` (dead code)
- Bridges removed from swos2_unit.s:
  - `_CRncDecompressV1` (7 lines) — replaced by direct `_RncDecompressV1()` calls
  - `_CRncDecompressV2` (7 lines) — replaced by direct `_RncDecompressV2()` calls
- C callers updated:
  - `asset_loader.c`: `_CRncDecompressV1` → `_RncDecompressV1`
  - `file_io.c`: `_CRncDecompressV1` → `_RncDecompressV1`, `_CRncDecompressV2` → `_RncDecompressV2`
- New xref entries in swos2_unit.s: `_RncDecompressV1`, `_RncDecompressV2`
- Removed xdef entries from swos2_unit.s: `_CRncDecompressV1`, `_CRncDecompressV2`
- Key porting notes:
  - V1 bitstream: 32-bit accumulator consumed from LSB, signed byte counter for remaining bits, 2-byte LE refill from compressed stream
  - V1 overlap handling: when dst < end_of_compressed, relocates compressed data above output end, saves/restores overwritten bytes
  - V1 literal re-sync: after copying literal bytes, bit buffer is re-merged with peeked 2 bytes from new source position
  - V2 bitstream: single-byte shift register (d2), MSB shifted into carry via `add.b d2,d2`; on zero (beq), refill loads next byte incorporating carry via `addx.b d2,d2`
  - V2 init skips 2 bits from first byte (matching V1's `RncGetBits(2,2)`)
  - V2 match copy: lsr.w #1 splits length into odd/even; d1==0 triggers RLE mode (repeat single byte)
  - ~475 lines of asm code removed
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 9 (2 exported + 7 static)
- **Running total: 458 functions ported (449 + 9)**

### Chunk 46 — Header Subsystem (9 functions → `src/headers.c`)
- Date: 2026-02-19
- **Scope:** Ball heading mechanics: jump headers, lob headers, flying headers, static headers, animation table setup
- **Exported (6):**
  - `_SetJumpHeaderHitAnimTable` (from `SetJumpHeaderHitAnimTable`) → conditionally sets jump-header-hit animation if HEADING==0, PLAYERDOWNTIMER==40, FRAMESWITCHCOUNTER<=2, tick bit 1 set; halves player speed
  - `_PlayerHittingJumpHeader` (from `PlayerHittingJumpHeader`) → ball contact during jump header: direction delta (mod 8) dispatch to flying/lob header, sets ball destination/speed from kDefaultDestinations + heading skill bonus, halves player speed
  - `_SetStaticHeaderDirection` (from `SetStaticHeaderDirection`) → per-frame aim steering: adjusts player direction ±1 toward input direction during static header attempt
  - `_PlayerHittingStaticHeader` (from `PlayerHittingStaticHeader`) → ball contact during static header: direction adjustment (1-2 steps toward input), ball speed from kStaticHeaderBaseBallSpeed + heading bonus, negates+halves deltaZ
  - `_AttemptStaticHeader` (from `AttemptStaticHeader`) → initiates static header: sets destination, speed, PL_STATIC_HEADING state, 20-frame timer
  - `_PlayerAttemptingJumpHeader` (from `PlayerAttemptingJumpHeader`) → initiates jump header: sets destination, speed, PL_JUMP_HEADING state, 50-frame timer
- **Static (3):**
  - `setPlayerJumpHeaderHitAnimationTable` — sets jumpHeaderHitAnimTable if FRAMESWITCHCOUNTER<=2
  - `doFlyingHeader` — low jump height (0x00020000), ball speed 75%, sets hit anim
  - `doLobHeader` — high jump height (0x00024000), ball speed ~94%, sets hit anim
- New files: `include/headers.h`, `src/headers.c`
- Assembly stubs in SWOS2.s:
  - `SetJumpHeaderHitAnimTable:` register-to-stack (a1 → stack, saves d2-d7/a2-a6)
  - `PlayerHittingJumpHeader:` register-to-stack (a1, a6 → stack, saves d2-d7/a2-a6)
  - `SetStaticHeaderDirection:` register-to-stack (a1, a6 → stack, saves d2-d7/a2-a6)
  - `PlayerHittingStaticHeader:` register-to-stack (a1, a6 → stack, saves d2-d7/a2-a6)
  - `AttemptStaticHeader:` register-to-stack (d0 zero-extended, a1 → stack, saves d2-d7/a2-a6)
  - `PlayerAttemptingJumpHeader:` register-to-stack (d0 zero-extended, a1 → stack, saves d2-d7/a2-a6)
  - `DoFlyingHeader: rts` (dead code, only called from PlayerHittingJumpHeader)
  - `DoLobHeader: rts` (dead code, only called from PlayerHittingJumpHeader)
  - `SetPlayerJumpHeaderHitAnimationTable: rts` (dead code, only called from DoFlyingHeader/DoLobHeader)
- Data labels exported via xdef in SWOS2.s: jumpHeaderHitAnimTable, jumpHeaderAttemptAnimTable, staticHeaderAttemptAnimTable, staticHeaderAimAnimTable, staticHeaderHitAnimTable, kBallJumpHeaderDeltaZ, kDefaultDestinations, kPlayerHeaderSpeedIncrease, kJumpHeaderSpeed, kHeaderLowJumpHeight, kHeaderHighJumpHeight, kStaticHeaderBaseBallSpeed, kStaticHeaderPlayerSpeed
- Inline data preserved: kHeaderLowJumpHeight (0x00020000), kHeaderHighJumpHeight (0x00024000), kStaticHeaderBaseBallSpeed (0x0700), kStaticHeaderPlayerSpeed (0x0100)
- Dead code after PlayerAttemptingJumpHeader rts (lines 17414-17434) removed with stub
- New xref entries in swos2_unit.s: _SetJumpHeaderHitAnimTable, _PlayerHittingJumpHeader, _SetStaticHeaderDirection, _PlayerHittingStaticHeader, _AttemptStaticHeader, _PlayerAttemptingJumpHeader
- Key porting notes:
  - PlayerHittingJumpHeader direction delta dispatch: 0=keep, 1=dec, 2=flying+dec, 3=lob+dec, 4=lob+keep, 5=lob+inc, 6=flying+inc, 7=inc, no-input=flying+keep
  - PlayerHittingStaticHeader uses offset 44 on team (SPRITE_SPEED EQU == TEAMGENERALINFO_CURRENTALLOWEDDIRECTION EQU == 44) — IDA naming collision, semantically it's the input direction
  - Similarly offset 42 on player sprite (TEAMGENERALINFO_ALLOWEDDIRECTIONS EQU == SPRITE_DIRECTION EQU == 42) — same numeric offset, different struct
  - kPlayerHeaderSpeedIncrease[8] is negative-to-zero: lower heading skill → bigger speed penalty
  - AttemptStaticHeader param order: (player, dir) — note D0 is zero-extended in stub
  - PlayerAttemptingJumpHeader param order: (dir, player) — matching original register convention
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 9 (6 exported + 3 static)
- **Running total: 467 functions ported (458 + 9)**

### Chunk 47 — Cards, Fouls & Tackle System (10 functions → `src/tackle_foul.c`)
- Date: 2026-02-19
- **Scope:** Tackle outcome resolution, card issuance, foul/penalty/free-kick state, goalkeeper catch, injury system
- **Exported (10):**
  - `_PlayerTackledTheBallWeak` (from `PlayerTackledTheBallWeak`) → weak tackle: direction adjustment via input, ball speed = 150% of halved player speed, marks good tackle if opponent far
  - `_SetPlayerDowntimeAfterTackle` (from `SetPlayerDowntimeAfterTackle`) → looks up tackling skill from player info, selects human/computer downtime table
  - `_PlayerTackled` (from `PlayerTackled`) → injury resolution: probability roll by game length, severity bucket cascade (7 levels), injury quota decrement, PL_TACKLED state
  - `_TryBookingThePlayer` (from `TryBookingThePlayer`) → yellow card issuance: eligibility check (human controller, injury quota), card state LUT (normal/highlight mode), second yellow→red upgrade with stats
  - `_TrySendingOffThePlayer` (from `TrySendingOffThePlayer`) → straight red card: eligibility check, card state via LUT or forced state 3, stats update
  - `_SendPlayerAway` (from `SendPlayerAway`) → marks player as sent off: clears marked player if applicable, sets SPRITE_CARDS=-1, SPRITE_SENTAWAY=1, destination off-pitch
  - `_PlayerTacklingTestFoul` (from `PlayerTacklingTestFoul`) → foul orchestrator: distance check to victim, goalkeeper exemption, pitch bounds check, PlayerTackled call, tackle quality assessment (direction diff), card probability with nearest-teammate search, dispatches to TryBooking/TrySending + TestFoulForPenalty + ActivateReferee
  - `_GoalkeeperCaughtTheBall` (from `GoalkeeperCaughtTheBall`) → sets catching animation, speed, destination from ballNextGround coords, clamps Y to 137..761
  - `_PlayerTackledTheBallStrong` (from `PlayerTackledTheBallStrong`) → strong tackle: direction adjustment, ball speed = 125% when PLAYERNUMBER flag set (duplicated computation faithfully), good tackle check
  - `_TestFoulForPenaltyAndFreeKick` (from `TestFoulForPenaltyAndFreeKick`) → determines foul type: penalty area check (top/bottom team), 7 free kick quadrants with mirrored assignment, simple foul fallback, sets break/stoppage state
- New files: `include/tackle_foul.h`, `src/tackle_foul.c`
- Assembly stubs in SWOS2.s:
  - `PlayerTackledTheBallWeak:` register-to-stack stub (a1, a6 → stack, saves d2-d7/a2-a6) — 1 asm caller
  - `SetPlayerDowntimeAfterTackle:` register-to-stack stub (a1, a6 → stack) — 1 asm caller
  - `PlayerTackledTheBallStrong:` register-to-stack stub (a1, a6 → stack) — 1 asm caller
  - `GoalkeeperCaughtTheBall:` register-to-stack stub (a1, a6 → stack) — 1 asm caller
  - `PlayerTacklingTestFoul:` register-to-stack stub (a1, a6 → stack) — 1 asm caller
  - `SendPlayerAway:` register-to-stack stub (a1, a6 → stack) — called by _CSendPlayerAway bridge (now removed, referee.c calls _SendPlayerAway directly)
  - `PlayerTackled: rts` (dead code — only caller was PlayerTacklingTestFoul)
  - `TryBookingThePlayer: rts` (dead code — only caller was PlayerTacklingTestFoul)
  - `TrySendingOffThePlayer: rts` (dead code — only caller was PlayerTacklingTestFoul)
  - `TestFoulForPenaltyAndFreeKick: rts` (dead code — only callers were PlayerTacklingTestFoul)
- Bridge removed: `_CSendPlayerAway` from swos2_unit.s (was 7 lines)
- Updated caller: referee.c: `_CSendPlayerAway` → `_SendPlayerAway`
- New xdef entries in swos2_unit.s (18): plgD3Param, cardsDisallowed, forceLeftTeam, nearestTeammatePtr, ballNextGroundX, ballNextYGroundY, cardAnimStatesNoFlag, cardAnimStatesWithFlag, kPlayerTacklingDownTime, kComputerTacklingDownTime, kInjuryLevels, kInjuryLevelAlreadyInjured, kInjuryRecoveryTime, kTackleInjuryProbability, kTackleInjuryProbabilityAlreadyInjured, playerTackledAnimTable, goalieCatchingBallAnimTable, kGoalkeeperCatchSpeed
- New xref entries in swos2_unit.s (10): _PlayerTackledTheBallWeak, _SetPlayerDowntimeAfterTackle, _PlayerTackled, _TryBookingThePlayer, _TrySendingOffThePlayer, _SendPlayerAway, _PlayerTacklingTestFoul, _GoalkeeperCaughtTheBall, _PlayerTackledTheBallStrong, _TestFoulForPenaltyAndFreeKick
- Key porting notes:
  - PlayerTacklingTestFoul uses `cmpa.l #TEAMGENERALINFO_OPPONENTSTEAM,a2` as null check (value 0) — C uses `== (uint8_t *)TGI_OPPONENTSTEAM` (== 0)
  - Injury severity cascade: 7 bucket levels, each subtracts threshold from random roll, adds 0x20 to severity; only upgrades existing injury (never downgrades)
  - PlayerTackledTheBallStrong duplicates speed computation `(speed>>2)+speed` twice — faithfully reproduced (original asm writes SPRITE_SPEED(a2) twice)
  - TestFoulForPenaltyAndFreeKick free kick quadrant assignment is mirrored for bottom team (left→right swap)
  - `forceLeftTeam==1` overrides team to bottomTeamData in apply_break path
  - ~800 lines of asm code removed
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 10 (10 exported)
- **Running total: 477 functions ported (467 + 10)**

### Chunk 48 — Goalkeeper Physics + Penalty System (6 functions → `src/goalkeeper.c`)
- Date: 2026-02-19
- Files: `src/goalkeeper.c`, `include/goalkeeper.h`
- Functions ported:
  - `getFramesNeededToCoverDistance` (static helper) — loop-based division for fixed-point 16.16 distance/speed
  - `ShouldGoalkeeperDive` (A1=keeper, A2=ball, A6=team → D0=0/1) — evaluates penalty vs open play distance gates, compares ball travel frames vs keeper reach frames using skill-based dive deltas
  - `GoalkeeperDeflectedBall` (A2=ball, A6=team) — sets deflected ball destination with random scatter, picks deflect speed (strong/medium/weak) based on goalkeeper skill table offsets $34/$36, adds random Z delta
  - `NextPenalty` (no params) — advances penalty shootout: checks end conditions (10+ attempts, can't-catch-up), toggles teamPlayingUp/teamStarting, picks shooter from rotating index, calls EndOfGame on completion
  - `GoalkeeperClaimedTheBall` (A1=keeper, A2=ball, A6=team) — sets keeper-holds-ball state, camera direction/turn flags, stoppage state; if keeper not diving, gives control to keeper
  - `CheckIfGoalkeeperClaimedTheBall` (no params) — dispatcher: re-calls GoalkeeperClaimedTheBall if ST_KEEPER_HOLDS_BALL, else enters break idle state
- Assembly stubs:
  - Register-to-stack bridges: ShouldGoalkeeperDive (A1,A2,A6→stack, returns D0), GoalkeeperDeflectedBall (A2,A6→stack), GoalkeeperClaimedTheBall (A1,A2,A6→stack), CheckIfGoalkeeperClaimedTheBall (no params, save/restore regs), NextPenalty (no params, save/restore regs)
  - Dead code stub: GetFramesNeededToCoverDistance (rts, all callers ported)
- Bridges removed:
  - `_CNextPenalty` (match_setup.c now calls `_NextPenalty` directly)
  - `_CCheckIfGoalkeeperClaimedTheBall` (bench_subs.c now calls `_CheckIfGoalkeeperClaimedTheBall` directly)
- xdef additions: penaltiesTimer, penaltyShooterSprite, ballDefensiveX, goalkeeperDiveAttemptCount, kKeeperPenaltySaveDistanceFar, kKeeperPenaltySaveDistanceNear, kKeeperSaveDistance, kGoalkeeperDiveDeltas, kGoalkeeperDeflectDeltaZ, kGoalkeeperStrongDeflectBallSpeed, kGoalkeeperMediumDeflectBallSpeed, kGoalkeeperWeakDeflectBallSpeed
- Notes:
  - GoalkeeperDeflectedBall direction condition: bottom team = index 0 (kick up), top team = index 4 (kick down)
  - ShouldGoalkeeperDive skill index: GOALIESKILL_INDEX (offset 10) + (tick & 0x3C) >> 1 as byte offset into shot chance table
  - NextPenalty uses bottomTeamData (set via lea) as the reference team for all setup
  - GoalkeeperClaimedTheBall can modify team parameter via forceLeftTeam override (was why asm saved a6 in CheckIfGoalkeeperClaimedTheBall)
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 6 (5 exported + 1 static)
- **Running total: 483 functions ported (477 + 6)**

### Chunk 49 — Game Timers + Ball Out of Play (2+7s functions → `src/game_timers.c`)
- Date: 2026-02-19
- Files: `src/game_timers.c`, `include/game_timers.h`
- Functions ported:
  - `_UpdateGameTimersAndCameraBreakMode` (exported) — main per-frame timer dispatcher: penalty timer, in-game counter, stoppage timers, fire-press end-game detection, break camera mode state machine (modes 0-8)
  - `_CheckIfBallOutOfPlay` (exported) — ball boundary detection: goal scoring (with goal counter bias system), corner kicks, goal kicks, throw-ins with zone-based direction selection
  - `negateTimers` (static) — negates resultTimer, timeVar, statsTimer
  - `handleEndGameTransmission` (static) — end-game state transitions after fire press
  - `checkFirePressedForEndGame` (static) — checks fire button for end-game states
  - `handleStoppedState` (static) — dispatches by gameState when stopped
  - `handleBreakCameraMode` (static) — break camera mode state machine (modes 0-8)
  - `writeRestartGlobalsAndStopPlayers` (static) — common restart cleanup (foul coords, camera dir, stop all players)
  - `handleGoalScored` (static) — goal detection, scorer attribution, replay camera setup, goal counter bias
- Assembly stubs:
  - UpdateGameTimersAndCameraBreakMode: save d2-d7/a2-a6, jsr _UpdateGameTimersAndCameraBreakMode, restore, rts. Inline data labels (ballOutOfGameTimer, stoppageAuxFlag) preserved after stub.
  - CheckIfBallOutOfPlay: jmp _CheckIfBallOutOfPlay (no register bridge needed — called only from C)
- Bridges removed:
  - `_CCheckIfBallOutOfPlay` in swos2_unit.s (ball_physics.c now calls `_CheckIfBallOutOfPlay` directly)
- xdef additions: ballOutOfGameTimer, stoppageAuxFlag, statsTeam1GoalsCopy2, statsTeam2GoalsCopy2, teamNumThatScored, aboutToThrowInAnimTable, initialKickWriteOnlyTicks, deadCameraBreakVar01-04
- Notes:
  - GCC ICE workaround: game_timers.c compiled without LTO (-flto/-fwhole-program stripped) due to m68k-amiga-elf-gcc segfault in GIMPLE CCP pass. Added per-file Makefile rule using $(if $(filter ...)) pattern.
  - All static helpers marked `__attribute__((noinline))` to prevent LTO from re-inlining (safety measure).
  - handleGoalScored: opponent team (from TGI_OPPONENTSTEAM) passed to writeRestartGlobalsAndStopPlayers, not the scoring team. Camera direction d3/d4 set from opponent identity (topTeamData→4/124, else→0/199).
  - Goal counter bias system: scoring to equalize=200+rand, first lead=300+rand, extending lead=200+rand, during penalties=just rand/2+100.
- Build status: **verified** — clean build from scratch, no new warnings
- Functions ported this chunk: 9 (2 exported + 7 static)
- **Running total: 492 functions ported (483 + 9)**

### Chunk 50 — Ball Possession + Pass/Kick (7 functions → `src/ball_possession.c`)
- Date: 2026-02-19
- Files: `src/ball_possession.c`, `include/ball_possession.h`
- Functions ported:
  - `_DoPass` (exported) — passing the ball: finds target player via GetClosestNonControlledPlayerInDirection, sets ball destination and speed based on distance, handles AI failed pass chance (botched passes), applies passing skill speed bonus
  - `_PlayerKickingBall` (exported) — kicking/shooting: sets ball dest from direction table, applies kicking deltaZ, checks shot-on-goal for finishing/shooting skill bonus, handles spin timer and kick sample
  - `_CalculateIfPlayerWinsBall` (exported) — ball possession contest: resolves 50/50 challenges via tackling+ballcontrol skill comparison with random element, or updates ball position when player has control (speed, angle boost, direction change detection)
  - `_UpdatePlayerSpeed` (exported) — sets player speed from skill tables, applies run-slower penalty (37.5%), injury handicap, ball possession drag (12.5%), pass-to-target speed adjustment, game state speed reductions, calculates frame delay
  - `_UpdatePlayerShotChanceTable` (exported) — sets team's shot chance table based on goalkeeper skill index or outfield player default table
  - `_GetBallDestCoordinatesTable` (exported) — returns pointer to appropriate ball destination delta table based on game state (throw-in/penalty/corner/default)
  - `_GetClosestNonControlledPlayerInDirection` (exported) — finds closest teammate (not controlled, not sent away, PL_NORMAL) facing approximately given direction (within ±16/256)
- Assembly stubs:
  - DoPass, PlayerKickingBall, UpdatePlayerSpeed, UpdatePlayerShotChanceTable: save d2-d7/a2-a6, push a1/a6 to stack, jsr C function, restore, rts
  - CalculateIfPlayerWinsBall: same pattern plus andi.l #$FFFF,d0 for zero-extending dir parameter
  - GetBallDestCoordinatesTable, GetClosestNonControlledPlayerInDirection: dead `rts` (only callers were DoPass/PlayerKickingBall, now ported to C)
- xdef additions: kBallSpeedPassingIncrease, kPassingSpeedCloserThan2500..kPassingSpeedFurtherThan122500, kFreePassReleasingBallSpeed, kPlayerSpeedsGameInProgress, kPlayerSpeedsGameStopped, kAIFailedPassChance, kBallSpeedDeltaWhenControlled, ballControlTurnThresholds, plAvgTacklingBallControlDiffChance, kBallSpeedKicking, kBallSpeedFinishing, kGoalieSkillTables, teamPlOfs24Table, penaltyGoalieSkillTable, kLeftThrowInBallDestDelta, kRightThrowInBallDestDelta, kPenaltyBallDestDelta, kUpperLeft/RightCornerBallDestDelta, kLowerLeft/RightCornerBallDestDelta, kInjuriesSpeedHandicap, kBallKickingDeltaZ, kBallKickingSpeed
- Notes:
  - CalculateIfPlayerWinsBall has a cross-team GetPlayerPointerFromShirtNumber call (first call: a1=opponent's player, a6=our team) — this is an original game bug, faithfully reproduced
  - Angle boost in CalculateIfPlayerWinsBall applies for diff in [64,192] inclusive (signed byte comparisons: bge #64 / bgt #192)
  - DoPass finalize: asm `move.w #1,$80(a6)` = TGI_PASSINPROGRESS (hex $80=128), not TGI_GOALKEEPERDIVINGRIGHT (decimal 80)
- Build status: **verified** — clean build, no new warnings
- Functions ported this chunk: 7
- **Running total: 499 functions ported (492 + 7)**

### Chunk 51 — Sprite Draw Pipeline (12 functions → `src/sprites.c`)
- Date: 2026-02-19
- Files: `src/sprites.c`, `include/sprites.h`
- Functions ported:
  - `_DrawSpriteClipped` (exported) — main clipped sprite blitter: clips to screen bounds, applies camera offset wrapping, programs Amiga blitter for 4-plane cookie-cut or straight copy, handles circular framebuffer Y-wrap with tail blit, manages blit queue entries (mask+fb+modulo+bltsize). Two code paths: normal (4 separate plane blits) and branch_linked_planes (single interleaved blit).
  - `_DrawSprite_entry2` (exported) — wrapper: saves/restores spriteBlitQueue around DrawSpriteClipped call, appends queue terminator
  - `_DrawSprite16Pixels` (exported) — resolves sprite by index from spriteGraphicsPtr table, calls DrawSprite_entry2
  - `_DrawSpriteCentered` (exported) — like DrawSprite16Pixels but adjusts x,y by sprite center offsets before drawing
  - `_DrawSpriteInCameraView` (exported) — camera-relative sprite draw with full bounds checking (cull if off-screen)
  - `_DrawSpriteHotspotInCameraView` (exported) — like DrawSpriteInCameraView but adjusts by hotspot (center) first
  - `_DarkenRectangle` (exported) — resolves sprite by index, calls DrawSpriteClipped (uses branch_linked_planes path for darken overlay)
  - `_DrawQueuedSprites` (exported) — pops sprites from main render stack, dispatches to QueueMaskedSpriteBlitClipped (players), DrawSprite_entry2 (save-sprite flag set), or DrawSpriteClipped (normal). Calls SaveCoordinatesForHighlights for highlight recording.
  - `_RenderSpriteBandsToCopperLists` (exported) — processes plane01 and plane23 render stacks, renders sprite band entries into copper list buffers via blitter A→D copy. Programs VPOS/HPOS/CTL words for hardware sprite positioning.
  - `renderSpriteBandPlanePair` (static) — helper for RenderSpriteBandsToCopperLists: writes copper list sprite control words and blits sprite data into copper list plane pair buffers
  - `_BuildSpriteBandAndGeneralRenderStacks` (exported) — iterates sorted sprites, culls off-screen, distributes visible sprites into plane01 band, plane23 band, or general render stack based on occlusion buckets and Y-band tracking. Updates sprite ONSCREEN and FIELD_56 flags.
  - `_DrawSprites` (exported) — top-level: SortSprites → BuildSpriteBandAndGeneralRenderStacks → DrawQueuedSprites → RenderSpriteBandsToCopperLists
- Assembly stubs:
  - DrawSpriteClipped, DrawSprite_entry2, DrawSprite16Pixels, DrawSpriteCentered: save d2-d7/a2-a6, push register args to stack, jsr C function, restore, rts
  - DrawSpriteInCameraView, DrawSpriteHotspotInCameraView, DarkenRectangle: same pattern with uint16_t spriteIndex + int16_t x,y
  - DrawQueuedSprites, RenderSpriteBandsToCopperLists, BuildSpriteBandAndGeneralRenderStacks, DrawSprites: save d2-d7/a2-a6, jsr C, restore, rts
  - RenderSpriteBandPlanePair: dead `rts` (only called from C now)
  - blitter_begin: dead `rts` (CopySprite already ported to C)
- New C-to-asm bridge:
  - `_CQueueMaskedSpriteBlitClipped` in swos2_unit.s: takes (sprGfx, sprStruct, x, y) from stack, loads into a0/a4/d1/d2, calls asm QueueMaskedSpriteBlitClipped
- Bridge optimizations:
  - `_CDrawSprite16Pixels`, `_CDrawSpriteInCameraView`, `_CDrawSpriteHotspotInCameraView`, `_CDarkenRectangle`: simplified to `jmp _CFuncName` (direct C-to-C forwarding, no register shuffle needed)
  - `_CDrawSprites`: updated to call `_DrawSprites` directly
- Symbols exported: renderStackPtr, renderStackBase, plane01RenderStackPtr, plane01RenderStackBase, plane23RenderStackPtr, plane23RenderStackBase, bottomCornerLeftPositions
- Dead code removed: blitter_begin body (~140 lines), DrawSpriteClipped body (~400 lines), BuildSpriteBandAndGeneralRenderStacks body + local data (~180 lines)
- Build status: **verified** — clean build, no new errors

### Chunk 52 — AI Helper Functions (4 functions → `src/ai.c`)
- Date: 2026-02-19
- Files: `src/ai.c`, `include/ai.h`
- Functions ported:
  - `_AI_SetDirectionTowardOpponentsGoal` — sets team's allowed direction toward opposing goal based on ball X position and which half the AI attacks. Three X zones (< 300, 300-371, > 371) map to 3 direction values.
  - `_AI_Kick` — AI kick decision. Checks opponent's controlled player facing direction vs our player direction. Triggers kick (fire + direction flags) when angle difference exceeds ±32 (more than 1 direction step apart).
  - `_AI_DecideWhetherToTriggerFire` — checks if AI player should shoot: validates facing toward opponent's goal (directions 3/4/5 for top team, 7/0/1 for bottom), ball proximity (≤648), ball height window ([8..14] ascending, [12..20] descending). Reverse-direction computation using byte-level +128 and >>5 conversion.
  - `_FindClosestPlayerToBallFacing` — scans all 22 players (both teams) for closest to ball facing within ±16 of target full direction. Returns Sprite* (or -1) and stores ball distance in `findClosestBallDist` global for asm stub d2 return.
- Not ported (left in asm):
  - `AI_ResumeGameDelay` — only 3 instructions, returns carry flag which is impractical to bridge to C.
- Assembly stubs:
  - AI_SetDirectionTowardOpponentsGoal: push a6, jsr, pop, rts
  - AI_Kick: push a6/a1, jsr, pop, rts
  - AI_DecideWhetherToTriggerFire: save d2-d7/a2-a6, push d7/a6/a5, jsr, pop, restore, tst.w d0, rts (preserves zero flag for caller)
  - FindClosestPlayerToBallFacing: push a6/d0, jsr, pop, movea.l d0,a0, load findClosestBallDist→d2, rts
- New globals exported from asm: aiCounter, aiAttackHalf, aiCounterWriteOnly, deadVarAlways0, playerKickerPtr
- New C global: findClosestBallDist (int32_t, used for multi-register return from FindClosestPlayerToBallFacing)
- Build status: **verified** — clean build, no new errors
- Functions ported this chunk: 12
- **Running total: 511 functions ported (499 + 12)**

### Chunk 53 — Ball Prediction + Player Actions (6 functions → `src/ball_physics.c`, `src/goalkeeper.c`, `src/tackle_foul.c`, `src/player_positioning.c`)
- **Scope:** Ball trajectory prediction, aftertouch/spin system, goalkeeper diving, player tackling, and tactical positioning
- **Exported (6):**
  - `_UpdateBallVariables` (ball_physics.c) — Integrates ball trajectory to predict crossing of goalkeeper Y and X positions. Samples at fixed Y windows (135/129 top, 763/769 bottom) for defensive/strike/notHigh variants.
  - `_CalculateBallNextGroundXYPositions` (ball_physics.c) — Predicts landing position by trying decreasing Z subtraction steps (20→13), integrating until Z goes negative.
  - `_ApplyBallAfterTouch` (ball_physics.c) — Aftertouch/spin system with kick vs pass paths. Spin from direction delta, applies kKickSpinFactor/kPassingSpinFactor tables. High kick detection at spinTimer==4. Timer increments to 10 then resets to 0xFFFF.
  - `_GoalkeeperJumping` (goalkeeper.c) — Sets dive speed (near/far/random), low/high dive animation based on ballNotHighZ, destination from kDefaultDestinations.
  - `_PlayerBeginTackling` (tackle_foul.c) — Sets tackle state, animation, direction, speed. Dead code: fasterTackle check always overwritten with 0xFF.
  - `_SetPlayerWithNoBallDestination` (player_positioning.c, NEW) — Looks up team tactics table, gets position from quadrant-based positioning (35 entries per player × ballQuadrantIndex). Goalkeeper uses proportional mapping.
- Assembly stubs: All 6 functions replaced with register-to-stack bridge stubs in SWOS2.s
- New xdef symbols (26): ballDefensiveY/Z, ballNotHighX/Y/Z, strikeDestY/Z, ballNextZDeadVar, kHighKickDeltaZ, kHighKickBallSpeed, kNormalKickDeltaZ, kNormalKickBallSpeed, kKickSpinFactor, kPassingSpinFactor, kSpinMultiplierFactor, kGoalkeeperNearJumpSpeed, kGoalkeeperFarJumpSpeed, kGoalkeeperFarJumpSlowerSpeed, 4 goalie animation tables, plTacklingAnimTable, kPlayerTacklingSpeed, playerXQuadrantsCoordinates, playerYQuadrantCoordinates
- Bug fix: IDA address-as-constant in UpdateBall (frontbufferBaseAlt+$1CAA → literal 0x00010002)
- Build status: **verified** — clean build, no new errors
- Functions ported this chunk: 6
- **Running total: 521 functions ported (515 + 6)**

### Chunk 54 — AI Main Controller (1 function → `src/ai.c`)
- **Scope:** AI_SetControlsDirection — the main AI controller (~827 lines of asm), drives all computer-team decision-making
- **Exported (1):**
  - `_AI_SetControlsDirection` (ai.c) — Main AI controller for computer-controlled teams. Handles all game states: result display fire simulation, keeper/goal/throw-in/foul/penalty direction, in-play ball chasing, fire decisions based on player proximity, aftertouch spin tables, corner spin randomization.
- Assembly stub: Register-to-stack bridge (movem.l d2-d7/a2-a6, push a6, jsr, pop, movem.l restore)
- New xdef symbols (11): aiTurnDirection, aiResumePlayTimer, aiRand, aiRandomRotateTable, aiLeftSpinTable, aiRotateRightTable, aiLongKickTable, aiThrowInDirections, AI_maxStoppageTime, team1Computer, team2Computer
- Key implementation notes:
  - AI_ResumeGameDelay carry flag inlined: `cmpi.w #13,passKickTimer` → `if (passKickTimer >= 13) return`
  - Byte-level arithmetic: `addi.b #128,d0` → `dir = (uint8_t)((uint8_t)dir + 128)` (180° flip)
  - Uninitialized a2 on foul path: C initializes opponentControlled to NULL with guard check
  - Dead code after unconditional branches identified and omitted
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 1
- **Running total: 522 functions ported (521 + 1)**

### Chunk 55 — UpdatePlayers (1 function → `src/update_players.c`)
- **Scope:** UpdatePlayers — the main per-frame player update loop (~2,690 lines of asm, ~2,700 lines of C)
- **Exported (1):**
  - `_UpdatePlayers` (update_players.c) — Per-frame update for all 11 players on a team. Dispatches by player state: goalkeeper AI/diving/catching, controlled player direction/fire/passing/kicking, throw-in mechanics, tackle state, heading (jump/static), injury/recovery, pass receiver following, idle player positioning, booking animations, substitution flow, post-match reactions.
- Assembly stub: Register-to-stack bridge in SWOS2.s (movem.l d2-d7/a2-a6, push a6, jsr _UpdatePlayers, pop, restore)
- _CUpdatePlayers bridge in swos2_unit.s retired — team_control.c now calls _UpdatePlayers directly
- New xdef symbols (39): throwInPassAnimTable, throwInKickAnimTable, plGettingYellowCardAnimTable, plGettingRedCardAnimTable, plGetting2ndYellowCardAnimTable, playerLosingReactionAnimTable, playerWinningReactionAnimTable, kGoalkeeperSpeedWhenGameStopped, kGoalkeeperGameSpeed, kGoalkeeperBurstSpeed, kGoalkeeperMoveToBallSpeed, kGoalKeeperRecoveryStepBase, kGoalKeeperRecoveryStepAlt, kGoalKeeperRecoveryStepFar, kShotAtGoalMinumumSpeed, kPlayerGroundConstant, kPlayerAirConstant, kInjuryForeverSpeed, kSubstitutedPlayerSpeed, kDefaultDestinations, kBallFriction, ballInUpperPenaltyArea, ballInLowerPenaltyArea, ballSpeed, ballSpeedAfterSave, bottomBallOutOfPlayPositions, topBallOutOfPlayPositions, goalScoredChances, kKeeperEaseBallTable, disallowedTurnFlagsCounter, deadThrowInDirectionVar, throwInPassOrKick, turnDirCopiedFromCameraCount, cameraSnapToPlayerCount, playerRunningAnimTable, plInjuredAnimTable, goalkeeperSkipFlag, _UpdatePlayers (xref)
- Compiled without LTO (CCFLAGS_NOLTO) — added update_players.o to Makefile filter
- Key implementation notes:
  - goto-based state machine with 3 common exit points (snap_dest, finalize, next_player)
  - Local static data moved from asm inline labels: prevLastPlayer, prevLastTeamPlayed, freeKickFactorsX_data, freeKickDestX_local
  - deadVarAlways0 and playerKickerPtr kept as asm data labels (referenced from ai.c)
  - Removed incorrect local #define constants — uses game_types.h authoritative values
  - Cross-struct offset translations from IDA misannotations identified in previous session
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 1
- **Running total: 523 functions ported (522 + 1)**

### Chunk 56 — Scroll Blitter System (2 functions → `src/scroll_blit.c`)
- Date: 2026-02-20
- **Scope:** Scroll blitter system — updates frontbuffer when camera scrolls by blitting pitch tile columns (X) and rows (Y)
- **Exported (2):**
  - `_ScrollXUpdateColumnsAndBlit` (scroll_blit.c) — Compares cameraX vs oldCameraX; if tile boundary crossed, blits column of pitch tiles into frontbuffer (partial first tile, full tiles to SCREEN_HEIGHT, wraps to top, full tiles to cameraYMod320, partial last tile), then copies column from frontbuffer to mask buffer, ends with 1-word dummy blit
  - `_ScrollYUpdateRowsAndBlit` (scroll_blit.c) — Compares cameraY vs oldCameraY; CPU-based row copy from pitch tile pointers into 4-plane interleaved frontbuffer with Y-wrap handling, then blitter-based mask copy in batches (max 0x400 rows per batch), with two-stream mask blit when Y-wrap occurs
- New files: `include/scroll_blit.h`, `src/scroll_blit.c`
- Assembly stubs in SWOS2.s:
  - `ScrollXUpdateColumnsAndBlit: jmp _ScrollXUpdateColumnsAndBlit`
  - `ScrollYUpdateRowsAndBlit: jmp _ScrollYUpdateRowsAndBlit`
  - Inline data labels removed (blitWordCount, maskBlitDestPtr, maskBlitPendingBytesA, maskBlitSecondStreamPtr, maskBlitPendingBytesB) — now static C variables
- Bridges removed from swos2_unit.s:
  - `_CScrollXUpdateColumnsAndBlit` (6 lines: movem save/restore + jsr)
  - `_CScrollYUpdateRowsAndBlit` (6 lines: movem save/restore + jsr)
- Caller updated: camera.c — `_CScrollXUpdateColumnsAndBlit()` → `_ScrollXUpdateColumnsAndBlit()`, added `#include "scroll_blit.h"`
- xref additions in swos2_unit.s: `_ScrollXUpdateColumnsAndBlit`, `_ScrollYUpdateRowsAndBlit`
- ~365 lines of asm code removed (lines 5504-5884)
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 2
- **Running total: 525 functions ported (523 + 2)**

### Chunk 57 — Player Status & Career Team Utilities (16 functions → `src/career_status.c`)
- Date: 2026-02-20
- **Scope:** Player injury/card/status-bucket decay, team initialization for match, availability counting
- **Exported (12):**
  - `_InitInGameTeams_PlayerControlled` — Decays status buckets and cycles injury/card flags for both teams
  - `_InitInGameTeams_WithMatchParam` — Same but loads mode from diyCompetitionStart+60 (also handles Alt variant)
  - `_BuildCareerTeamUnavailablePlayerList` — Builds list (max 4) of injured career team players
  - `_ClearCareerMatchPlayerStatusBucket` — Clears status bucket bits (5-7) for all career players
  - `_ShouldApplyRandomPlayerStatusAfterSim` — Career/national tournament check for random status
  - `_IsPlayerInjured` — Check injury/card/bucket bits with mode (-1/0/1)
  - `_GetNumberOfSubstitutes` — Count goalkeeper-position players
  - `_GetNumberOfSubstitutesInSelectionList` — Count goalkeepers in selection list (indices 11+)
  - `_GetNumberOfAvailablePlayers` — Count available players, returns packed (disabled<<16)|available
  - `_GetNumberOfAvailableSubstitutes` — Count available goalkeepers, same packed return
  - `_GetNumberOfAvailableStartingPlayers` — Count available non-goalkeepers, same packed return
  - `_GetAgeAdjustedPlayerFormFactor` — Age/form calculation with divisor table lookup
- **Static helpers (3):** maybeDecayPlayerStatusBucket, maybeDecayPlayerStatusBucketForTeam, cyclePlayerStatusFlagsForTeam
- **Dead-code stubs (3):** CyclePlayerStatusFlagsForTeam, MaybeDecayPlayerStatusBucketForTeam, MaybeDecayPlayerStatusBucket → `rts` (only called from now-C InitInGameTeams_*)
- **Dead unlabeled variants removed (3):** btst #1,$66(a6) variants after GetNumberOfAvailableStartingPlayers
- New files: `include/career_status.h`, `src/career_status.c`
- New structs in game_types.h: `PlayerFile` (38 bytes), `TeamFileEx` (684 bytes)
- Assembly stubs: register-to-stack bridges for all 12 exported functions + rts for 3 dead-code
- xdef additions: careerTeam, currentMatchPlayers, careerNumPlayers, currentTeam, playerPositions, numPlayers, isNationalSelector, careerUnavailablePlayerList, careerUnavailablePlayerLookupTable, playerFormModifiers, diyCompetitionStart, IsMajorTournamentAllowingRandomStatus
- xref additions: all 12 C function symbols
- ~400+ lines of asm code removed
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 16 (12 exported + 3 static + 1 shared alias)
- **Running total: 541 functions ported (525 + 16)**

### Chunk 58 — Match Simulation System (10 functions → `src/match_sim.c`)
- Date: 2026-02-20
- **Scope:** "View result" match simulation — calculates simulated goals, assigns to scorers, applies player status changes
- **Exported (6):**
  - `_CalculateViewResult` — Main entry: computes simulated match result using skill indices, strength diff, tactic-based probability tables
  - `_CalcTeamViewOffset` — Computes X/Y offset bias from team's global number, season, random phase; returns packed uint32_t
  - `_ComputeOldPlayerSkillForViewResult` — Calculates weighted average skill for team, accounting for goalkeeper; writes to oldPlayerSkill
  - `_InitPlayerRemapTableForTeam` — Assigns random status buckets to players based on availability
  - `_ExtraTimeSecondPhaseScoring` — Second Y-offset scoring phase for extra time (separate entry from GetCachedOrCalculateExtraTimeResult)
  - `_PenaltyShootoutScorerPair` — Picks scorer pair for penalty shootout (separate entry from GetCachedOrCalculatePenaltyShootoutResult)
- **Static helpers (4):** ScaleViewIndexByTeamPriceRank, AssignSimulatedGoalsToScorers, ApplyRandomPlayerStatusAfterSim, BuildAvailablePlayersRemapAndMask
- **Dead-code stubs (2):** BuildAvailablePlayersRemapAndMask → `rts`, ScaleViewIndexByTeamPriceRank → `rts` (only called from CalculateViewResult)
- **Code-as-data preservation:** Dead code at loc_122D6A kept — bytes read as goalkeeper (role 0) row by GetPlayerPrice
- **C-callable asm bridges (3):** `_CCalcTeamPriceRankPercentOffset`, `_CUpdatePlayerPositionAndReturnRole`, `_CCareer_ApplyPostMatchPlayerStatModsIfCareerTeam`
- New files: `include/match_sim.h`, `src/match_sim.c`
- xdef additions: inEditTactics, team1/2GoalCounterByPlayer, oldPlayerSkill, playerPositionsArray, viewRandomPhase, simGoalsByStrengthDiffTable, simHighGoalFallbackValues, simGoalProbByTacticPtrs, simExtraGoalProbByTacticPtrs, simGoalsByIndexDiffTable, simExtraGoalThresholdsA/B, simGoalScorerPairTable, goalScorerDistributionByBallOutTactic, kUserTeamOffsetBiasLUT, kTeamViewOffsetBaseLUT, availablePickCountByGoalsInTable, preferredTacticsSlotsByRole, candidatePlayersByGoalsInTable, playerStatusBucketThresholds, + 3 bridge xdefs
- xref additions: 6 C function symbols
- Bug fixed: ComputeOldPlayerSkillForViewResult goalkeeper lookup used `team + (-0x20)` (asm adjacency hack) — fixed to `currentMatchPlayers`
- ~550 lines of asm code removed/replaced
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 10 (6 exported + 4 static)
- **Running total: 551 functions ported (541 + 10)**

### Chunk 59 — Career Finance, Chairman Warnings, Board Confidence & Reputation (27 functions → `src/career_status.c`)
- Date: 2026-02-20
- **Scope:** Post-match financial calculations, chairman warning state machine, board confidence scoring, career reputation bonuses, season financial initialization
- **Exported (18):**
  - `_ClampNewBalanceAndUpdateClubStatus` — Clamp balance to 100M, set clubBusinessActive, manage overdraftStreak
  - `_ClearOverdraftStateIfRecovered` — Clear warning if balance positive
  - `_IsMajorInternationalTournament` — Check current/secondary comp against 9 tournament symbols each, returns d0 + out-param context pointer
  - `_IsMajorTournamentAllowingRandomStatus` — Wraps above, checks offset 0x44
  - `_IsMajorTournamentAllowingSquadChanges` — Wraps above, checks offset 0x46
  - `_IsTournamentAllowingSquadChangesIfNotCareer` — Career guard for above
  - `_ShouldSetupPlayers` — Career+national+squad check
  - `_UpdateCareerFinancesAfterMatch` — Dispatches by competition type, calculates gate receipts
  - `_MaybeSackManagerAfterSeasonReview` — Random sacking based on seasonReview level
  - `_UpdateCareerChairmanWarningsAfterMatch` — Performance/overdraft warning state machines
  - `_UpdateCareerAfterMatch` — Orchestrator calling multiple sub-functions
  - `_ApplyWeeklyFinancialDeductions` — Weekly wages/running costs deduction
  - `_InitializeCareerSeasonFinancials` — Season init: team value, running costs, clear accumulators
  - `_GetBoardConfidenceScore` — Sum of delta + 3 history slots + base score
  - `_UpdateBoardConfidenceScore` — Add trophy/cup bonuses, clamp to 200
  - `_PushBoardConfidenceHistory` — Shift history slots, calc new delta
  - `_UpdateBoardConfidenceModifiers` — Check qualifying flags, tournament winners, set trophy bonus flags
  - `_UpdateCareerReputationBonuses` — National reputation delta, club reputation from league rank + trophies
  - `_InitializeStartingBalanceForNewSeason` — Starting balance from team value + trophy bonuses
- **Static helpers (8):** setOverdraftWarningCooldown, setPerformanceWarningCooldown, calcLeagueMatchGateReceiptsFactor, calcCupMatchGateReceiptsFactor, calcBoardConfidenceDeltaPercent, addReputationFromBonusValueDiv4, addReputationFromBonusValueDiv8, getExpectedLeagueRankIndex
- **Data tables moved to C static const (4):** leagueMatchReceiptsMultiplierTable, cupMatchReceiptsMultiplierTable, seasonReviewConfidenceDeltaTable, nationalSelectorSeasonReviewReputationDeltaTable
- **Dead-code stubs (8):** SetOverdraftWarningCooldown, SetPerformanceWarningCooldown → `rts`; IsMajorInternationalTournament → `rts`; CalcBoardConfidenceDeltaPercent → `rts`; AddReputationFromBonusValueDiv4/Div8 → `rts`; GetExpectedLeagueRankIndex → `rts`
- **Condition-flag stubs (4):** ShouldSetupPlayers, IsMajorTournamentAllowingRandomStatus, IsMajorTournamentAllowingSquadChanges, IsTournamentAllowingSquadChangesIfNotCareer → `jsr _C; tst.w d0; rts`
- **C-callable asm bridges (9):** `_CVerifyTeamLoaded`, `_CGetTeamValueForGateReceipts`, `_CGetTeamValueBase`, `_CFindTeamInDIY`, `_CGetMoreExpensiveTeamsCount`, `_CShowChairmanMessage`, `_CAddCareerTeamPlayer`, `_CCheckPlayersLeavingComing`, `_CTeamSelectionKeyCache_GetCurrentListPtr`
- IDA bug fix: L28446 `lea ($2DAD0).l,a0` → should reference `diyFileBufferCopy` (fixed in C port)
- Updated `_ShouldApplyRandomPlayerStatusAfterSim` to call C `_IsMajorTournamentAllowingRandomStatus` directly
- Fixed `PanicHaltLoop` → `_PanicHaltLoop` (correct underscore convention)
- ~100 xdef entries added for globals, ~18 xref entries for C functions
- ~750 lines of asm code removed/replaced
- Build status: **verified** — clean build, only pre-existing warnings
- Functions ported this chunk: 27 (18 exported + 8 static + 1 already-ported updated)
- **Running total: 578 functions ported (551 + 27)**

### Chunk 60 — DIY Competition Core, Player Pricing & Tournament Progression (16 functions → `src/diy_competition.c`)
- Date: 2026-02-20
- **Scope:** DIY competition/tournament management, player pricing utilities, team shuffling, league table sorting, bracket progression
- **Exported (16):**
  - `_GetAveragePlayerPrice` — Average price of 16 players in TeamFileEx, returns uint16_t
  - `_GetLowestAndHighestPlayerPrice` — Min/max price in career team via currentMatchPlayers remap, returns packed (lowest<<16)|highest
  - `_CountNearnessFactor` — Multiply price by nearness ratio using quadrant distance tables (6 register params)
  - `_DIY_ShuffleTeams` — Fisher-Yates shuffle of team indices in competition slot
  - `_DIY_SortAndShuffleTeamsByValue` — Bubble sort by descending avg player price, then shuffle within groups
  - `_DIY_ShuffleLeagueTeamOrder` — Shuffle with synchronized indices and team offsets
  - `_InitDIYTournament` — Initialize tournament data from 20-entry init table (6 slots × 0x76 stride)
  - `_DIY_InitCompetitionDefaults` — Initialize league defaults, clear 64 league table stat entries, init shuffle indices
  - `_DIY_ResetCompetitionProgressAndShuffle` — Reseed random, reset progress, clear stats, shuffle league order
  - `_DIYTournament_RecalculateMatchesPerTeam` — Recalculate match counts per slot (cup formula vs league default), rebuild progression
  - `_DIY_CacheSelectedTeamPointers` — Build pointer cache: store teamPtr offsets relative to &selTeamsPtr
  - `_DIY_CountUnselectedTeams` — Count teams without bit 1 set, stores in comp+0x32E, returns count + Z flag
  - `_DIY_SortLeagueTableByStandings` — Multi-criteria bubble sort: points > goal diff > goals scored > alphabetical
  - `_DIY_ComputeAverageTeamStrengthTier` — Sum skill bytes across teams, compute avg and tier (clamped 0-7), returns packed (avg<<16)|tier
  - `_DIY_InitCupBracketProgressionTable` — Build bracket progression table with month interpolation
  - `_DIYTournament_BuildMatchesPerTeamProgressionTable` — Build per-slot matches progression with month interpolation
- **Static helpers (2):** clearLeagueTableStats (clear 7 stat words for 64 entries × 0x12 stride), initTeamShuffleAndOffsets (init 64 indices + 0x12 offsets)
- **Static data (1):** diyTournamentInitTable (20 uint16_t entries — team counts, cup flags, groups, teams-per-group for 6 slots)
- **Assembly stubs in SWOS2.s:**
  - Simple `jmp`: DIY_ShuffleLeagueTeamOrder, InitDIYTournament, DIY_InitCompetitionDefaults, DIY_ResetCompetitionProgressAndShuffle, DIYTournament_RecalculateMatchesPerTeam, DIY_CacheSelectedTeamPointers, DIY_SortLeagueTableByStandings, DIY_InitCupBracketProgressionTable, DIYTournament_BuildMatchesPerTeamProgressionTable
  - Register bridges: GetAveragePlayerPrice (a0→stack, d0 return), GetLowestAndHighestPlayerPrice (unpack d0→d4/d5), CountNearnessFactor (d3,d6,d7,a2,a3,a4→stack, d0→d7), DIY_ShuffleTeams/DIY_SortAndShuffleTeamsByValue (a2,a3→stack), DIY_ComputeAverageTeamStrengthTier (d0,a0→stack, unpack d0→d0/d1)
  - Z-flag stub: DIY_CountUnselectedTeams (tst.w d0 after restore)
- **New xdef entries (8):** selTeamsPtr, selTeamNameBase, diyTeamShuffleIndices, diyTeamEntryOffsetsByIndex, diyTournamentTeamIds, quadrantDistanceFactor, aDiyTournament, aDiyLeague
- **New xref entries (16):** All 16 ported C functions
- **Bug fix:** DIY_CacheSelectedTeamPointers stores offsets relative to &selTeamsPtr (asm: `suba.l #selTeamsPtr,a0`), read back via selTeamNameBase in SortLeagueTableByStandings (the 5-byte difference is the team name offset)
- **Type fix:** viewRandomPhase declared as uint8_t (was incorrectly uint16_t; asm is `ds.b $1`)
- **New files:** `include/diy_competition.h`, `src/diy_competition.c`
- ~800 lines of asm code removed/replaced
- Build status: **verified** — clean build, no new warnings
- Functions ported this chunk: 18 (16 exported + 2 static)
- **Running total: 596 functions ported (578 + 18)**

### Future Chunk Candidates
- Sprite rendering pipeline: `DrawSprite_entry2`, `DrawSpriteClipped`, etc. (complex, ~13 functions)
- Menu/career subsystems (~65,000+ lines)

---

## TODO Index (actionable backlog)

### Immediate
- [x] Chunks 0-32: All completed (see above)
- [x] Port audio IRQ handlers (Chunk 32)
- [x] Port scroll blitter system (ScrollXUpdateColumnsAndBlit, ScrollYUpdateRowsAndBlit) — Chunk 56
- [x] Port player status & career team utilities — Chunk 57
- [x] Port match simulation system — Chunk 58
- [ ] Port DrawSprite16Pixels / DrawSpriteCentered / DrawSprite_entry2
- [ ] Port menu callbacks and game flow handlers

### Ongoing
- [ ] Keep `SWOS2.s` shrinking as code migrates
- [ ] Maintain buildability via stubs
- [ ] Move shared helpers (bit operations, fixed-point, endian) into `math68k.c`

### Chunk 61 — Career Init, Cup Round Strings & DIY League Scratch (10 functions)
- Date: 2026-02-20
- Files: `src/teams.c` (1 func), `src/diy_competition.c` (6 funcs), `src/career_init.c` (3 funcs, new file)
- Headers: `include/teams.h`, `include/diy_competition.h`, `include/career_init.h` (new)
- Functions ported:
  1. `_CheckIsTeamNational` — sets isNationalTeam flag based on continent byte (teams.c)
  2. `_GetNextCupRoundStringIndex` — cup round name index for next round (d0,d1,d2→d0)
  3. `_GetNextCupRoundString` — cup round name string for next round (d0,d1,d2→a0)
  4. `_GetCupRoundStringIndex` — complex cup round index with flag/stage/opt (d0-d4→d0)
  5. `_GetCupRoundString` — cup round name string with full params (d0-d4→a0)
  6. `_DIY_LoadLeagueScratchFromTable` — copies 2000 bytes from state table to scratch
  7. `_DIY_SaveLeagueScratchToTable` — copies 2000 bytes from scratch to state table
  8. `_Career_InitTransferMarketState` — clears transfer counters, 3x rand offsets (career_init.c)
  9. `_Career_InitSeasonTransferMarketState` — season quota/limits/backlog init
  10. `_Career_RandomizeInvitationTournamentCup` — 50% chance swap invitation→tour
- Stubs: 1 simple jmp (CheckIsTeamNational), 4 register bridges (cup round string funcs with d0-d4→stack, a0 return via d0), 5 simple jmp (DIY load/save, career init funcs)
- ASM lines removed: ~165 lines
- xref entries: 10 new, xdef entries: 23 new
- No new warnings (all pre-existing)

### Chunk 62 — DIY Buffer Copy Functions (12 functions + 1 static helper → `src/diy_competition.c`)
- Date: 2026-02-20
- Functions: SaveDIYTournament, RestoreDIYTournament, DIY_CopyCompetitionFileBufferToDiyFileBufferCopy, DIY_CopyDiyFileBufferCopyToCompetitionFileBuffer, DIY_CopyCompetitionFileBufferToCompetitionStart, DIY_CopyCompetitionStartToCompetitionFileBuffer, DIY_CopyCompetitionFileBufferToCompetitionStart_Alt, DIY_CopyCompetitionStartToCompetitionFileBuffer_Alt, SaveDIYLeague, RestoreDIYLeague, DIY_SaveCompetitionFileBufferToDIYCompetition, DIY_RestoreCompetitionFileBufferFromDIYCompetition
- Static helper: `copyWords()` — volatile uint16_t* word copy to prevent memcpy optimization
- All 12 simple jmp stubs, ~120 lines asm removed, 12 xref entries
- No new warnings

### Chunk 63 — DIY Cup Init & Helpers (5 functions + 1 asm alias → `src/diy_competition.c`)
- Date: 2026-02-20
- Functions: InitDIYCup, DIY_ShuffleTeamOrderList, DIY_ShuffleTeamGridPairs, DIY_SyncProgressToDiyFileBufferCopy, DIY_ValidateLeagueScratchBuffer
- DIY_SyncProgressToDiyFileBufferCopy_2 is identical duplicate → both stubs jmp to same C function
- ValidateLeagueScratchBuffer: bridge stub preserves scratch registers (d0-d1/a0-a1) matching original all-reg save
- 5 jmp stubs + 1 bridge stub, ~120 lines asm removed, 5 xref + 1 xdef (aDiyCup) entries
- No new warnings

### Chunk 64 — DIY Cup Bracket & Team Grid (5 functions + 1 static helper → `src/diy_competition.c`)
- Date: 2026-02-20
- Functions: DIY_ApplyHomeAwaySwapFlagsToTeamGridPairs, DIY_ReorderTeamGridByOrderList, DIY_RecountMissingTeamsInTeamGrid, DIY_BuildTeamSelectionPointerCache, DIY_InitCupBracketState
- Static helper: `decrementMissingIfNotLoaded()` — replaces DIY_DecrementMissingTeamCountIfNotLoaded (dead asm, stubbed as rts)
- Temp buffers diyTeamGridRemapTemp/diyTeamGridPairSwapTemp replaced with stack arrays
- volatile pointers used to prevent GCC memcpy/memset optimization (no libc)
- 5 jmp stubs + 1 dead rts stub, ~200 lines asm removed, 5 xref + 1 xdef (diyCupTeamIds)
- No new warnings

### Chunk 65 — European Cup Qualification & Grid Caching (7 exported + 12 static → `src/career_eurocups.c`)
- Date: 2026-02-20
- New files: `include/career_eurocups.h`, `src/career_eurocups.c`
- **Exported (7):**
  1. `_Career_ProcessCompetitionForEuropeanCupQualification` — entry point: checks country is European, qualifies teams for Euro Cup/CWC/UEFA
  2. `_DIY_CachePrimaryQualifiedTeamsGrid` — caches 9 league-top teams from competition file to grid
  3. `_DIY_CacheSecondaryQualifiedTeamsGrid` — caches 3 cup-qualified teams from grid bytes
  4. `_DIY_CacheTertiaryQualifiedTeamGrid` — caches 1 tertiary team from grid byte
  5. `_Career_CopyEuroCupTeamGridAndTitleHolder` — copies Euro Cup title holder grid
  6. `_Career_CopyCupWinnersCupTeamGridAndTitleHolder` — copies CWC title holder grid
  7. `_Career_CopyUefaCupTeamGridAndTitleHolder` — copies UEFA Cup title holder grid
- **Static (12):** isTeamInList (loc_12B1F4), appendTeamToList (loc_12B20A), tryConsumeCompetitionIdFromList, isTeamInCupWinnersCupList, isTeamInUefaCupList, clearEuropeanCupTitleHolderIfMatchTeam, appendTeamToEuroCupList, appendTeamToCupWinnersCupList, appendTeamToUefaCupList, qualifyTeamForEuroCup, qualifyTeamForCupWinnersCup, qualifyTeamsForUefaCup
- **C-only helpers (2):** copyTeamGridWords (volatile word copy), copyTitleHolderGrid (shared title holder copy)
- New C-to-asm bridge: `_CCareer_AppendTeamToTmdTeamPool`
- 7 jmp stubs + 10 dead rts stubs, ~300 lines asm removed
- 7 xref + 26 xdef entries
- Boundary spill: UEFA qualification walks through contiguous BSS buffers
- No new warnings

### Chunk 66 — Team Cache, Save/Restore & Fixture Helpers (9 exported + 2 static → `src/career_teams.c` + `src/diy_competition.c`)
- Date: 2026-02-20
- New files: `include/career_teams.h`, `src/career_teams.c`
- **career_teams.c — Exported (4):**
  1. `_VerifyTeamLoaded` — searches team cache (currentTeam, careerTeam, selTeamsPtr) and panics if not found
  2. `_CopySelectedTeamNumbers` — copies first word (team number) from each selected team record
  3. `_SaveSelectedTeams` — saves 0x5582 bytes from numSelectedTeams to static backup buffer
  4. `_RestoreSelectedTeams` — restores saved teams from backup buffer
- **career_teams.c — Static (1):** findTeamInCache — searches currentTeam, careerTeam (with careerAutoAdvanceInProgress check), selTeamNumber, selTeamsPtr array
- **diy_competition.c — Exported (5):**
  1. `_DIY_RecomputeCompetitionDerivedFields` — computes groups, fixture counts, random phase, progression table, strength tier
  2. `_DIY_GetCurrentFixtureTeamPtrs` — determines team competition record pointers for current fixture
  3. `_DIY_AdvanceLeagueProgressAfterMatch` — updates match counters, fixture/round progression, random phase
  4. `_DIY_GetCurrentFixtureTeamClass` — determines match class from team classes
  5. (wrapper for getCurrentFixtureTeamPtrs via globals for asm bridge)
- **diy_competition.c — Static (1):** getCurrentFixtureTeamPtrs — core fixture team pointer lookup logic
- Replaced old `_CVerifyTeamLoaded` C-to-asm bridge with direct `_VerifyTeamLoaded` in career_eurocups.c, career_status.c, diy_competition.c
- New C-to-asm bridge: `_CDIY_RecomputeCompetitionDerivedProgressionTable`
- Global bridge vars: `fixtureTeam1CompPtr`, `fixtureTeam2CompPtr` (C-defined, asm-read)
- 9 asm stubs (jmp/register bridges), ~200 lines asm removed
- LTO fix: globals accessed from asm need `__attribute__((used))`
- No new warnings beyond pre-existing

### Chunk 67 — Competition Grid & Team List Utilities (7 exported + 1 career_season.c → `src/diy_competition.c` + `src/career_season.c`)
- Date: 2026-02-20
- New files: `include/career_season.h`, `src/career_season.c`
- **career_season.c — Exported (1):**
  1. `_Career_FillRandomTeamListExcludingSelected` — Fisher-Yates shuffle with selected team exclusion/injection
- **diy_competition.c — Exported (6):**
  1. `_DIY_BuildQualifiedTeamListFromSortedTable` — merged EC/WC variants, builds qualified team list from sorted league table
  2. `_DIY_BuildQualifiedTeamListFromDiyFileBuffer` — builds from file buffer via byte indices
  3. `_DIY_SyncCompetitionTeamGridOrBuildBracket` — dispatches to seeded bracket build or grid copy
  4. `_DIY_CopyCompetitionTeamIndexTable` — copies team index words from competition data
  5. `_DIY_ApplyPromotionRelegationWeightsToScratch` — applies weights to league scratch buffer, sets promo/relegation flags
  6. `_DIY_UpdatePromotionRelegationScratchCounts` — updates scratch counts for promoted/relegated teams
- New C-to-asm bridges: `_CDIY_BuildSeededBracketAndUpdateScores`, `_CDIY_CopyTeamGridFromCompToDIY` (4 pointer params each)
- 8 asm stubs (jmp/register bridges), ~150 lines asm removed
- New xdefs: playRelegatedAnimation, diyQualifiedTeamIndices
- LTO fix: globals accessed from asm need `__attribute__((used))`
- No new warnings beyond pre-existing

### Chunk 68 — Career European Cup Qualification State (1 exported → `src/career_season.c`)
- Date: 2026-02-20
- **Exported (1):**
  1. `_Career_PrepareEuropeanCupQualificationState` — sets up Euro Cup, CWC, and UEFA Cup team lists for qualification draws; temporarily overrides selTeamNumber from teamNoOfferAccepted; UEFA Cup has special seed bucket count loop
- Bug fix: `_Career_FillRandomTeamListExcludingSelected` used `(uint8_t)selTeamNumber` (LSB) instead of `(uint8_t)(selTeamNumber >> 8)` (MSB = country/league index). selTeamNumber encoding is `(country << 8) | teamWithinCountry`, and 68k `move.b (selTeamNumber)` reads the MSB on big-endian. Fixed to `>> 8`.
- 1 asm stub (jmp, no bridge needed — void function), ~80 lines asm removed
- New xdefs: euroCupCopy, cupWinnersCupCopy, uefaCupCopy, teamNoOfferAccepted, euroCupQualGroupSeedLists_4_6_11, euroCupQualGroupSeedListsReduced_4_6_11, euroCupQualGroupSeedBuckets_15_18_22
- Cumulative: 669 functions across 68 chunks

### Chunk 69 — Team Value Utilities (4 exported → `src/teams.c`)
- Date: 2026-02-20
- **Exported (4):**
  1. `_GetTeamValueBase` — table lookup: avgPrice byte → avgTeamValues[idx] → avgTeamValueBases[val]; mirrors ext.w + byte table + word table asm pattern
  2. `_CalcGateReceiptsBaseFromTeamValue` — calls GetTeamValueBase then: base * 1000 / 768 * 1536; returns d0.w (uint16_t truncation of mulu.w result)
  3. `_IsTeamNational` — returns 1 if not career mode, or if team's continent byte is in [80,85]; returns 0 otherwise
  4. `_GetAveragePlayerPriceInSelectedTeams` — loops over selTeamsPtr buffer at 684-byte intervals, calls _GetAveragePlayerPrice, divides total by numSelectedTeams
- 4 register bridge stubs (d0→stack, a1→stack patterns); ~35 lines asm replaced
- New xdefs: avgTeamValues, avgTeamValueBases
- Eliminates 2 C-to-asm bridge round-trips (_CGetTeamValueBase and CalcGateReceiptsBaseFromTeamValue calls from bridges now go through stubs directly)
- Cumulative: 673 functions across 69 chunks

### Chunk 70 — Player Display Value Utilities (2 exported → `src/teams.c`)
- Date: 2026-02-20
- **Exported (2):**
  1. `_GetPlayerRandomNum` — deterministic player random: XORs price, position/face (masked 0xFB), two name bytes, season number, and random seed, then looks up kRandomTable[result]
  2. `_GetDisplayedPlayerValue` — returns displayed player value (1-80). Two paths: "normal" (currentTeam/careerTeam buffers) uses GetAgeAdjustedPlayerFormFactor + avgTeamValues; "random" (other pointers) computes via avgTeamValues only (asm dead code: random delta computed but discarded by move.w d2,d0 overwrite)
- Player data accessed via byte offsets from team entry pointer (TEAMFILE_PLAYERS=76 + PLAYERFILE_xxx offsets). GetPlayer returns base + playerIndex*38, so offsets are relative to that adjusted pointer.
- 2 register bridge stubs (a0→stack pattern), ~65 lines asm removed
- New xdef: displayedPlayerValueRandomDeltaTable
- Cumulative: 675 functions across 70 chunks

### Chunk 71 — Player Value Snap/Display Utilities (3 exported → `src/teams.c`)
- Date: 2026-02-21
- **Exported (3):**
  1. `_InitPlayerValueDisplayStruct` — computes displayed player value, maps to sprite frame index ((val-1)/8 + 0x71), writes 4 words at dst+0x22..0x28
  2. `_SnapToNearestAvgTeamValueBase` — scans avgTeamValueBases for nearest entry to value, rounds UP at exact midpoint (lsr.w #1 truncation)
  3. `_SnapUpToAvgTeamValueBase` — scans avgTeamValueBases for first entry >= value
- 3 register bridge stubs, ~30 lines asm removed
- Cumulative: 678 functions across 71 chunks

### Chunk 72 — Transfer Pool & Career Market Management (11 exported → `src/transfer_market.c`)
- Date: 2026-02-21
- **New module:** `transfer_market.c` / `transfer_market.h`
- **Exported (11):**
  1. `_GetRemainingTransferPoolSlots` — returns transferPoolSlotLimit - transferPoolSlotsUsed
  2. `_GetRemainingOfferSwapSlots` — returns offerSwapSlotLimit - offerSwapSlotsUsed
  3. `_RemoveTransferOfferAtIndex` — shifts transfer offer array left, decrements numOffers
  4. `_PrunePlayerOffersAgainstRosterLimits` — two-phase pruning: (1) remove offers exceeding slot limits, (2) remove buy-only offers when roster full (>=32)
  5. `_IsPlayerInCareerContest` — searches homeMarketPlayers for (team,player) pair, returns index or -1
  6. `_AddPlayerToCareerContest` — appends to homeMarketPlayers (max 25), returns 0=full/1=added
  7. `_RemovePlayerFromCareerContest` — finds and removes from homeMarketPlayers with shift-left
  8. `_FindForeignMarketPlayer` — searches foreignMarketPlayers, returns index or -1
  9. `_AddForeignMarketPlayer` — appends to foreignMarketPlayers (max 15), returns 0=full/1=added
  10. `_RemoveForeignMarketPlayer` — finds and removes from foreignMarketPlayers with shift-left
  11. `_FindPlayerInCareerOrForeignMarket` — searches both markets, returns 0=found/1=not found
- 11 register bridge stubs. Slot getters return in d2 (not d0). Search functions return Z flag for asm callers.
- Used `volatile uint32_t *` for entry shift loops to prevent GCC memmove optimization (no libc).
- New xdefs: transferOfferTable, numHomeMarketPlayers, homeMarketPlayers, numForeignMarketPlayers, foreignMarketPlayers
- ~200 lines asm removed
- Cumulative: 689 functions across 72 chunks

### Chunk 73 — Transfer Market Operations (10 exported → `src/transfer_market.c`)
- Date: 2026-02-21
- **Exported (10):**
  1. `_RemovePlayerFromTransferOffers` — removes all transfer offers involving a player (primary/secondary/counter)
  2. `_FillForeignMarketPlayersData` — copies career foreign market player data into pool database entries
  3. `_LoadPoolplyrDat` — loads "data/poolplyr.dat" file (cached), then fills foreign market data
  4. `_FindPoolplyrDatPlayer` — linear search through pool player database (no bounds check, assumes match exists)
  5. `_AddCareerTeamPlayer` — selects random player from non-home team in same country, adds to home market with price filtering
  6. `_FillCareerTeamPlayers` — populates home market with random players based on numSelectedTeams (max 25)
  7. `_AddRandomForeignMarketPlayer` — selects random foreign pool player, 25% chance to enforce price range
  8. `_FillForeignMarketPlayers` — populates foreign market with 8-15 random foreign players
  9. `_UpdateHomeMarketPlayers` — randomly removes/adds home market players based on backlog
  10. `_UpdateForeignMarketPlayers` — randomly removes/adds foreign market players based on backlog
- 10 register bridge stubs, ~300 lines asm removed
- New xdefs: poolPlayerStrideRef, careerForeignMarketPlayers, homeMarketRandomPickRetries, foreignMarketRandomPickRetries, aDataPoolplyrDa
- Used volatile for FillForeignMarketPlayersData copy loop to prevent memcpy optimization
- Cumulative: 699 functions across 73 chunks

### Chunk 74 — Transfer Offer Helpers (7 exported → `src/transfer_market.c`)
- Date: 2026-02-21
- **Exported (7):**
  1. `_FindTeamIdInDiyTeamsList` — searches diyTeamIdListType0/1/2 based on competitionFileBuffer value, returns 0=found/1=not found
  2. `_IsOfferTeamPlayerPairUnique` — loops transferOfferTable checking teamId+playerIdx pair uniqueness, returns 1=unique/0=duplicate
  3. `_IsTeamEligibleForOfferByRefSkill` — scans 16 players at 0x26 stride checking skill vs refSkill, returns 0=eligible if skill range spans refSkill
  4. `_RemoveOfferPlayerFromMarketsAndCount` — removes player from both markets, increments selected/other team counter by country match
  5. `_RemoveNegotiatedOfferPlayersFromMarkets` — removes primary player + optional extra player from markets
  6. `_PickRandomEligibleExtraPlayerOrdinal` — 16 random attempts to find eligible extra player (not primary, not on transfer list, not sold, skill <= refSkill)
  7. `_PickRandomEligibleSwapPlayerOrdinalFromTeam` — finds min/max skill in selected team, 16 random attempts to pick offering team player within skill range
- New C-to-asm bridge: `_CGetPlayerAtIndex` in swos2_unit.s (wraps asm GetPlayerAtIndex, returns pointer via d0, stores file index in _cGetPlayerAtIndexFileIdx global)
- Functions 6-7 return in d1 (not d0): stubs exclude d1 from save set, do `move.w d0,d1` after C call
- RemoveOfferPlayerFromMarketsAndCount stubbed as `rts` (dead — only caller is now-C RemoveNegotiatedOfferPlayersFromMarkets)
- All other stubs use full `d0-d7/a0-a6` save/restore (asm callers use d0/a0 after calls)
- 7 asm stubs, ~160 lines asm removed
- New xdefs (11): _CGetPlayerAtIndex, competitionFileBuffer, diyTeamIdListType0/1/2, numDiyTeams, teamOffering, offerPrimaryPlayerOrdinal, numExtraPlayersWanted, removedOfferPlayersFromSelectedTeamCount, removedOfferPlayersFromOtherTeamsCount
- New xrefs (9): 7 C functions + _cGetPlayerAtIndexFileIdx + GetPlayerAtIndex
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 706 functions across 74 chunks

### Chunk 75 — Transfer Market Lists, Career Finance Reset, Pool & Foreign Market Patching (9 exported + 1 static → `src/transfer_market.c`)
- Date: 2026-02-21
- **Exported (9):**
  1. `_FindPlayerOnTransferMarket` — linear search playersOnTransferMarket[], returns 0=found/1=not found
  2. `_AddPlayerToTransferMarket` — appends playerId to transfer market list (max 5)
  3. `_RemovePlayerFromTransferMarket` — finds and removes playerId with shift-left compaction
  4. `_CheckForTransferOffers` — searches transferOfferTable for any offer with byte 0x0D==0, returns 0=found/1=none
  5. `_Career_CommitNewBalanceAndResetFinanceTotals` — if financeTotalsNeedResetFlag set, commits newBalance→old_balance, clears 7 financial accumulators
  6. `_AddCareerPoolPlayer` — appends player to careerForeignMarketPlayers pool (42-byte entry: teamNum+ord+randomPhase+38 bytes player data from TEAMFILE_PLAYERS)
  7. `_Career_PatchTeamWithForeignMarketPlayers` — iterates pool, copies player data into matching team's player slot
  8. `_Career_ApplyForeignMarketPlayersToSelectedTeams` — applies ALL pool entries to selTeamsPtr teams
  9. `_Career_ApplyPendingForeignMarketPlayersToSelectedTeams` — applies only non-pending (total - pending) entries
- **Static helper (1):** applyForeignMarketPlayersToTeams — shared loop body for functions 8 and 9
- Assembly stubs:
  - FindPlayerOnTransferMarket: save d1-d7/a0-a6 (not d0), push d1, jsr C, restore, tst.w d0 (Z flag)
  - AddPlayerToTransferMarket: full save, push d0, jsr C, restore
  - RemovePlayerFromTransferMarket: full save, push d1, jsr C, restore
  - CheckForTransferOffers: save d1-d7/a0-a6 (not d0), jsr C, restore, tst.w d0 (Z flag)
  - Career_CommitNewBalanceAndResetFinanceTotals: simple `jmp` (void function, callers use jsr)
  - AddCareerPoolPlayer: full save, push a0/d1/d0, jsr C, restore
  - Career_PatchTeamWithForeignMarketPlayers: full save, push a0, jsr C, restore
  - Career_ApplyForeignMarketPlayersToSelectedTeams: simple `jmp`
  - Career_ApplyPendingForeignMarketPlayersToSelectedTeams: simple `jmp`
- New xdefs (4): playersOnTransferMarket, financeTotalsNeedResetFlag, playerPurchases, playerSales
- New xrefs (9): all 9 C function symbols
- volatile uint16_t* used for shift loops to prevent memmove optimization
- ~160 lines asm removed
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 716 functions across 75 chunks

### Chunk 76 — Career Wage Management, DIY Fixture Advancement, Cache Insertion & Team Removal (6 exported + 1 static → career_status.c, transfer_market.c, career_teams.c)
- Date: 2026-02-21
- **Exported (6):**
  1. `_DIY_AdvanceFixtureIndex` → transfer_market.c — applies pending foreign market players to selected teams for players whose randomPhase falls within [lo..hi], career mode only
  2. `_Career_RemovePlayerFromTeamInternal` → transfer_market.c — removes player from transfer offers, then either from transfer market (own team) or from career contest + foreign market (other team)
  3. `_TeamSelectionKeyCache_InsertSpaceSorted` → career_teams.c — sorted insertion into team selection key cache; returns insert point pointer (a4 output for asm callers); panics if overflow
  4. `_GetAvgTeamValueBaseForCurrentPlayer` → career_status.c — calls GetPlayer + GetDisplayedPlayerValue to get displayed value, indexes into avgTeamValueBases table
  5. `_Career_UpdatePlayerWeeklyWageCache` → career_status.c — calculates player wage (avg base, 0x9B=33% trialist, 0x9A=66% youth, scale by 1000/768*96/wageScalingDivisor), stores in weeklyWageByPlayerIndex, recalcs total
  6. `_Career_RemovePlayerWeeklyWageAndRecalcWeeklyWages` → career_status.c — clears wage entry and recalculates total weekly wages
- **Static helper (1):** `recalcTotalWeeklyWages` — shared recalc loop for functions 5 and 6; uses _CGetPlayerAtIndex bridge + _cGetPlayerAtIndexFileIdx
- New C-callable bridge: `_CallGetPlayer` in swos2_unit.s — wraps asm GetPlayer(d0=team, d1=playerIdx → a0=ptr), returns pointer
- Assembly stubs:
  - DIY_AdvanceFixtureIndex: full save, push d7/d6, jsr C, restore
  - Career_RemovePlayerFromTeamInternal: full save, push d1/d0, jsr C, restore
  - TeamSelectionKeyCache_InsertSpaceSorted: save d0-d7/a0-a3/a5-a6 (NOT a4), push d5/d0, jsr C, movea.l d0,a4, restore
  - GetAvgTeamValueBaseForCurrentPlayer: save d1-d7/a0-a6 (NOT d0), push d1/d0, jsr C, restore (d0=return value)
  - Career_UpdatePlayerWeeklyWageCache: full save, push d1, jsr C, restore
  - Career_RemovePlayerWeeklyWageAndRecalcWeeklyWages: full save, push d1, jsr C, restore
- New xdefs (5): selTeamSelectedPlayerPosition, weeklyWageByPlayerIndex, teamSelectionKeyCache, teamSelectionKeyCacheCount, teamSelectionKeyCacheEnd
- New xrefs (6): 6 C functions
- Key insight: GetPlayer returns teamBase + d1*38; player's SOMEFLAG at offset 0x71 = TEAMFILE_PLAYERS(76) + PLAYERFILE_SOMEFLAG(37) from GetPlayer base
- ~180 lines asm removed (shared recalc loop code removed)
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 723 functions across 76 chunks

### Chunk 77 — Chairman Messages + TMD Team Pool Management (9 exported → career_status.c, career_teams.c)
- Date: 2026-02-21
- **Exported (9):**
  1. `_ShowLeavingMessage` → career_status.c — sets currentMessage (national=32, club=18), calls ShowChairmanMessage
  2. `_ShowGreetingMessage` → career_status.c — sets currentMessage (national=33, club=19), calls ShowChairmanMessage
  3. `_ShowLastSeasonMessage` → career_status.c — sets currentMessage (national=34, club=20), calls ShowChairmanMessage
  4. `_ShowWishYouSuccessMessage` → career_status.c — sets currentMessage (national=35, club=21), calls ShowChairmanMessage
  5. `_ShowEnjoyLastSeasonMessage` → career_status.c — sets currentMessage (national=36, club=22), calls ShowChairmanMessage
  6. `_ShowRetirementMessage` → career_status.c — sets currentMessage (national=37, club=23), calls ShowChairmanMessage
  7. `_SearchTeamInTmdFile` → career_teams.c — linear search in tmdFileBuffer for team number, returns 0=found/1=not found (Z flag in asm stub)
  8. `_Career_RemoveTeamFromTmdTeamPool` → career_teams.c — find team by number + shift-left to compact pool, decrement count
  9. `_Career_AppendTeamToTmdTeamPool` → career_teams.c — validate not duplicate (panics 0x3443), copy 684 bytes to end of pool, increment count
- Assembly stubs:
  - Show*Message (6): full save d0-d7/a0-a6, jsr C, restore, rts
  - SearchTeamInTmdFile: save d1-d7/a0-a6 (NOT d0), push d0, jsr C, tst.w d0 (Z flag), restore
  - Career_RemoveTeamFromTmdTeamPool: full save, push d0, jsr C, restore
  - Career_AppendTeamToTmdTeamPool: full save, push a0, jsr C, restore
- Updated _CCareer_AppendTeamToTmdTeamPool bridge to call C directly (was calling asm)
- New xdefs (1): tmdFileBuffer
- New xrefs (9): 9 C functions
- ~130 lines asm removed
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 732 functions across 77 chunks

### Chunk 78 — Training Match State Management (7 exported → training_match.c)
- Date: 2026-02-21
- **Exported (7):**
  1. `_TrainingMatch_SaveSelectedPlayerId` → training_match.c — saves player file index to savedPlayers at careerNumPlayers offset
  2. `_TrainingMatch_RemoveSavedPlayerId` → training_match.c — removes player from saved array with position-aware swap logic for starting 11 replacement from subs (same position group, highest price), then shifts array and marks last slot 0xAA
  3. `_TrainingMatch_SaveLastMatchState` → training_match.c — saves shirt numbers (via GetPlayer for 32 indices), player assignments, tactics, position, controls, coach mode
  4. `_TrainingMatch_RestoreLastMatchState` → training_match.c — restores all saved state back
  5. `_TrainingMatch_RestoreSavedTeamSelectionState` → training_match.c — restores team selection from training match saved data
  6. `_TrainingMatch_SaveTeamSelectionState` → training_match.c — saves team selection for training match
  7. `_CheckIfTeamIsChanged` → training_match.c — compares first 11 players + tactics + position, returns 0=same/1=different (Z flag stub)
- New file: training_match.c / training_match.h
- Assembly stubs:
  - SaveSelectedPlayerId: full save, push d1, jsr C, restore
  - RemoveSavedPlayerId: full save, push d1, jsr C, restore
  - Save/Restore/RestoreSaved/SaveTeamSelection: full save, jsr C (no args), restore
  - CheckIfTeamIsChanged: save d1-d7/a0-a6 (NOT d0), jsr C, tst.w d0 (Z flag), restore
- New xdefs (10): trainingMatchSavedPlayers, trainingMatchSavedPlayerPos, trainingMatchSavedShirtNums, trainingMatchSavedTactics, lastMatchSavedShirtNums, lastMatchPlayers, currentTactics, lastMatchSavedPlayerPos, lastMatchSavedTeamControls, lastMatchSavedCoachOrPlayer
- Uses _CallGetPlayer bridge (from Chunk 76) for GetPlayer calls
- Fix: added optimize("no-tree-loop-distribute-patterns") to 4 functions to prevent GCC converting byte-copy loops to memcpy
- ~230 lines asm removed (RemoveSavedPlayerId alone was ~70 lines)
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 739 functions across 78 chunks

### Chunk 79 — Job Offer Queue Management (6 exported → job_offers.c)
- Date: 2026-02-21
- **Exported (6):**
  1. `_IsOldJobOffer` → job_offers.c — linear search in jobOffers array by team number, returns 0=found/1=not found (Z flag stub)
  2. `_GetJobOfferPtr` → job_offers.c — returns pointer to jobOffers + index * 26 (pointer-return stub via a0)
  3. `_AddJobOffer` → job_offers.c — adds new job offer to front of newJobOffers queue, shifts existing entries right by 26 bytes (word-at-a-time backwards), max 5 offers
  4. `_RemoveJobOffer` → job_offers.c — finds job offer by team number in newJobOffers, shifts following entries left, decrements count
  5. `_IsNewJobOffer` → job_offers.c — linear search in newJobOffers array by team number, returns 0=found/1=not found (Z flag stub)
  6. `_GetNewJobOfferPtr` → job_offers.c — returns pointer to newJobOffers + index * 26 (pointer-return stub via a0)
- New file: job_offers.c / job_offers.h
- Job offer structure: 26 bytes (teamNo:2, teamName:18, amountOffered:4, unknown:2)
- Assembly stubs:
  - IsOldJobOffer/IsNewJobOffer: Z flag stubs (save d1-d7/a0-a6, tst.w d0)
  - GetJobOfferPtr/GetNewJobOfferPtr: pointer-return stubs (save d1-d7/a1-a6, movea.l d0,a0)
  - AddJobOffer: full save, push d1 then d0, jsr C, restore
  - RemoveJobOffer: full save, push d0, jsr C, restore
- New xdefs (4): numJobOffers, jobOffers, numNewJobOffers, newJobOffers
- Key detail: AddJobOffer shift uses 68k `move.w -28(a0),-(a0)` where source EA evaluates before pre-decrement → effective 26-byte rightward shift
- ~130 lines asm removed
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 745 functions across 79 chunks

### Chunk 80 — Tactics Selection + Chairman Memos + Club Business (12 exported → tactics_menu.c, chairman_memo.c)
- Date: 2026-02-21
- **Exported (12):**
  1. `_User_A_Tactics` → tactics_menu.c — sets chosenTactics to 12 (USER_A), calls SetExitMenuFlag
  2. `_User_B_Tactics` → tactics_menu.c — sets chosenTactics to 13
  3. `_User_C_Tactics` → tactics_menu.c — sets chosenTactics to 14
  4. `_User_D_Tactics` → tactics_menu.c — sets chosenTactics to 15
  5. `_User_E_Tactics` → tactics_menu.c — sets chosenTactics to 16
  6. `_User_F_Tactics` → tactics_menu.c — sets chosenTactics to 17
  7. `_ExitChooseTacticsMenu` → tactics_menu.c — sets chosenTactics to 0xFFFF (abort), calls SetExitMenuFlag
  8. `_ShowChairmanMessage` → chairman_memo.c — loads chairmanMemosMenu, calls ShowMenu
  9. `_ChairmanMemosInit` → chairman_memo.c — sets fireResetFlag=1, changes title to "MEMO FROM THE PRESIDENT" if national, falls through to OnReturn
  10. `_ChairmanMemosOnReturn` → chairman_memo.c — formats "DEAR %a %b" greeting + 2 message lines from clubMessagesTable via PrintFormatted, draws menu, waits 100 VBlanks, shows Continue button
  11. `_ChairmanMemosContinue` → chairman_memo.c — clears currentMessage, calls SetExitMenuFlag
  12. `_ShowClubBussinesMenu` → chairman_memo.c — clears clubBusinessActive, loads clubBusinessMenu, calls ShowMenu
- New files: tactics_menu.c / tactics_menu.h, chairman_memo.c / chairman_memo.h
- Assembly stubs: all 12 use full save/restore (movem.l d0-d7/a0-a6)
- New xdefs (9): chosenTactics, chairmanMemosMenu, clubBusinessMenu, plNameForMessage, managerSexBuffer, clubMessagesTable, aMemoFromThePre, aThePresident, aDearAB_0
- Fix: corrected clubBusinessActive type from int8_t to uint16_t in career_status.c (asm uses ds.b $2)
- ~80 lines asm removed
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 757 functions across 80 chunks

### Chunk 81 — Job Offer Utilities + ConvertPlayerDressColor (6 exported → job_offers.c, team_colors.c)
- Date: 2026-02-21
- **Exported (6):**
  1. `_JobOffers_RemoveOffer` → job_offers.c — removes team from jobOffers (old) array by linear search, shifts remaining entries left word-at-a-time
  2. `_JobOffers_MaybeExpireSomeNewOffers` → job_offers.c — iterates new offers backwards, 1/32 chance (Rand2 < 0x20) to clear amountOffered
  3. `_JobOffers_FillMissingNewOfferTeamNames` → job_offers.c — for entries with teamName[0]==0xFF, looks up team via _CCallGetTeamPointer bridge and copies name
  4. `_JobOffers_RemoveExpiredOffers` → job_offers.c — removes expired (amountOffered==0) from both jobOffers and newJobOffers arrays
  5. `_JobOffers_ConvertNewOfferToOffer` → job_offers.c — finds entry in newJobOffers, copies name to fundsBuffer, removes from new, adds to old array (max 5)
  6. `_ConvertPlayerDressColor` → team_colors.c — ext.w on color index, looks up shirtColorTable, masks to byte
- Assembly stubs: void no-arg (3) use full save d0-d7/a0-a6; ConvertNewOfferToOffer pushes d0+d1; RemoveOffer pushes d0; ConvertPlayerDressColor saves d1-d7/a0-a6 preserving d0 return
- New bridge: `_CCallGetTeamPointer` in swos2_unit.s — C-callable wrapper for asm GetTeamPointer (d0=teamId → a0=ptr, returns ptr in d0)
- New xdefs (2): fundsBuffer, _CCallGetTeamPointer
- ~120 lines asm removed
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 763 functions across 81 chunks

### Chunk 82 — Edit Tactics Menu Initialization & Buffer Management (11 exported → edit_tactics.c)
- Date: 2026-02-21
- **Exported (11):**
  1. `_ChooseTacticsInit` → edit_tactics.c — sets menuEntries[1..6].fg to teamTacticsUSERA-F addresses, calls CheckIsTeamNational
  2. `_ShowStandardChooseTacticsMenu` → edit_tactics.c — stores team ptr, shows chooseTacticsMenu, if tactic chosen shows editTacticsMenu via InitLittlePlayerSprites
  3. `_EditTacticsInit` → edit_tactics.c — sets bench player numbers, determines editTacticsNumPlayers (16 or careerNumPlayers), clears selection state, falls through to OnRestore
  4. `_EditTacticsOnRestore` → edit_tactics.c — calls EnableOrDisablePlayerFacesItems, CopyCurrentTacticName, UpdateExtraPlayerMenuVisibility, SetBigCheckSprite
  5. `_EnableOrDisablePlayerFacesItems` → edit_tactics.c — loops 10 player face entries (29-38), sets isDisabled based on flipOnOff
  6. `_CopyCurrentTacticName` → edit_tactics.c — builds "EDIT TACTICS (name)" string in menuEntries[1].fg buffer
  7. `_EditTactics_UpdateExtraPlayerMenuVisibility` → edit_tactics.c — shows/hides entries 27-28 for 17th/18th players based on editTacticsNumPlayers
  8. `_SetBigCheckSprite` → edit_tactics.c — shows/hides check/not-check sprite on entry 0 based on oldPlayerSkill vs newPlayerSkill
  9. `_CopyCurrentTactics` → edit_tactics.c — copies 370 bytes from teamTacticsTable[chosenTactics] to editTacticsCurrentTactics
  10. `_ApplyCurrentEditedTactics` → edit_tactics.c — copies 370 bytes back from editTacticsCurrentTactics, calls InitializeTacticsPositions
  11. `_SaveCurrentTactics` → edit_tactics.c — copies 370 bytes editTacticsCurrentTactics → savedCurrentTactics
  12. `_RestoreTactics` → edit_tactics.c — copies 370 bytes savedCurrentTactics → editTacticsCurrentTactics
- New files: edit_tactics.c / edit_tactics.h
- Assembly stubs: all 11 use full save/restore (movem.l d0-d7/a0-a6); ShowStandardChooseTacticsMenu pushes a0 param
- New bridges (5): _CCallSetBenchPlayersNumbers, _CCallEditTactics_CommitPlayerPositionsAndRecalcSkill, _CCallEditTacticsMenuSavePlayerPositions, _CCallInitLittlePlayerSprites, _CCallInitializeTacticsPositions
- New xdefs (11): newPlayerSkill, savedCurrentTactics, editTacticsNumPlayers, aEditTactics, editTacticsSelectedPairSlotIndex, editTacticsSelectedPlayerMenuEntryPtr, editTacticsSelectedPlayerNameIndex, chooseTacticsMenu, editTacticsMenu + 5 bridge xdefs
- ~170 lines asm removed
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 774 functions across 82 chunks

### Chunk 83 — Mixed Edit Tactics Drawing + Import Tactics + Highlights Menu (12 exported → edit_tactics.c, highlights_menu.c)
- Date: 2026-02-21
- **Exported (12):**
  1. `_DrawPlayerNamesAndFaces` → edit_tactics.c — draws menu items 11, 29-38, and 0 via CallDrawMenuItem/CallDrawMultipleItems
  2. `_EditTacticsMenuSavePlayerPositions` → edit_tactics.c — copies newPlayerSkill to oldPlayerSkill, copies 32 bytes playerPositionsArray→oldPlayerPositionsArray, calls SetBigCheckSprite
  3. `_EditTactics_CommitPlayerPositionsAndRecalcSkill` → edit_tactics.c — saves positions, sets teamFile flag, calls ComputeOldPlayerSkillForViewResult with tactics=0x12, restores tactics, calls SetBigCheckSprite
  4. `_ImportTacticsInit` → edit_tactics.c — empty (rts only)
  5. `_ImportTacticsSelectTactics` → edit_tactics.c — takes MenuEntry* param, computes ordinal diff for importMenuSelectedTactics, exits menu
  6. `_ExitImportTactics` → edit_tactics.c — sets importMenuSelectedTactics=-1, exits menu
  7. `_HighlightsMenuInit` → highlights_menu.c — calls HighlightsMenuCommon
  8. `_HighlightsMenuOnReturn` → highlights_menu.c — calls HighlightsMenuCommon
  9. `_HighlightsMenuCommon` → highlights_menu.c — enables/disables menu entries 1,3 based on hilNumGoals (0→disabled+gray, nonzero→enabled+colored)
  10. `_ExitHighlights` → highlights_menu.c — calls SetExitMenuFlag
  11. `_MarkSelectedTeamsTacticsChanged` → edit_tactics.c — marks careerTeam and matching selectedTeams' tactics-changed flag
  12. `_FillSkinColorConversionTable` → edit_tactics.c — fills convertTable from skinAndHairColorConversionTable using skinColor index
- New files: highlights_menu.c / highlights_menu.h
- Assembly stubs: all 12 use full save/restore; ImportTacticsSelectTactics pushes a5, FillSkinColorConversionTable pushes a1+d0
- New xdefs (2): oldPlayerPositionsArray, importMenuSelectedTactics
- ~90 lines asm removed
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 786 functions across 83 chunks

### Chunk 84 — Edit Tactics Menu Actions + RotatePlayersLeft (11 exported → edit_tactics.c, teams.c)
- Date: 2026-02-21
- **Exported (11):**
  1. `_SelectCopyTactics` → edit_tactics.c — trivial: calls SetCurrentEntry(3)
  2. `_UndoTactics` → edit_tactics.c — RestoreTactics, CallDrawMenuItem(entry 2), CommitPlayerPositionsAndRecalcSkill, DrawPlayerNamesAndFaces
  3. `_AbortEditTactics` → edit_tactics.c — saves positions, checks tacticsChangedFlag, shows DoContinueAbortMenu if changed, exits menu
  4. `_FlipOnOff` → edit_tactics.c — toggles flipOnOff, calls EnableOrDisablePlayerFacesItems, DrawMenu
  5. `_ImportTactics` → edit_tactics.c — shows import menu, copies 370 position bytes from selected tactic, marks tactics changed
  6. `_SaveAndExitEditTactics` → edit_tactics.c — complex save flow: checks changed flag, apply tactics, DoContinueAbortMenu, file dialog with .TAC extension, WriteTacticsFile
  7. `_EditTacticsOnSelect28` → edit_tactics.c — calls RotatePlayersLeft via bridge + DrawMenuItemsWaitForVSync
  8. `_EditTacticsAfterDrawPlayerFace` → edit_tactics.c — takes MenuEntry* param, applies row highlighting, draws entry 39
  9. `_EditTactics28AfterDraw` → edit_tactics.c — draws menu entry 27
  10. `_EditTactics2AfterDraw` → edit_tactics.c — calls DrawEditTacticsPitchAndPlayers via bridge
  11. `_RotatePlayersLeft` → teams.c — rotates substitute player slots left by one position, handles currentTeam/careerTeam/other team pointers
- Assembly stubs: all 10 edit_tactics functions use full save/restore; EditTacticsAfterDrawPlayerFace pushes a5 param; RotatePlayersLeft passes d0/a0 params
- New bridges (3): _CCallDrawEditTacticsPitchAndPlayers, _CCallEditTactics_ApplyPlayerRowHighlighting, _CCallRotatePlayersLeft
- New xdefs (12): tacticsChangedFlag, importTacticsMenu, editTacticsRotateRedrawList, aEditedTacticsWillBeLost, aTacticsHaveBeenChanged, aSaveEditedTact, aSave, aExit, chairmanMemosMenu + 3 bridge xdefs
- Fix: added optimize("no-tree-loop-distribute-patterns") to RotatePlayersLeft to prevent GCC converting byte-shift loop to memmove
- Fix: SaveAndExitEditTactics uses selectedFilename global directly (fixes latent asm stub bug where a0 return from SelectFileToSaveDialog was not preserved)
- ~200 lines asm removed
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 797 functions across 84 chunks

### Chunk 85 — Edit Tactics Pitch Coordinate + Drawing Helpers (16 exported → edit_tactics.c)
- Date: 2026-02-21
- **Exported (16):**
  1. `_GetPlayerQuadrant` → edit_tactics.c — extracts x/y quadrant from tactics position byte (high/low nibble). Multi-register return (d0,d1,d2), asm body kept
  2. `_SetPlayerQuadrant` → edit_tactics.c — encodes x/y quadrant into position byte, sets tacticsChangedFlag
  3. `_GetBallQuadrant` → edit_tactics.c — converts ball location index to col/row via division. Multi-register return, asm body kept
  4. `_GetBallXYPositionFromQuadrant` → edit_tactics.c — converts ball row/col to screen X/Y using lookup tables. Multi-register return, asm body kept
  5. `_GetPlayerXYPositionFromQuadrant` → edit_tactics.c — gets player screen position; if quadrant=15x15, returns ball position. Multi-register return, asm body kept
  6. `_EditTactics_GetDirectionDelta` → edit_tactics.c — reads direction input, returns dx/dy via lookup table. Multi-register return, asm body kept
  7. `_GetEditTacticsItemXYPosition` → edit_tactics.c — dispatcher: ball/player position lookup by index. Multi-register return, asm body kept
  8. `_EditTactics_TryMoveQuadrantCursor` → edit_tactics.c — attempts cursor move with bounds checking. Multi-register return, asm body kept
  9. `_EditTactics_SetAllPlayersQuadrantForBallLoc` → edit_tactics.c — copies all 10 players' positions from one ball location to another
  10. `_EditTactics_FlipMaybe_MirrorPlayersInArea` → edit_tactics.c — mirrors player X positions and swaps paired players using unkTable
  11. `_EditTactics_FlipMaybe_MirrorBallArea` → edit_tactics.c — computes mirrored ball area, copies and mirrors players there
  12. `_EditTactics_CommitDraggedPlayerPosition` → edit_tactics.c — commits dragged position with conditional mirroring for opposite sides
  13. `_SortEditTacticsDrawOrderByY` → edit_tactics.c — bubble-sorts 12-entry draw order array by Y coordinate
  14. `_SortPlayerAreasByY` → edit_tactics.c — bubble-sorts 35-entry player areas by Y for marked player
  15. `_EditTacticsDrawCursor` → edit_tactics.c — draws animated cursor on pitch sprite (4-frame animation)
  16. `_DrawPlayerDirections` → edit_tactics.c — draws direction arrows between current/previous ball locations using CalculateDeltaXAndY
- Assembly stubs: 9 void functions get full save stubs (SetPlayerQuadrant pushes d0-d3, SetAllPlayers pushes d1/d3, MirrorPlayersInArea pushes d3, MirrorBallArea pushes d1, others are no-param); 7 multi-register return functions keep asm bodies (for asm callers) with C version comments
- New xdefs (22): editTacticsPitchStatus, markedPlayerNumber, ballRow, ballColumn, editTacticsBallLocation, editTacticsLastBallLocation, editTacticsPlayerX, editTacticsBallRow, editTacticsBallColumn, editTacticsDisableLittlePlayersAndBallDraw, editTacticsTransitionActive, editTacticsLastTick, editTacticsPlayerPositions, editTacticsPlayerNumbers, playerAreas, ballXQuadrantCoordinates, ballYQuadrantCoordinates, xQuadrantToCoordinates, yQuadrantToCoordinates, arrowSpritesOffsets, tacticCursorFrames, editTacticsDirectionDeltaTable
- Key design: Multi-register return functions (d1/d2 output) keep asm bodies alive for asm callers. C versions use output pointer parameters. When remaining asm callers are later ported, asm bodies become dead code.
- ~250 lines asm code replaced with stubs + comments
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 813 functions across 85 chunks

### Chunk 86 — Team Buffer + Edit Tactics Rendering (6 exported → teams.c + edit_tactics.c)
- Date: 2026-02-21
- **Exported (6):**
  1. `_AddTeamToBuffer` → teams.c — copies 684-byte team record to selTeamsPtr buffer, increments numSelectedTeams, returns pointer to copy. Stub returns a0 from C d0 return.
  2. `_FindTeam` → teams.c — searches selTeamsPtr for team by ID (first word), returns pointer + remaining count via output pointer. Panics if not found. Stub returns a0 + d2.
  3. `_DeselectTeam` → teams.c — finds team and shifts remaining entries down, decrements numSelectedTeams
  4. `_InitLittlePlayerSprites` → edit_tactics.c — initializes edit tactics display: clears state, copies tactics, sets up dress colors, copies/colorizes little player and goalie sprites. Replaces _CCallInitLittlePlayerSprites bridge call.
  5. `_DrawLittlePlayersAndBall` → edit_tactics.c — draws all 12 items (ghost ball, real ball, 10 players) on edit tactics pitch with blinking for selected items based on pitchStatus, plus goalkeeper at fixed position
  6. `_DrawEditTacticsHighlightedPlayerRoute` → edit_tactics.c — when route view active (disable flag set), draws marked player's sprite at all 35 ball area positions with shirt numbers
- Assembly stubs: AddTeamToBuffer returns a0 (d0→a0 bridge), FindTeam returns a0+d2 (pea/temp approach), DeselectTeam/InitLittlePlayerSprites/DrawLittlePlayersAndBall/DrawEditTacticsHighlightedPlayerRoute are full-save void stubs
- New xdefs (1): littlePlayersConvertTable
- ~210 lines asm code replaced with stubs
- Updated C caller: _ShowStandardChooseTacticsMenu now calls _InitLittlePlayerSprites directly (removed _CCallInitLittlePlayerSprites bridge)
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 819 functions across 86 chunks

### Chunk 87 — Edit Tactics State Machine & Input (7 exported → edit_tactics.c)
- Date: 2026-02-21
- **Exported (7):**
  1. `_DrawEditTacticsPitchAndPlayers` → edit_tactics.c — draws pitch background bars, tactics pitch sprite, player directions, cursor, little players/ball, and highlighted route. Replaces _CCallDrawEditTacticsPitchAndPlayers bridge.
  2. `_HandlePlayersOrBallInEditTactics` → edit_tactics.c — expanding cone search for nearest player/ball in joystick direction. Width starts at 384, expands by 64 to 1280, then by 192 to 7680. Sets markedPlayerNumber or pitchStatus=0xFFFF on failure.
  3. `_EditTacticsReadControls` → edit_tactics.c — state machine for pitch controls when no player marked. Handles statuses 2(idle), 3(ball-browse), 4(selected), 6(dragging). Fire press/release timing controls transitions.
  4. `_EditTactics_HandlePitchInput` → edit_tactics.c — state machine for pitch input when player IS marked. Handles statuses 2(idle), 4(quadrant move), 5(ball area browse). Player quadrant movement with collision detection, ball area navigation with wrapping.
  5. `_EditTacticsStatus` → edit_tactics.c — main dispatcher: initializes status, waits for fire release, delegates to HandlePitchInput or ReadControls based on markedPlayerNumber. Always draws pitch before returning. Status 0xFFFF clears transition.
  6. `_EditTacicsMovePlayersOrBall` → edit_tactics.c — menu action: saves positions, enters status 0, redraws entry[2], sets current entry to 4
  7. `_CopyTactics` → edit_tactics.c — menu action: changes entry[4] bg to pink, enters status 1, on exit restores to yellow-green, commits positions
- Assembly stubs: EditTacticsStatus uses d0 param bridge (save d1-d7/a0-a6), all others full-save void stubs
- New xdefs (2): editTacticsInputRepeatCounter, editTacticsNearestSelectPitchWidth
- ~420 lines asm code replaced with stubs
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 826 functions across 87 chunks

### Chunk 88 — Edit Tactics Player Swap, Pair Linking & Position Lookup (6 exported → edit_tactics.c)
- Date: 2026-02-21
- **Exported (6):**
  1. `_GetPlayerInfoBreakPosition` → edit_tactics.c — remaps slot via formation position table, calls _CGetPlayerAtIndex. Multi-register return (asm body kept for asm callers), C version uses output pointers.
  2. `_EditTactics_FindFreePairId` → edit_tactics.c — scans 10-byte unkTable, builds used-ID bitmask, returns first free pair ID
  3. `_EditTactics_ClearPairId` → edit_tactics.c — sets all unkTable entries matching given pair ID to 0xFF
  4. `_EditTactics_SwapPlayersInSlots` → edit_tactics.c — swaps two players by slot index. Handles playing/bench boundary crossings via _CGetPlayerAtIndex, swaps positions and shirt numbers
  5. `_EditTacticsCommon` → edit_tactics.c — menu handler for pair entry selection. Links/unlinks player pairs, redraws pair entries. Uses FindFreePairId/ClearPairId.
  6. `_EditTacticsPlayerNamesCommon` → edit_tactics.c — menu handler for player name selection. Swaps players on second click, cancels on same-player click, updates display.
- Assembly stubs: GetPlayerInfoBreakPosition keeps asm body (multi-register return). FindFreePairId returns d0 bridge. ClearPairId takes d0 param. SwapPlayersInSlots takes d3/d4. EditTacticsCommon/PlayerNamesCommon take a5 (MenuEntry*).
- New xdefs (2): editTacticsSelectedPlayerSlotIndex, editTacticsSelectedPairMenuEntryPtr
- ~208 lines asm code replaced with stubs
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 832 functions across 88 chunks

### Chunk 89 — LoadTactics, Stadium UI, Highlights Menu, Career Search (9 exported → edit_tactics.c + menus.c + highlights_menu.c + diy_competition.c)
- Date: 2026-02-22
- **Exported (9):**
  1. `_LoadTactics` → edit_tactics.c — prompts save disk, loads .TAC file, validates extension, reads tactics, updates importMenuSelectedTactics
  2. `_ShowStadiumInit` → menus.c — stadium menu init: loads background, sets game/round names, team names with colors, player name pointers for entries 16-31/43-58, bench visibility
  3. `_StadiumBeforeDrawLeftTeam` → menus.c — menu beforeDraw callback for top team player entries, delegates to StadiumPlayerSpritesBeforeDraw
  4. `_StadiumBeforeDrawRightTeam` → menus.c — menu beforeDraw callback for bottom team player entries
  5. `_StadiumPlayerSpritesBeforeDraw` → menus.c — sets little player sprite type and color table (goalie vs outfield), fills skin color conversion
  6. `_LoadHighlightsMenu` → highlights_menu.c — prompts disk, loads .HIL file, validates signature, copies filename
  7. `_SaveHighlightsMenu` → highlights_menu.c — prompts disk, saves highlights with .HIL extension
  8. `_Career_IsTeamIdNotInListOrReturnSentinel` → diy_competition.c — searches diyCupTeamIds for teamId, returns sentinel bit 16 if not found
  9. `_Career_IsTeamIdNotInListOrReturnSentinel2` → diy_competition.c — same pattern for diyTournamentTeamIds
- Assembly stubs: LoadTactics/ShowStadiumInit/LoadHighlightsMenu/SaveHighlightsMenu = full save void stubs. StadiumBeforeDrawLeft/Right = a5 param bridges. StadiumPlayerSpritesBeforeDraw = a5+d1 bridge. Career functions = d0/a2 bridges with btst #16,d0 for Z flag.
- New xdefs (11): hilStarted, kHilFileSig, stadiumMenuLittlePlayer, littlePlayersColorTable, littlePlayersSpriteTable, aLoadEditedTact, aNotATacticsFil, aLoadHighlights, aSaveHighlights, aNotAHighlights, aCannotPlayOldV
- ~220 lines asm code replaced with stubs
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 841 functions across 89 chunks

### Chunk 90 — Career Competition Team Selection (12 exported → career_teams.c)
- Date: 2026-02-22
- **Exported (12):**
  1. `_Career_SortTeamFilePtrsByAvgPlayerPrice` → career_teams.c — loads team file, bubble sorts by avg price (descending), random 25% adjacent swaps, copies sorted IDs
  2. `_LoadSomeEuroCup` → career_teams.c — loads eurocup/eurocwc/uefacup TMD, appends to TMD pool, calls SetTeamGlobalNumbers
  3. `_AppendTeamsToSelectionFromTmdOrTeamFile` → career_teams.c — finds teams by ID in TMD cache or team files, copies to selection buffer
  4. `_Career_TryAddRandomTeamByRegionToLists` → career_teams.c (dead stub, logic inlined into fillCupTeamSelectionFromFile)
  5. `_FatalError` → career_teams.c — panic 0x1939
  6. `_Career_InitCompetitionTeamSelection_Dispatch` → career_teams.c — dispatches on spec[1] to one of three handlers
  7. `_Career_InitCompetitionTeamSelectionFromTableEntrySpec` → career_teams.c — extracts team IDs from spec or loads by league
  8. `_Career_InitCompetitionTeamSelectionFromTableHeaderSpec` → career_teams.c — extracts cup team IDs or fills with random picks + region weights
  9. `_Career_InitCompetitionTeamSelectionFromAltTableHeaderSpec` → career_teams.c — tournament variant
  10-12. Three static helpers: `fillLeagueTeamSelectionFromFile`, `fillCupTeamSelectionFromFile`, `fillTournamentTeamSelectionFromFile`
- Inline blocks ported: loc_15158E (league fill), loc_151628 (cup fill), loc_1517C2 (tournament fill) — all dead-stubbed as `rts`
- Assembly stubs: SortTeam = d0 param bridge. LoadSomeEuroCup = a0 bridge. AppendTeams = a2/a6/d7 three-param bridge. Dispatch/EntrySpec = a0/d1 bridges. HeaderSpec/AltHeaderSpec = a0 bridges. FatalError = jmp. TryAddRandom = dead rts.
- New xdefs (11): teamSortBuffer, teamIdSelectionProcessedFlags, euroCup, cupWinnersCup, uefaCup, careerRegionPickWeightsTable, careerRegionPickWeightsTable2, aDataEurocupTmd, aDataEurocwcTmd, aDataUefacupTmd
- ~350 lines asm code replaced with stubs
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 853 functions across 90 chunks

### Chunk 91 — Team/Player Lookup & National Selector Functions (12 exported → career_teams.c + teams.c + career_status.c)
- Date: 2026-02-22
- **Exported (12):**
  1. `_GetTeamPointer` → career_teams.c — look up team by ID through cache/TMD/file, applies foreign patches
  2. `_GetPlayer` → teams.c — player record ptr from team number + file index (teamBase + index*38)
  3. `_GetPlayerAtIndex` → teams.c — player ptr from team number + ordinal, remaps through position tables
  4. `_GetPlayerSlotIndexByPlayerNumber` → teams.c — reverse lookup: player number → slot index
  5. `_GetNationalityRecordPtr` → teams.c — search nationalTeams table by team file+ordinal
  6. `_GetNationalTeamNumber` → teams.c — search nationalTeams by nationality code, returns team number or -1
  7. `_GetNationalTeamNumberFromTeamFileId` → teams.c — search nationalTeams by team file ID
  8. `_GetNationalityAndTeamFile` → teams.c — returns packed (nationality<<16 | teamFileId), panics if not found
  9. `_IsTeamCodeOkForNationalSelector` → teams.c — check season mask for team code ('P' or other)
  10. `_IsPlayerOkForNationalSelector` → teams.c — check season masks A/B for player availability
  11. `_CalculateStartingBalance` → career_status.c — team value * 3 * random(80-119%) * 1000
  12. NotOkForNationalSelector — dead stub (callers ported to C), kept as `clr.w d0; rts`
- Assembly stubs: GetTeamPointer = d0 bridge (preserves d0-d7/a1-a6). GetPlayer = d0/d1 bridge (preserves d1). GetPlayerAtIndex = d0/d1 bridge (reads _cGetPlayerAtIndexFileIdx for d1 output). GetPlayerSlotIndexByPlayerNumber = d0/d1 bridge (preserves d0/a0). National functions = standard d2-d7/a2-a6 bridges. CalculateStartingBalance = result in d1.
- New xdefs (5): careerJobOfferMode, nationalSelectorSeasonMaskA, nationalSelectorSeasonMaskB, nationalTeams, numNationalTeams
- ~200 lines asm code replaced with bridges
- Fixes FindTeamInCache dead-stub bug: GetTeamPointer now calls C directly instead of dead asm FindTeamInCache
- Build status: **verified** — clean build, only pre-existing warnings

### Chunk 92 — Career Competition State & Team Selection Key Cache (12 exported → career_teams.c)
- Date: 2026-02-22
- **Exported (12):**
  1. `_Career_LoadCompetitionSlotStateFromBuffer` → career_teams.c — copies competition state from buffer to active area (3 sizes by type)
  2. `_Career_StoreCompetitionSlotStateToBuffer` → career_teams.c — reverse of Load
  3. `_Career_CopyCompetitionFileBufferToOrFromActive` → career_teams.c — wrapper: loads from competitionFileBuffer
  4. `_Career_CopyCompetitionContextToOrFromActive` → career_teams.c — wrapper: loads from competitionContext
  5. `_Career_CopyTertiaryCompetitionContextToOrFromActive` → career_teams.c — wrapper: loads from tertiaryCompetitionContext
  6. `_Career_CopyEuroCupsCompetitionContextToOrFromActive` → career_teams.c — wrapper: loads from euroCupsCompetitionContext
  7. `_TeamSelectionKeyCache_GetListPtr` → career_teams.c — walks cache by stride to find entry at index
  8. `_TeamSelectionKeyCache_GetListElementPtr` → career_teams.c — GetListPtr + elementIndex * 12
  9. `_TeamSelectionKeyCache_GetCurrentListPtr` → career_teams.c — cache base + offset
  10. `_TeamSelectionKeyCache_GetCurrentSelectedElementPtr` → career_teams.c — current entry + (byte[5]-byte[6])*12
  11. `_Career_UpdateCareerFinishedFlag` → career_teams.c — walks remaining entries to check if career done
  12. `_Career_AdvanceToNextTeamSelectionKeyCacheEntry` → career_teams.c — advance to next unprocessed entry, financial counters if career
- Assembly stubs: Load/Store = a0 param bridge. Copy* = void bridge. GetListPtr/GetCurrentListPtr = returns in a0. GetListElementPtr = calls GetListPtr for a0, computes a1 in asm. GetCurrentSelectedElementPtr = returns in a1. UpdateCareerFinished/AdvanceToNext = void bridges.
- New xdefs (5): careerFinished, careerSlotProcessingInProgress, teamSelectionKeyCacheOffset, teamSelectionKeyCacheIndex, Career_FillCurrentSeasonInfo
- ~200 lines asm code replaced with bridges
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 865 functions across 91 chunks

### Chunk 93 — Career Match Flow & Competition Slot Processing (6 exported → career_teams.c)
- Date: 2026-02-22
- **Exported (6):**
  1. `_CareerPlayMatch` → career_teams.c — post-match loop: processes remaining slots until season finished
  2. `_Career_PlayMatchMenuLoop` → career_teams.c — interactive match menu loop with slot skip/dispatch
  3. `_Career_NextMatch` → career_teams.c — advances to next match, dispatches by slot type
  4. `_Career_OpenCompetitionSlotMenuAndApplyChanges` → career_teams.c — opens competition menu (d2 param), updates state, rebuilds caches
  5. `_Career_ProcessCurrentCompetitionSlotForMatch` → career_teams.c — loads slot, dispatches auto-sim by type (league/cup/hybrid)
  6. `_Career_ProcessCurrentCompetitionSlotAfterMatch` → career_teams.c — loads slot, dispatches after-match processing by type
- Assembly stubs: 6 bridges (5 void, 1 with d2 param). 3 new C-callable asm wrappers (_asm_ShowDiy*_Career) for menu functions that take d0=1,d1=0,d2=param.
- New xdefs (18): careerCompetitionSlotPtrTable, careerMatchSimulatedFlag, 9 asm functions (Career_AutoSimulate*, Career_AutoPlay*, DIY_*), ShowDiyLeaguesTournamentMenu, ShowDIYCupMenuWithParams, DIY_EditCompetitionSlot, 3 _asm_* wrappers
- ~250 lines asm code replaced with bridges
- Build status: **verified** — clean build, only pre-existing warnings
- Cumulative: 883 functions across 93 chunks

### Chunk 94 — Career Auto-Simulate & DIY Competition Slot Advancement (8 → career_teams.c)
- Date: 2026-02-24
- **Exported (8):**
  1. `_Career_AutoSimulateLeagueFixturesUntilHumanTeam` → career_teams.c — loop: simulate league fixtures until human team encountered
  2. `_Career_AutoSimulateCupMatchesUntilHumanTeam` → career_teams.c — loop: simulate cup matches until human team encountered
  3. `_Career_AutoSimulatePendingMatchUntilHumanTeam` → career_teams.c — dispatches to league or cup auto-simulate by slot type
  4. `_Career_AutoPlayOrSimulateLeagueFixtureLoop` → career_teams.c — loop: play league fixtures until careerSlotProcessingInProgress set
  5. `_Career_AutoPlayCupMatchAndAdvanceLoop` → career_teams.c — loop: play cup matches until done, then calls DIY_UpdateCompetitionProgress
  6. `_Career_UpdateHybridCompetitionProgressAndMaybeAdvanceSlot` → career_teams.c — handles hybrid competition: dispatches cup/league loop, copies progress, may advance slot
  7. `_DIY_AdvanceToNextCompetitionSlot` → career_teams.c — advances to next competition slot, builds bracket/copies grid, loads new slot
  8. `getCurrentMatchTeamSlots` (static) → career_teams.c — C port of DIY_GetCurrentMatchTeamSlots, computes cup match team slot pointers
- 4 new C-callable asm bridges: `_asm_DIY_SimulateLeagueFixture`, `_asm_DIY_SimulateCupMatch`, `_asm_DIY_LoadCurrentCompetitionSlot`, `_asm_DIY_PostLoadCompetitionDispatch`
- 7 asm forwarding stubs replacing ~105 lines of asm code
- New xdefs: 4 asm bridges, diyMinSubstitutes; 7 xrefs for C functions
- Build status: **verified** — clean build, exit code 0
- Cumulative: 891 functions across 94 chunks

### Chunk 95 — Post-Match Stats, Trial Players, Roster Management (13+3s → career_status.c, teams.c)
- Date: 2026-02-24
- **Exported to career_status.c (8):**
  1. `_PlayerJoinedClubOnTrial` — sets plNameForMessage + currentMessage "joined on trial", calls _CShowChairmanMessage
  2. `_TrialPlayerLeftClubMessage` — sets plNameForMessage + currentMessage "trialist left", calls _CShowChairmanMessage
  3. `_ReservePlayerLeftClubMessage` — sets plNameForMessage + currentMessage "reserve left", calls _CShowChairmanMessage
  4. `_Career_UpdateSinglePlayerPostMatchStats` — handles form transitions ($9B→$9A, $9A→restored, in-form +2 clamped at 100)
  5. `_Career_ApplyPostMatchPlayerStatModsIfCareerTeam` — iterates 11 match players, applies stat mods via _Career_UpdateSinglePlayerPostMatchStats
  6. `_AddTrialPlayer` — picks random position, delegates to _TrainingMatch_AddTrialPlayerAtPosition
  7. `_TrainingMatch_AddTrialPlayerAtPosition` — finds unused slot, sets up trialist, inserts into currentMatchPlayers
  8. `_CheckPlayersLeavingComing` — manages trialist/reserve arrivals and departures between matches
- **Exported to teams.c (5+3s):**
  1. `_SetBenchPlayersNumbers` — assigns shirt numbers 12-16 to bench player slots
  2. `_Career_RemovePlayerFromSelectedTeamRoster` — removes player from starting/bench/reserve, finds replacement, compacts array
  3. `_RemovePlayerFromTeam` — wrapper: calls internal remove + roster remove + dirty flag + prune offers
  4. `findReplacementPlayerByPositionOrder` (static) — scans position range for best positional match, prefers trialists
  5. Position search order data tables (8 arrays, replPosSearchOrderTable)
  6. `SelectBenchReplacementForStartingPlayer` (dead stub) — only called from now-C code
  7. `SelectReserveReplacementForBenchPlayer` (dead stub) — only called from now-C code
  8. `FindReplacementPlayerByPositionOrder` (dead stub) — only called from now-C code
- 3 new C-callable asm bridges: `_CCareer_GenerateTrialistForTeamPlayer`, `_CCareer_ConvertPlayerToTrialistFromName`, `_CTrainingMatch_SaveSelectedPlayerId`

### Chunk 96 — DIY Competition Slot Editing & Progress (7+1s → diy_competition.c)
- Date: 2026-02-24
- **Exported to diy_competition.c (7+1s):**
  1. `_DIY_EditCompetitionSlot` — routes to league/cup editor with paramA/paramB/paramC, handles career vs DIY paths
  2. `_DIY_ReorderTeamsBySelectionFlags` — reorders teams by selection bit flags in competition context
  3. `_DIY_UpdateCompetitionProgress` — advances competition round progress (cup bracket or league fixture)
  4. `_DIY_RecordCupBracketProgress` — processes cup bracket results, handles second leg, qualifiers
  5. `_DIY_UpdateCompetitionProgressOrAdvanceSlot` — dispatches by competition type (cup/league/custom)
  6. `_DIY_RecordLeagueFixtureResultAndAdvanceProgress` — processes league fixture results, advances progress
  7. `_DIY_RebuildCompetitionAndTeamSelectionCaches` — rebuilds team selection caches from competition context
  8. `diy_ResetCompetitionStatsAndProgress` (static) — resets all team stats for new round
- 3 new C-callable asm bridges: `_asm_Career_AutoAdvanceInternationalCompetitionFixtures`, `_asm_ShowDiyLeaguesTournamentMenu`, `_asm_ShowDIYCupMenuWithParams`
- Fixed LTO type mismatch: `_AppendTeamsToSelectionFromTmdOrTeamFile` extern in diy_competition.c corrected to match actual signature
- Fixed memset undefined: replaced byte loop with 8 explicit uint32_t stores
- 13 asm stubs (3 forwarding, 3 dead + 7 dead helper stubs) replacing ~400 lines of asm code
- New xdefs: removedPlayerStatAccum, plNameForMessage, 3 asm bridges; 11 xrefs for C functions
- Build status: **verified** — clean build, exit code 0
- Cumulative: 907 functions across 95 chunks

### Chunk 97 — DIY Competition Setup Functions (8+2s → diy_competition.c)
- Date: 2026-03-01
- **Exported to diy_competition.c (8):**
  1. `_DIY_LoadCurrentCompetitionSlot` — loads competition data from file buffer slot into active competition structure (league or cup path)
  2. `_DIY_ResetAndLoadCurrentCompetitionSlot` — resets competition state, seeds random, initializes team order, loads slot
  3. `_DIY_InitCupFromTeamGridAndValidateTeams` — initializes cup from team grid, validates all teams loaded, shuffles pairs
  4. `_DIY_ShuffleTeamsAndBuildCompTeamList` — shuffles/seeds teams then copies to competition team list, verifies loaded
  5. `_DIY_BuildTeamGridAndIndexMaps` — builds team grid with seeded ordering, then builds team index and group maps
  6. `_DIY_CopyTeamGridFromCompToDIY` — copies team grid from comp to diy after home/away swap and world cup adjustments
  7. `_DIY_BuildSeededBracketAndUpdateScores` — builds seeded bracket with scoring for team strength and group position
  8. `_DIY_AdjustWorldCupBracketScores` — adjusts seeding scores for World Cup format
- **Static helpers (2):** buildCompSpecificTeamShuffle (bracket seeding shuffle variant), buildCompSpecificTeamList (bracket seeding list variant)
- **Static data (1):** diyCompSpecificShuffle4 — 4-byte shuffle order extracted from sub_129960 machine code bytes
- Assembly stubs:
  - `DIY_LoadCurrentCompetitionSlot`: save-all bridge (movem.l d0-d7/a0-a6)
  - `DIY_ResetAndLoadCurrentCompetitionSlot`: jmp stub
  - `DIY_InitCupFromTeamGridAndValidateTeams`: jmp stub
  - `DIY_ShuffleTeamsAndBuildCompTeamList`: register bridge (a2,a3,a4→stack)
  - `DIY_BuildTeamGridAndIndexMaps`: register bridge (a2,a3,a5→stack)
  - `DIY_CopyTeamGridFromCompToDIY`: register bridge (a2,a3,a4→stack)
  - `DIY_BuildSeededBracketAndUpdateScores`: register bridge (a2,a3,a4,a5→stack)
  - `DIY_AdjustWorldCupBracketScores`: register bridge (a2,a3,a5→stack)
  - `DIY_BuildCompSpecificTeamShuffle`: rts (dead, callers now in C)
  - `DIY_BuildCompSpecificTeamList`: rts (dead, callers now in C)
- Removed C-to-asm bridges: `_CDIY_BuildSeededBracketAndUpdateScores`, `_CDIY_CopyTeamGridFromCompToDIY` — callers in diy_competition.c and career_teams.c updated to call C functions directly
- New xdefs: diyTeamShuffleScratch, diyShuffleOrder8Teams, diyShuffleOrder4Teams, diyTournamentShuffleOrder8, diyLeagueTeamShuffleMap, diyGroupPatternLookup, worldCup
- New xrefs: 8 C functions
- Data labels preserved: diyTeamShuffleScratch (64 bytes), diyShuffleOrder8Teams (8 bytes), diyShuffleOrder4Teams (4 bytes), diyLeagueTeamShuffleMap (16 bytes), diyGroupPatternLookup (76 bytes), diyTournamentShuffleOrder8 (8 bytes), sub_129960 code bytes (kept as asm)
- Build status: **verified** — clean build, exit code 0
- Cumulative: 925 functions across 97 chunks

### Chunk 98 — Play Match Menu Functions (29+1s → play_match_menu.c)
- Date: 2026-03-01
- **New module:** `src/play_match_menu.c` + `include/play_match_menu.h`
- **Exported functions (29):**
  1. `_GetPlayerPosition` — reverse lookup: given team + player file index, return position slot
  2. `_SetCurrentSelectedPlayerIndexPointer` — determine which player index pointer to use based on isFirstPlayerChoosingTeam/viewingOpponentTeam
  3. `_UpdateCheckMarkSprite` — compare line-up skills, show big check/X mark
  4. `_UpdateLittleCheckmark` — copy positions array and update check mark
  5. `_SavePositionsAndUpdateLittleCheckmark` — save positions, set team changed flag, compute old skill
  6. `_SetGameStartButtonText` — set play/view/coach match button text and color
  7. `_ShowUseMatchTeamIfTraining` — show/hide use match team button for training games
  8. `_ShowOrHidePlayMatchButton` — complex visibility logic for play match button
  9. `_UpdateTeamCheckmark` — show/hide blinking checkmark next to selected player
  10. `_SetUpTacticsButtons` — set colors/disabled state of 18 tactics buttons
  11. `_SetPlayerSurnamesOnSidePitch` — draw player surnames on side pitch diagram
  12. `_SetVariableButtons` — set view opponent/team buttons, coach/player label, team name, etc.
  13. `_PlayMatchMenuReinit` — reinitialize menu on return (set player count, hide extra entries)
  14. `_PlayMatchMenuInit` — full menu initialization (IRQ check, clear flags, set CPU teams flag)
  15. `_OnSwapPlayer` — handle player swap on fire press (CPU or player team paths)
  16. `_PlayMatchMenuSwapPlayers` — swap two players in squad (positions, shirts, match players)
  17. `_HandleSquadSwapSelectionToggle` — handle long fire selection toggle for squad management
  18. `_PlayMatchChangeTactics` — change tactics when tactics button pressed
  19. `_ExitPlayMatch` — set exit flag, clear player flags, set isCareer
  20. `_PlayMatchSelected` — handle play match button press (set playerNumThatStarted, exit menu)
  21. `_EditTacticsInPlayMatchMenu` — open tactics editor from play match menu
  22. `_ViewOpponentsSelect` — toggle between viewing own team and opponent/training B team
  23. `_ReturnFromViewOpponent` — return from viewing opponent
  24. `_PlayMatchOnSelect30` — rotate players left, redraw entries
  25. `_PlayMatch30AfterDraw` — draw entry after play match 30 draw
  26. `_RedrawPlayerNames` — redraw 11 player entries plus first entry
  27. `_UseMatchTeamSelect` — restore match team for training, rebuild team state
  28. `_TogglePlayerCoach` — toggle player/coach mode for career or regular game
  29. `_DrawPlayerEntries` — comprehensive player row rendering (colors, status, names, skills, values)
- **Static helper (1):** GetPlayerPosition wrapper logic (inline in _GetPlayerPosition)
- **C-callable asm bridges (5):** _CFormTopSkillsString, _CFormatPlayerValueOrStatusText, _CSetSquadMenuEntryStatusSprite, _CTrainingMatch_BuildCurrentTeamFromCareerTeam, _CTrainingMatch_ApplyUseMatchTeamAndSwapSubs
- Assembly stubs: all 29 functions replaced with movem.l save/restore + jsr bridges; PlayMatchMenuSwapPlayers as dead rts stub (only called from C)
- New xdefs: exitGameFlag + all chunk 98 data symbols
- New xrefs: 29 C functions + 5 C-callable bridges
- Fix: exitGameFlag was missing from xdef, causing link error
- Fix: selTeamsPtr extern changed from `uint8_t` to `uint8_t[]` to fix overflow warning
- Build status: **verified** — clean build, links successfully
- Cumulative: 954 functions across 98 chunks

### Chunk 99 — Job Offers Menu Functions (8 → job_offers.c)
- Date: 2026-03-01
- **Extended module:** `src/job_offers.c` + `include/job_offers.h`
- **Exported functions (8):**
  1. `_GotJobOfferInit` — initialize job offer menu: set up teamNoOfferingJob, jobOfferFunds, call GotJobOfferFinish
  2. `_GotJobOfferFinish` — set up job offer display text (new offer vs accepted, domestic vs international)
  3. `_AcceptJobOffer` — accept a job offer: set teamNoOfferAccepted/teamFundsAccepted, clear offer queues, update season
  4. `_j_SetExitMenuFlag_14` — tail-call to SetExitMenuFlag
  5. `_RejectJobOffer` — reject current job offer: remove from appropriate queue
  6. `_ViewFutureTeam` — show future team squad via ShowSquad
  7. `_ViewFutureTeamLeague` — show future team's league competition slot
  8. `_ConsiderJobOffer` — convert new offer to old offer, exit menu
- **Static helpers (2):** `printToEntry` (format string into menu entry text), `getCurrentSeasonPointer` (compute season info pointer)
- **C-callable asm bridges (3):** `_CInt2Pounds`, `_CShowSquad`, `_CCareer_ViewOrEditCompetitionSlot`
- Assembly stubs: all 8 functions replaced with movem.l save/restore + jsr bridges
- New xdefs: jobOffersFromRetired, jobOfferOrdinal, oldJobOfferOrdinal, teamNoOfferingJob, teamOfferingJobPtr, jobOfferFunds, viewFutureTeamModeFlag, competitionsTable, seasonInformationsArray, teamFundsAccepted, 27 string constants
- New xrefs: 8 C functions + 3 C-callable bridges
- Fix: ViewFutureTeam changed from string literal "VIEW TEAM" to extern aViewTeam
- Build status: **verified** — clean build, links successfully
- Cumulative: 962 functions across 99 chunks

### Chunk 100 — Number Formatting & Menu Utility (6 → string_utils.c)
- Date: 2026-03-01
- **Extended module:** `src/string_utils.c` + `include/string_utils.h`
- **Exported functions (5):**
  1. `_Int2Pounds` — format unsigned value as "£123,456"
  2. `_Int2PoundsSigned` — format signed value as "- £123,456" or "£123,456"
  3. `_WriteAccountBalance` — format signed value with +/- prefix: "+ £123" or "- £123"
  4. `_GetBankBalance` — format bank balance as "BANK  £123,456"
  5. `_PrepareMenuForFadeIn` — prepare menu for fade-in transition
- **Static helpers (1):** `int2StrWithCommas` — format number with comma separators (e.g. 1,234,567)
- Assembly stubs: all 5 functions replaced with register bridges (d0/a1 → stack params)
- Note: `Int2StrWithCommas`, `Int2AsciiLessThan100Check`, `Int2AsciiSaveD0` are now dead code (only called by old `Int2Pounds` body)
- New xdefs: aPound, nullString_0, aBank
- Fix: added `int32_t` typedef to string_utils.h
- Build status: **verified** — clean build, links successfully
- Cumulative: 968 functions across 100 chunks

### Chunk 101 — ClubBusinessInit & Career Season Finance Init (2+3s → career_status.c)
- Date: 2026-03-01
- **Extended module:** `src/career_status.c` + `include/career_status.h`
- **Exported functions (2):**
  1. `_ClubBusinessInit` — initializes Club Business menu display (3 paths: contract negotiation active, not staying in old club, staying in old club with sponsorship)
  2. `_Career_InitializeNewSeasonFinances` — recalculates wages, resets accumulators, handles sponsorship randomization and competition bonuses
- **Static helpers (3):** `computeTotalProfitLoss`, `formatNewBalanceEntry`, `bgColor_cs`/`setBgColor_cs` (inlines)
- Assembly stubs: both functions replaced with save-all forwarding stubs
- New xdefs: contractNegotiationActive, sensisoftPlcSponsorshipFactor, sensisoftPlcSponsorship, additionalInvestments, aTotalLoss, aBalanceAtStart, aBalanceAtEndOf, aSponsorshipFor, aSeasonFromSens, aAdditionalInve
- Added includes: menu_system.h, string_utils.h to career_status.c
- Build status: **verified** — clean build, links successfully
- Cumulative: 970 functions across 101 chunks

### Chunk 102 — Match Team Init & Play (3+1s → match_sim.c)
- Date: 2026-03-01
- **Extended module:** `src/match_sim.c` + `include/match_sim.h`
- **Exported functions (3):**
  1. `_InitTeams` — initializes all selected teams (clears cards, injuries, goal stats); also inits career team with 32 players if applicable
  2. `_PlayFriendly` — sets up team pointers and calls InitAndPlayGame for a friendly match
  3. `_CheckContinueAbortPrompt` — shows continue/abort prompt based on gameType, returns 0 (abort) or non-zero (continue)
- **Static helpers (1):** `teamInit` — per-team player field reset loop
- Assembly stubs: 3 functions replaced with forwarding stubs, TeamInit made dead (`rts`)
- New C-callable bridge: `_CInitAndPlayGame` (a1=team1, a2=team2)
- New xdefs: aDiyCompetitionPrompt, aPresetCompetitionPrompt, aSeasonPrompt, aCareerPrompt

### Chunk 103 — Game Init Player Utilities (6+2d → game_init.c)
- Date: 2026-03-01
- **Extended module:** `src/game_init.c` + `include/game_init.h`
- **Exported functions (6):**
  1. `_UpdatePlayerInjuries` — copies injury bitfield from in-game PlayerInfo back to PlayerFile for non-computer teams
  2. `_AccumulateMatchGoalsIntoTeamFile` — adds own-goals count to TeamFile, accumulates per-player goals into goalStats
  3. `_SetPlayerNameAndFace` — copies full player name and extracts face index from PlayerFile to PlayerInfo
  4. `_InitInGamePlayer` — full player init: short name, face, position, substitution status, cards, injuries, shirt number
  5. `_ApplyPlayerStatusAfterSim` — applies card/injury effects from in-game state back to PlayerFile after simulated match
  6. `_UpdateCareerPlayerStatsAfterMatch` — calls Career_UpdateSinglePlayerPostMatchStats for each player who played full match
- **Dead code stubbed (2):** `sub_11FBF6` (hex long formatter, never called), `PanicLookupMessagePtr` (hex word formatter, already ported as static in panic.c)
- Assembly stubs: 6 functions replaced with register-passing bridges, 2 dead functions stubbed to `rts`
- Key pattern: PlayerInfoHeader uses 42-byte prefix before PlayerInfo fields; bridge stubs add `lea PLAYERINFOHEADER_SUBSTITUTED(a6),a0` to pass adjusted PlayerInfo pointer

### Chunk 104 — InitInGameTeamStructure (1 → game_init.c)
- Date: 2026-03-01
- **Extended module:** `src/game_init.c` + `include/game_init.h`
- **Exported functions (1):**
  1. `_InitInGameTeamStructure` — full team game structure initialization: computes price %, loops 16 players calling GetPlayerAtIndex, InitInGamePlayer, GetPlayerInfoBreakPosition, SetPlayerNameAndFace, clears skill fields, copies team name
- Assembly stub: register-passing bridge (a2=TeamFile*, a4=TeamGame*, a6=int16_t* avgPricePct)
- New xdefs: avgSelectedTeamsPlayerPrice, team1AppPercent, team2AppPercent
- Build status: **verified** — clean build, links successfully
- Cumulative: 973 functions across 102 chunks

### Chunk 105 — Skill Adjustment & Team Tactics (4 exported + 3 dead → match_sim.c)
- Date: 2026-03-01
- **Extended module:** `src/match_sim.c` + `include/match_sim.h`
- **Exported functions (4):**
  1. `_ScaleSkill` — scales raw 0-7 skill nibble by factor × team appearance percentage, with random fractional rounding
  2. `_GetPlayerPricePercentageChange` — adds per-player random (d0 from caller), price rank offset, and [-12,+11] random to price change factor; skips adjustments for friendly or player+coach teams
  3. `_AdjustPlayerSkills` — full player skill adjustment: calculates price factor (goalkeeper=file price, field player=GetPlayerPrice), percentage vs file price, bottom-team penalty, scales 7 skills via ScaleSkill, computes goalie skill with team-control-based clamping
  4. `_ApplyTeamTactics` — loops 11 starting players: temporarily overrides tactics byte, gets player file via GetPlayer, saves/restores random state, seeds per-player random, calls AdjustPlayerSkills
- **Dead code stubbed (3):** `AdjustPlayerSkills` (asm), `GetPlayerPricePercentageChange` (asm), `ScaleSkill` (asm) — all only called from ApplyTeamTactics which is now ported
- Assembly stubs:
  - `ApplyTeamTactics`: save-all bridge pushing a4 (TeamGame*) → _ApplyTeamTactics
  - 3 dead functions stubbed to `rts`
- New C-callable bridge: `_CGetPlayerPrice` (a0=teamFile, a1=playerBase, d7=playerNumber → d7=price, returned via d0)
- New xdefs: gameTeam1, gameTeam2, _CGetPlayerPrice
- New xrefs: _ApplyTeamTactics
- Key porting discovery: asm d0 register carries GetPlayerRandomNum^gameRandValue from ApplyTeamTactics through to GetPlayerPricePercentageChange as an intentional additional random factor (Ghidra confirmed as `unaff_D0w`). Passed explicitly as `callerD0` parameter in C.
- Build status: **verified** — clean build, links successfully
- Cumulative: 986 functions across 105 chunks

### Chunk 106 — BuildMatchResultString (1 → match_sim.c)
- Date: 2026-03-01
- **Extended module:** `src/match_sim.c` + `include/match_sim.h`
- **Exported functions (1):**
  1. `_BuildMatchResultString` — formats match result string from scores/flags. Selects from 19 format strings based on bit flags (two-leg aggregate, extra time, penalties, away goals, replay). Supports dual format strings (wide/narrow) via control parameter that skips past first null terminator.
- Assembly bridge: saves d0-d7/a1-a6, pushes 7 args (control/d0, leg2/d4, leg1/d5, pens/d6, teamA/a1, teamB/a2, flags/d7), returns via a0=d0
- New xdefs: resultString, 19 format string labels (a1stLeg01, aAWin23OnAgg, etc.)
- Build status: **verified**
- Cumulative: 987 functions across 106 chunks

### Chunk 107 — Team Selection Callbacks (4 → teams.c)
- Date: 2026-03-01
- **Extended module:** `src/teams.c` + `include/teams.h`
- **Exported functions (4):**
  1. `_DeselectAllTeams` — loops selected teams, deselects those with teamControls==0
  2. `_ChangeTeams` — deselects all, sets changeTeams=1, clears others, exits menu
  3. `_AbortChooseTeamsFriendly` — deselects all, sets abortSelectTeams=1, exits menu
  4. `_ChooseTeamsFriendlyOnSelect76` — deselects all, sets chooseTeamsResult=1, exits menu
- Assembly stubs: DeselectAllTeams=save-all bridge, 3 callbacks=jmp to C
- New xdefs: changeTeams, chooseTeamsResult, viewSelectedTeams, abortSelectTeams, exitSelectTeams
- Build status: **verified**
- Cumulative: 991 functions across 107 chunks

### Chunk 108 — Menu Callbacks Batch (10 → teams.c, diy_competition.c, career_teams.c)
- Date: 2026-03-01
- **Exported functions (10):**
  - teams.c (3): `_ViewSelected`, `_ExitChooseTeams2`, `_ChooseTeamsPlayMatchSelected`
  - diy_competition.c (4): `_SelectDiyLeagueMenu`, `_SelectDiyCupMenu`, `_SelectDiyTournamentMenu`, `_SelectDiyExitMenu`
  - career_teams.c (3): `_NationalTeamAddPlayerHomeOnSelect`, `_NationalTeamAddPlayerAbroadOnSelect`, `_NationalTeamAddPlayerExit`
- All are simple flag-setters + _SetExitMenuFlag() calls
- New xdefs: topOfStackMark, diySelected, careerNationalTeamAddPlayerType
- Build status: **verified**
- Cumulative: 1001 functions across 108 chunks

### Chunk 109 — Team Navigation Stack & Contest Checking (10 → teams.c)
- Date: 2026-03-01
- **Extended module:** `src/teams.c` + `include/teams.h`
- **Exported functions (10):**
  1. `_GetCountryName` — looks up country name from countriesTable, skips length byte
  2. `_PushContinent` — pushes country id onto continent navigation stack
  3. `_PopContinent` — pops country id, writes CN_OCEANIA sentinel
  4. `_SetSelectedTeamsStackTop` — writes 0xFFFF at current depth
  5. `_PushLastSelectedTeam` — stores lastSelectedTeam at current depth
  6. `_GetSelectedTeamsStackTop` — reads stack into prevSelectedTeamEntry
  7. `_MarkSelectedTeamsStackLocation` — sets topOfStackMark based on isCountryNumber, gotCountryName, numberOfLeagues, isContestContinentalCup
  8. `_IsContestContinentalCup` — checks selectedContest against continentalCups table
  9. `_IsClubTeamsContest` — linear search in nationalTeamCompetitionsAndQualifications; returns 0=national, 1=club
  10. `_SetTeamsTypeForSelectedContest` — sets teamsType based on IsClubTeamsContest result
- Assembly bridges: register bridges for param/return conversion; IsClubTeamsContest bridge includes `tst.w d0` for Z flag
- New xdefs: countriesTable, curCountryStr, countryStackPtr, teamDepthLevel, selectedTeamsStack, lastSelectedTeam, prevSelectedTeamEntry, isCountryNumber, gotCountryName, numberOfLeagues, isContestContinentalCup, continentalCups, selectedContest, teamsType, nationalTeamCompetitionsAndQualifications
- Build status: **verified**
- Cumulative: 1011 functions across 109 chunks

### Chunk 110 — Manager Setup Menu Callbacks (6 → career_init.c)
- Date: 2026-03-01
- **Extended module:** `src/career_init.c` + `include/career_init.h`
- **Exported functions (6):**
  1. `_SetPlayerCoachFieldColor` — updates bg color palette index for menu entry 9 based on coachOrPlayer toggle
  2. `_CheckSex` — copies "MR" or "MS" to managerSexBuffer based on mrOrMs
  3. `_ChangeSex` — toggles mrOrMs, calls CheckSex, redraws current menu item
  4. `_SetName` — starts text input for first name (max 9 chars), uses entry->fg
  5. `_SetSurname` — starts text input for surname (max 13 chars), uses entry->fg
  6. `_CoachOrPlayerCoach` — toggles coachOrPlayer, updates color, redraws menu item
- Assembly bridges: save-all + push a5 (MenuEntry*) for callbacks that need current entry
- New xdefs: coachOrPlayer, mrOrMs, managerSexBuffer, aMr, aMs
- Build status: **verified**
- Cumulative: 1017 functions across 110 chunks

### Chunk 111 — Squad Menu Callbacks (5 → menus.c)
- Date: 2026-03-01
- **Extended module:** `src/menus.c` + `include/menus.h`
- **Exported functions (5):**
  1. `_SelectTraining` — calls PlayTrainingGame, reactivates squadMenu, sets entry 9
  2. `_SelectGoals` — enters goals/clubs view mode, saves positions, updates menu state
  3. `_SquadOnSelect11` — returns from goals view to normal squad view
  4. `_SquadOnSelect45` — rotates players left by 5, redraws items 23,44
  5. `_Squad45AfterDraw` — draws menu entry 44
- Assembly stubs: simple `jmp _CFunc` for all
- New xdefs: PlayTrainingGame, SquadMenuSavePlayerPositions, SquadUpdateMenuState, squadMenu, squadPrevSelectedPlayerIndex, isSquadViewMode, squadTeamFilePtr
- Build status: **verified**
- Cumulative: 1022 functions across 111 chunks

### Chunk 112 — Squad Operations (4 → menus.c)
- Date: 2026-03-01
- **Extended module:** `src/menus.c` + `include/menus.h`
- **Exported functions (4):**
  1. `_UpdateBigCheckMark` — updates check mark sprite on entry 1 based on skill diff
  2. `_SquadRedrawPlayerListAndCheckMark` — redraws entries 28-38 and entry 1
  3. `_SquadMenuSavePlayerPositions` — copies positions array + calls UpdateBigCheckMark
  4. `_SquadCommitPositionsAndUpdateSkill` — copies positions, marks team file, computes skill
- Note: Used volatile byte copy to prevent GCC from emitting memcpy (no libc)
- Assembly bridge: SquadMenuSavePlayerPositions preserves all regs (movem save/restore)
- New xdefs: teamSkillDiffAfterChange
- Build status: **verified**
- Cumulative: 1026 functions across 112 chunks

### Chunk 113 — Options + Transfer Menu Callbacks (15 → menus.c, transfer_market.c)
- Date: 2026-03-01
- **Options callbacks in menus.c (7):**
  1. `_OptionsMenuBeforeDraw` — clears showPrereleaseCounter
  2. `_ChangePitchType` — cycles pitchType (0-6, then 0xFFFE/RANDOM)
  3. `_ChangeGameLength` — cycles gameLength (0-3)
  4. `_ChangeAutoReplays` — toggles autoReplays
  5. `_ChangeAllPlTeamsEqual` — toggles allPlayerTeamsEqual
  6. `_ChangeAutoSaveHighlights` — toggles autoSaveHighlights
  7. `_ChangeMenuMusic` — toggles menuMusic, restarts music
- **Transfer callbacks in transfer_market.c (8):**
  1. `_TransferPlayerOnSelect10` — reject transfer (0xFFFF)
  2. `_TransferPlayerOnSelect11` — accept transfer (0)
  3. `_TransferPlayerOnSelect12` — counter-offer (1)
  4. `_AcceptOffer` — accept third-party offer (1)
  5. `_RejectOffer` — reject third-party offer (2)
  6. `_AskForMore` — negotiate offer (3)
  7. `_ConsiderOffer` — consider offer (0)
  8. `_ThirdPartyOfferMenuExit` — exit offer menu (0)
- Assembly bridges: options use `move.l a5,-(sp); jsr; addq.l #4,sp; rts` (pass MenuEntry*), transfer use `jmp`
- New xdefs: showPrereleaseCounter, transferPlayerMenuResult, thirdPartyOfferResult
- Build status: **verified**
- Cumulative: 1041 functions across 113 chunks

### Chunk 114 — Simple Menu Exit/Draw Callbacks (9 → menus.c)
- Date: 2026-03-01
- 9 trivial menu callbacks:
  1. `_CompetitionTopGoalScorersExit` — SetExitMenuFlag
  2. `_TopGoalScorersExit` — SetExitMenuFlag
  3. `_BuyPlayerExit` — SetExitMenuFlag
  4. `_ViewSquadExitContinue` — viewSquadSelectedPlayer=0xFFFF, SetExitMenuFlag
  5. `_ViewSquad21AfterDraw` — DrawMenuItem(&menuEntries[20])
  6. `_SelectNationalTeamExit` — selectNationalTeamMenuResult=0xFFFF, SetExitMenuFlag
  7. `_SelectNationalTeam25AfterDraw` — DrawMenuItem(&menuEntries[24])
  8. `_SelTeamForTacticsExit` — teamSelected=0xFFFF, SetExitMenuFlag
  9. `_AbortSaveFile` — selectedFilename[0]=0, SetExitMenuFlag
- Assembly stubs: all `jmp _CFunc`
- New xdefs: viewSquadSelectedPlayer, selectNationalTeamMenuResult, teamSelected, selectedFilename
- Build status: **verified**
- Cumulative: 1050 functions across 114 chunks

### Chunk 115 — CareerOver + ViewSquad + National Callbacks (3 → menus.c)
- Date: 2026-03-01
- 3 functions:
  1. `_CareerOverInit` — conditional SetCurrentEntry based on currentSeasonShowing, numNewJobOffers, numJobOffers, youreSacked
  2. `_ViewSquadOnSelect21` — RotatePlayersLeft + DrawMenuItem(&menuEntries[20])
  3. `_SelectNationalTeamOnSelect25` — RotatePlayersLeft(careerTeam, 5) + DrawMenuItem(&menuEntries[24])
- Assembly stubs: all `jmp _CFunc`
- New xdefs: squadTeamPtr
- Build status: **verified**
- Cumulative: 1053 functions across 115 chunks

### Chunk 116 — OpenDiy* + Menu Navigation + Job Offers (13 → diy_competition.c, menus.c, job_offers.c)
- Date: 2026-03-01
- **diy_competition.c (5):** _ShowDiyCompetitionForSource, _OpenDiyFromFile, _OpenDiyPreset1, _OpenDiyPreset2, _OpenDiyPreset3
- **menus.c (3):** _SWOSMainMenuInit, _ManagementRecordMenu, _ViewCompetitionsMenuRetired
- **job_offers.c (5):** _JobOffers_UpdateMenuEntryLayout, _JobOffersFinish, _JobOffersCommon, _OldJobOffersCommon, _JobOffersExit
- ShowDiyCompetitionForSource bridge: push a0, call C, clean stack (asm callers pass ptr in a0)
- JobOffersCommon/OldJobOffersCommon bridges: push a5 (MenuEntry*) on stack
- Build status: **verified**
- Cumulative: 1066 functions across 116 chunks

### Chunk 117 — Simple Standalone Functions (6 → bench_subs.c, transfer_market.c, edit_tactics.c)
- Date: 2026-03-01
- **bench_subs.c (2):** _ShowFormationMenu (set bench state globals), _SubsBenchOff (clear bench state)
- **transfer_market.c (3):** _TransferMenuSetBankState (get bank balance to entry 1), _BuyPlayerInit (same), _TransferMarketFinish (refresh all transfer menu state)
- **edit_tactics.c (1):** _EditTeamsFinish (call InitEditTeamsMenu, skip nullsub_16)
- All stubs: `jmp _CFunc`
- New xdefs: InitEditTeamsMenu, CheckNumPlayersOnTransferMarket, CheckIfPlayerSelected, EnableDisableOffers
- Build status: **verified**
- Cumulative: 1072 functions across 117 chunks

### Chunk 118 — CommonMenuExit (1 → menus.c)
- Date: 2026-03-01
- **menus.c (1):** _CommonMenuExit — critical menu init function used by ~20 callers
  - Switches on gameType (0=none, 1=DIY, 2=preset, 3=season, 4=career)
  - No competition: hides entries 11/12, copies y from 11 to 13
  - Active: formats "CONTINUE %a"/"REPLAY %a" into entry 11 fg, "SAVE %a" into saveFileTitle
  - Uses _PrintFormatted with %a string substitution
- Stub: `jmp _CommonMenuExit`
- New xdefs: diyPresetNameOfCompetition, aContinueA, aReplayA, aSaveA, aSeason_1, aCareer_1, saveFileTitle
- Build status: **verified**
- Cumulative: 1073 functions across 118 chunks

### Chunk 119 — Menu Callbacks + ShowFriendlyMenu + ShowSelectRepresentationMenu + Career_HandleRetirement (9 → menus.c, career_status.c)
- Date: 2026-03-01
- **menus.c (8):** _FriendlyMenu (seed RNG + Friendly + CommonMenuExit), _EditCustomTeamMenu, _EditTacticsMenu, _HighlightsMenu, _OptionsMenuSelected, _SaveDiskFilingMenu (all simple: call function + CommonMenuExit), _ShowFriendlyMenu (init friendly defaults + ShowMenu), _ShowSelectRepresentationMenu (returns d0)
- **career_status.c (1):** _Career_HandleRetirement (retired=1, 5 career update calls)
- All stubs: `jmp _CFunc` (ShowSelectRepresentationMenu returns d0 via GCC convention)
- Seed fix: `seed` + `randXorIndex` split into two byte writes (big-endian word write)
- New xdefs: Friendly, EditCustomTeam, EditTactics, highlightsMenu, optionsMenu, diskMenu, friendlyGameMenu, pitchTypeOrSeason, friendlyPitchType, friendlySeason, minSubstitutes, maxSubstitutes, friendlyOk, selectNationalTeamMenu, retired, Career_MaybeGenerateJobOffer
- Build status: **verified**
- Cumulative: 1082 functions across 119 chunks

### Chunk 120 — DIY Design Menu Functions + SetSelectTwoTeamsText (15 → diy_competition.c, menus.c)
- Date: 2026-03-01
- **diy_competition.c (14):**
  - League: _ShowDesignDIYLeagueMenu (save + init + show), _ShowLeagueInfo (view-only), _DesignDIYLeagueOK (set flag + activate), _SetNumberOfTeamsToSelect (PrintFormatted "SELECT N TEAMS" entry 23)
  - Cup: _ShowDesignDIYCupMenu (copy buffer + init + show), _ShowCupInfo (view-only), _DesignDIYCupOK (set flag + activate), _DesignDIYCup_UpdateMinSubstitutesAndPointsStrings (Int2Ascii entries 13/15), _DesignDIYCup_UpdateSelectTeamsPrompt (PrintFormatted entry 51)
  - Tournament: _ShowDesignDIYTournamentMenu (save + init + show), _ShowTournamentInfo (view-only), _DesignDIYTournamentOK (set flag + activate), _DesignDIYTournament_UpdateTwoNumericFieldsText (Int2Ascii entries 11/13 from diyFileBufferCopy), _DesignDIYTournament_UpdateSelectTeamsPrompt (PrintFormatted entry 92)
- **menus.c (1):** _SetSelectTwoTeamsText (PrintFormatted "SELECT 2 TEAMS" entry 10)
- All stubs: `jmp _CFunc`
- New xdefs: diyLeagueLookOnly, diyLeagueSelectTeams, designDiyLeagueMenu, diyCupLookOnly, diyCupSelectTeams, designDiyCupMenu, diyTournamentLookOnly, diyTournamentSelectTeams, designDiyTournamentMenu, aSelect0Teams
- Build status: **verified**
- Cumulative: 1097 functions across 120 chunks

### Chunk 121 — FriendlyOK + ViewSelectedTeams + ShowDomesticPlayerMarket + ContestMenuScroll (5 → menus.c, transfer_market.c)
- Date: 2026-03-01
- **menus.c (4):** _FriendlyOK (set flag + ActivateMenu), _ViewSelectedTeams (set flag + clear 4 globals + SetExitMenuFlag), _ContestMenuScrollUp (scroll + rebuild + redraw 15), _ContestMenuScrollDown (same)
- **transfer_market.c (1):** _ShowDomesticPlayerMarket (update home market + show menu)
- All stubs: `jmp _CFunc`
- New xdefs: viewSelectedTeams, chooseTeamsResult, changeTeams, abortSelectTeams, exitSelectTeams, showDomesticPlayerMarket, showPlayerMarketMenu, ScrollUpInGameList, ScrollDownInGameList, BuildGameListArrayFromTeamSelectionKeyCache
- Build status: **verified**
- Cumulative: 1102 functions across 121 chunks

### Chunk 122 — Contest Menu Helpers (15 → menus.c)
- Date: 2026-03-01
- **menus.c (15):** _ScrollUpInGameList, _ScrollDownInGameList, _RestoreContestScrollPosition, _SelectActiveContestAndRound, _CheckClubBusiness, _CheckJobOffers, _ContestMenuOnReturn_SetButtonsAndDefaultSelection (a5 bridge), _SeasonLowerButtons, _CheckTransfers, _BuildGameListArrayFromTeamSelectionKeyCache, _PrintNameAndSeason, _SetNationalSelectorButtons, _ContestSetButtons, _ContestMenuInit (tamper check), _ContestMenuOnReturn
- Key patterns: menuEntries[N] indexed access (entries 0-18), color manipulation (bg & 0xFFF0 + constant), stringFlags bit 13 (blinking), teamSelectionKeyCache entry parsing (offset 2=size, 5=numGames, 6=remaining), gameListArray 14-pair building, irqCookieObfuscated XOR tamper check
- IDA artifact: ($297E).w → menuEntries[6] (CLUB BUSINESS entry)
- All stubs: `jmp _CFunc` (ContestMenuOnReturn_SetButtonsAndDefaultSelection uses a5 bridge)
- New externs: teamSelectionKeyCacheEnd, contestEntryHeight, gameListY, etc.
- Build status: **verified**
- Cumulative: 1117 functions across 122 chunks

### Chunk 123 — Contest Menu Action Handlers (9 → menus.c)
- Date: 2026-03-01
- **menus.c (9):** _PlayMatchMenu (a5 bridge), _NextMatch (a5 bridge), _ViewCompetitionsMenu, _ClubBusinessMenu, _JobOffersMenu, _SeasonFinished, _TransfersMenu, _ViewWorldMenu, _jmp_SetExitMenuFlag
- Key patterns: PlayMatchMenu/NextMatch use a5 bridges (caller's MenuEntry passed as param for ContestMenuOnReturn_SetButtonsAndDefaultSelection), ActivateMenu(contestMenu) pattern, Career_PostMatchUpdate still in asm
- Skipped: SquadMenu (complex pc-relative jumps to internal asm labels loc_1391AC/SelectAndShowSquadToView)
- New xdefs: Career_PostMatchUpdate, SeasonFinished_ComputeSeasonReview, ChooseCompetitionMenu
- Build status: **verified**
- Cumulative: 1126 functions across 123 chunks

### Chunk 124 — Competition Menu Functions (7 → menus.c)
- Date: 2026-03-01
- **menus.c (7):** _DIYCompetitionMenu, _PresetCompetitionMenu, _SeasonMenu, _CareerMenu, _StartContest, _SaveContest, _LoadOldCompetitionMenu
- Key patterns: Large dispatcher in StartContest (DIY/preset/season/career × over/not-over × league/cup/tournament), asm bridges for register-param functions (_CInitDIYCompetitionFromTableEntry, _CInitDIYCupFromRecord, _CInitDIYLeagueFromRecord, _CInitNewSeason), word write to seed for seeding both seed+randXorIndex
- New xdefs: SelectTeamsForPresetCompetition, ChooseSeasonMenu, diyCompetitionMenu, careerMenu, aDiyCompetition_0, aPresetCompetit_0, aSeason_1, aCareer_1, aLoadOldCompeti, aNotACompetitio, saveFileTitle
- Build status: **verified**
- Cumulative: 1133 functions across 124 chunks

### Chunk 125 — Choose/Select Team Menu Functions (4 → menus.c)
- Date: 2026-03-02
- **menus.c (4):** _ChooseCompetitionMenu, _SelectTeamsForPresetCompetition, _ChooseSeasonMenu, _GoGetTeamsForPlay
- Key patterns: All three Choose functions set globals and call SelectTeamsFinalMenu(255) via _CSelectTeamsFinalMenu bridge. GoGetTeamsForPlay has register bridge (d0,d2,d3,d4,a0,a1 → stack params). choosingPreset data preserved between stub and ChooseSeasonMenu.
- New asm bridge: _CSelectTeamsFinalMenu(uint16_t mode) — loads d0 from param, calls SelectTeamsFinalMenu
- New xdefs: gameTypeStr, chooseTeamsHeader1stPart, chooseTeamsFunc, careerSquadScreenDidSelectPlayerFlag, selectTeamsNoLimit, loadTeam, showCupsAndOther, playSeason, chooseCompetitionOnly, choosingPreset, showClubTeams, showNationalTeams, showOrigCustomTeams, playWhatStr, numNeededTeams, aChooseCompetit, aChoosePresetCo, aChooseSeason, chooseTeamsMenu, SelectTeamsFinalMenu, GoGetTeamsForPlay
- Build status: **verified**
- Cumulative: 1137 functions across 125 chunks

### Chunk 126 — DesignDIYLeague Change Callbacks (12 → diy_competition.c)
- Date: 2026-03-02
- **diy_competition.c (12):** _DesignDIYLeagueInit, _DesignDIYLeagueSetName, _DesignDIYLeagueChangePitchType, _DesignDIYLeagueChangeStartMonth, _DesignDIYLeagueChangeEndMonth, _DesignDIYLeagueChangeNumberOfTeams, _SetPlayEachTeamString, _DesignDIYLeagueChangePlayEachTeam, _DesignDIYLeagueChangeNumPointsForWin, _ConvertMinMaxSubstitutesNumbers, _DesignDIYLeagueChangeMinSubstitutes, _DesignDIYLeagueChangeMaxSubstitutes
- Key patterns: All callbacks use a5 bridges (MenuEntry* passed as param). Functions ending with `jmp CallDrawMenuItem` call _CallDrawMenuItem(entry) in C. Init falls through to DesignDIYLeagueFinish (still asm). Month fields are bytes (0-95, step 8, wrap at 96, divide by 8 for index).
- New xdefs: DesignDIYLeagueFinish, diyLeagueStartingMonth, diyLeagueEndingMonth, diyLeagueMinSubstitutes, diyLeagueMaxSubstitutes, aX0
- Build status: **verified**
- Cumulative: 1149 functions across 126 chunks

### Chunk 127 — DesignDIYCup Callbacks & Helpers (17+1s → diy_competition.c)
- Date: 2026-03-02
- **diy_competition.c (17+1s):** _DesignDIYCup_UpdateStartEndMonthIndices, _DesignDIYCup_UpdatePitchTypeOrSeasonSelectionVisibility, _DesignDIYCup_SetupTeamsSelectionUI, _DesignDIYCupReinit, _DesignDIYCupInit, _DesignDIYCupOnSelect2, _DesignDIYCupOnSelect3, _DesignDIYCupOnSelect4, _DesignDIYCupOnSelect5, _DesignDIYCupOnSelect7, _DesignDIYCupOnSelect9, _DesignDIYCupOnSelect11, _DesignDIYCupOnSelect13, _DesignDIYCupOnSelect15, _DesignDIYCupOnSelect20, _DesignDIYCupOnSelect21, _DesignDIYCupOnSelect22, +1 static (getCupRoundWordOffset)
- Key patterns: MenuEntry struct fields (bg at offset 30 as uint16_t* for color, background=28, isDisabled=6, isInvisible=4). CupReinit has 3 branches (normal/selectTeams/lookOnly). GetRoundIndex computes (ordinal-18)/5 for round word offset. DesignDIYCupOnDraw remains in asm.
- New xdefs: DesignDIYCupOnDraw, diyCupPitchTypeOrSeasonSelection, diyCupStartMonthIndex, diyCupEndMonthIndex
- Build status: **verified**
- Cumulative: 1167 functions across 127 chunks

### Chunk 128 — DesignDIYTournament Callbacks & Helpers (17+1s → diy_competition.c)
- Date: 2026-03-02
- **diy_competition.c (17+1s):** _DesignDIYTournament_UpdateMonthPageIndices, _DesignDIYTournament_ApplyInactiveMenuColors, _DesignDIYTournament_UpdatePitchTypeSeasonalUI, _DesignDIYTournamentReinit, _DesignDIYTournamentInit, _DesignDIYTournamentOnSelect2, _DesignDIYTournamentOnSelect3, _DesignDIYTournamentOnSelect4, _DesignDIYTournamentOnSelect5, _DesignDIYTournamentOnSelect7, _DesignDIYTournamentOnSelect9, _DesignDIYTournamentOnSelect11, _DesignDIYTournamentOnSelect13, _DesignDIYTournamentOnSelect21, _DesignDIYTournamentOnSelect24, _DesignDIYTournamentOnSelect25, _DesignDIYTournamentOnSelect26, _DesignDIYTournamentOnSelect28, +1 static (getTournamentStageBlock)
- Key patterns: Tournament uses diyFileBufferCopy (vs Cup's diyCompetitionStart). Stage blocks at stageIndex*0x76 offset. Reinit uses entry indices 89,90,91,92. ApplyInactiveMenuColors loops 87 entries. GetStageBlockFromMenuEntry left in asm (still called by unported OnSelect23).
- New xdefs: DesignDIYTournamentOnDraw, diyTournamentUseSeasonalPitchEffective, diyTournamentStartMonthPage, diyTournamentEndMonthPage
- Build status: **verified**
- Cumulative: 1185 functions across 128 chunks

### Chunk 129 — DesignDIYTournament Remaining (8+2s → diy_competition.c)
- Date: 2026-03-02
- **diy_competition.c (8+2s):** _DesignDIYTournamentOnDraw (large ~275-line draw callback), _DesignDIYTournamentOnSelect23, _DesignDIYTournamentOnSelect20, _DesignDIYTournament_AdjustStageQualifiers, _DesignDIYTournament_CacheStageSelection, _DesignDIYTournament_RedrawStageItemsFromBlock, _DIYTournament_RecalculateNumStages, +2 static helpers (computeGroupLayoutForTeams, adjustGroupLayoutForMaxTeams)
- Also stubbed: DesignDIYTournament_GetStageBlockFromMenuEntry (dead, rts), DIYTournament_ComputeGroupLayoutForTeams (dead), DIYTournament_AdjustGroupLayoutForMaxTeams (dead)
- Bug fix: ($2A26).w in OnDraw was hardcoded absolute address for menuEntries[9] — replaced with proper symbolic reference
- New xdefs: diyTournamentSelectedStageIndex1Based, diyTournamentSelectedStagePtr, diyTournamentPrevValueBackup, 22 string constants (a3rdPlayOff, aFinal_4, aSemiFinal, etc.)
- Updated Chunk 128 C callers: DesignDIYTournamentOnDraw() → _DesignDIYTournamentOnDraw()
- Build status: **verified**
- Cumulative: 1195 functions across 129 chunks

### Chunk 130 — DesignDIYCup Draw & Cup Menu Helpers (10 → diy_competition.c)
- Date: 2026-03-02
- **diy_competition.c (10):** _DesignDIYCupOnDraw (iterates cup rounds, configures menu entries), _DesignDIYCupOnSelect48 (play cup callback), _DesignDIYCup_UpdateRoundOptionsVisibility, _DesignDIYCup_BuildMenuTitleString (complex string builder with replay/leg suffixes), _DesignDIYCup_UpdateCurrentMatchTeamEntryStrings (inlines DIY_GetCurrentMatchTeamSlots), _DesignDIYCup_DrawCurrentMatchTeamEntries, _DesignDIYCup_HideCurrentMatchTeamEntries, _DesignDIYCup_UpdateMatchListScrollAndRebuildStrings (scroll clamping with Z-flag return), _DesignDIYCup_UpdateMatchListAndRedrawIfNeeded, _DesignDIYCup_UpdateCurrentMatchResultHeaderEntries
- UpdateMatchListScrollAndRebuildStrings uses special asm bridge (saves regs, calls C, tst.w d0, rts) to preserve Z-flag semantics for asm callers
- Bug fix: ($2A96).w in UpdateCurrentMatchResultHeaderEntries was hardcoded absolute address — should be menuEntries[11]
- Updated Chunk 127 C callers: DesignDIYCupOnDraw() → _DesignDIYCupOnDraw()
- New xdefs: diyCupMatchListScrollIndex, diyCupMenuArg0Copy, diyCupMenuArg1, diyCupMenuArg0, diyCupMenuArg2, diyCupMatchTeamNamePtr1/2, diyCupMatchListDrawModeFlag, diyCupLastMatchTeam1/2Ptr, diyCupLastMatchScoreTeam1/2, aV, aMinus, aOr, aDraw, aReplay, aReplays, a1stLeg, a2ndLeg, aPlayCup, rndStringTable, replayStringTable, chooseTeamsResult, seasonUpdateInProgress, fire, DesignDIYCup_BuildMatchListEntryStrings
- Deferred: AppendOrTeamNameToMenuEntry, AppendTeamNameToMenuEntry (register-param functions called from unported BuildMatchListEntryStrings), BuildMatchListEntryStrings (~164 instr), UpdateMenuEntriesVisibilityAndSelection (~260 instr)
- Build status: **verified**
- Cumulative: 1205 functions across 130 chunks

### Chunk 131 — DesignDIYLeague remaining + DIYCup menu helpers (12 → diy_competition.c)
- Date: 2026-03-02
- **diy_competition.c (12):** _DIYSetMonthsOrPitchTypeEntries (toggle pitch/month visibility), _DIYLeagueSetVariousFields (month indices, numeric fields), _DIYLeagueOKSelected (gray out entries 3-19), _DesignDIYLeagueUpdatePromotionRelegationText (format promoted/relegated text with PrintFormatted), _DesignDIYLeagueFinish (menu onReturn for league design), _DesignDIYLeagueChooseTeams (choose teams, start league play), _ShowDIYCupMenuWithParams (store params, show cup menu), _DIYCupFinish (finalize cup display), _DIYCupInit (init cup menu, falls through to Finish), _DIYCupOnSelect10 (cup play/exit callback), _DIYCupOnSelect16 (scroll up), _DIYCupOnSelect17 (scroll down)
- ShowDIYCupMenuWithParams uses register bridge stub (d0/d1/d2 → stack params)
- DIYCupInit falls through to DIYCupFinish (bra.w *+4 = no-op), ported as explicit call to _DIYCupFinish()
- Updated DesignDIYLeagueInit to call _DesignDIYLeagueFinish() directly instead of asm DesignDIYLeagueFinish()
- New xdefs: diyPitchTypeOrSeason_1, diyCupMenu, writeOnlyVar01, aPlayLeague, aWinnerQualifie, aTop0Qualify, aTeamsPromoted0, aTeamsPromoted0_0, aTeamsRelegated, aTeamsRelegated_0, DesignDIYCup_UpdateMenuEntriesVisibilityAndSelection, MenuCheckControls
- Build status: **verified**
- Cumulative: 1217 functions across 131 chunks

### Chunk 132 — DIYCup/League/Tournament select callbacks (10 → diy_competition.c)
- Date: 2026-03-02
- **diy_competition.c (9+1s):** _DIYLeagueChangePitchTypeOrSeason (toggle pitchTypeOrSeason, redraw), _DIYCupOnSelect1 (cup state dispatch switch on offset 0x4A), _DIYCupOnSelect3 (ordinal→offset 62, mode=2), _DIYCupOnSelect4 (ordinal→offset 62, mode=3), _DIYCupOnSelect5 (ordinal→offset 62, mode=0), _DIYCupOnSelect6 (ordinal→offset 66, mode=1), _DIYCupOnSelect7 (ordinal→offset 66, mode=3), _DIYCupOnSelect8 (ordinal→offset 66, mode=0), _DesignDIYTournamentOnSelect89 (choose teams & play tournament), static DIYCupOnSelectCommonC (play cup match, update state machine)
- Created _asm_DIY_PlayCupMatchAndAdvance bridge: stores a1/a2/d1/d2 to diyCupLastMatch* globals, returns d0
- DIYCupOnSelect3-8 each set a field then call DIYCupOnSelectCommonC; asm DIYCupOnSelectCommon stubbed as rts (dead)
- loc_134C96 inlined into DIYCupOnSelect1 default case
- New xdefs: _asm_DIY_PlayCupMatchAndAdvance, aPlayTournament
- Build status: **verified**
- Cumulative: 1227 functions across 132 chunks

### Chunk 133 — EditTeams functions (22 → edit_tactics.c)
- Date: 2026-03-02
- **edit_tactics.c (17+5s):** _EditTeamsInit (copy team to edit buffer), _EditTeamsFinish (updated to call _InitEditTeamsMenu), _EditTeamsUndo (restore team, reinit), _EditTeamsOK (save if changed), _EditTeamsChangeShirtsPrimary, _EditTeamsChangeShirtsSecondary, _EditTeamsChangeShortsPrimary, _EditTeamsChangeShortsSecondary, _EditTeamsChangeSocksPrimary, _EditTeamsChangeSocksSecondary, _EditTeamsSelectPrimaryShirtType (a5 bridge), _EditTeamsSelectSecondaryShirtType (a5 bridge), _EditTeamsChangePlayerName (a5 bridge), _EditTeamsSelectTeamName (a5 bridge), _EditTeamsSelectCoachName (a5 bridge), _EditTeamsChangePlayerFace (a5 bridge), _InitEditTeamsMenu (set player faces/names/kit colors), _EditTeamsPlayersBeforeDraw (a5 bridge), static PrimaryKitColorConversionC, static SecondaryKitColorConversionC, static MarkFirstKitShirtC, static MarkSecondKitShirtC, static EditTeamsGetPlayerPointerC
- Dead asm stubs: PrimaryKitColorConversion, SecondaryKitColorConversion, MarkFirstKitShirt, MarkSecondKitShirt, EditTeamsGetPlayerPointer (only called from now-ported C functions)
- New xdefs: editTeamsSelectedTeam, saveCustomTeams, teamIsImported, kTeamColorsTable, editTeamsPrimaryShirtColTable, editTeamsShortsColTable, editTeamsSocksColTable, editTeamsSecondaryShirtColTable, editTeamsSecondaryShortsColTable, editTeamsSecondarySocksColTable, shirtTypesTable, shirtsIndexTable, playerPositionsStringTable
- Build status: **verified** (warnings about selTeamsPtr size-1 extern are expected/harmless)
- Cumulative: 1249 functions across 133 chunks

### Chunk 134 — Top Goal Scorers / Statistics cluster (13 → statistics.c)
- Date: 2026-03-02
- **statistics.c (5+8s):** _TopGoalScorersMenu_0 (entry for career top scorers), _StatsHighestScorerList (entry from stats menu), _CompetitionTopGoalScorersInit (menu init for competition scorers), _TopGoalScorersInit (menu init for career multi-comp view), _TopGoalScorersBeforeDrawCommon (a5 bridge, per-row before-draw), static InitHighestScorersTable, static UpdateHighestScorersMinScoreAndCount, static FindReplaceableLowestScorerSlot, static AddTeamScorersToHighestScorersTable, static PopulateHighestScorersTableFromSelectedTeams, static SortHighestScorerTable, static GetScorerSurname, static ShowCompetitionTopScorers
- Dead asm stubs: loc_1382F6 (ShowCompetitionTopScorers), PopulateHighestScorersTableFromSelectedTeams, InitHighestScorersTable, AddTeamScorersToHighestScorersTable, UpdateHighestScorersMinScoreAndCount, FindReplaceableLowestScorerSlot, SortHighestScorerTable, GetScorerSurname (all only called from now-ported C)
- jmp stubs: TopGoalScorersMenu_0→_TopGoalScorersMenu_0, CompetitionTopGoalScorersInit→_CompetitionTopGoalScorersInit, TopGoalScorersInit→_TopGoalScorersInit, StatsHighestScorerList→_StatsHighestScorerList
- a5 bridge: TopGoalScorersBeforeDrawCommon (push a5, jsr _TopGoalScorersBeforeDrawCommon)
- New xdefs: highestScorersTable, diyStatsName, diyNameOfCompetitionPtr, diyUnknownWord_2C, competitionTopGoalScorersMenu, topGoalScorersMenu, aLparen, aRparen, aCup_10
- Build status: **verified** (clean build, no new warnings)
- Cumulative: 1262 functions across 134 chunks

### Chunk 135 — Career Over callbacks, Stats dispatchers, Select Teams callbacks (9+1s → menus.c)
- Date: 2026-03-02
- **menus.c (9+1s):** _CareerOverFinish (menu finish callback, builds manager/team/season text), static CareerOverUpdateJobOffersEntry (updates job offers entry visibility/color/text), _JobOffersMenuRetired (shows job offers from career over), _ExitCareerMenu (exit handler, clears career if sacked), _CompetitionInfo (dispatches to ShowLeagueInfo/ShowCupInfo/ShowTournamentInfo), _StatsExit (trivial SetExitMenuFlag), _ChooseTeamsPlayGame (play game button handler), _ExitSelectTeams (exit/abort based on depth level), _SelectTeamsCommon (a5 bridge, converts ordinal to team index), _SelectLeaguesCommon (a5 bridge, converts ordinal to league index)
- Dead asm stubs: CareerOverUpdateJobOffersEntry (only called from _CareerOverFinish)
- jmp stubs: CareerOverFinish, JobOffersMenuRetired, ExitCareerMenu, CompetitionInfo, StatsExit, ChooseTeamsPlayGame, ExitSelectTeams
- a5 bridges: SelectTeamsCommon, SelectLeaguesCommon
- New xdefs: selectedTeamIndex, selectedLeagueIndex, selectedTeamText, startingTeamIndex, numTeamsColumn1, numTeamsColumn2, numTeamsColumn3, diyCompetitionType, a0EndSeason
- Build status: **verified** (clean build, no new warnings)
- Cumulative: 1272 functions across 135 chunks

### Chunk 136 — ViewCompetitions Init/OnReturn, Career Menu Init/Finish, Nationality, SelTeamForTactics (7+1s → menus.c)
- Date: 2026-03-02
- **menus.c (7+1s):** _ViewCompetitionsInit (sets starting entry for tour/friendly, falls through to OnReturn), _ViewCompetitionsOnReturn (complex visibility: hides unavailable competition slots, shuffles Y coordinates), _CareerMenuInit (calls SetDefaultNameAndSurname + CareerMenuFinish), _CareerMenuFinish (updates sex/coach color/nationality display), static SetNationalityField (writes 3-char country code to menu entry), _ChangeNationality (a5 bridge, cycles nationality ordinal on left/right input), _SelTeamForTacticsCommon (a5 bridge, ordinal→index via column layout)
- Dead asm stubs: SetNationalityField (only called from now-ported C)
- jmp stubs: ViewCompetitionsInit, ViewCompetitionsOnReturn, CareerMenuInit, CareerMenuFinish
- a5 bridges: ChangeNationality, SelTeamForTacticsCommon
- Stub updates: jmp_SetExitMenuFlag_2→_SetExitMenuFlag, j_SetExitMenuFlag2→_SetExitMenuFlag, TopGoalScorersMenu→_TopGoalScorersMenu_0
- New xdefs: secondaryCompetitionName, tertiaryCompetitionName, quaternaryCompetitionName, nationalityOrdinalsArray, nationalityOrdinal, shortCountryNames, aOther, selTeamsColumn1, selTeamsColumn2
- Build status: **verified** (clean build, no new warnings)
- Cumulative: 1280 functions across 136 chunks

### Chunk 140 — File Selection Helpers (4+1s → file_io.c)
- Date: 2026-03-02
- Ported: SelectFilenameToSaveAndExit, AddExtension, SelFilesOnSelectCommon, RepositionSelectFileEntries (exported), CopyFilenameExtensionAndSetExitMenuFlag (static)
- jmp stubs: SelectFilenameToSaveAndExit
- Register bridges: SelFilesOnSelectCommon (a5→stack), AddExtension (a0→stack), RepositionSelectFileEntries (save/restore all)
- Dead asm stub: CopyFilenameExtensionAndSetExitMenuFlag→rts (all asm callers ported)
- New xdefs: selFilesColumn1NumItems, selFilesColumn2NumItems, selFilesColumn3NumItems, selFilesColumnStartY, selFilesColumnEndY
- Key note: asm StringCopy stub sets a1=d0 (return from _StringCopy), so a1 points AT null (not past it). AddExtension correctly overwrites null with extension.
- Build status: **verified** (clean build, no new warnings)
- Cumulative: 1303 functions across 140 chunks

### Chunk 148 — EditTeamsCallback + Career/Team Selection Helpers (8 → edit_tactics.c, career_init.c, career_teams.c, diy_competition.c)
- Date: 2026-03-02
- Ported:
  1. EditTeamsCallback (→edit_tactics.c) — store team pointer, show edit teams menu
  2. ChooseTeamAndStartCareer (→career_init.c) — call SelectTeamToManage, if selected call InitCareer, activate contestMenu
  3. LoadLiverpool (→career_init.c) — set coachOrPlayer=1, call InitCareer(0x811), activate contestMenu
  4. Career_UpdateNationalTeamMenuEntries (→career_teams.c) — update visibility of national team squad menu entries based on careerNumPlayers and pending add state; format "%a SQUAD" header
  5. Career_UpdateRemovePlayersMenuEntry (→career_teams.c) — show/hide "REMOVE PLAYER(S)" entry based on careerIntlPlayersRemoveMask bit count
  6. ChooseTeamsSetHeader (→diy_competition.c) — build "CHOOSE/CHANGE TEAMS FOR <type> - <country>" header string
  7. CheckNumTeamsAllowed (→diy_competition.c) — update menu entry visibility/status text based on numSelectedTeams vs numNeededTeams
  8. Bureaucracy (→diy_competition.c) — call ChooseTeamsSetHeader + CheckNumTeamsAllowed + DrawMenuItemsWaitForVSync
- Stubs: EditTeamsCallback (a0→stack bridge + locret_1373A6 preserved), ChooseTeamAndStartCareer (jmp), LoadLiverpool (jmp), Career_UpdateNationalTeamMenuEntries (jmp), Career_UpdateRemovePlayersMenuEntry (jmp), ChooseTeamsSetHeader (jmp), CheckNumTeamsAllowed (jmp), Bureaucracy (jmp)
- New C→asm bridges: _CSelectTeamToManage (returns a0 as ptr, stores d0 in global), _CInitCareer (uint16_t teamNo→d0)
- Bug fix: Fixed _CCreateRandomTransferOfferForPlayer bridge offset (60→64, accounting for return address)
- New xrefs: _EditTeamsCallback, _ChooseTeamAndStartCareer, _LoadLiverpool, _Career_UpdateNationalTeamMenuEntries, _Career_UpdateRemovePlayersMenuEntry, _ChooseTeamsSetHeader, _CheckNumTeamsAllowed, _Bureaucracy
- New xdefs: editTeamsMenu, chooseTeamsHeader1stPart, gameTypeStr, selectTeamsNoLimit, numNeededTeams, playWhatStr, aChooseTeamsFor, aChangeTeamsFor, aSelect1MoreTea, aSelect0MoreTea, aSelect1LessTea, aSelect0LessTea, aError, bureaucracyMenuItems, careerNationalTeamHasPendingAddPlayer, careerIntlPlayersRemoveMask, aASquad, aSelect1MoreInt, aSelect0MoreInt, aRemovePlayer, aRemovePlayers
- Build status: **verified** (clean build)
- Cumulative: 1335 functions across 148 chunks

### Chunk 149 — Friendly Menu Cluster + ContinueCareer (10 → menus.c, career_init.c)
- Date: 2026-03-02
- Ported:
  1. Friendly (→menus.c) — if no game in progress show friendly menu; else ContestInProgress (save/restore gameType+selTeamNumber around ShowFriendlyMenu)
  2. DrawNumbersOfSubstitutes (→menus.c) — write min/max sub counts to menu entries 5,6 via Int2Ascii, call SetFriendlyPitchOrSeason
  3. DisableEntries (→menus.c) — disable entries 1-6 that have frame+back background and non-zero low nibble
  4. FriendlyMenuInit (→menus.c) — set header text if friendlyOk, call FriendlyMenuReinit
  5. FriendlyMenuReinit (→menus.c) — disable pitch/season entries when pitchType!=SEASONAL, update sub counts, toggle visibility based on friendlyOk
  6. ChooseTeamsMenu (→menus.c) — set up friendly 2-team selection, random swap, init teams, play
  7. MinSubstitutes (→menus.c) — increment minSubstitutes with wraparound to 1, redraw entry
  8. MaxSubstitutes (→menus.c) — increment maxSubstitutes 0-5, enforce minSubstitutes>=1, redraw entries
  9. RandomlySwapTeams (→menus.c) — 50% chance swap 684 bytes between team 0 and team 1 at selTeamsPtr
  10. ContinueCareer (→career_init.c) — if season==19 show retirement+reset; else advance season+activate contest menu
- Stubs: Friendly (jmp), FriendlyMenuInit (jmp), FriendlyMenuReinit (jmp), DrawNumbersOfSubstitutes (jmp), DisableEntries (jmp), ChooseTeamsMenu (jmp), MinSubstitutes (jmp), MaxSubstitutes (jmp), RandomlySwapTeams (jmp), ContinueCareer (jmp)
- ContestInProgress merged into _Friendly (label preserved for safety)
- New xrefs: _Friendly, _FriendlyMenuInit, _FriendlyMenuReinit, _DrawNumbersOfSubstitutes, _DisableEntries, _ChooseTeamsMenu, _MinSubstitutes, _MaxSubstitutes, _RandomlySwapTeams, _ContinueCareer
- New xdefs: aFriendly_1, aPlayFriendly, ContinueCareerNextSeason
- Build status: **verified** (clean build, no new errors)
- Cumulative: 1345 functions across 149 chunks

### Chunk 150 — DIY League/Tournament Menu Helpers (13 → diy_competition.c)
- Date: 2026-03-02
- Ported:
  1. jmp_SetExitMenuFlag_7 (→diy_competition.c) — thin wrapper calling _SetExitMenuFlag
  2. DIYLeagueTournamentStats (→diy_competition.c) — calls _SetupDIYCompetitionStatsMenu
  3. DIYLeagueTournamentExit (→diy_competition.c) — complex guard logic on hasGroups($48) and field at $60
  4. DIYLeagueTournament_UpdateViewOptionsAvailability (→diy_competition.c) — sets flag based on match progress ≥75% or group stage
  5. DIYSetTeamNamesAndResults (→diy_competition.c) — populates 5 menu entries (11-15): separator, team names, scores
  6. DIYLeagueTournament_EnsureCurrentFixtureVisible (→diy_competition.c) — adjusts currentDiyLine for scroll window, returns uint16_t
  7. DIYLeagueTournament_ScrollFixtureListToCurrentFixture (→diy_competition.c) — calls EnsureCurrentFixtureVisible, redraws if changed
  8. DIYLeagueTournamentScrollUp (→diy_competition.c) — decrements currentDiyLine, calls DrawMenu
  9. DIYLeagueTournamentScrollDown (→diy_competition.c) — increments currentDiyLine with bounds check
  10. DIYLeagueTournament_SetupCurrentFixtureTeamNames (→diy_competition.c) — gets fixture team ptrs, fills menu entries with team names
  11. DIYLeagueTournament_DrawPlayMatchAndFixtureHeader (→diy_competition.c) — sets "PLAY MATCH" text, draws entries
  12. DIYLeagueTournamentNextMatch (→diy_competition.c) — checks progress, sets fixture view mode, calls scroll/draw/update
  13. DIYLeagueTournamentInit (→diy_competition.c) — initializes currentDiyLine, view options, focus entries
- Stubs: all 13 simple jmp stubs
- New xrefs: _jmp_SetExitMenuFlag_7, _DIYLeagueTournamentStats, _DIYLeagueTournamentExit, _DIYLeagueTournament_UpdateViewOptionsAvailability, _DIYSetTeamNamesAndResults, _DIYLeagueTournamentScrollUp, _DIYLeagueTournamentScrollDown, _DIYLeagueTournament_ScrollFixtureListToCurrentFixture, _DIYLeagueTournament_EnsureCurrentFixtureVisible, _DIYLeagueTournament_SetupCurrentFixtureTeamNames, _DIYLeagueTournament_DrawPlayMatchAndFixtureHeader, _DIYLeagueTournamentNextMatch, _DIYLeagueTournamentInit
- New xdefs: currentDiyLine, diyAllMatchesPlayed, diyFixtureViewMode, diyTournamentViewOnly, diyLeagueTournament_viewOptionsAvailable, diyLeftTeamPtr, diyRightTeamPtr, diyLeftTeamResult, diyRightTeamResult, numDiyResults, diyLeagueTotalMatches, diyLeagueMatchProgress, diyInitFocusOverride, DIYLeagueTournament_UpdateMenuEntryVisibilityAndFocus, aNextRound
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1358 functions across 150 chunks

### Chunk 151 — Utility Functions & Competition Init (7 → teams.c, play_match_menu.c, career_teams.c, diy_competition.c, career_init.c)
- Date: 2026-03-02
- Ported:
  1. FormatPlayerValueOrStatusText (→teams.c) — formats player value/status as text (TRIAL/RES/LOAN/price XK/XM/X.YM)
  2. FormTopSkillsString (→play_match_menu.c) — builds 3-letter skill abbreviation from player data
  3. GetTeamLeague (→career_teams.c) — loads team file, scans for team number, returns league byte
  4. IsNationalTeamCompetitionOrQualification (→diy_competition.c) — scans competition list, returns 0 (found) or 1 (not found)
  5. InitCompetitionTeamSelectionFromTableEntry (→career_init.c) — init team selection from table entry spec
  6. InitCompetitionTeamSelectionFromTableHeader (→career_init.c) — init team selection from table header spec
  7. InitCompetitionTeamSelectionFromAltTableHeader (→career_init.c) — init team selection from alt table header spec
- Stubs: all 7 register bridge stubs (FormatPlayerValueOrStatusText: a0,a1→stack; FormTopSkillsString: a1,a6→stack; GetTeamLeague: d0→stack; IsNationalTeamCompetitionOrQualification: a5→stack with tst.w d0; InitCompetitionTeamSelection*: d0,a0→stack or a0→stack)
- New xrefs: _FormatPlayerValueOrStatusText, _FormTopSkillsString, _GetTeamLeague, _IsNationalTeamCompetitionOrQualification, _InitCompetitionTeamSelectionFromTableEntry, _InitCompetitionTeamSelectionFromTableHeader, _InitCompetitionTeamSelectionFromAltTableHeader
- New xdefs: nationalTeamCompetitionsAndQualifications1, topSkillLettersEven, topSkillLettersOdd, topSkillBitRotEven, topSkillBitRotOdd, aTrial, aRes, aLoan, a01m, a0m, a1k
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1365 functions across 151 chunks

### Chunk 152 — DIY League/Tournament Match Callbacks & Visibility (8 → diy_competition.c)
- Date: 2026-03-02
- Ported:
  1. DIYLeagueTournamentMatch (→diy_competition.c) — callback: stores ordinal to field3A+4, calls MatchCommon(2)
  2. DIYLeagueTournamentOnSelect4 (→diy_competition.c) — callback: stores ordinal, calls MatchCommon(3)
  3. DIYLeagueTournamentResult (→diy_competition.c) — callback: stores ordinal, calls MatchCommon(0)
  4. DIYLeagueTournamentViewMatch (→diy_competition.c) — callback: stores ordinal to field3A+8, calls MatchCommon(1)
  5. DIYLeagueTournamentViewHighlights (→diy_competition.c) — callback: stores ordinal, calls MatchCommon(3)
  6. DIYLeagueTournamentViewResult (→diy_competition.c) — callback: stores ordinal, calls MatchCommon(0)
  7. DIYLeagueTournamentMatchCommon (→diy_competition.c) — plays league fixture, updates state, sorts table, redraws
  8. DIYLeagueTournament_UpdateMenuEntryVisibilityAndFocus (→diy_competition.c) — complex visibility/focus management for 8+ menu entries
- Stubs: 6 register bridges (a5→stack), 2 jmp stubs
- New asm bridge: _asm_DIY_PlayOrSimulateLeagueFixture — stores multi-reg returns (a1,a2,d1,d2) to globals
- New xrefs: _DIYLeagueTournamentMatch, _DIYLeagueTournamentOnSelect4, _DIYLeagueTournamentResult, _DIYLeagueTournamentViewMatch, _DIYLeagueTournamentViewHighlights, _DIYLeagueTournamentViewResult, _DIYLeagueTournamentMatchCommon, _DIYLeagueTournament_UpdateMenuEntryVisibilityAndFocus
- New xdefs: _asm_DIY_PlayOrSimulateLeagueFixture, diyTournamentType
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1373 functions across 152 chunks

### Chunk 153 — ShowDiyLeaguesTournamentMenu + GetBenchPlayerPointer (3 → diy_competition.c, bench_subs.c)
- Date: 2026-03-02
- Ported:
  1. ShowDiyLeaguesTournamentMenu (→diy_competition.c) — sets diyTournamentType/diyFixtureViewMode/etc, sorts league table, waits for fire release if seasonUpdateInProgress, calls ShowMenu
  2. GetBenchPlayerPointer (→bench_subs.c) — maps bench index (0-10) to in-game player pointer using tactics formation table; returns ordinal, stores pointer in benchPlayerResultA0
  3. UpdatePlayerToBeSubstitutedPosition (→bench_subs.c) — reads plToBeSubstitutedOrd, calls GetBenchPlayerPointer, stores ordinal to plToBeSubstitutedPos
- Stubs: ShowDiyLeaguesTournamentMenu (register bridge d0/d1/d2→stack), GetBenchPlayerPointer (register bridge d0→stack, a0←global, d1 preserved), UpdatePlayerToBeSubstitutedPosition (jmp)
- C caller updates: replaced _asm_ShowDiyLeaguesTournamentMenu → _ShowDiyLeaguesTournamentMenu in menus.c, diy_competition.c; replaced _asm_ShowDiyLeaguesTournamentMenu_Career(d2) → _ShowDiyLeaguesTournamentMenu(1,0,d2) in career_teams.c
- New xrefs: _ShowDiyLeaguesTournamentMenu, _GetBenchPlayerPointer, _UpdatePlayerToBeSubstitutedPosition
- New xdefs: benchPlayerResultA0, diyLeagueTournamentMenu
- Build status: **verified** (clean build)
- Cumulative: 1376 functions across 153 chunks

### Chunk 154 — IsPlayerOkToSubstitute + FindInitialPlayerToBeSubstituted (2 → bench_subs.c)
- Date: 2026-03-02
- Ported:
  1. IsPlayerOkToSubstitute (→bench_subs.c) — checks if highlighted player can be substituted based on subsState, cards, and disallow-subs flag. Returns 0=can substitute, 1=skip
  2. FindInitialPlayerToBeSubstituted (→bench_subs.c) — searches all 11 bench positions for matching player position: exact > group (DEF/RB/LB or MID/RW/LW) > first available. Sets plToBeSubstitutedOrd and plToBeSubstitutedPos
- Stubs: IsPlayerOkToSubstitute (register bridge with tst.w d0 for Z flag), FindInitialPlayerToBeSubstituted (jmp)
- New xrefs: _IsPlayerOkToSubstitute, _FindInitialPlayerToBeSubstituted
- Build status: **verified** (clean build)
- Cumulative: 1378 functions across 154 chunks

### Chunk 155 — HandleFormationMenuMovement + SwapPlayerNameData (2 → bench_subs.c)
- Date: 2026-03-02
- Ported:
  1. HandleFormationMenuMovement (→bench_subs.c) — handles left/right (cycle selectedFormationEntry 0-17), up/down (call SubsBenchOff to exit)
  2. SwapPlayerNameData (→bench_subs.c) — swaps entries in plOrdinalsTable and plNamesLengthTableCopy; offsets +16 for bottom team
- Stubs: HandleFormationMenuMovement (jmp), SwapPlayerNameData (register bridge d6/d7→stack)
- New xrefs: _HandleFormationMenuMovement, _SwapPlayerNameData
- Build status: **verified** (clean build)
- Cumulative: 1380 functions across 155 chunks

### Chunk 156–159 — (Previous session, context lost)
- Date: 2026-03-02
- Note: These chunks were completed in the previous session which ran out of context.
  The session summary reported 1393 functions across 159 chunks at session end.
  Exact chunk details were lost due to context overflow.
- Cumulative: 1393 functions across 159 chunks

### Chunk 160 — Dead Code Stubs (bench drawing + string utils)
- Date: 2026-03-02
- Stubbed (dead code — only called from already-ported C functions):
  1. DrawOpponentsBench (rts — called from static drawOpponentsBench in bench_subs.c)
  2. DrawBenchPlayersAndCoach (rts — called from static drawBenchPlayersAndCoach in bench_subs.c)
  3. DrawSubstitutesMenu (rts — called from static drawSubstitutesMenu in bench_subs.c)
  4. DrawSubstitutesMenuEntry (rts — called from static drawSubstitutesMenuEntry in bench_subs.c)
  5. DrawFormationMenu (rts — called from static drawFormationMenu in bench_subs.c)
  6. DrawFormationMenuEntry (rts — called from static drawFormationMenuEntry in bench_subs.c)
  7. Int2StrWithCommas (rts — called from static int2StrWithCommas in string_utils.c)
  8. Int2AsciiLessThan100Check (rts — shared label with above)
  9. Int2AsciiSaveD0 (rts — shared label with above)
- Build status: **verified**
- Cumulative: 1393 functions across 160 chunks (no new C code, dead code cleanup)

### Chunk 161 — Show/View Menu Utilities (6 → career_season.c, highlights_menu.c, transfer_market.c)
- Date: 2026-03-02
- Ported:
  1. ShowSeasonOpinion (→career_season.c) — maps seasonReview 0-4+ to chairman message constants, calls ShowChairmanMessage
  2. ShowHighlights (→highlights_menu.c) — sets hilStarted, copies subs limits, loads teams, shows stadiumMenu, plays highlights via viewAndRestoreMenu helper, restores music
  3. ShowMatchSequence (→highlights_menu.c) — viewAndRestoreMenu(_ViewMatchSequence)
  4. ViewHighlightsSaveCurrentMenu (→highlights_menu.c) — viewAndRestoreMenu(_ViewHighlights)
  5. JmpShowHighlights (→highlights_menu.c) — trivial forwarder to _ShowHighlights
  6. ViewOffersMenu (→transfer_market.c) — shows offersMenu, handles transferCompleted/no offers
- Static helper: viewAndRestoreMenu() in highlights_menu.c (saves/restores menu state around view calls)
- Stubs: all 6 as jmp _FuncName
- New xrefs: _ShowSeasonOpinion, _ShowHighlights, _ShowMatchSequence, _ViewHighlightsSaveCurrentMenu, _JmpShowHighlights, _ViewOffersMenu
- New xdefs: stadiumMenu, offersMenu
- Build status: **verified** (clean build)
- Cumulative: 1399 functions across 161 chunks

### Chunk 162 — Transfer Market Menu Helpers (8 → transfer_market.c)
- Date: 2026-03-02
- Ported:
  1. CheckNumPlayersOnTransferMarket (→transfer_market.c) — hides/shows menu entries based on transfer market count (5 max, 0 min)
  2. CheckIfPlayerSelected (→transfer_market.c) — shows/hides entries 5,7 based on playerSelected state
  3. EnableDisableOffers (→transfer_market.c) — checks for transfer offers, disables/grays out entry 3 if no offers
  4. TransferMarketInit (→transfer_market.c) — clears state, calls CheckNum/CheckOffers, falls through to TransferMarketFinish
  5. BuyPlayerMenu (→transfer_market.c) — checks slots >= 2, shows buy player menu, exits if transfer completed
  6. GiveFreeTransfer (→transfer_market.c) — removes selected player from team, resets selection
  7. TakeOffMarketMenu (→transfer_market.c) — removes selected player from transfer market list
  8. TransferMarketCommon (→transfer_market.c) — toggles player selection, redraws entries (a5 register bridge)
- Static helpers: checkNumPlayersOnTransferMarket(), checkIfPlayerSelected() (used internally by multiple functions)
- Stubs: 5 as jmp, TransferMarketCommon as register bridge (a5→stack), 2 as jmp
- New xrefs: 8 C functions
- New xdefs: playerSelected, buyPlayerMenu, buyPlayerRedrawList
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1407 functions across 162 chunks

### Chunk 163 — Transfer/Squad/PlayerMarket Helpers (13 → transfer_market.c, menus.c)
- Date: 2026-03-02
- Ported:
  1. BuildTransferPlayerMenuEntry (→transfer_market.c) — populates 12 consecutive MenuEntry structs for a transfer player row (6-param register bridge: a0,a6,d7,d6,d5,a4)
  2. TransferMarketAfterDrawCommon (→transfer_market.c) — computes player index from entry ordinal, calls BuildTransferPlayerMenuEntry + DrawMultipleItems (a5 bridge)
  3. ShowPlayerOfferMenuForPlayer (→transfer_market.c) — init offer state, compute affordable amount, show playerOffersMenu (d0+d1 bridge)
  4. ShowPlayerMarketInit (→transfer_market.c) — reset 7 filter/list variables
  5. ShowPlayerMarket (→transfer_market.c) — set title string, update balance, call PlayerMarketUpdateFilterMenuEntryState
  6. PlayerMarketUpdateFilterMenuEntryState (→transfer_market.c) — update menuEntries[6] color and disabled state based on position filter
  7. PlayerMarketUpdateMoreListLabel (→transfer_market.c) — set menuEntries[7].fg to "MORE" or "LIST" based on scroll state
  8. PlayerMarketResetListState (→transfer_market.c) — clear list state, update label, redraw menu
  9. BuyOtherPlayersCommon (→transfer_market.c) — get player from team, show offer menu (a5 bridge)
  10. BuyOtherPlayersAfterDrawCommon (→transfer_market.c) — get player, call BuildTransferPlayerMenuEntry at menuEntries[19] (a5 bridge)
  11. ViewSquadInit (→menus.c) — init view squad menu: show/hide entries based on roster size, set title/team name, handle pre-selection
  12. ViewSquadCommon (→menus.c) — compute selected player/team from entry ordinal, exit menu (a5 bridge)
  13. ViewSquadAfterDrawCommon (→menus.c) — draw player entry with selection/flag masks overlaid (a5 bridge)
- Stubs: 5 jmp, 6 register bridges (3×a5, 1×d0+d1, 1×6-param, 1×a5)
- New xrefs: 13 C functions
- New xdefs: playerOffersMenu, aPlayersOnForei, aPlayersOnDomes, aMore, aList, playerMarketFilter*, playerMarketTotal*, playerMarketVisible*, playerMarketList*, viewSquadTitle, viewSquadFlags, viewSquadMask*, viewSquadArg*, viewSquadSelectedTeam
- Build status: **verified** (clean build)
- Cumulative: 1420 functions across 163 chunks

### Chunk 164 — ShowSquad + Squad Callers + OffersInit (5 → menus.c, transfer_market.c)
- Date: 2026-03-03
- Ported:
  1. ShowSquad (→menus.c) — core squad viewer: stores 6 register params to globals, calls ShowMenu(viewSquadMenu), returns selectedPlayer
  2. ShowSquadWithFlags (→shares asm bridge with ShowSquad) — variant that sets viewSquadFlags from d4 before common body
  3. ThirdPartyOfferViewSquad (→transfer_market.c) — builds transfer market player bitset, calls _ShowSquad
  4. BuyPlayerViewSquad (→transfer_market.c) — identical to ThirdPartyOfferViewSquad
  5. OffersInit (→transfer_market.c) — empty init callback (original was just rts)
- Static helper: showSquadWithTransferMarketMask() in transfer_market.c (shared by ThirdPartyOfferViewSquad and BuyPlayerViewSquad)
- Stubs: ShowSquad as 6-param register bridge (a0,a1,d0,d1,d2,d3→stack, returns d0+d3), ShowSquadWithFlags shares bridge, 3 jmp stubs
- New xrefs: _ShowSquad, _ThirdPartyOfferViewSquad, _BuyPlayerViewSquad, _OffersInit
- New xdefs: viewSquadMenu, aViewSquad, careerTeam
- Build status: **verified** (clean build)
- Cumulative: 1425 functions across 164 chunks

### Chunk 165 — AfterDrawCommon variants + OffersCommon + PlayerOffers callbacks (11 → transfer_market.c)
- Date: 2026-03-03
- Ported:
  1. TransferPlayerAfterDrawCommon (→transfer_market.c) — shared helper: base=entries[13], target=entries[17], teamOffering for d7<2
  2. ThirdPartyOfferMenuAfterDrawCommon (→transfer_market.c) — shared helper: base=entries[15], target=entries[19], selTeamNumber for d7<2 (reversed)
  3. PlayerOffersAfterDrawCommon (→transfer_market.c) — shared helper: base=entries[14], target=entries[18], teamOffering for d7<2
  4. j_SetExitMenuFlag_11 (→transfer_market.c) — forwards to _SetExitMenuFlag
  5. PlayerOffersOnSelect13 (→transfer_market.c) — forwards to _SetExitMenuFlag
  6. OffersCommon (→transfer_market.c) — main offer negotiation loop with ShowMenu + accept/reject/ask-for-more logic
  7. PlayerOffersOnSelect8 (→transfer_market.c) — squad selection for main player to buy
  8. PlayerOffersOnSelect10 (→transfer_market.c) — squad selection for swap player
  9. PlayerOffersOnSelect11 (→transfer_market.c) — squad selection for extra swap player
  10. PlayerOffersOnSelect12 (→transfer_market.c) — confirm offer, evaluate negotiation
  11. PlayerOffersAdjustOfferAmount (→transfer_market.c) — input handler for offer amount (step-based increment/decrement)
- Static helper: offerAfterDrawCommonShared() — consolidates 3 near-identical AfterDrawCommon functions
- Key pattern: contiguous offer ordinals (offerPrimaryPlayerOrdinal thru offerExtraPlayerOrdinal) accessed as int16_t array
- Stubs: 3 a5 register bridges (AfterDrawCommon), 1 a5 bridge (OffersCommon), 4 no-param bridges (OnSelect8/10/11/12), 1 no-param bridge (AdjustOfferAmount), 2 jmp stubs
- New xrefs: _TransferPlayerAfterDrawCommon, _ThirdPartyOfferMenuAfterDrawCommon, _PlayerOffersAfterDrawCommon, _j_SetExitMenuFlag_11, _PlayerOffersOnSelect13, _OffersCommon, _PlayerOffersOnSelect8, _PlayerOffersOnSelect10, _PlayerOffersOnSelect11, _PlayerOffersOnSelect12, _PlayerOffersAdjustOfferAmount
- New xdefs: offerStatus, thirdPartyOfferResult, thirdPartyOfferMenu, teamWillingToSellOrExchange, transferPlayerMenuResult, lastOfferPrimaryPlayerOrdinal, lastOfferExtraPlayersWanted, lastOfferSwapPlayerOrdinal, lastOfferExtraPlayerOrdinal, offerNegotiationAttemptCount, aSelectPlayerTo, aSelectPlayerTo_0, playerTransferMenu, sellExchangeStrings, sellExchangeResponseStrings, Career_AcceptOffer_SellPlayer, Career_AcceptOffer_BuyPlayer, EvaluateTransferOfferNegotiation
- Build status: **verified** (clean build)
- Cumulative: 1436 functions across 165 chunks

### Chunk 167 — Career Transfer Accept + Trialist System (9 exported + 3 static → transfer_market.c)
- Date: 2026-03-03
- Ported:
  1. Career_BuildForeignMarketPoolFromSelectedTeam (→transfer_market.c) — rearranges careerTeam players by selTeamPlayerIndices, adds all 16 to pool, updates TMD
  2. Career_TransferPlayerAndAddReplacedToMarketPool (→transfer_market.c) — finds lowest-price player of matching position in source team, copies player data over, adds replaced player to career pool
  3. Career_AcceptOffer_SellPlayer (→transfer_market.c) — handles sell offer: adds sale amount, processes swap/extra, flags teams changed
  4. Career_AcceptOffer_BuyPlayer (→transfer_market.c) — handles buy offer: subtracts purchase amount, processes swap/extra
  5. Career_RemovePlayerAndAddAsTrialist (→transfer_market.c) — removes from source team, finds free slot, inserts into roster at correct position, converts to trialist
  6. Career_GenerateTrialistForTeamPlayer (→transfer_market.c) — picks random origin, copies name, clears stats, marks as trialist
  7. Career_ConvertPlayerToTrialistFromName (→transfer_market.c) — converts player from another team to trialist using source team's country byte
  8. TrainingMatch_AddTrialPlayerToSelectedTeam (→transfer_market.c) — finds free slot, marks as trialist with specified role
  9. TrainingMatch_AdjustSelectedTeamRosterForTrialists (→transfer_market.c) — removes excess GK/subs, adds trial players to fill gaps
- Static helpers:
  - pickRandomTeamRecordAvoidingTeam() — picks random team from TMD/selected teams avoiding specified team
  - pickRandomTrialistOriginValue() — picks random player name parts for trialist surname
  - careerInitTrialistAttributes() — computes skill attributes from team average and random distribution
- Dead code stubs: Career_InitTrialistAttributes, Career_PickRandomTrialistOriginValue, PickRandomTeamRecordAvoidingTeam (all called only from now-ported C)
- Stubs: Career_BuildForeignMarketPoolFromSelectedTeam (no-param bridge), Career_AcceptOffer_SellPlayer/BuyPlayer (no-param bridge), Career_TransferPlayerAndAddReplacedToMarketPool (d0,d1 bridge), Career_RemovePlayerAndAddAsTrialist (d0,d1 bridge), Career_GenerateTrialistForTeamPlayer (d1 bridge), Career_ConvertPlayerToTrialistFromName (d0,d1 bridge), TrainingMatch_AddTrialPlayerToSelectedTeam (d0 bridge), TrainingMatch_AdjustSelectedTeamRosterForTrialists (d0,d1,a0 bridge)
- New xrefs: 9 (all exported functions above)
- New xdefs: tempTeamBuffer, selTeamPlayerIndices, transferReplacementPriorityTable, trialistNameBuffer, trialistNameBufTerminator, trialistSkillDistribution, trialistSkillTemplates, trialistSkillBitMasks
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1450 functions across 167 chunks

### Chunk 174 — Season Review functions (4 → career_season.c)
- Date: 2026-03-03
- Ported:
  1. SeasonReview_AdjustWeightsForCompetition (static helper in career_season.c) — classifies competition by pointer comparison against known globals (worldCup2Copy, friendlyCopy, continental cups, WC qualifiers, domestic), multiplies category by weight (8/6/4/3/1), accumulates weight in d5. Friendly/tournament/tour: weight 1 but d7 NOT multiplied.
  2. SeasonReview_ComputeCategoryForCompetition (static helper in career_season.c) — calls FindTeamInDIY (suppressed side effects) and GetMoreExpensiveTeamsCount, computes ratio = (rank-position)*100/numTeams, assigns category 0-4 based on ratio thresholds (30/10/-10/-30). Adjusts: top-25% percentile upgrades (guarded), bottom-75% downgrades (guarded). Returns -1 if team not found.
  3. SeasonFinished_ComputeSeasonReview — orchestrates season review for primary+secondary competitions: computes category and weights for each, then weighted average with rounding ((sum + totalWeight/2) / totalWeight). Bonus: wonWorldCup → rating 0, wonContinentalCup → -2. Clamps to >= 0, stores in seasonReview.
  4. ShowChairmanOpinionAboutTheSeason — national teams: delegates to ShowSeasonOpinion. Club teams: inline category computation (same logic but NO suppressed side effects, NO guards on percentile adjustments). Checks secondary/tertiary/quaternary competitions for top-2 finishes (-2/-2/-3 bonus). Clamps to [0,4+], maps to chairman message IDs 13-17.
- Stubs: SeasonFinished_ComputeSeasonReview (jmp, void→void), ShowChairmanOpinionAboutTheSeason (jmp). AdjustWeights and ComputeCategory stubbed as rts (dead — only caller was ComputeSeasonReview, now ported).
- New xrefs: _SeasonFinished_ComputeSeasonReview, _ShowChairmanOpinionAboutTheSeason
- Build status: **verified** (clean build)
- Cumulative: 1468 functions across 174 chunks

### Chunk 176 — Training Match Sub Swap & Trial Players (5 → training_match.c)
- Date: 2026-03-03
- Ported:
  1. TrainingMatch_IsPlayerBetterSwapCandidate (static) — compares two player candidates for sub swap: avoids trialist/special flagged players (someFlag 0x9A/0x9B), prefers higher price. Returns 0=better, 1=not better.
  2. TrainingMatch_FindBestSubSwapCandidates (static) — iterates players from index 11 to careerNumPlayers, finds top 2 non-goalkeeper outfield players for swap using IsPlayerBetterSwapCandidate. Returns struct with indices and skill values.
  3. TrainingMatch_ApplyUseMatchTeamAndSwapSubs — saves team state, copies tactics byte from careerTeam to currentTeam, finds best sub candidates and swaps them into positions 11/12 of trainingMatchSavedPlayers.
  4. TrainingMatch_TryAddTrialPlayerAtPosition — calls AddTrialPlayerAtPosition(position), then ~40% chance (random >= 0x99) marks player's someFlag as 0x9A and updates wage cache.
  5. TrainingMatch_AddTrialPlayersForNewSeason — if not national selector, adds 10 trial players at positions (2,1,5,4,3,3,6,6,7,7) then applies sub swap.
- Note: Fixed latent bug where TrainingMatch_AddTrialPlayerAtPosition was incorrectly stubbed as `rts` while still called from live asm (TryAddTrialPlayerAtPosition). This caused writes to uninitialized a3 pointer ~40% of the time.
- Stubs: AddTrialPlayersForNewSeason (jmp, called from asm). IsPlayerBetterSwapCandidate, FindBestSubSwapCandidates, ApplyUseMatchTeamAndSwapSubs, TryAddTrialPlayerAtPosition all rts (dead — only callers ported).
- New xrefs: _TrainingMatch_AddTrialPlayersForNewSeason
- Build status: **verified** (clean build)
- Cumulative: 1475 functions across 176 chunks

### Chunk 175 — DIY Progression Table + Career Post-Match Update (2 → diy_competition.c, career_status.c)
- Date: 2026-03-03
- Ported:
  1. DIY_RecomputeCompetitionDerivedProgressionTable (→diy_competition.c) — self-contained ~30 instr. Computes interpolated progression table from diyCompetitionStart: count at +0x1C4, start/end month at +0x38/+0x39, output at +0x1CE. If count==1, stores start. Otherwise interpolates: out[i] = start + i*(end-start)/(count-1). Wraps end by +0x60 if end < start.
  2. Career_PostMatchUpdate (→career_status.c) — ~48 instr dispatcher. Calls Career_FillCurrentSeasonInfo, removes expired job offers, generates 1-2 job offers (up to 100 attempts), auto-advances all competition slots (primary/secondary/tertiary/quaternary) with saved/restored selTeamNumber and teamControls, falls through to Career_HandleRetirement.
- New C-callable wrapper: _CCareer_AutoAdvanceCompetitionAndPersistSlotState (a0 = compPtr) in swos2_unit.s
- Stubs: DIY_RecomputeCompetitionDerivedProgressionTable (jmp), Career_PostMatchUpdate (jmp)
- New xrefs: _DIY_RecomputeCompetitionDerivedProgressionTable, _Career_PostMatchUpdate
- Build status: **verified** (clean build)
- Cumulative: 1470 functions across 175 chunks

### Chunk 179 — Career Auto-Advance Chain (3 → career_status.c)
- Date: 2026-03-02
- Ported:
  1. Career_AutoAdvanceCompetitionAndPersistSlotState (→career_status.c) — Identifies competition context buffer from pointer comparison, loads slot state, dispatches to AutoAdvanceLeagueFixtures/CupMatches/CompetitionSlots by type, stores state back. Replaces old _CCareer_AutoAdvanceCompetitionAndPersistSlotState wrapper.
  2. Career_AutoAdvanceCupCompetitionFromRecord (→career_status.c) — Auto-advance cup competition: handles English cup aliases, seeds RNG, initializes team selection, loads cup, runs cup matches, restores RNG.
  3. Career_AutoAdvanceLeagueCompetitionFromRecord (→career_status.c) — Same as above but for league competitions. Calls InitDIYLeagueFromRecord and Career_AutoAdvanceCompetitionSlots.
- Stubs: Register bridges (a0 param) for all 3
- New xdefs: Career_AutoAdvanceLeagueFixtures, Career_AutoAdvanceCupMatches, Career_AutoAdvanceCompetitionSlots, englishCup, englishCup_1, englishLeagueCup, englishLeagueCup_1
- New C-callable wrappers: _CInitDIYCupFromRecordParam, _CInitDIYLeagueFromRecordParam
- Build status: **verified** (clean build)
- Cumulative: 1480 functions across 179 chunks

### Chunk 185 — Career Cup Pick List System (13 → career_season.c)
- Date: 2026-03-02
- Ported:
  1. pickRandomNationalTeamId (static) — picks random national team ID with region/price filtering and duplicate checking. Uses natTeamsRandLimit2, nationalTeams (6-byte entries). 100-attempt price filtering, then unlimited without price check.
  2. fillWorldCupQualifyingGroupTeams (static) — fills WC qualifying group from sorted team list. Handles league (type 0, offset 0xD) and cup (type 2, offset 0xF) competition formats. Places player's team if region matches.
  3. setupWorldCupQualifyingGroups (static) — only runs if someTeamFlags bit 6 set. Calculates group 8 size from European host/qualifying team count. Column-major fill of 8 European WC qualifying groups, then 5 continental qualification groups (NA, SA, Africa, Asia, Oceania with special count adjustment).
  4. setCupPickListCommon (static) — shared helper for 5 simple regional functions (Oceania, NorthAmerica, Asia, Africa, SouthAmerica). Selects cup pick list based on someTeamFlags bits, copies 5 uint32_t values to careerCupPickList.
  5. setCupPickList_Europe (static) — Europe-specific variant with EC championship (bit 1) and EC qualifying (bit 0) flag checks. After standard cup pick list copy, does EC qualifying group column-major fill if bit 0 set: 8 EC groups, 46 total slots, group 8 limited to 4 teams.
  6. setupFriendlyCupTeams (static) — fills friendly competition team list using pickRandomNationalTeamId with price range [avgPrice*75/100, avgPrice*125/100].
  7. setupInvitationTournamentTeams (static) — fills invitation tournament team list similarly.
  8. setupTourCupTeams (static) — fills tour cup using random region with price range [0, 0x31].
  9. _Career_SetCupPickList_ForSelectedRegion (exported) — main dispatcher: switches on (selTeamNumber >> 8) region code, calls appropriate regional/setup functions.
  10-13. Career_SetCupPickList_Oceania/NorthAmerica/Asia/Africa/SouthAmerica/Europe (6 regional functions → inlined via setCupPickListCommon + setCupPickList_Europe helpers, assembly bodies replaced with rts)
- Stubs: Career_SetCupPickList_ForSelectedRegion (register bridge, push a6), 6 regional + 3 setup + PickRandomNationalTeamId + SetupWorldCupQualifyingGroups + FillWorldCupQualifyingGroupTeams (all rts, dead)
- New xrefs: _Career_SetCupPickList_ForSelectedRegion
- New xdefs: natTeamsRandLimit2, ecQualifyingGroupsTable, worldCupQualifyingGroupsTable, tourStringTable, 38 cupPick* arrays (6 per region × 5 regions + 8 Europe)
- Build status: **verified** (clean build, map confirmed)
- Cumulative: 1493 functions across 185 chunks

### Chunk 186 — Career Season Update Functions (3 → career_season.c)
- Date: 2026-03-02
- Ported:
  1. Career_AdvanceToNextSeason_UpdateCompetitions (→career_season.c) — Main season transition: loads league scratch, clears promotion/relegation flags, prepares Euro cup qualification, updates all cup slots, processes all competitions via careerCompetitionIdRemapTable, draws European cup teams, validates team counts.
  2. Career_ProcessPrimaryCompetitionForNextSeason (→career_season.c, static) — Processes primary competition: league (type 0) loops through divisions with interactive/auto-advance paths, fills qualified team indices, processes qualification entries; cup (type 2) edits slot or auto-advances.
  3. Career_ProcessDiyLeagueQualificationEntry (→career_season.c, static) — Processes a single DIY league qualification entry: interactive path for player's competition (ShowDIYCupMenuWithParams), auto-fill path for other competitions (fills team data from diyQualifiedTeamIndices, auto-advances cup).
- Stubs: Career_AdvanceToNextSeason_UpdateCompetitions (register bridge jsr), Career_ProcessPrimaryCompetitionForNextSeason (rts, dead), Career_ProcessDiyLeagueQualificationEntry (rts, dead)
- Eliminated wrapper: _CCareer_ProcessPrimaryCompetitionForNextSeason now dead — replaced with direct call to static processPrimaryCompetitionForNextSeason
- New xrefs: _Career_AdvanceToNextSeason_UpdateCompetitions
- New xdefs: careerCompetitionIdRemapTable
- Bug fix: Cup path was calling Career_AutoAdvanceCupCompetitionFromRecord but assembly calls Career_AutoAdvanceLeagueCompetitionFromRecord — corrected.
- Build status: **verified** (clean build)
- Cumulative: 1496 functions across 186 chunks

### Chunk 187 — GetPlayerPrice + UpdatePlayerPositionAndReturnRole (4 → match_sim.c)
- Date: 2026-03-02
- Ported:
  1. sub_120A10 (dead code, stubbed as rts) — unreferenced function, never called
  2. sub_120B0A (dead code, stubbed as rts) — unreferenced function, never called
  3. GetPlayerPrice (→match_sim.c) — ~120 instr. Calculates player's effective price/skill based on 35 pitch quadrants, 7 skills per quadrant with position-weighted ponders, distance factor matrix, nearness factor, and player density at quadrant. Uses code-as-data pattern: GK skill row is 35 bytes of machine code before skillsPerQuadrant (accessed via skillsPerQuadrant - 35 in C). Returns clamped price index (0-49) with file price table lookup for increase.
  4. UpdatePlayerPositionAndReturnRole (→match_sim.c) — ~90 instr. Maps player index to slot via currentMatchPlayers (career team) or playerNumbers (other teams), computes effective price via GetPlayerPrice for outfield players, handles GK/sub slots specially, checks sold/injury status for human teams. Returns packed (role << 16) | position.
- Stubs: sub_120A10 (rts), sub_120B0A (rts), UpdatePlayerPositionAndReturnRole (rts, dead — only called from C bridge), GetPlayerPrice (rts, dead code body preserved for code-as-data bytes)
- Bridges updated: _CGetPlayerPrice → jmp _GetPlayerPrice, _CUpdatePlayerPositionAndReturnRole → jmp _UpdatePlayerPositionAndReturnRole
- New xrefs: _GetPlayerPrice, _UpdatePlayerPositionAndReturnRole
- New xdefs: skillsPerQuadrant, priceSkillPonders, quadrantCoordinatesToBallQuadrant, skillValuePonders, plPricesIncreaseTable, plPositions, totalSkillsFactorSum, numPlayersAtQuadrant
- Build status: **verified** (clean build, map confirmed)
- Cumulative: 1500 functions across 187 chunks

### Chunk 190 — Match Result Processing (5 → diy_competition.c)
- Date: 2026-03-02
- Ported:
  1. GetCachedOrCalculateViewResult (→static getCachedOrCalcViewResult in diy_competition.c) — ~12 instr. Checks cached view result; if cached, returns from cache and invalidates; otherwise calls _CalculateViewResult.
  2. GetCachedOrCalculateExtraTimeResult (→static getCachedOrCalcExtraTimeResult) — ~8 instr. Same cache pattern for extra time scoring.
  3. GetCachedOrCalculatePenaltyShootoutResult (→static getCachedOrCalcPenaltyResult) — ~8 instr. Same cache pattern for penalty shootout.
  4. ProcessDIYLeagueMatchResultAndCareerUpdates (→_ProcessDIYLeagueMatchResultAndCareerUpdates) — ~60 instr. Processes DIY league match: gets view result, builds flags byte (draw/win/loss bits), updates career form/contract/loss flag. Returns d1=goals1, d2=goals2, d7=flags.
  5. ProcessDIYKnockoutMatchResultAndCareerUpdates (→_ProcessDIYKnockoutMatchResultAndCareerUpdates) — ~190 instr. Processes DIY knockout match: first leg, return leg (aggregate, away goals, extra time, penalties), single leg paths. Returns d4=packedGoals, d5=packedAggregate, d6=packedPenalty, d7=flags.
- Parameters: TeamFileEx* for team1/team2 (struct-typed, not uint8_t*)
- Stubs: ProcessDIYLeague* → output-pointer bridge (3 outputs via stack); ProcessDIYKnockout* → output-pointer bridge (4 outputs via stack, d5/d6 inputs); GetCachedOrCalculate* → rts (dead code)
- New xrefs: _ProcessDIYLeagueMatchResultAndCareerUpdates, _ProcessDIYKnockoutMatchResultAndCareerUpdates
- New xdefs: cachedViewResultTeam1Goals, cachedViewResultTeam2Goals, cachedExtraTimeTeam1Goals, cachedExtraTimeTeam2Goals, cachedPenaltyShootoutTeam1Score, cachedPenaltyShootoutTeam2Score
- Build status: **verified** (clean build, map confirmed at 0x00154256 / 0x0015439e)
- Cumulative: 1507 functions across 190 chunks

### Chunk 196 — Career_SetCurrentCompetitionSlot + TeamSelectionKeyCache builders (4+2d → career_status.c/career_teams.c)
- Date: 2026-03-02
- Ported:
  1. Career_SetCurrentCompetitionSlot (→career_status.c) — ~88 instr. Sets up a competition slot: stores comp in currentCompetitionPtr[slot], loads slot state, validates comp type byte vs slot (slot 0 rejects type 1, slot 4 requires type 1, slot 3 always valid, slots 1-2 accept type 1 or national types 0/2), fills diyCupTeamIds for slot 4, copies team selections or dispatches Career_InitCompetitionTeamSelection_Dispatch, calls InitCompetitionFromRecord, stores state, builds TeamSelectionKeyCache. Hidden d1 parameter (divisionIndex) passed through to InitCompetitionFromRecord and Career_InitCompetitionTeamSelection_Dispatch.
  2. TeamSelectionKeyCache_BuildFromCompetitionBlock (→career_teams.c) — ~72 instr. Builds team selection key cache from competition block. Dispatches based on block type: 0=league (sorted insert per team), 1=cup (per-round entries with two-leg/replay handling, separate roundPtr advancing by 2 per round), 2+=multi-slot (per-slot entries with cup or league sub-types, GetCupRoundStringIndex with opt parameter from base+0x15A).
  3. addNextRoundCupEntry (static, →career_teams.c) — ~16 instr. Cup entry builder for type 1 competitions.
  4. addCupEntry (static, →career_teams.c) — ~19 instr. Cup entry builder for type 2+ competitions with 5th opt parameter passed through to GetCupRoundStringIndex.
  5. TeamSelectionKeyCache_AddNextRoundCupEntry — stubbed as rts (dead, only caller ported)
  6. TeamSelectionKeyCache_AddCupEntry — stubbed as rts (dead, only caller ported)
- Asm→C bridges: Career_SetCurrentCompetitionSlot (4 params: a0, d0, d1, d2; saves d1-d7/a2-a6 to preserve d1 for subsequent callers), TeamSelectionKeyCache_BuildFromCompetitionBlock (a0 param)
- Key findings: d1 is a hidden parameter (divisionIndex) to Career_SetCurrentCompetitionSlot, used by InitCompetitionFromRecord and Career_InitCompetitionTeamSelection_Dispatch. Type 1 cup path uses separate roundPtr (a3) vs constant base (a6) for per-round flag lookups. Type 2+ multi-slot path reads opt from base+0x15A for GetCupRoundStringIndex 5th param.
- Build status: **verified** (clean build)
- Cumulative: 1526 functions across 196 chunks

### Chunk 197 — InitCareer + InitializeNewSeason family (6 → career_init.c)
- Date: 2026-03-02
- Ported:
  1. InitCareer (→career_init.c) — ~86 instr. Initializes new career: clears 2000 bytes of diyLeagueSeasonStateTable, clears ~40 career globals (board confidence, stats, bonuses, flags), calls Career_InitTransferMarketState/InitCareerVariables(4)/CalculateStartingBalance/Career_InitializeSelectedTeamAndStartingBalance, copies careerContests table entries, sets all 4 comp slot states to {3,2,8}, calls InitializeNewSeason.
  2. ContinueCareerNextSeason (→career_init.c) — ~14 instr. If teamNoOfferAccepted != 0xFFFF: optionally show leaving message, update competitions, init team with new funds. Otherwise: show chairman opinion, update competitions, recalculate balance. Both paths call InitializeNewSeason.
  3. Career_SelectEuroCupSlotIfTeamParticipates (→career_init.c) — ~35 instr. Checks if selTeamNumber is in euro cup team list. If found, sets up quaternary slot (3) for euro cup. Returns 0 if participating, 1 if not.
  4. DIY_SelectCompetitionSlot4FromCurrentDivision (→career_init.c) — ~20 instr. If primary comp is league type, checks for linked DIY competition and sets up slot 4.
  5. InitializeNewSeason (→career_init.c) — ~247 instr. Large function: increments season, clears board confidence modifiers, inits season info struct, determines national vs club team, sets up competition slots 0-4 (primary/secondary/tertiary via competitionsTable or careerCupPickList, quaternary via euro cups, slot 4 via DIY), builds team budget rank table, computes wageScalingDivisor from key cache walk, copies manager name, fills market players, inits finances, shows chairman message, adds trial players for new season.
  6. InitNewSeason (→career_init.c) — ~65 instr. Simplified new season init for job changes: sets up competition slots from competitionsTable, counts non-computer squads, inits teams.
- Asm→C bridges: InitCareer (d0 param), InitializeNewSeason (no params), ContinueCareerNextSeason (no params), Career_SelectEuroCupSlotIfTeamParticipates (a0+d7, returns d0 with tst), DIY_SelectCompetitionSlot4FromCurrentDivision (no params), InitNewSeason (a0+d1 params)
- Updated _ChooseTeamAndStartCareer and _LoadLiverpool to call _InitCareer directly (removed _CInitCareer dependency)
- Key fixes: CalculateStartingBalance returns balance (don't discard return value), volatile loops to prevent memset optimization, 23 new xdefs for asm globals
- Build status: **verified** (clean build)
- Cumulative: 1532 functions across 197 chunks

### Chunk 198 — Career_ViewOrEditCompetitionSlot (1 → career_init.c)
- Date: 2026-03-02
- Ported:
  1. Career_ViewOrEditCompetitionSlot (→career_init.c) — ~76 instr. Maps competition pointer to its career copy via careerContests table (10-byte stride loop), applies special europeanChampionships/worldCup → 2Copy replacements, finds which competition slot (0-4) owns the competition, then dispatches: league→ShowDiyLeaguesTournamentMenu, cup→ShowDIYCupMenuWithParams, edit→DIY_EditCompetitionSlot, or auto-advance for slots not matching current.
- Asm→C bridge: Career_ViewOrEditCompetitionSlot (a0+d0 params)
- Added xdefs: europeanChampionships, europeanChampionships2Copy, worldCup, worldCup2Copy
- Build status: **verified** (clean build)
- Cumulative: 1533 functions across 198 chunks

### Chunk 195 — Career_AutoAdvanceCompetitionSlots (1 → career_status.c)
- Date: 2026-03-02
- Ported:
  1. Career_AutoAdvanceCompetitionSlots (→career_status.c) — ~50 instr. Iterates competition slots dispatching to league or cup auto-advance path. Uses diyFileBufferCopy + slotIndex * 0x76 to find slot record, checks type flag at offset 0x15E (nonzero=league, zero=cup). Sets hasGroups flag (1=normal, 2=last slot). For league: calls _Career_AutoAdvanceLeagueFixtures, _DIY_SortLeagueTableByStandings, _DIY_AdvanceToNextCompetitionSlot. For cup: calls _Career_AutoAdvanceCupMatches, _DIY_AdvanceToNextCompetitionSlot. Loop restarts unless doneFlag(0x58) set or base+0x48 goes negative. Original recursive tail-call replaced with goto-based loop.
- Asm→C bridge: Career_AutoAdvanceCompetitionSlots (simple jsr)
- Fixed call site at line 3237 (_Career_AutoAdvanceLeagueCompetitionFromRecord) from un-prefixed to _-prefixed
- Build status: **verified** (clean build)
- Cumulative: 1520 functions across 195 chunks

### Chunk 194 — Career_AutoAdvanceLeagueFixtures + Career_AutoAdvanceCupMatches (2+2 → career_status.c)
- Date: 2026-03-02
- Ported:
  1. Career_AutoAdvanceLeagueFixtures (→career_status.c) — ~88 instr. Auto-simulates all league fixtures up to current game phase. Guard checks (careerAutoAdvanceInProgress/seasonUpdateInProgress/internationalAutoAdvanceFlag/seasonFinished), TeamSelectionKeyCache phase lookup, main sim loop calling _DIY_PlayOrSimulateLeagueFixture(0,0), fixture advance via _DIY_AdvanceFixtureIndex, catch-up loop, groups type check, ShowDiyLeaguesTournamentMenu tail calls.
  2. Career_AutoAdvanceCupMatches (→career_status.c) — ~98 instr. Same pattern for cup matches. Uses _DIY_PlayCupMatchAndAdvance(0,0), checks roundComplete(0x54)/finishedFlag(0x56), calls _DIY_UpdateCompetitionProgress between rounds, ShowDIYCupMenuWithParams tail calls.
  3. DIY_AdvanceLeagueFixtureIndex — stubbed as rts (dead, only caller ported)
  4. DIY_AdvanceCupFixtureIndex — stubbed as rts (dead, only caller ported)
- Asm→C bridges: Career_AutoAdvanceLeagueFixtures (simple jsr), Career_AutoAdvanceCupMatches (simple jsr)
- Added xdef for careerLastMatchIndexByCompetitionSlot
- Updated C call sites in career_status.c from asm extern to C function calls
- Build status: **verified** (clean build)
- Cumulative: 1519 functions across 194 chunks

### Chunk 193 — DIY_PlayCupMatchAndAdvance + DIY_PlayOrSimulateLeagueFixture (2 → diy_competition.c)
- Date: 2026-03-02
- Ported:
  1. DIY_PlayCupMatchAndAdvance (→diy_competition.c) — ~190 instr. Full DIY cup match orchestration: mode routing (0=sim, 1/2=play mode 0, 3=play mode 1), calls SetupPlayers for sim path with human teams, PlayDIYCupFixtureAndCacheResult for play paths. Post-match: loads first-leg goals, ProcessDIYKnockoutMatchResultAndCareerUpdates, stores results/goals/penalties to competition tables. Round advancement with match counter, month table viewRandomPhase update. Career stat update (compType=1). Two-leg check, round completion with groups/non-groups paths, team shuffle, competition finish with grid bracket winner determination.
  2. DIY_PlayOrSimulateLeagueFixture (→diy_competition.c) — ~175 instr. Full DIY league match orchestration: similar mode routing, uses DIY_GetCurrentFixtureTeamPtrs + fixtureTeam1CompPtr/fixtureTeam2CompPtr globals for comp pointer tracking. Post-match: ProcessDIYLeagueMatchResultAndCareerUpdates, DIY_AdvanceLeagueProgressAfterMatch. League table updates: wins/draws/losses (offsets 0x2BA/0x2BC/0x2BE), goals for/against (0x2C0/0x2C2), points (0x2C4) with pointsForAWin from base+0x62. Career stat update (compType=0, packedGoals only, 0 for aggregate/penalty). Standings sort when diyMinSubstitutes==0, promotions processing with divu.w #$12 ranking→standings lookup, relegations processing.
- Asm→C bridges: DIY_PlayCupMatchAndAdvance (d6/d7 → stack, Z flag return via tst.w d0), DIY_PlayOrSimulateLeagueFixture (d6/d7 → stack, Z flag return)
- Build status: **verified** (clean build)
- Cumulative: 1515 functions across 193 chunks

### Chunk 192 — PlayDIYLeagueMatchAndCacheResult + PlayDIYCupFixtureAndCacheResult (2 → diy_competition.c)
- Date: 2026-03-02
- Ported:
  1. PlayDIYLeagueMatchAndCacheResult (→diy_competition.c) — ~90 instr. DIY league match play: calls SetupPlayers, sets isCareer, copies competition name/round, sets game params (pitch/season/goals), calls InitializeInGameTeamsAndStartGame via C→asm bridge, caches view result goals. Has computeGameSeason helper. Uses d6/d7 from caller (allowSquadChanges/mode).
  2. PlayDIYCupFixtureAndCacheResult (→diy_competition.c) — ~217 instr. DIY cup fixture play with three paths: first leg (simple match), second leg (loads first-leg goals from table, extra time/penalties), auto/sim path (seq-based randomize pitch). Builds complex round strings with replay/leg suffix. Tracks d2 side-effect through string building for cachedViewResultTeam2Goals. Caches extra time goals and penalty scores if states go negative (indicating they were actually played).
- Asm→C bridges: PlayDIYLeagueMatchAndCacheResult (d6/d7/a1/a2 → stack, Z flag return), PlayDIYCupFixtureAndCacheResult (d6/d7/a1/a2 → stack, Z flag return)
- Fixed duplicate extern declarations with type conflicts (diyPointsForAWin uint8_t[] vs uint16_t, cachedView* int16_t vs uint16_t)
- Build status: **verified** (clean build)
- Cumulative: 1513 functions across 192 chunks

### Chunk 191 — DIY Match Team Slots + SetupPlayers + InitAndPlayGame (4 → diy_competition.c, game_init.c)
- Date: 2026-03-02
- Ported:
  1. DIY_GetCurrentMatchTeamSlots (→diy_competition.c) — 17 instr. Reads current match index from diyCompetitionStart+0x64, looks up team slot indices in grid pairs table at +0x2CE, returns two slot base pointers (base + slotIdx * 4).
  2. DIY_CompareCurrentMatchTeamCategories (→diy_competition.c) — 22 instr. Calls DIY_GetCurrentMatchTeamSlots, loads team pointers from slot +0x344 (selTeamsPtr-relative), compares teamControls. Returns: same → that type; one is COMPUTER → other's type; otherwise → 2.
  3. SetupPlayers (→game_init.c) — 112 instr. Pre-match player setup. Shows ShowPlayMatchMenu for each human player, handles career team remap, coach mode, player/coach number assignment. Uses TeamFileEx* for team params. Calls _ShouldSetupPlayers, _InitPlayerRemapTableForTeam, _CShowPlayMatchMenu (C→asm bridge).
  4. InitAndPlayGame (→game_init.c) — 43 instr. Friendly/training match entry point. Calls _SetupPlayers, copies game name ("TRAINING"/"FRIENDLY"), sets pitch/season/goals params, calls _CInitializeInGameTeamsAndStartGame (C→asm bridge).
- New C→asm bridges: _CShowPlayMatchMenu (7 reg params), _CInitializeInGameTeamsAndStartGame (7 reg params) in swos2_unit.s
- Asm→C bridges: SetupPlayers (d1/d2/d6/d7/a1/a2 → stack), InitAndPlayGame (a1/a2 → stack), DIY_GetCurrentMatchTeamSlots (output a1/a2 via stack), DIY_CompareCurrentMatchTeamCategories (result d0)
- New xdefs: aTraining
- Build status: **verified** (clean build)
- Cumulative: 1511 functions across 191 chunks

### Chunk 189 — Career_UpdateStatsAfterMatch (1 → career_status.c)
- Date: 2026-03-02
- Ported:
  1. Career_UpdateStatsAfterMatch (→career_status.c) — ~120 instr. Updates board confidence modifiers (mod0-9), win/loss/draw/goals stats, checks international tournament (aEuropeanTour..aOceaniaTour address range), stores match results (goals/aggregate/penalty) to competition slot record, sets careerSlotProcessingInProgress.
- Stubs: Career_UpdateStatsAfterMatch → register bridge (a1=team1, a2=team2, d0=compType, d4=packedGoals, d5=aggregate, d6=penalty, d7=flags)
- New xrefs: _Career_UpdateStatsAfterMatch
- New xdefs: boardConfidenceMod0-9, boardConfidenceValue0-1, diyLeagueCurrentSlot
- Build status: **verified** (clean build, map confirmed)
- Cumulative: 1502 functions across 189 chunks

### Chunk 188 — UpdateCareerPlayersFormAfterMatch (1 → career_status.c)
- Date: 2026-03-02
- Ported:
  1. UpdateCareerPlayersFormAfterMatch (→career_status.c) — ~180 instr. Loops through career team players after match, adjusting form (someFlag) based on: injury status, player slot (GK/outfield/sub/unused), performance vs expected form (someFlag/10 + price vs field_1), goal scorer bonuses, and random tables. Clamps form to [-100..100]. Uses team1/team2GoalCounterByPlayer, playerFormDeltaRandomTable[144].
- Stubs: UpdateCareerPlayersFormAfterMatch → register bridge (a0=compCtx, a1=team1, a2=team2, d1=goals1, d2=goals2)
- New xrefs: _UpdateCareerPlayersFormAfterMatch
- New xdefs: team1GoalCounterByPlayer, team2GoalCounterByPlayer, avgPlayerPriceHalfClamped, playerFormDeltaRandomTable
- Build status: **verified** (clean build, map confirmed)
- Cumulative: 1501 functions across 188 chunks

### Chunk 178 — CheckNumberOfTeams (1 → menus.c)
- Date: 2026-03-02
- Ported:
  1. CheckNumberOfTeams (→menus.c) — Updates team selection menu entries (86-88) based on numSelectedTeams vs numNeededTeams. Handles selectTeamsNoLimit, builds "SELECT N MORE/LESS TEAMS" text via PrintFormatted/StringCopy.
- Stubs: CheckNumberOfTeams → jmp _CheckNumberOfTeams
- New xrefs: _CheckNumberOfTeams
- Build status: **verified** (clean build)
- Cumulative: 1477 functions across 178 chunks

### Chunk 177 — PutPlayerOnMarketMenu (1 → transfer_market.c)
- Date: 2026-03-02
- Ported:
  1. PutPlayerOnMarketMenu (→transfer_market.c) — Transfer market menu allowing user to put players on transfer market via ShowSquad. Loops: builds eligible mask (positionAndFace & 0x02), builds marketMask from playersOnTransferMarket[], calls ShowSquad with both masks, toggles player on/off market, breaks on cancel (viewSquadSelectedTeam < 0) or 5 players reached. Calls CheckNumPlayersOnTransferMarket and SetCurrentEntry(6) if maxed.
- Stubs: PutPlayerOnMarketMenu → jmp _PutPlayerOnMarketMenu
- New xrefs: _PutPlayerOnMarketMenu
- New xdefs: aPutPlayerOnTra
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1476 functions across 177 chunks

### Chunk 173 — FindTeamInDIY (1 → diy_competition.c)
- Date: 2026-03-03
- Ported:
  1. FindTeamInDIY (→diy_competition.c) — complex team lookup across 3 competition types (~85 instr). League: searches via index table at offset 110 using divu #18 for position mapping. Cup: searches via byte index at offset 718, with group stage handling (slots 0-1 map directly), bracket processing (find largest power-of-2 bracket position), or pre-knockout return. Tournament: searches byte index at offset 90, can redirect to secondary competition (+0x420) in group stage mode (CUP or LEAGUE search), with stage-walk loop for knockout positioning and optional rank-based adjustment via GetMoreExpensiveTeamsCount. Sets globals diyFoundTeamIndex, diyCompHasGroupStage (0xFF00/0 big-endian word pattern from seq/sne). Panics (0x6006) if not found and not national selector.
- Stubs: FindTeamInDIY (d0+a6 register bridge → d1 return). Updated _CFindTeamInDIY to simple jmp alias (avoids double bridge).
- New xrefs: _FindTeamInDIY
- New xdefs: diyFoundTeamIndex, findTeamInDIY_SuppressSideEffectsFlag
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1464 functions across 173 chunks

### Chunk 172 — Budget Rank Table & International Squad Init (2 → diy_competition.c, career_teams.c)
- Date: 2026-03-03
- Ported:
  1. InitializeNewSeason_BuildTeamBudgetRankTable (→diy_competition.c) — builds per-team budget rank table for current competition. Selects index table and team ID array by competition type (league:238/302, cup:718/590, tournament:90/154). For each team, looks up team ID via sign-extended index, calls GetMoreExpensiveTeamsCount, stores count in teamBudgetRankTable.
  2. Career_InitInternationalSquadAndLookupTablesForNewSeason (→career_teams.c) — initializes 20 international player slots: clears lookup table entries to 0xFFFF, sets currentMatchPlayers[i]=i, calls GetPlayer for each slot, marks player[0x4F]=0xFF, assigns default positions from careerDefaultInternationalSquadSlotsTable for first 11 slots, modifies player[0x66] bits 5-6 (set for slots 1-10, 12-19; clear for 0, 11), resets careerNumPlayers to 0.
- Stubs: InitializeNewSeason_BuildTeamBudgetRankTable (jmp, void→void), Career_InitInternationalSquadAndLookupTablesForNewSeason (a0 register bridge)
- New xrefs: _InitializeNewSeason_BuildTeamBudgetRankTable, _Career_InitInternationalSquadAndLookupTablesForNewSeason
- New xdefs: teamBudgetRankTable, careerDefaultInternationalSquadSlotsTable
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1463 functions across 172 chunks

### Chunk 171 — Player Position Lookup, Remove International Players, Expensive Teams Count (3 → career_teams.c, diy_competition.c)
- Date: 2026-03-03
- Ported:
  1. GetPlayerPositionFromCurrent (→career_teams.c) — searches currentMatchPlayers[0..19] for a player index, returns position. Panics (0x5771) if not found.
  2. Career_RemoveInternationalPlayersByMask (→career_teams.c) — iterates 20 international player slots (38-byte stride from careerTeam). For each bit set in removeMask: clears lookup table entry, finds position in currentMatchPlayers, shifts entries down for positions >= 16 (rotating removed player to end), marks slot 0x4F as 0xFF, decrements careerNumPlayers.
  3. GetMoreExpensiveTeamsCount (→diy_competition.c) — counts DIY competition teams with higher average player price than given team. Selects team ID array offset by competition type (league:302, cup:590, tournament:154).
- Stubs: GetPlayerPositionFromCurrent (d1→stack, return d0→d1), Career_RemoveInternationalPlayersByMask (d0.l register bridge), GetMoreExpensiveTeamsCount (d0+a6→stack, return d0→d2)
- Note: Career_RemoveInternationalPlayersByMask shift loop uses volatile to prevent GCC memmove optimization
- New xrefs: _GetPlayerPositionFromCurrent, _Career_RemoveInternationalPlayersByMask, _GetMoreExpensiveTeamsCount
- Build status: **verified** (clean build)
- Cumulative: 1461 functions across 171 chunks

### Chunk 170 — Career Init & National Team Helpers (4 → career_init.c, career_season.c, career_teams.c)
- Date: 2026-03-03
- Ported:
  1. Career_InitializeSelectedTeamAndStartingBalance (→career_init.c) — sets starting balance, copies 684 bytes of team data to both careerTeam and selTeamsPtr, sets coach/player control byte, copies playerOrder to currentMatchPlayers, initializes financial globals
  2. Career_ContinueToNextSeasonInitTeamAndFunds (→career_init.c) — clears transfer market, increments season counter, optionally rebuilds foreign market pool, applies 80-104% random jitter to funds, tail-calls InitializeSelectedTeamAndStartingBalance
  3. Career_UpdateLastNationalityCall (→career_season.c) — determines national team for player's nationality via GetNationalTeamNumber/GetNationalTeamNumberFromTeamFileId, stores in lastNationalityCall (0xFFFF if same as selected team)
  4. Career_CopyUnavailablePlayerToCareerTeamSlot (→career_teams.c) — copies international player to career team's unavailable slot: name to careerUnavailablePlayerNames (17-byte entries), team/player index to lookup table (4-byte entries), copies 38 bytes of player data preserving control byte, clears status bytes, calls SetBenchPlayersNumbers
- Stubs: Career_InitializeSelectedTeamAndStartingBalance + Career_ContinueToNextSeasonInitTeamAndFunds (d0,d1 register bridges), Career_UpdateLastNationalityCall (jmp), Career_CopyUnavailablePlayerToCareerTeamSlot (a0,a1,a2,d2,d1,d3 register bridge)
- New xrefs: _Career_InitializeSelectedTeamAndStartingBalance, _Career_ContinueToNextSeasonInitTeamAndFunds, _Career_UpdateLastNationalityCall, _Career_CopyUnavailablePlayerToCareerTeamSlot
- New xdefs: careerUnavailablePlayerNames, lastNationalityCall, careerTeamLeague
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1458 functions across 170 chunks

### Chunk 169 — JobOffers Init/DrawOld/AfterDraw (3 → job_offers.c)
- Date: 2026-03-03
- Ported:
  1. JobOffersInit (→job_offers.c) — menu init callback; anti-piracy cookie check, fills missing team names, resets selectedOldJobOfferOrdinal, falls through to JobOffersFinish
  2. JobOffers_DrawOldOfferLine (→job_offers.c) — after-draw callback for old offer entries; formats "JOB OFFER FROM %a" / "INTERNATIONAL" / "WITHDRAWN" text, highlights selected offer, draws entries 13-14
  3. JobOffersAfterDrawCommon (→job_offers.c) — after-draw callback for new offer entries; same format logic, draws entries 11-12
- Stubs: JobOffersInit (no-param bridge), DrawOldOfferLine + AfterDrawCommon (a5 bridges)
- New xrefs: _JobOffersInit, _JobOffers_DrawOldOfferLine, _JobOffersAfterDrawCommon
- New xdefs: aInternationalJ_0, aJobOfferFromA, aJobOfferFromAW
- Build status: **verified** (clean build)
- Cumulative: 1454 functions across 169 chunks

### Chunk 168 — OffersAfterDrawCommon (1 → transfer_market.c)
- Date: 2026-03-03
- Ported:
  1. OffersAfterDrawCommon (→transfer_market.c) — after-draw callback for offers view menu. Calculates offer index from current entry ordinal minus entry[2] ordinal. Sets y-positions and visibility for entries 7-10. Looks up offer in transferOfferTable. Selects format strings based on offer type (normal/improved/further improved × single/two player × offer/exchange). Formats player names via PrintFormatted. Handles text wrapping (single line if ≤47 chars, otherwise split into two lines). Draws entries 7-10 via DrawMultipleItems.
- Stubs: a5 register bridge (movem.l d2-d7/a2-a6,-(sp); move.l a5,-(sp); jsr _OffersAfterDrawCommon; addq.l #4,sp; movem.l (sp)+,...; rts)
- New xrefs: _OffersAfterDrawCommon
- New xdefs: offerFmtSinglePlayer, offerFmtTwoPlayers, exchangeFmtSinglePlayer, exchangeFmtTwoPlayers, offerFmtLine1a, offerFmtLine2a, offerFmtLine1b, offerFmtLine2b, exchangeFmtLine1a, exchangeFmtLine2a, exchangeFmtLine1b, exchangeFmtLine2b, aAOfferForB, aAOfferForBAndC, aAExchangeOffer, aAExchangeOffer_0, aAExchangeOffer_1, aAExchangeOffer_2, aAOfferFor, aAOfferFor_0, aB, aB_0, aB_1, aB_2, aB_3, aBAndC, aBAndC_0, aBAndC_1, aBAndC_2, aBAndC_3, aBAndC_4, aAImprovedOffer, aAImprovedOffer_0, aAImprovedOffer_1, aAImprovedOffer_2, aAImprovedExcha, aAImprovedExcha_0, aAImprovedExcha_1, aAImprovedExcha_2, aAFurtherImprov, aAFurtherImprov_0, aAFurtherImprov_1, aAFurtherImprov_2, aAFurtherImprov_3, aAFurtherImprov_4, aAFurtherImprov_5, aAFurtherImprov_6, aOfferForB
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1451 functions across 168 chunks

### Chunk 166 — TransferPlayerInit + ThirdPartyOfferMenuInit (2 → transfer_market.c)
- Date: 2026-03-03
- Ported:
  1. TransferPlayerInit (→transfer_market.c) — init callback for playerTransferMenu; handles teamWillingToSellOrExchange visibility logic, formats "DEAR %a %b", selects sell/exchange strings from table, formats team names
  2. ThirdPartyOfferMenuInit (→transfer_market.c) — init callback for thirdPartyOfferMenu; formats offer amount, "DEAR %a %b", handles offerStatus 0-4 with different text lines, manages entry visibility
- Stubs: both a5 register bridges (movem.l d2-d7/a2-a6,-(sp); move.l a5,-(sp); jsr _Func; addq.l #4,sp; movem.l (sp)+,...; rts)
- New xrefs: _TransferPlayerInit, _ThirdPartyOfferMenuInit
- New xdefs: managerSexBuffer, managerSurnameBuffer, aDearAB, aInADealWithAFo, aUnlessTheOffer, aWeHereAtAWould, aWeHereAtAWishT, aWeHereAtAWishT_0, aOurOfferToBuy, aWeHereAtAWishT_1, aImproveOurOffe, aWeHereAtARefus, aImproveOnOurOf, aWeHereAtAWithd, aOfferOn, aFromAInExchang
- Build status: **verified** (clean build)
- Cumulative: 1438 functions across 166 chunks

### Chunk 147 — BuyOtherPlayers Init/Finish + Pitch/Season Menu Callbacks (5 → transfer_market.c, menus.c)
- Date: 2026-03-02
- Ported:
  1. BuyOtherPlayersInit (→transfer_market.c) — get team pointer from teamOffering, store in buyOtherPlayersTeamPtr, fall through to Finish
  2. BuyOtherPlayersFinish (→transfer_market.c) — set menu entry 0 title and entry 1 team name from stored pointers
  3. PitchTypeOrSeasonProc (→menus.c) — toggle pitchTypeOrSeason, update table pointer, redraw 2 entries
  4. SetFriendlyPitchOrSeason (→menus.c) — set menu entry 4's fg to friendlyPitchTypeTable or friendly_season_table
  5. FriendlyPitchTypeProc (→menus.c) — cycle pitch type (0-6→RANDOM) or season (0-11), redraw entry
- Stubs: BuyOtherPlayersInit (jmp), BuyOtherPlayersFinish (jmp), PitchTypeOrSeasonProc (jmp), SetFriendlyPitchOrSeason (jmp), FriendlyPitchTypeProc (a5→stack bridge)
- New xrefs: _BuyOtherPlayersInit, _BuyOtherPlayersFinish, _PitchTypeOrSeasonProc, _SetFriendlyPitchOrSeason, _FriendlyPitchTypeProc
- New xdefs: buyOtherPlayersTeamPtr, buyOtherPlayersTitlePtr, friendlyPitchTypeTable, friendly_season_table
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1327 functions across 147 chunks

### Chunk 146 — Offer Expiry + Random Transfer Offers (3 → transfer_market.c)
- Date: 2026-03-02
- Ported:
  1. ProcessOfferExpiryCountdown (→transfer_market.c) — loop: skip if national selector, decrement offerExpiryIterationsRemaining, call UpdateAndExpireTransferOffers each iteration
  2. UpdateAndExpireTransferOffers (→transfer_market.c) — decrement expiry counters in transferOfferTable, remove expired offers, then generate random offers
  3. GenerateRandomTransferOffers (→transfer_market.c) — loop through careerNumPlayers starting at random offset, skip players with bit 0x02 at offset 0x66 or status 0x9A/0x9B at 0x71, roll random chance based on whether player is on transfer market, call CreateRandomTransferOfferForPlayer
- Stubs: all 3 simple jmp stubs
- New asm bridge: _CCreateRandomTransferOfferForPlayer (C→asm, puts param in d1, saves/restores all regs)
- New xrefs: _ProcessOfferExpiryCountdown, _UpdateAndExpireTransferOffers, _GenerateRandomTransferOffers
- New xdefs: _CCreateRandomTransferOfferForPlayer
- Key notes: GenerateRandomTransferOffers uses _cGetPlayerAtIndexFileIdx (set by GetPlayerAtIndex bridge) to pass file index to FindPlayerOnTransferMarket and CreateRandomTransferOfferForPlayer. UpdateAndExpireTransferOffers originally fell through to GenerateRandomTransferOffers via `bsr.w *+4`; now directly calls it.
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1322 functions across 146 chunks

### Chunk 145 — Player Offers Menu Helpers + GetCompetitionAbbrevStr (4 exported + 1 dead → transfer_market.c, career_season.c)
- Date: 2026-03-02
- Ported:
  1. PlayerOffersUpdateOfferAmountText (→transfer_market.c) — format offerAmount * 1000 into menu entry 9's text string via Int2Pounds
  2. UpdatePlayerOfferMenuIndicators (→transfer_market.c) — toggle +/- string and isInvisible for entries 5-11 based on numExtraPlayersWanted, offerSwapPlayerOrdinal, offerExtraPlayerOrdinal signs
  3. PlayerOffersInit (→transfer_market.c) — init player offers menu: bank balance, two PrintFormatted team name strings, calls above two functions
  4. GetCompetitionAbbrevStr (→career_season.c) — returns abbreviation string ("FR","WC","WCQ","EC","ECQ","NAC","CA","ANC","AC","OC","TNM","TR") by comparing competition pointer against known buffer addresses
- Dead code: IsTeamIdForSelectedTeam — no callers found, stubbed as rts
- Stubs: PlayerOffersUpdateOfferAmountText (jmp), UpdatePlayerOfferMenuIndicators (jmp), PlayerOffersInit (jmp), GetCompetitionAbbrevStr (bridge: a1→stack, d0→a2)
- New xrefs: _PlayerOffersUpdateOfferAmountText, _UpdatePlayerOfferMenuIndicators, _PlayerOffersInit, _GetCompetitionAbbrevStr
- New xdefs: offerSwapPlayerOrdinal, offerExtraPlayerOrdinal, offerAmount, aPlus
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1319 functions across 145 chunks

### Chunk 144 — ShowForeignPlayerMarket + ViewForeignPlayerMarket (2 → transfer_market.c)
- Date: 2026-03-02
- Ported:
  1. ShowForeignPlayerMarket (→transfer_market.c) — loads pool player data, optionally adds random foreign market player (if removedOfferPlayersFromOtherTeamsCount > 0), updates foreign market players, shows player market menu
  2. ViewForeignPlayerMarket (→transfer_market.c) — calls ShowForeignPlayerMarket then duplicate transferCompleted check (preserved from original)
- Stubs: both simple jmp stubs
- New xrefs: _ShowForeignPlayerMarket, _ViewForeignPlayerMarket
- Key notes: Same duplicate transferCompleted check pattern as ViewDomesticPlayerMarket. showDomesticPlayerMarket set to 0 before showing menu.
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1315 functions across 144 chunks

### Chunk 143 — ExitOptions + SetSquadMenuEntryStatusSprite + SetSquadMenuEntryStatusSprite_AfterSim (3 → menus.c)
- Date: 2026-03-02
- Ported:
  1. ExitOptions (→menus.c) — options menu exit with prerelease easter egg counter (if controlMask==0x30, count to 15 to reveal hidden menu entry 14)
  2. SetSquadMenuEntryStatusSprite (→menus.c) — sets player status sprite ID in fg high word based on injury/status bits; two paths (flag==0 vs !=0) with different bit masks; also clears bg low nibble on some exits
  3. SetSquadMenuEntryStatusSprite_AfterSim (→menus.c) — after-sim variant: calls ShouldApplyRandomPlayerStatusAfterSim first, delegates to SetSquadMenuEntryStatusSprite(flag=0) or computes simpler sprite
- Stubs: ExitOptions (jmp), SetSquadMenuEntryStatusSprite (d0/a6/a0→stack bridge), SetSquadMenuEntryStatusSprite_AfterSim (a6/a0→stack bridge)
- New xrefs: _ExitOptions, _SetSquadMenuEntryStatusSprite, _SetSquadMenuEntryStatusSprite_AfterSim
- Key notes: showPrereleaseCounter is asm variable (already xdef'd from Chunk 113), shared between _OptionsMenuBeforeDraw and _ExitOptions. Player address range check: careerTeam <= player < &careerNumPlayers.
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1313 functions across 143 chunks

### Chunk 142 — StringCopyUntilChar bridge + SaveCareer + SelectFilesInputSaveFilenameOnSelect + ViewDomesticPlayerMarket (4 → string_utils.c bridge, menus.c, file_io.c, transfer_market.c)
- Date: 2026-03-02
- Ported:
  1. StringCopyUntilChar — asm bridge to existing _StringCopyUntilChar in string_utils.c (a0/a1/d0→stack)
  2. SaveCareer (→menus.c) — save career file dialog: prompt insert disk, select file, save, copy filename
  3. SelectFilesInputSaveFilenameOnSelect (→file_io.c) — handle save filename input in file selection; checks max 45 files, InputText, AddExtension tail-call, or restore default name
  4. ViewDomesticPlayerMarket (→transfer_market.c) — show domestic market + duplicate transferCompleted check (preserved)
- Stubs: SaveCareer (jmp), ViewDomesticPlayerMarket (jmp), SelectFilesInputSaveFilenameOnSelect (a5→stack bridge), StringCopyUntilChar (a0/a1/d0→stack bridge)
- New xdefs: transferCompleted
- New xrefs: _StringCopyUntilChar, _SaveCareer, _SelectFilesInputSaveFilenameOnSelect, _ViewDomesticPlayerMarket
- Key notes: SaveCareer uses selectedFilename global directly (a0 undefined after SelectFileToSaveDialog C bridge); both success and error paths call ShowInsertDisk2Dialog. SelectFilesInputSaveFilenameOnSelect preserves exact == 45 check (not >=). ViewDomesticPlayerMarket has duplicate transferCompleted check — likely original bug, preserved.
- Build status: **verified** (clean build, only pre-existing warnings)
- Cumulative: 1310 functions across 142 chunks

### Chunk 141 — DIY Stats Menu + File Type Description + Player Face Entry (3 → diy_competition.c, file_io.c, menus.c)
- Date: 2026-03-02
- Ported: SetupDIYCompetitionStatsMenu (→diy_competition.c), SelFilesDrawItemDescription (→file_io.c), SetMenuEntryPlayerFace (→menus.c)
- jmp stub: SetupDIYCompetitionStatsMenu
- Register bridges: SelFilesDrawItemDescription (a5→stack), SetMenuEntryPlayerFace (a0,a6→stack)
- New xdefs: diyNumberOfTeams, diyTeamsBufferPtr, competitionStatsMenu
- Key notes: SelFilesDrawItemDescription uses C string literals ("DIY","PRESET",etc.) instead of asm strings. SetMenuEntryPlayerFace extracts face bits 3-4 from TEAMFILE_PLAYERS+PLAYERFILE_POSITIONANDFACE and adds SPR_PL_FACE_WHITE_GUY(186).
- Build status: **verified** (clean build, no new warnings)
- Cumulative: 1306 functions across 141 chunks

### Chunk 139 — Team Selection Type Handlers + League/Country Names (7 → menus.c)
- Date: 2026-03-02
- Ported: ClubTeams, NationalTeams, OriginalCustomTeams, EditedCustomTeams, ExitChooseTeams, SetLeagueNames, SetCountryNames — all to menus.c
- jmp stubs: all 7 functions
- New xdefs: numLeaguesInCountry, teamNames, chooseTeamsNumTeams, countryNames, teamsLowerBound
- Key notes: SetLeagueNames has dead code (first lea+tst overwritten by second lea), SetCountryNames uses 68k divu column layout
- Build status: **verified** (clean build, no new warnings from menus.c)
- Cumulative: 1298 functions across 139 chunks

### Chunk 138 — FillManagementRecordInfo + helpers (1+3s → statistics.c)
- Date: 2026-03-02
- Ported: FillManagementRecordInfo (exported), SetTrophySprites_Internal (static), SetTrophySprite_Internal (static), RenderCompLine_Internal (static) — all to statistics.c
- jmp stub: FillManagementRecordInfo
- Dead asm stubs: RenderCompLine→rts, SetTrophySprites→rts, SetTrophySprite→rts (only called from now-C FillManagementRecordInfo)
- Fixed: _StringCopy local extern in statistics.c (was wrong pointer-to-pointer signature, fixed to match header)
- New xdefs: aFriendlies, aWon0Drawn1Lost, aEuropeanTour, aOceaniaTour, aWinners, aRunnerUp, aFinalist, aReachedSemiFin, aReachedQuarter, aThirdPlace, aFourthPlace, aStillInCompeti, aKnockedOutInRo, aPosition0, aQualified, aNotQualified, aPromoted, aRelegated, a12Season, a102Season, aNational, aA, aA0, aSeasonSTopScor, aSeasonSTopScor_0, aRetired, trophySprites
- Build status: **verified** (clean build, no new warnings)
- Cumulative: 1291 functions across 138 chunks

### Chunk 137 — Management Record + SelTeamForTactics Reflow (6+1s → statistics.c, menus.c)
- Date: 2026-03-02
- Ported: GetCurrentSeasonPointer, ManagementRecordInit, ManagementArrowUpSelect, ManagementArrowDownSelect, Career_StorePromotionRelegationInSeasonInfo (→ statistics.c), SelTeamForTactics_ReflowTeamListLayout (→ menus.c)
- jmp stubs: ManagementRecordInit, ManagementArrowUpSelect, ManagementArrowDownSelect, GetCurrentSeasonPointer, Career_StorePromotionRelegationInSeasonInfo, SelTeamForTactics_ReflowTeamListLayout
- Stub updates: jmp_SetExitMenuFlag_3→_SetExitMenuFlag
- New xdefs: currentSeasonNumber, gamesPlayed, numWins, numDrawn, numLost, goalsScored, goalsTaken, aManagementReco, aPl0W1D2L3F4A5, aDash, selTeamsColumn3, numTeamsCountryBuffer, selTeamsListTopY, selTeamsListBottomY
- Build status: **verified** (clean build, no new warnings)
- Cumulative: 1287 functions across 137 chunks

### Phase Transitions (see CLAUDE.md "Project Phases")
- [ ] Phase 1 complete: all subroutines ported → output `*** READY FOR TESTING PHASE! ***` and STOP
- [ ] Phase 2: user tests game, fixes any bugs found
- [ ] Phase 3: remove assembly code (stubs, dead floppy), keep only data in asm
- [ ] Phase 4: gradual data migration (asm → C) in small tested batches with adjacency-aware planning

---

## Cumulative Function Count

| Chunk | Functions | Cumulative |
|-------|-----------|------------|
| 0     | 1         | 1          |
| 1     | 7         | 8          |
| 1.5   | 6         | 14         |
| 2     | 10        | 24         |
| 3     | 6+4s      | 34         |
| 4     | 5         | 39         |
| 5     | 7+1s      | 47         |
| 6     | 7         | 54         |
| 7     | 3         | 57         |
| 8     | 7         | 64         |
| 9     | 5+1s      | 70         |
| 10    | 3+2s      | 75         |
| 11    | 11        | 86         |
| 12    | 17        | 103        |
| 13    | 14        | 117        |
| 14    | 8+1s      | 126        |
| 15    | 13        | 139        |
| 16    | 15+2s     | 156        |
| 17    | 8+1       | 165        |
| 18    | 7+4s      | 176        |
| 19    | 30        | 206        |
| 20    | 11        | 217        |
| 21    | 5         | 222        |
| 22    | 7+2s      | 231        |
| 23    | 5         | 236        |
| 24    | 2         | 238        |
| 25    | 8+1s      | 247        |
| 26    | 4+2s      | 253        |
| 27    | 10        | 263        |
| 28    | 7         | 270        |
| 29    | 5         | 275        |
| 30    | 6         | 281        |
| 31    | 21        | 302        |
| 32    | 34        | 336        |
| 33    | 5         | 341        |
| 34    | 12        | 353        |
| 35    | 5         | 358        |
| 36    | 5         | 363        |
| 37    | 16        | 379        |
| 38    | 11        | 390        |
| 39    | 22        | 412        |
| 40    | 7         | 419        |
| 41    | 10        | 429        |
| 42    | 2         | 431        |
| 43    | 12        | 443        |
| 44    | 6         | 449        |
| 45    | 9         | 458        |
| 46    | 9         | 467        |
| 47    | 10        | 477        |
| 48    | 6         | 483        |
| 49    | 2+7s      | 492        |
| 50    | 7         | 499        |
| 51    | 12        | 511        |
| 52    | 4         | 515        |
| 53    | 6         | 521        |
| 54    | 1         | 522        |
| 55    | 1         | 523        |
| 56    | 2         | 525        |
| 57    | 12+3s+1a  | 541        |
| 58    | 6+4s      | 551        |
| 59    | 18+8s     | 578        |
| 60    | 16+2s     | 596        |
| 61    | 10        | 606        |
| 62    | 12+1s     | 619        |
| 63    | 5+1a      | 625        |
| 64    | 5+1s      | 631        |
| 65    | 7+12s     | 650        |
| 66    | 9+2s      | 661        |
| 67    | 7         | 668        |
| 68    | 1         | 669        |
| 69    | 4         | 673        |
| 70    | 2         | 675        |
| 71    | 3         | 678        |
| 72    | 11        | 689        |
| 73    | 10        | 699        |
| 74    | 7         | 706        |
| 75    | 9+1s      | 716        |
| 76    | 6+1s      | 723        |
| 77    | 9         | 732        |
| 78    | 7         | 739        |
| 79    | 6         | 745        |
| 80    | 12        | 757        |
| 81    | 6         | 763        |
| 82    | 11        | 774        |
| 83    | 12        | 786        |
| 84    | 11        | 797        |
| 85    | 16        | 813        |
| 86    | 6         | 819        |
| 87    | 7         | 826        |
| 88    | 6         | 832        |
| 89    | 9         | 841        |
| 90    | 9+3s      | 853        |
| 91    | 12        | 865        |
| 92    | 12        | 877        |
| 93    | 6         | 883        |
| 94    | 8+1s      | 891        |
| 95    | 13+3s     | 907        |
| 96    | 7+1s      | 915        |
| 97    | 8+2s      | 925        |
| 98    | 29+1s     | 954        |
| 99    | 8         | 962        |
| 100   | 5+1s      | 968        |
| 101   | 2+3s      | 970        |
| 102   | 3+1s      | 973        |
| 103   | 6+2d      | 981        |
| 104   | 1         | 982        |
| 105   | 4+3d      | 986        |
| 106   | 1         | 987        |
| 107   | 4         | 991        |
| 108   | 10        | 1001       |
| 109   | 10        | 1011       |
| 110   | 6         | 1017       |
| 111   | 5         | 1022       |
| 112   | 4         | 1026       |
| 113   | 15        | 1041       |
| 114   | 9         | 1050       |
| 115   | 3         | 1053       |
| 116   | 13        | 1066       |
| 117   | 6         | 1072       |
| 118   | 1         | 1073       |
| 119   | 9         | 1082       |
| 120   | 15        | 1097       |
| 121   | 5         | 1102       |
| 122   | 15        | 1117       |
| 123   | 9         | 1126       |
| 124   | 7         | 1133       |
| 125   | 4         | 1137       |
| 126   | 12        | 1149       |
| 127   | 17+1s     | 1167       |
| 128   | 17+1s     | 1185       |
| 129   | 8+2s      | 1195       |
| 130   | 10        | 1205       |
| 131   | 12        | 1217       |
| 132   | 9+1s      | 1227       |
| 133   | 17+5s     | 1249       |
| 134   | 5+8s      | 1262       |
| 135   | 9+1s      | 1272       |
| 136   | 7+1s      | 1280       |
| 137   | 6+1s      | 1287       |
| 138   | 1+3s      | 1291       |
| 139   | 7         | 1298       |
| 140   | 4+1s      | 1303       |
| 141   | 3         | 1306       |
| 142   | 4         | 1310       |
| 143   | 3         | 1313       |
| 144   | 2         | 1315       |
| 145   | 4+1d      | 1319       |
| 146   | 3         | 1322       |
| 147   | 5         | 1327       |
| 148   | 8         | 1335       |
| 149   | 10        | 1345       |
| 150   | 13        | 1358       |
| 151   | 7         | 1365       |
| 152   | 8         | 1373       |
| 153   | 3         | 1376       |
| 154   | 2         | 1378       |
| 155   | 2         | 1380       |
| 156-159 | 13      | 1393       |
| 160   | 0d        | 1393       |
| 161   | 6         | 1399       |
| 162   | 8         | 1407       |
| 163   | 13        | 1420       |
| 164   | 5         | 1425       |
| 165   | 11+1s     | 1436       |
| 166   | 2         | 1438       |
| 167   | 9+3s      | 1450       |
| 168   | 1         | 1451       |
| 169   | 3         | 1454       |
| 170   | 4         | 1458       |
| 171   | 3         | 1461       |
| 172   | 2         | 1463       |
| 173   | 1         | 1464       |
| 174   | 4         | 1468       |
| 175   | 2         | 1470       |
| 176   | 5         | 1475       |
| 177   | 1         | 1476       |
| 178   | 1         | 1477       |
| 179   | 3         | 1480       |
| 180-184 | —       | —          |
| 185   | 13        | 1493       |
| 186   | 3         | 1496       |
| 187   | 2+2d      | 1500       |
| 188   | 1         | 1501       |
| 189   | 1         | 1502       |
| 190   | 5+3s      | 1507       |
| 191   | 4         | 1511       |
| 192   | 2         | 1513       |
| 193   | 2         | 1515       |
| 194   | 2+2d      | 1519       |
| 195   | 1         | 1520       |
| 196   | 4+2d      | 1526       |
| 197   | 6         | 1532       |
| 198   | 1         | 1533       |

(s = static/internal helpers, a = alias/shared, d = dead code stubbed)
