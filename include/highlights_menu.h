/* highlights_menu.h — Highlights menu functions. */
#pragma once
#include "game_types.h"

void _HighlightsMenuInit(void);
void _HighlightsMenuOnReturn(void);
void _HighlightsMenuCommon(void);
void _ExitHighlights(void);

/* --- Highlights file load/save (Chunk 89) --- */
void _LoadHighlightsMenu(void);
void _SaveHighlightsMenu(void);

/* --- Chunk 161: Show/view highlights --- */
void _ShowHighlights(void);
void _ShowMatchSequence(void);
void _ViewHighlightsSaveCurrentMenu(void);
void _JmpShowHighlights(void);
