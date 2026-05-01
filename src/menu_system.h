/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;

/* ---- Menu struct (22 bytes) ---- */
/* Packed to match 68k memory layout exactly */
typedef struct __attribute__((packed)) {
    uint32_t onInit;        /* 0: function pointer (or 0) */
    uint32_t afterDraw;     /* 4: function pointer (or 0) */
    uint32_t onDraw;        /* 8: function pointer (or 0) */
    uint32_t selectedEntry; /* 12: pointer to selected MenuEntry */
    uint16_t numEntries;    /* 16 */
    uint32_t endOfMenuPtr;  /* 18 */
} Menu;

/* ---- MenuEntry struct (56 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint16_t alreadyDrawn;   /* 0 */
    uint16_t ordinal;        /* 2 */
    uint16_t isInvisible;    /* 4 */
    uint16_t isDisabled;     /* 6 */
    int8_t   leftEntry;      /* 8 */
    int8_t   rightEntry;     /* 9 */
    int8_t   upEntry;        /* 10 */
    int8_t   downEntry;      /* 11 */
    int8_t   directionLeft;  /* 12 */
    int8_t   directionRight; /* 13 */
    int8_t   directionUp;    /* 14 */
    int8_t   directionDown;  /* 15 */
    int8_t   skipLeftEntry;  /* 16 */
    int8_t   skipRightEntry; /* 17 */
    int8_t   skipUpEntry;    /* 18 */
    int8_t   skipDownEntry;  /* 19 */
    uint16_t x;              /* 20 */
    uint16_t y;              /* 22 */
    uint16_t dwWidth;        /* 24 */
    uint16_t dwHeight;       /* 26 */
    uint16_t background;     /* 28 */
    uint32_t bg;             /* 30 */
    uint16_t content;        /* 34 */
    uint16_t stringFlags;    /* 36 */
    uint32_t fg;             /* 38 */
    uint32_t onSelect;       /* 42 */
    uint16_t controlMask;    /* 46 */
    uint32_t beforeDraw;     /* 48 */
    uint32_t afterDrawCb;    /* 52 */
} MenuEntry;

/* Helpers to access the low memory word of MenuEntry.bg / MenuEntry.fg.
 * The unpacker writes and _DrawMenuItem reads the first 2 bytes of these
 * uint32 slots in memory order — on Amiga (BE) that's the upper 16 bits
 * of the field value, on x86 (LE) it's the lower 16 bits. We don't care
 * which "half" of the value it is, only that the bytes match across
 * writer and reader.
 *
 * Earlier helpers used `e->bg >> 16` which is correct on BE but wrong
 * on LE: runtime color overrides written via the helper went to the
 * upper half while the draw read the lower half, so the entry kept its
 * unpack-time template color. Use `*(uint16_t *)&e->bg` so the compiler
 * computes the field's offset (portable to 64-bit) and we still hit the
 * low memory bytes the renderer reads. */
static inline uint16_t getBgColorWord(MenuEntry *e) {
    return *(uint16_t *)&e->bg;
}
static inline void setBgColorWord(MenuEntry *e, uint16_t v) {
    *(uint16_t *)&e->bg = v;
}
/* Set only the low nibble (color index) of the bg color word */
static inline void setBgColorIdx(MenuEntry *e, uint16_t colorIdx) {
    uint16_t *w = (uint16_t *)&e->bg;
    *w = (*w & 0xFFF0) | colorIdx;
}

/* Same shape for the fg slot — the unpacker stores a 16-bit content
 * payload (sprite index, number, etc.) and the renderer reads it back
 * the same way. */
static inline uint16_t getFgWord(MenuEntry *e) {
    return *(uint16_t *)&e->fg;
}
static inline void setFgWord(MenuEntry *e, uint16_t v) {
    *(uint16_t *)&e->fg = v;
}

/* Compile-time size checks */
typedef char _menu_size_check[(sizeof(Menu) == 22) ? 1 : -1];
typedef char _menuentry_size_check[(sizeof(MenuEntry) == 56) ? 1 : -1];

/* ---- Constants ---- */
#define MENU_HEADER_SIZE   22
#define MENUENTRY_STRIDE   56
#define NO_BACKGROUND      0
#define NO_FOREGROUND      0

/* Background types (MENUENTRY_BACKGROUND field) */
#define ENTRY_FUNC1                 1
#define ENTRY_FRAME_AND_BACK_COLOR  2
#define ENTRY_SPRITE1               3

/* Content/foreground types (MENUENTRY_CONTENT field) */
#define ENTRY_FUNC2            1
#define ENTRY_STRING           2
#define ENTRY_SPRITE2          3
#define ENTRY_STRING_TABLE     4
#define ENTRY_MULTI_LINE_TEXT  5
#define ENTRY_NUMBER           6
#define ENTRY_SPRITE_COPY      7

/* String flags masks */
#define STR_COLOR_MASK   15
#define STR_FLAGS_MASK   240

/* Color constants */
#define YELLOW_GREEN_BLACK_15  15

/* CharTable offsets */
#define CHARTABLE_CHARHEIGHT       2
#define CHARTABLE_CHARSPACING      8

/* SpriteGraphics offsets */
#define SPRITEGRAPHICS_PIXWIDTH   10
#define SPRITEGRAPHICS_NLINES     12

/* ---- Asm helpers (save/restore all regs around callback) ---- */
extern void _CallMenuCallback(void *funcPtr);
extern void _CallMenuOnSelect(void *funcPtr, void *selectedEntry);
extern void _CallMenuFuncWithXY(void *funcPtr, uint16_t x, uint16_t y);

/* ---- Ported functions ---- */
void _MenuProc(void);
void _ActivateMenu(uint8_t *packedMenu);
void _ShowMenu(uint8_t *packedMenu);
void _RestorePreviousMenu(uint32_t oldSelectedEntry, uint8_t *oldPackedMenu);
void _DrawMenu(void);
void _MenuCheckControls(void);
void _DrawSelectedMenuItemFrame(void);
void _UpdateBlinkingMenuText(void);
void _DrawMenuPanelBackground(void);
void _SetExitMenuFlag(void);
MenuEntry *_CalcMenuEntryAddress(uint16_t ordinal);
void _UnpackMenu(uint8_t *packed, uint8_t *dest);
void _DrawMenuItem(MenuEntry *entry);
void _CallDrawMenuItem(MenuEntry *entry);
void _DrawMenuItemInnerFrame(uint16_t color, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void _DrawMenuItemOuterFrame(uint16_t color, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void _DrawMenuItemBackground(uint16_t color, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void _DrawMenuItemBackgroundMasked(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void _DrawMenuBackWithCheck(MenuEntry *entry);
void _DrawBackAllItems(uint16_t maxItems, MenuEntry *firstEntry);
uint16_t _GetKey(void);
void _BlitterCopyOverlayToScreen(void);
void _Int2Ascii(uint16_t value, uint8_t *out);
void _SetCurrentEntry(uint16_t index);
void _BlitMaskedRectAltToFront(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void _DrawMenuItemsFromList(const int16_t *list);
void _DrawMultipleItems(uint16_t maxItems, MenuEntry *firstEntry);
void _DrawMenuItemsWaitForVSync(const int16_t *list);
void _CallDrawMultipleItems(uint16_t maxItems, MenuEntry *firstEntry);
void _CallDrawMultipleItemsRect(uint16_t maxItems, MenuEntry *firstEntry);

#endif
