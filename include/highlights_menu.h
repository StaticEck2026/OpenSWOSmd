/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* highlights_menu.h — Highlights menu functions. */
#pragma once
#include "game_types.h"

void HighlightsMenuInit(void);
void HighlightsMenuOnReturn(void);
void HighlightsMenuCommon(void);
void ExitHighlights(void);

/* --- Highlights file load/save (Chunk 89) --- */
void LoadHighlightsMenu(void);
void SaveHighlightsMenu(void);

/* --- Chunk 161: Show/view highlights --- */
void ShowHighlights(void);
void ShowMatchSequence(void);
void ViewHighlightsSaveCurrentMenu(void);
void JmpShowHighlights(void);
