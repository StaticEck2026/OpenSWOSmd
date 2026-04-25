/*
 * swos_globals.c -- Storage for data symbols that originally lived in
 * SWOS2.s (the Amiga assembly data section).
 *
 * On the Amiga build these were defined by `dc.b / dc.w / dc.l / ds.b`
 * directives in the assembler file. When we moved everything to pure C
 * we left the *_data.c modules (competition_data, palette_data, ...) as
 * the C-data variants, but there is still a long tail of BSS-style
 * scratch buffers and flags that the assembly defined.
 *
 * This file defines those symbols as zero-initialised globals. Adding a
 * new entry here is the go-to fix for an "undefined reference to X" link
 * error.
 */

#include "swos_platform.h"
#include "display.h"
#include "game_types.h"

#include <stdint.h>

/* ---- Input state shared with input_sdl.c --------------------------- */
uint8_t  keyState[128];
uint16_t joy0Dat;
uint16_t joy1Dat;
/* CIAA PRA bit 7 = fire button (active low). 0xFF = idle (no buttons). */
uint8_t  ciaa_pra = 0xFF;

/* ---- nextEntryDirectionOffsets ----
 * Maps menu cursor direction (0=Up, 2=Right, 4=Down, 6=Left, after
 * dirIdx>>1) to the byte offset of the corresponding navigation field
 * inside MenuEntry, measured from offset +8 (leftEntry). The packed
 * MenuEntry layout is:
 *   +8  leftEntry   +9  rightEntry   +10 upEntry   +11 downEntry
 * so the deltas relative to +8 are 0/1/2/3 for left/right/up/down.
 * _MenuProc indexes this with dir>>1, so the array order is
 * Up/Right/Down/Left -> 2/1/3/0. */
uint16_t nextEntryDirectionOffsets[4] = { 2, 1, 3, 0 };

/* ---- Framebuffer / bitplane storage -------------------------------- *
 * On the real hardware the Amiga kept the bitplanes in chip RAM, with
 * the copper pointing at them via bplXpt. We allocate fixed-size
 * buffers that match FRONTBUFFER_SIZE_TOTAL_BYTES. */

/* Size of one bitplane scanline = PLANE_STRIDE (44). There are
 * FRONTBUFFER_TOTAL_LINES (273) scanlines, for a grand total of
 * PLANE_STRIDE * FRONTBUFFER_TOTAL_LINES = 12012 bytes per plane. */
#define BITPLANE_BYTES (PLANE_STRIDE * FRONTBUFFER_TOTAL_LINES)

/* "Front" set (visible this frame) + "alt" set (drawn into). */
uint8_t bitPlane0Base[BITPLANE_BYTES];
uint8_t bitPlane1Base[BITPLANE_BYTES];
uint8_t bitPlane2Base[BITPLANE_BYTES];
uint8_t bitPlane3Base[BITPLANE_BYTES];
#ifdef AGA
uint8_t bitPlane4Base[BITPLANE_BYTES];
uint8_t bitPlane5Base[BITPLANE_BYTES];
uint8_t bitPlane6Base[BITPLANE_BYTES];
uint8_t bitPlane7Base[BITPLANE_BYTES];
#endif
uint8_t bitPlane0BaseAlt[BITPLANE_BYTES];
uint8_t bitPlane1BaseAlt[BITPLANE_BYTES];
uint8_t bitPlane2BaseAlt[BITPLANE_BYTES];
uint8_t bitPlane3BaseAlt[BITPLANE_BYTES];
#ifdef AGA
uint8_t bitPlane4BaseAlt[BITPLANE_BYTES];
uint8_t bitPlane5BaseAlt[BITPLANE_BYTES];
uint8_t bitPlane6BaseAlt[BITPLANE_BYTES];
uint8_t bitPlane7BaseAlt[BITPLANE_BYTES];
#endif

/* Bitplane base "pointers" — the Amiga sets these to the address of
 * each bitplane for the copper. On the host we store raw addresses. */
volatile uint32_t bitPlane0BasePtr;
volatile uint32_t bitPlane1BasePtr;
volatile uint32_t bitPlane2BasePtr;
volatile uint32_t bitPlane3BasePtr;
#ifdef AGA
volatile uint32_t bitPlane4BasePtr;
volatile uint32_t bitPlane5BasePtr;
volatile uint32_t bitPlane6BasePtr;
volatile uint32_t bitPlane7BasePtr;
#endif

/* "Frontbuffer" arrays. Layout depends on build:
 *   Amiga: bitplane-interleaved (NUM_PLANES planes per scanline).
 *   SDL  : 8bpp chunky, 1 byte per pixel = palette index. */
uint8_t frontbufferBase[FRONTBUFFER_SIZE_TOTAL_BYTES];
uint8_t frontbufferBaseAlt[FRONTBUFFER_SIZE_TOTAL_BYTES];
uint8_t frontbufferMaskBuffer[FRONTBUFFER_SIZE_TOTAL_BYTES];
volatile uint32_t frontbufferBasePtr;
volatile uint32_t frontbufferBaseAltPtr;

#ifdef SWOS_SDL_BUILD
/* SDL-only scratch buffer used by `_ProcessImageCommandStream` and
 * `_LoadOverlayToFrontbuffer` to receive raw RNC-decompressed bitplane
 * file loads. The Amiga build wrote bitplane data directly into
 * `frontbufferMaskBuffer`, but we now keep that buffer in chunky 8bpp
 * format (matching the frontbuffer), so we need a separate area for the
 * raw bitplane data the loader produces. 128 KB covers the largest
 * single-asset load (HERO.INS at ~286 KB is split, the largest single
 * grafs file is well under 128 KB). */
uint8_t rawAssetLoadBuffer[128 * 1024];
#endif

/* Copper list (OCS 320 bytes / AGA 1316 bytes). */
uint8_t  copperListBuffer[COPPERLIST_SIZE_BYTES];
uint32_t copperZeroLong;

/* ---- menuPalette: 16-entry OCS palette referenced by SWOS.c (AGA path)
 * and Platform_PreArmIsrGlobals. The Amiga build had this in SWOS2.s
 * inside an IFEQ/ELSE block -- the 256-entry AGA variant first, then an
 * OCS 16-entry fallback. We want the OCS values here; the earlier copy
 * took the first 16 AGA entries by mistake (which is why the menu bg
 * came out grey -- indices 1/7/8/9 were pure greys in the AGA table and
 * the menubg image pixel values are tuned to map to the OCS blues). */
uint16_t menuPalette[16] = {
    0x0001, 0x0AAB, 0x0FFF, 0x0102, 0x0621, 0x0A40, 0x0F71, 0x0667,
    0x0204, 0x0445, 0x0F00, 0x000F, 0x0702, 0x088F, 0x0380, 0x0FF0
};

/* ---- Title color tables: 16 RGB triplets (48 bytes) each.
 * `_BuildPaletteFromColors` compresses each triplet into a 12-bit Amiga
 * colour word and writes into titleLogoPalette / titleScreenPalette /
 * titleTextPalette. Originally defined in SWOS2.s. */
uint8_t kTitleLogoColors[48] = {
    0x00,0x00,0x22, 0xAA,0x00,0x00, 0xFF,0xBB,0x00, 0x33,0x33,0x33,
    0x44,0x33,0xAA, 0x00,0x88,0x00, 0xCC,0xCC,0xCC, 0x88,0x88,0x88,
    0x55,0x55,0x55, 0xFF,0xFF,0x99, 0xDD,0xDD,0x88, 0xBB,0xBB,0x77,
    0x99,0x99,0x66, 0x77,0x77,0x44, 0x55,0x55,0x33, 0x33,0x33,0x22
};
uint8_t kTitleScreenColors[48] = {
    0x00,0x00,0x23, 0x00,0x00,0x00, 0x23,0x23,0x23, 0x47,0x47,0x47,
    0x77,0x77,0x77, 0x9B,0x9B,0x9B, 0xCF,0xCF,0xCF, 0xEF,0xEF,0xEF,
    0x00,0x33,0xAB, 0x00,0x8B,0xCF, 0xEF,0xCF,0x33, 0x8B,0x00,0x00,
    0xAB,0x00,0x00, 0xDF,0x00,0x00, 0x00,0x67,0x00, 0xFF,0xFF,0xFF
};
uint8_t kTitleTextColors[48] = {
    0x00,0x00,0x22, 0xFF,0x00,0x44, 0xFF,0x00,0x44, 0x00,0x00,0x44,
    0xFF,0x00,0x44, 0xFF,0x00,0x44, 0xFF,0x00,0x44, 0xFF,0x00,0x44,
    0x00,0x00,0x66, 0xFF,0xFF,0x99, 0xDD,0xDD,0x88, 0xBB,0xBB,0x77,
    0x99,0x99,0x66, 0x77,0x77,0x44, 0x55,0x55,0x33, 0x33,0x33,0x22
};

/* ---- introMenuImageScript: transcribed from Data.c with the
 * embedded filename-pointer halves replaced by zero placeholders.
 * The IDA exporter emitted the Amiga runtime address of
 * `aGrafsCharsetRa` as two magic integers (`16, 6594`); those bytes
 * are overwritten at startup by `_SwosSDL_InitImageScripts()` below
 * with the low/high halves of the real host symbol.
 *
 * The remaining 340 integer words are plain data (sprite coords,
 * widths, heights) and don't need any relocation. */
extern const char aGrafsCharsetRa[];
extern const char aGrafsMenusRaw[];
extern const char aGrafsMenus2Raw[];

uint16_t introMenuImageScript[343] = {
    0x7000, 0, 0,            /* filename ptr halves filled at init */
#include "intro_menu_image_script_cmds.inc"
};

/* ---- menuBGImageCmdStream: transcribed from Data.c with the two
 * embedded filename-pointer halves replaced by zero placeholders.
 * The script loads grafs/menus.raw, extracts a batch of sprites,
 * then loads grafs/menus2.raw and extracts the rest. Pointer slots:
 *   [1..2]    -> &aGrafsMenusRaw   (first  0x7000 opcode at index 0)
 *   [393..394]-> &aGrafsMenus2Raw  (second 0x7000 opcode at index 392)
 * Both slots are written by _SwosSDL_InitImageScripts() below. */
uint16_t menuBGImageCmdStream[406] = {
#include "menu_bg_image_script_cmds.inc"
};

extern uint16_t menuColorAndTextScript[];  /* defined below */
extern const char aGrafsCjcgrafsR[];
extern const char aGrafsCjcteamgR[];
extern const char aGrafsCjcbitsRa[];
extern const char aGrafsSoccerSRa[];
extern const char aGrafsCjcbenchR[];
extern uint32_t team1Filename;
extern uint32_t team2Filename;

#include <stddef.h>

void _SwosSDL_InitImageScripts(void)
{
    uintptr_t p = (uintptr_t)aGrafsCharsetRa;
    introMenuImageScript[1] = (uint16_t)(p);
    introMenuImageScript[2] = (uint16_t)(p >> 16);

    uintptr_t m1 = (uintptr_t)aGrafsMenusRaw;
    menuBGImageCmdStream[1] = (uint16_t)(m1);
    menuBGImageCmdStream[2] = (uint16_t)(m1 >> 16);

    uintptr_t m2 = (uintptr_t)aGrafsMenus2Raw;
    menuBGImageCmdStream[393] = (uint16_t)(m2);
    menuBGImageCmdStream[394] = (uint16_t)(m2 >> 16);

    /* menuColorAndTextScript pointer slots. The .inc file lists the
     * word offsets as comments. The script opcode interpreter reads
     * a dc.l as two uint16 halves (low word first, then high word) —
     * matching the $7000 / 0x7000 load-file opcode convention used
     * throughout the image-command pipeline. */
    struct { uint32_t off; uintptr_t val; } mcts[] = {
        {    1, (uintptr_t)aGrafsCjcgrafsR },
        {  406, (uintptr_t)&team1Filename   },
        { 1877, (uintptr_t)&team2Filename   },
        { 3419, (uintptr_t)aGrafsCjcteamgR },
        { 4548, (uintptr_t)aGrafsCjcbitsRa },
        { 4695, (uintptr_t)aGrafsSoccerSRa },
        { 4890, (uintptr_t)aGrafsCjcbenchR },
    };
    for (size_t i = 0; i < sizeof(mcts) / sizeof(mcts[0]); i++) {
        menuColorAndTextScript[mcts[i].off    ] = (uint16_t)(mcts[i].val);
        menuColorAndTextScript[mcts[i].off + 1] = (uint16_t)(mcts[i].val >> 16);
    }
}

/* menuColorAndTextScript — 5166-word image-command script run from
 * _ConvertColorsAndTextSprites at match setup. Loads grafs/cjcgrafs.raw
 * and extracts ~300 reserve-player / score-digit / team-name sprite
 * slots into spriteGraphics[0x71..0xE2]. 7 slots are absolute
 * pointers (dc.l) that need to be patched at init with the address
 * of the host-side C symbol (filename string or team1Filename /
 * team2Filename globals). Body transcribed from SWOS2.s line 32848;
 * pointer slot indices are noted in the .inc. */
extern const char aGrafsCjcgrafsR[];
extern const char aGrafsCjcteamgR[];
extern const char aGrafsCjcbitsRa[];
extern const char aGrafsSoccerSRa[];
extern const char aGrafsCjcbenchR[];
extern uint32_t team1Filename;
extern uint32_t team2Filename;
uint16_t menuColorAndTextScript[5166] = {
#include "menu_color_text_script_cmds.inc"
};

/* ---- currentMenu + menuEntries: adjacent-in-memory pair.
 * On Amiga, SWOS2.s laid these out as:
 *   currentMenu:  ds.b $16         ; 22-byte Menu header
 *   menuEntries:  ds.b $14000      ; 80 KiB of MenuEntry[100]
 * immediately consecutive in BSS. The menu unpacker (_UnpackMenu)
 * writes the Menu header into `dest` (currentMenu) and then entries
 * starting at dest+22 -- which on Amiga lands inside menuEntries.
 * Our SDL build placed symbols wherever the linker liked, so the
 * unpacker's dest+22 writes fell OUTSIDE menuEntries and _DrawMenu
 * read zeros/garbage.
 *
 * Fix: one backing buffer declared in a dedicated named section;
 * `currentMenu` starts at the base, `menuEntries` at +22. The extra
 * 8 KiB of head room absorbs the loader-bootstrap's 2226-byte
 * boundary-spill copy into currentMenu.
 *
 * The `used,aligned(4)` attributes keep the buffer + guarantee the
 * linker doesn't merge it. The inline asm at the bottom emits the
 * `menuEntries` symbol at offset +22 within the same storage. */
__attribute__((used, aligned(4)))
uint8_t currentMenu[8 * 1024 + 80 * 1024];
__asm__(".globl menuEntries\n"
        ".equ   menuEntries, currentMenu + 22\n");

/* ---- Stadium-menu little-player sprite-copy block --------------------
 * The stadium menu (pre-match lineup screen) draws one mini-player
 * sprite per squad slot with MENU_CMD_SPRITE_COPY opcodes whose fg
 * points at `stadiumMenuLittlePlayer`. The SPRITE_COPY handler then
 * reads a 36-byte contiguous struct starting there:
 *
 *   +0..+1   src sprite index (stadiumMenuLittlePlayer — written at
 *            runtime by _StadiumPlayerSpritesBeforeDraw)
 *   +2..+3   dst sprite index (littlePlayerSpriteGeneric = 0x00D6 / 214)
 *   +4..+35  littlePlayersColorTable (16 × uint16_t palette indices)
 *
 * On the Amiga these three symbols were consecutive in BSS and the
 * Amiga C port's Data.c reproduced that by placing the three globals
 * directly adjacent (SWOS2.s line ~116128 onward). The SDL build had
 * them in three separate translation units at linker-determined
 * addresses, so SPRITE_COPY read `dst` and the colour table from
 * random adjacent globals — the bench mini-player came out as an
 * uninitialised sprite index (often 0 → an empty `□` or similar
 * placeholder glyph). This backing buffer restores the Amiga layout
 * with asm symbol aliases, matching the `currentMenu`/`menuEntries`
 * pattern above. */
__attribute__((used, aligned(2)))
uint8_t stadiumMenuLittlePlayerBackingBuf[36] = {
    /* +0  stadiumMenuLittlePlayer (uint16 LE) = 0 */
    0x00, 0x00,
    /* +2  littlePlayerSpriteGeneric (uint16 LE) = 0x00D6 */
    0xD6, 0x00,
    /* +4..+35 littlePlayersColorTable (uint16_t[16] LE, identity 0..15) */
    0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00,
    0x08, 0x00, 0x09, 0x00, 0x0A, 0x00, 0x0B, 0x00,
    0x0C, 0x00, 0x0D, 0x00, 0x0E, 0x00, 0x0F, 0x00,
};
__asm__(".globl stadiumMenuLittlePlayer\n"
        ".equ   stadiumMenuLittlePlayer, stadiumMenuLittlePlayerBackingBuf\n"
        ".globl littlePlayerSpriteGeneric\n"
        ".equ   littlePlayerSpriteGeneric, stadiumMenuLittlePlayerBackingBuf + 2\n"
        ".globl littlePlayersColorTable\n"
        ".equ   littlePlayersColorTable, stadiumMenuLittlePlayerBackingBuf + 4\n");

/* ---- Edit-teams kit-editor sprite-copy blocks -----------------------
 * Six MENU_CMD_SPRITE_COPY blocks that preview the primary/secondary
 * shirt/shorts/socks in the kit editor. Each is a 36-byte struct:
 *
 *   +0..+1   src sprite index   (editTeamsXxx[0])
 *   +2..+3   dst sprite index   (editTeamsXxx[1])
 *   +4..+35  16 × uint16 palette table (editTeamsXxxColTable)
 *
 * On Amiga the 12 labels (`editTeamsXxx` + `editTeamsXxxColTable`)
 * sat directly adjacent in BSS, so SPRITE_COPY read both through a
 * single `editTeamsXxx` pointer. On SDL they were in separate TUs so
 * the colour table read from whichever unrelated global the linker
 * placed next — the editor's sprite preview drew with a random colour
 * remap. Same pattern + fix as `stadiumMenuLittlePlayerBackingBuf`.
 *
 * Sprite indices from Sprites enum (SWOS2.h:2472+):
 *   SPR_BIG_SHIRT=190, SPR_BIG_SHORTS=191, SPR_BIG_SOCKS=192,
 *   SPR_EDIT_TEAMS_PRIMARY_SHIRT=193, SPR_EDIT_TEAMS_SHORTS=194,
 *   SPR_EDIT_TEAMS_SOCKS=195, SPR_EDIT_TEAMS_SECONDARY_SHIRT=196,
 *   SPR_EDIT_TEAMS_SHORTS_SECONDARY=197,
 *   SPR_EDIT_TEAMS_SECONDARY_SOCKS=198.
 *
 * Colour tables are stored as native-LE uint16 (Data.c declared them
 * uint8[32] with byte-pairs {0,N} that on Amiga BE read back as int16
 * = N — the int8[2N]-vs-int16[N] endian trap; storing the final int16
 * values directly so both BE and LE see the same palette remap). */

#define EDIT_TEAMS_BUF(SRC, DST, ...) {                       \
    (uint8_t)((SRC) & 0xFF), (uint8_t)((SRC) >> 8),           \
    (uint8_t)((DST) & 0xFF), (uint8_t)((DST) >> 8),           \
    __VA_ARGS__                                               \
}

/* Shared colour-table LE byte patterns. Shirt tables have index 1 in
 * slot 1, zeros in slots 2, 10, 11, 14 (special-case palette holes
 * for the kit sprite). Shorts/Socks tables are identity 0..15 with
 * slot 11 punched out. These match Data.c:editTeams*ColTable. */
#define COL_SHIRT \
    0,0, 1,0, 0,0, 3,0, 4,0, 5,0, 6,0, 7,0, \
    8,0, 9,0, 0,0, 0,0, 12,0, 13,0, 0,0, 15,0
#define COL_SHORTS \
    0,0, 1,0, 2,0, 3,0, 4,0, 5,0, 6,0, 7,0, \
    8,0, 9,0, 10,0, 0,0, 12,0, 13,0, 14,0, 15,0

__attribute__((used, aligned(2)))
uint8_t editTeamsPrimaryShirtsSpriteCopyBuf[36]   = EDIT_TEAMS_BUF(190, 193, COL_SHIRT);

__attribute__((used, aligned(2)))
uint8_t editTeamsShortsSpriteCopyBuf[36]          = EDIT_TEAMS_BUF(191, 194, COL_SHORTS);

__attribute__((used, aligned(2)))
uint8_t editTeamsSocksSpriteCopyBuf[36]           = EDIT_TEAMS_BUF(192, 195, COL_SHORTS);

__attribute__((used, aligned(2)))
uint8_t editTeamsSecondaryShirtSpriteCopyBuf[36]  = EDIT_TEAMS_BUF(190, 196, COL_SHIRT);

__attribute__((used, aligned(2)))
uint8_t editTeamsSecondaryShortsSpriteCopyBuf[36] = EDIT_TEAMS_BUF(191, 197, COL_SHORTS);

__attribute__((used, aligned(2)))
uint8_t editTeamsSecondarySocksSpriteCopyBuf[36]  = EDIT_TEAMS_BUF(192, 198, COL_SHORTS);

#undef EDIT_TEAMS_BUF
#undef COL_SHIRT
#undef COL_SHORTS

__asm__(".globl editTeamsPrimaryShirts\n"
        ".equ   editTeamsPrimaryShirts, editTeamsPrimaryShirtsSpriteCopyBuf\n"
        ".globl editTeamsPrimaryShirtColTable\n"
        ".equ   editTeamsPrimaryShirtColTable, editTeamsPrimaryShirtsSpriteCopyBuf + 4\n"
        ".globl editTeamsShorts\n"
        ".equ   editTeamsShorts, editTeamsShortsSpriteCopyBuf\n"
        ".globl editTeamsShortsColTable\n"
        ".equ   editTeamsShortsColTable, editTeamsShortsSpriteCopyBuf + 4\n"
        ".globl editTeamsSocks\n"
        ".equ   editTeamsSocks, editTeamsSocksSpriteCopyBuf\n"
        ".globl editTeamsSocksColTable\n"
        ".equ   editTeamsSocksColTable, editTeamsSocksSpriteCopyBuf + 4\n"
        ".globl editTeamsSecondaryShirt\n"
        ".equ   editTeamsSecondaryShirt, editTeamsSecondaryShirtSpriteCopyBuf\n"
        ".globl editTeamsSecondaryShirtColTable\n"
        ".equ   editTeamsSecondaryShirtColTable, editTeamsSecondaryShirtSpriteCopyBuf + 4\n"
        ".globl editTeamsSecondaryShorts\n"
        ".equ   editTeamsSecondaryShorts, editTeamsSecondaryShortsSpriteCopyBuf\n"
        ".globl editTeamsSecondaryShortsColTable\n"
        ".equ   editTeamsSecondaryShortsColTable, editTeamsSecondaryShortsSpriteCopyBuf + 4\n"
        ".globl editTeamsSecondarySocks\n"
        ".equ   editTeamsSecondarySocks, editTeamsSecondarySocksSpriteCopyBuf\n"
        ".globl editTeamsSecondarySocksColTable\n"
        ".equ   editTeamsSecondarySocksColTable, editTeamsSecondarySocksSpriteCopyBuf + 4\n");

/* ---- selTeamsPtr buffer + adjacent aliases ---------------------------
 * SWOS2.s reserves a single ~45 KB contiguous BSS region starting at
 * `selTeamsPtr` (line 128686), with seven adjacent labels that the game
 * treats as offsets into the same buffer:
 *
 *   +0       selTeamsPtr               (ds.b 4 — nominally a TeamFile*,
 *                                        but asm code writes 684-byte
 *                                        TeamFileEx records starting
 *                                        here, so the "pointer" label
 *                                        is actually the buffer base)
 *   +4       selTeamIsComputerFlag     (1 byte — CPU-team flag)
 *   +5       selTeamNameBase           (1 byte — first char of team name)
 *   +6       (19 bytes unnamed)        — rest of team name / gap
 *   +25      selTeamImportField        (1 byte — offset 0x19 in team entry)
 *   +26      (4 bytes unnamed)
 *   +30      selTeamSocksColor         (1 byte — socks colour in team entry)
 *   +31      (29 bytes unnamed)
 *   +60      selTeamPlayerIndices      (0x51C = 1308 bytes — squad order)
 *   +1368    editTeamsBuffer           (0xAB02 = 43778 bytes — edit-teams
 *                                        workspace: holds teamFileBuffer
 *                                        copy for import, plus competition
 *                                        slots addressed as selTeamsPtr-
 *                                        relative in career_teams.c:1660)
 *   +45146   (end — editTeamsSaveVarsArea is a separate standalone buffer)
 *
 * C code declares `extern uint8_t selTeamsPtr;` and uses
 * `&selTeamsPtr + N*684` for byte arithmetic into the buffer, plus
 * `(uint8_t*)&selTeamsPtr + offset` for dynamic offsets (see e.g.
 * career_teams.c:1660 which stores selTeamsPtr-relative offsets in
 * competition slot fields). Previously selTeamsPtr was defined as
 * `TeamFile *selTeamsPtr` (a 4-byte pointer), so `_AddTeamToBuffer`
 * writing 684 bytes to `&selTeamsPtr` stomped whatever global the
 * linker had placed next — corrupting state and hanging the menu
 * after the first team was selected in "CHOOSE TEAMS FOR FRIENDLY".
 *
 * Same pattern + fix as `currentMenu`/`menuEntries`, `stadiumMenuLittlePlayerBackingBuf`,
 * and the edit-teams sprite-copy blocks above. */
__attribute__((used, aligned(4)))
uint8_t selTeamsBackingBuf[45146];
__asm__(".globl selTeamsPtr\n"
        ".equ   selTeamsPtr, selTeamsBackingBuf\n"
        ".globl selTeamIsComputerFlag\n"
        ".equ   selTeamIsComputerFlag, selTeamsBackingBuf + 4\n"
        ".globl selTeamNameBase\n"
        ".equ   selTeamNameBase, selTeamsBackingBuf + 5\n"
        ".globl selTeamImportField\n"
        ".equ   selTeamImportField, selTeamsBackingBuf + 25\n"
        ".globl selTeamSocksColor\n"
        ".equ   selTeamSocksColor, selTeamsBackingBuf + 30\n"
        ".globl selTeamPlayerIndices\n"
        ".equ   selTeamPlayerIndices, selTeamsBackingBuf + 60\n"
        ".globl editTeamsBuffer\n"
        ".equ   editTeamsBuffer, selTeamsBackingBuf + 1368\n");

/* ---- Highlight scene ring buffer (hilStartOfGoalsBuffer + inGameHilBuffer)
 *
 * On Amiga these are two adjacent BSS labels (SWOS2.s line 2779..2781):
 *   hilStartOfGoalsBuffer: $4A38 bytes  (19000 — one "goal" scene slot)
 *   inGameHilBuffer:       $29BF8 bytes (170488 — 8.97 additional slots)
 *
 * The highlight system addresses both as one combined pool:
 *   _SaveHighlightScene writes 19000-byte scenes at
 *     `hilStartOfGoalsBuffer + N * 19000` for N in 0..HIL_MAX_SCENES-1, and
 *   game_loop.c:1003 sets goalBasePtr to the same offset for replay.
 *
 * On SDL each label was a separate stub, so writing scene 1+ past the
 * 19000-byte hilStartOfGoalsBuffer stomped whatever the linker placed
 * next (goalBase, editTeamsSaveVarsArea, sprite-pool overflow). Merge
 * them into one contiguous buffer with asm aliases, matching the Amiga
 * BSS layout — same pattern as `selTeamsBackingBuf` above. */
#define HIL_BACKING_BUF_SIZE (0x4A38 + 0x29BF8)
__attribute__((used, aligned(4)))
uint8_t hilBackingBuf[HIL_BACKING_BUF_SIZE];
__asm__(".globl hilStartOfGoalsBuffer\n"
        ".equ   hilStartOfGoalsBuffer, hilBackingBuf\n"
        ".globl inGameHilBuffer\n"
        ".equ   inGameHilBuffer, hilBackingBuf + 0x4A38\n");
#undef HIL_BACKING_BUF_SIZE

/* ---- Audio buffers: sized to hold the real Amiga sound files.
 *
 * On the Amiga, these symbols were just labels into a big contiguous BSS
 * region; the songs/banks overran into adjacent BSS which happened to be
 * unused. On SDL each symbol must be its own correctly-sized buffer so
 * the loads don't stomp unrelated globals (we hit this when loading
 * menu.ins zeroed out menuImageStreamBaseEnd). Sizes include headroom.
 *
 *   sound/menu.sng = 1471 bytes
 *   sound/menu.ins = 43826 bytes
 *   sound/sfx.sng  = 4281 bytes
 *   sound/sfx.in2  = 81068 bytes
 *
 * Note: sound/sfx.in1 (71500 bytes) loads into poolplyrDatBuffer
 * (52200 bytes) — which overruns by ~19 KB. Fix when sfx path lights up. */
uint8_t menuSongData[8  * 1024];
uint8_t menuInsBank [64 * 1024];
uint8_t sfxSongData [8  * 1024];
uint8_t sfxInsBank2 [128 * 1024];

/* ---- Menu font tables (small + big) --------------------------------
 * Each CharTable is a 238-byte struct: header (unk1, charHeight, unk2,
 * charSpacing, spaceWidth, spriteIndexOffset) + 224-byte ASCII→sprite
 * conversion table. _DrawMenuTextCentered reads charHeight/spaceWidth
 * and indexes into spriteGraphicsPtr via `conversionTable[c] +
 * spriteIndexOffset`. Values transcribed verbatim from the Amiga C
 * port's Data.c (smallCharsTable / bigCharsTable). The 2-entry
 * `charTables` pointer table below is what menu_system.c actually
 * reads to pick the small/big font. */
typedef struct CharTable {
    uint16_t unk1;
    uint16_t charHeight;
    uint32_t unk2;
    uint16_t charSpacing;
    uint16_t spaceWidth;
    uint16_t spriteIndexOffset;
    uint8_t  conversionTable[224];
} CharTable;

/* Per the Amiga SWOS2.s smallCharsTable / bigCharsTable: only these
 * indices are non-zero, all others 0:
 *   5=51, 7-9=1-3, 10=45, 11=50, 12-15=4-7, 16-25=8-17, 26=49, 27=52,
 *   31=48, 33-47=18-32, 48-58=33-43, 96=47 ('£' = 0x9C), 110=53,
 *   121=55, 122=54, 124=46, 127=56, 223=44 (cursor)
 * The table is 224 bytes (16 cols × 14 rows). */
CharTable smallCharsTable = {
    6, 6, 327685, 0, 3, 0,
    { 0, 0, 0, 0, 0, 51, 0, 1, 2, 3, 45, 50, 4, 5, 6, 7,             /*  0- 15 */
      8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 49, 52, 0, 0, 0, 48,     /* 16- 31 */
      0, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, /* 32- 47 */
      33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 0, 0, 0, 0, 0,     /* 48- 63 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /* 64- 79 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /* 80- 95 */
      47, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 53, 0,              /* 96-111 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 55, 54, 0, 46, 0, 0, 56,            /*112-127 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /*128-143 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /*144-159 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /*160-175 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /*176-191 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /*192-207 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 44 }              /*208-223 */
};

CharTable bigCharsTable = {
    8, 8, 458759, 0, 4, 57,
    { 0, 0, 0, 0, 0, 51, 0, 1, 2, 3, 45, 50, 4, 5, 6, 7,             /*  0- 15 */
      8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 49, 52, 0, 0, 0, 48,     /* 16- 31 */
      0, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, /* 32- 47 */
      33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 0, 0, 0, 0, 0,     /* 48- 63 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /* 64- 79 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /* 80- 95 */
      47, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 53, 0,              /* 96-111 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 55, 54, 0, 46, 0, 0, 56,            /*112-127 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /*128-143 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /*144-159 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /*160-175 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /*176-191 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                /*192-207 */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 44 }              /*208-223 */
};

/* 2-entry pointer table read by _DrawMenuItem etc:
 *   charTables[0] = &smallCharsTable
 *   charTables[1] = &bigCharsTable
 * Index is derived from the menu entry's stringFlags (see menu_system.c). */
uint32_t charTables[2] = {
    (uint32_t)(uintptr_t)&smallCharsTable,
    (uint32_t)(uintptr_t)&bigCharsTable
};

/* ---- outerFrameColorsTable: 16 background-colour pairs (top-left,
 * bottom-right) used by `_DrawMenuItemOuterFrame` to give each menu
 * button a beveled edge. Transcribed from Amiga Data.c. */
uint8_t outerFrameColorsTable[32] = {
    1, 9, 1, 9, 1, 9, 1, 9, 5, 9, 6, 4, 2, 4, 1, 9,
    1, 9, 1, 3, 6, 9, 13, 8, 10, 8, 1, 9, 1, 9, 1, 9
};

/* ---- kColorTableShine: 8-entry palette cycle used by
 * `_DrawSelectedMenuItemFrame` to draw the animated shine on the
 * currently-selected menu entry. Indexed by `(menuCursorFrame & 14) >> 1`.
 * Transcribed from Amiga Data.c. */
uint16_t kColorTableShine[8] = { 2, 2, 1, 1, 7, 7, 1, 1 };

/* ---- Friendly-game STRING_TABLE entries -----------------------------
 * The friendly menu's pitch-type/season cycler stores its choices in a
 * compact table format read by _DrawMenuItem (ENTRY_STRING_TABLE branch):
 *   +0: pointer to the index variable (uint16 read from this address)
 *   +4: uint16 base value subtracted from the index
 *   +6: array of N string pointers, indexed by (idx - base)
 *
 * Original Amiga had these in SWOS2.s. The stub generator replaced them
 * with 4 KiB zero-filled buffers, which segfaults at the very first deref
 * (`*(uint16_t *)indexPtr` with indexPtr = 0). Transcribed verbatim from
 * SWOS2.s lines 77666–77696. */
extern char pitchTypeOrSeason[];
extern char friendlyPitchType[];
extern char friendlySeason[];
extern const char aPitchType[], aSeason_1[];
extern const char aRandom[], aFrozen[], aMuddy[], aWet[],
                  aSoft[], aNormal[], aDry[], aHard[];
extern const char aJan[], aFeb[], aMar[], aApr[], aMay[], aJun[],
                  aJul[], aAug[], aSep[], aOct[], aNov[], aDec[];

struct __attribute__((packed)) {
    void    *indexPtr;
    uint16_t base;
    void    *strs[2];
} pitchTypeOrSeasonTable = {
    pitchTypeOrSeason, 0, { (void *)aPitchType, (void *)aSeason_1 }
};

struct __attribute__((packed)) {
    void    *indexPtr;
    uint16_t base;
    void    *strs[8];
} friendlyPitchTypeTable = {
    friendlyPitchType, (uint16_t)-1,
    { (void *)aRandom, (void *)aFrozen, (void *)aMuddy, (void *)aWet,
      (void *)aSoft,   (void *)aNormal, (void *)aDry,   (void *)aHard }
};

struct __attribute__((packed)) {
    void    *indexPtr;
    uint16_t base;
    void    *strs[12];
} friendly_season_table = {
    friendlySeason, 0,
    { (void *)aJan, (void *)aFeb, (void *)aMar, (void *)aApr,
      (void *)aMay, (void *)aJun, (void *)aJul, (void *)aAug,
      (void *)aSep, (void *)aOct, (void *)aNov, (void *)aDec }
};

/* ---- bureaucracyMenuItems ----
 * Redraw list passed to _DrawMenuItemsWaitForVSync from _Bureaucracy
 * (called after each team selection). The function reads int16 entry
 * ordinals terminated by a NEGATIVE value. Without the real terminator
 * the 4 KiB zero stub iterates 2048 times redrawing entry 0 (the menu
 * header), producing a multi-second flicker before returning, and the
 * real targets — 58 (numTeamsPrompt), 57, 55 — never get refreshed,
 * so "SELECT N MORE TEAMS" never updates after a selection.
 * Transcribed from SWOS2.s line 65223. */
int16_t bureaucracyMenuItems[5] = { 58, 57, 55, 0, -1 };

/* ---- Other _DrawMenuItemsWaitForVSync redraw lists.
 * Same pattern as bureaucracyMenuItems: int16 ordinals terminated by a
 * negative value, called from various menu callbacks. Stubs of zeros
 * cause the draw loop to walk 2048 entries redrawing entry 0.
 * Transcribed from SWOS2.s. */
int16_t editTacticsRotateRedrawList[3]      = { 10, 27, -1 };
int16_t filterMenuRedrawList[3]             = {  4,  6, -1 };
int16_t playMatchRedrawList[3]              = { 12, 29, -1 };
int16_t togglePlayerCoachRedrawItemList[4]  = {  9,  1,  6, -1 };
/* This one is dc.l in SWOS2.s — pairs of int16 packed as little-endian
 * within each long word: $004D004C,$004AFFFF reads (BE 68k load) as
 * 0x004D, 0x004C, 0x004A, 0xFFFF i.e. ordinals 77, 76, 74, terminator. */
int16_t chooseTeamsFriendly_changeTeamsPromptDrawList[4] = { 77, 76, 74, -1 };

/* ---- savedRandState / savedRandStatePtr ----
 * SWOS2.s line 21645 defines a uint8[15] save-stack buffer and a
 * pointer into it that's pre-initialised to the buffer's address.
 * _SaveRand2State / _RestoreRand2State push/pop 3 bytes at a time.
 * Without this definition the linker resolves savedRandState to 0,
 * the pointer ends up NULL, and `*ptr++ = seed2` in _SaveRand2State
 * segfaults on the first match-start push. */
uint8_t savedRandState[15];
uint8_t *savedRandStatePtr = savedRandState;

/* Camera position as FixedPoint unions (16.16 fixed-point) — matches the
 * Amiga m68k layout where `cameraX` was a 4-byte dc.l accessed via both
 * int16 (integer half) and int32 (full FP) views of the same storage.
 * FixedPoint.s.i is the integer part, FixedPoint.s.frac the fractional,
 * and FixedPoint.raw the full 32-bit value. The union overlay in
 * game_types.h handles the LE/BE byte-order swap so each half lives at
 * the expected memory offset on both hosts. */
FixedPoint cameraX = {0};
FixedPoint cameraY = {0};

/* ---- kGoalieSkill[0..7]Table + kGoalieSkillTables ----
 * 8 GoalieSkill lookup tables (30 int16 fields each). Indexed at
 * runtime by the goalkeeper's skill rating via kGoalieSkillTables[].
 * Without these _UpdatePlayers dereferences a NULL shotChanceTable.
 * Transcribed from SWOS2.s lines 28755–29017. */
int16_t kGoalieSkill7Table[30] = { 0x0063, 0x0580, 0x00E2, 0x0400, 0x0100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0002, 0x0003, 0x0009, 0x0006, 0x0001, 0x000C, 0x0004, 0x0009, 0x0006, 0x0001, 0x000C };
int16_t kGoalieSkill6Table[30] = { 0x0001, 0x0580, 0x00E2, 0x0400, 0x0100, 0x0000, 0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0002, 0x0003, 0x0004, 0x0008, 0x0006, 0x0002, 0x000B, 0x0005, 0x0008, 0x0006, 0x0002, 0x000B };
int16_t kGoalieSkill5Table[30] = { 0x0002, 0x04DC, 0x00B4, 0x03E0, 0x00F0, 0x0000, 0x0001, 0x0001, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0003, 0x0004, 0x0005, 0x0007, 0x0006, 0x0003, 0x000A, 0x0006, 0x0007, 0x0006, 0x0003, 0x000A };
int16_t kGoalieSkill4Table[30] = { 0x0003, 0x0438, 0x0086, 0x03C0, 0x00E0, 0x0000, 0x0001, 0x0002, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0004, 0x0005, 0x0006, 0x0006, 0x0006, 0x0004, 0x0009, 0x0007, 0x0006, 0x0006, 0x0004, 0x0009 };
int16_t kGoalieSkill3Table[30] = { 0x0004, 0x0394, 0x0058, 0x03A0, 0x00D0, 0x0001, 0x0002, 0x0003, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0005, 0x0006, 0x0007, 0x0006, 0x0005, 0x0005, 0x0008, 0x0008, 0x0005, 0x0006, 0x0005, 0x0008 };
int16_t kGoalieSkill2Table[30] = { 0x0005, 0x02F0, 0x002A, 0x0380, 0x00C0, 0x0002, 0x0003, 0x0004, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0006, 0x0007, 0x0007, 0x0005, 0x0005, 0x0006, 0x0007, 0x0009, 0x0004, 0x0006, 0x0006, 0x0007 };
int16_t kGoalieSkill1Table[30] = { 0x0006, 0x024C, -4,     0x0360, 0x00B0, 0x0003, 0x0004, 0x0005, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0007, 0x0007, 0x0007, 0x0004, 0x0005, 0x0007, 0x0006, 0x000A, 0x0003, 0x0006, 0x0007, 0x0006 };
int16_t kGoalieSkill0Table[30] = { 0x0007, 0x01A8, -50,    0x0340, 0x00A0, 0x0004, 0x0005, 0x0006, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0003, 0x0005, 0x0008, 0x0005, 0x000B, 0x0002, 0x0006, 0x0008, 0x0005 };
/* ---- pitchDatBuffer + pitchChunkyBuffer ----
 * Amiga pitch files pack 2310 tile-pointer offsets followed by
 * bitplane tile data. The Amiga build's original 9240-byte buffer
 * relied on BSS adjacency to currentMenu for the tile overflow;
 * on SDL we size the buffer to hold the largest decompressed pitch
 * file (~48 KB; 64 KB gives headroom). After load, we decode each
 * 128-byte 4-plane tile into a 256-byte 8bpp chunky tile in
 * pitchChunkyBuffer (indexed by pointer slot) and rewrite the
 * pointer table entries to point to the chunky data. Renderers
 * then read 16×16 chunky bytes per tile via the same pointer
 * table the Amiga code already uses. */
#define PITCH_POINTER_COUNT 2310
#define PITCH_CHUNKY_TILE_BYTES 256   /* 16 px * 16 rows * 1 byte/px */
uint8_t pitchDatBuffer[64 * 1024];
uint8_t pitchChunkyBuffer[PITCH_POINTER_COUNT * PITCH_CHUNKY_TILE_BYTES];

uint32_t kGoalieSkillTables[8] = {
    (uint32_t)(uintptr_t)kGoalieSkill0Table,
    (uint32_t)(uintptr_t)kGoalieSkill1Table,
    (uint32_t)(uintptr_t)kGoalieSkill2Table,
    (uint32_t)(uintptr_t)kGoalieSkill3Table,
    (uint32_t)(uintptr_t)kGoalieSkill4Table,
    (uint32_t)(uintptr_t)kGoalieSkill5Table,
    (uint32_t)(uintptr_t)kGoalieSkill6Table,
    (uint32_t)(uintptr_t)kGoalieSkill7Table,
};

/* ---- diyFileBufferCopy / diyCompetitionStart / competitionFileBuffer ----
 * `_SaveDIYTournament` does a word-copy of 1449 entries (2898 bytes) from
 * `competitionFileBuffer` into `diyFileBufferCopy`. The auto-extractor
 * sized them as a 2-byte stub and a 1508-byte DIYCompetitionEx, so the
 * copy overflowed and stomped `magicValue` (which sits ~0xC08 past
 * diyFileBufferCopy on our linker layout — on Amiga BSS it was a real
 * adjacent global, here it's just whatever .o the linker placed next).
 *
 * Use 4096-byte backing buffers so word-copies up to ~4 KB are safe, and
 * expose the named symbols via .equ so existing
 *   `extern DIYCompetitionEx diyFileBufferCopy;` / `.base.field`
 * accesses keep working unchanged. */
__attribute__((used, aligned(4))) uint8_t diyFileBufferCopyBuf[4096];
__asm__(".globl diyFileBufferCopy\n"
        ".equ   diyFileBufferCopy, diyFileBufferCopyBuf\n");

__attribute__((used, aligned(4))) uint8_t diyCompetitionStartBuf[4096];
__asm__(".globl diyCompetitionStart\n"
        ".equ   diyCompetitionStart, diyCompetitionStartBuf\n");

__attribute__((used, aligned(4))) uint8_t competitionFileBufferBuf[4096];
__asm__(".globl competitionFileBuffer\n"
        ".equ   competitionFileBuffer, competitionFileBufferBuf\n");

/* diyLeagueTeamIds: on Amiga, the label sits at
 * `diyCompetitionStart + 0x12E` — the same memory the
 * `(uint16_t *)(diyCompetitionStart + 0x12E)` reads in
 * _DIY_CacheSelectedTeamPointers / DIY_RecomputeCompetitionDerivedFields
 * resolve to. SDL must mirror this overlap so writes via the global
 * and reads via the offset hit the same bytes. */
__asm__(".globl diyLeagueTeamIds\n"
        ".equ   diyLeagueTeamIds, diyCompetitionStartBuf + 0x12E\n");

/* diyCupTeamIds: same Amiga BSS overlap pattern. The cup-init paths
 * write team IDs via the named global (e.g. _AppendTeamsToSelection
 * with diyCupTeamIds as the destination), while
 * _DIY_BuildTeamSelectionPointerCache reads them as
 * `(uint16_t *)(diyCompetitionStart + 0x24E)`. Alias to the same
 * bytes so populate-then-cache works. */
__asm__(".globl diyCupTeamIds\n"
        ".equ   diyCupTeamIds, diyCompetitionStartBuf + 0x24E\n");

/* diyTeamEntryOffsetsByIndex (offset 0x6E) and diyTeamShuffleIndices
 * (offset 0xEE) overlap diyCompetitionStart per the Amiga BSS layout.
 * Without this alias, _FindTeamInDIY reads zeros at `comp + 0x6E` and
 * fails to locate the player's team. */
__asm__(".globl diyTeamEntryOffsetsByIndex\n"
        ".equ   diyTeamEntryOffsetsByIndex, diyCompetitionStartBuf + 0x6E\n"
        ".globl diyTeamShuffleIndices\n"
        ".equ   diyTeamShuffleIndices, diyCompetitionStartBuf + 0xEE\n");

/* diyPresetNameOfCompetition: 36-byte buffer holding the primary
 * competition name. On Amiga BSS this label sits at
 * `competitionFileBuffer + 4` (after competitionFileBuffer's 2-byte
 * type stub and competitionOver's 2-byte stub). It overlaps the
 * `competitionName` field at offset 4 of the DIYCompetition struct,
 * so when slot 0's state is stored into competitionFileBuffer the
 * name automatically ends up here. SDL must mirror this overlap or
 * the View Competitions menu shows a blank primary-comp button. */
__asm__(".globl diyPresetNameOfCompetition\n"
        ".equ   diyPresetNameOfCompetition, competitionFileBufferBuf + 4\n");

/* careerFileBuffer + saveFile* header globals.
 * On Amiga the careerFileBuffer label is a 2-byte stub at the head
 * of a ~98 KB BSS run that the save file content overlays. The
 * save-header globals (saveFileVersion, three 4-byte magic words,
 * saveFileAmigaType) sit at fixed byte offsets WITHIN that run —
 * computed by walking the ds.b allocations in SWOS2.s:
 *   saveFileVersion    : careerFileBuffer + 98458
 *   saveFileMagic1..3  : +98460, +98464, +98468 (4 bytes each)
 *   saveFileAmigaType  : +98472
 *
 * `_SaveCompetitionFile` writes via the named globals; `_LoadFileEx`
 * loads the file into careerFileBuffer and `_LoadCompetitionFile`
 * reads the header back via the globals. Both paths require the
 * globals to live INSIDE careerFileBuffer or the magic check fails.
 * Without this overlap the SDL build also overflows: a 100 KB save
 * loaded into a 2-byte stub stomps anything in the linker's path
 * (most visibly magicValue, which makes _MenuProc panic as soon as
 * the error menu tries to draw on top of the corrupted state).
 *
 * 256 KB covers a full career (header 98476 + up to 64 × 684 + 2). */
__attribute__((used, aligned(4))) uint8_t careerFileBufferBuf[256 * 1024];
__asm__(".globl careerFileBuffer\n"
        ".equ   careerFileBuffer, careerFileBufferBuf\n"
        ".globl saveFileVersion\n"
        ".equ   saveFileVersion,   careerFileBufferBuf + 98458\n"
        ".globl saveFileMagic1\n"
        ".equ   saveFileMagic1,    careerFileBufferBuf + 98460\n"
        ".globl saveFileMagic2\n"
        ".equ   saveFileMagic2,    careerFileBufferBuf + 98464\n"
        ".globl saveFileMagic3\n"
        ".equ   saveFileMagic3,    careerFileBufferBuf + 98468\n"
        ".globl saveFileAmigaType\n"
        ".equ   saveFileAmigaType, careerFileBufferBuf + 98472\n"
        ".globl numSelectedTeams\n"
        ".equ   numSelectedTeams,  careerFileBufferBuf + 98476\n");

/* numberOfFiles + foundFilenamesBuffer: contiguous BSS run on Amiga.
 * AssetSys_ResolveAssetList receives &numberOfFiles as the destination
 * pointer (passed via _CAssetSys_Dispatch from _FindFilesExtensionPresent),
 * writes the file count at offset 0..1, and packs 32-byte entries
 * starting at offset 0 (so entry 0's name field at offset 4 lands at
 * &numberOfFiles + 4 = foundFilenamesBuffer + 0, entry 1 name at
 * foundFilenamesBuffer + 32, etc.). _GetFilenameAndExtension then
 * iterates `foundFilenamesBuffer` with a 32-byte stride and scans
 * each slot for the '.' that starts the file extension. The Amiga
 * sized this run as numberOfFiles(2) + 2 pad + foundFilenamesBuffer($77E)
 * = 0x782 bytes total. */
__attribute__((used, aligned(4))) uint8_t numberOfFilesBuf[0x782];
__asm__(".globl numberOfFiles\n"
        ".equ   numberOfFiles, numberOfFilesBuf\n"
        ".globl foundFilenamesBuffer\n"
        ".equ   foundFilenamesBuffer, numberOfFilesBuf + 4\n");

/* careerTeam: 1292 bytes (TeamFile 76 + 32 × PlayerFile 38) — see
 * comment in swos_data_typed_stubs.c. The C TeamFileEx struct only
 * declares 16 players, but `teamInit(&careerTeam, 31)` reaches index
 * 31. .equ exposes the symbol; field accesses still go through the
 * struct, the extended player slots are accessed via pointer
 * arithmetic that the compiler doesn't bounds-check. */
__attribute__((used, aligned(4))) uint8_t careerTeamBuf[0x50C];
__asm__(".globl careerTeam\n"
        ".equ   careerTeam, careerTeamBuf\n");

/* Slot 1/2/3/4 competition context buffers. On Amiga each label is a
 * tiny stub at the head of a multi-KB BSS run, and
 * `_Career_StoreCompetitionSlotStateToBuffer` writes up to 2900 bytes
 * (type=2+) into the slot state. SDL must give each its own
 * properly-sized backing buffer or the writes overflow into adjacent
 * globals (last seen: diyCompetitionContext stomping careerNumPlayers). */
__attribute__((used, aligned(4))) uint8_t competitionContextBuf[4096];
__asm__(".globl competitionContext\n"
        ".equ   competitionContext, competitionContextBuf\n");

__attribute__((used, aligned(4))) uint8_t tertiaryCompetitionContextBuf[4096];
__asm__(".globl tertiaryCompetitionContext\n"
        ".equ   tertiaryCompetitionContext, tertiaryCompetitionContextBuf\n");

__attribute__((used, aligned(4))) uint8_t euroCupsCompetitionContextBuf[4096];
__asm__(".globl euroCupsCompetitionContext\n"
        ".equ   euroCupsCompetitionContext, euroCupsCompetitionContextBuf\n");

__attribute__((used, aligned(4))) uint8_t diyCompetitionContextBuf[4096];
__asm__(".globl diyCompetitionContext\n"
        ".equ   diyCompetitionContext, diyCompetitionContextBuf\n");

/* ---- teamSelectionKeyCache + teamSelectionKeyCacheEnd ----
 * Cache of competition slot keys built by
 * `_TeamSelectionKeyCache_BuildFromCompetitionBlock`. On Amiga the
 * symbol `teamSelectionKeyCache` is a 1-byte ds.b stub at the start of
 * a ~16 KB BSS run, and `teamSelectionKeyCacheEnd` sits just past the
 * far end. The builder fills variable-length entries forward from
 * teamSelectionKeyCache and InsertSpaceSorted bounds-checks against
 * teamSelectionKeyCacheEnd. A typical league has ~34 × 22-118-byte
 * entries — i.e. several hundred bytes to multiple KB.
 *
 * On SDL each global is its own .o symbol, so the original 1-byte stub
 * caused every cache write to overflow into adjacent globals (most
 * visibly stomping `teamSelectionKeyCacheCount` to 0). Use one 64 KB
 * backing buffer and expose both names via `.equ` aliases. */
__attribute__((used, aligned(4))) uint8_t teamSelectionKeyCacheBuf[64 * 1024];
__asm__(".globl teamSelectionKeyCache\n"
        ".equ   teamSelectionKeyCache, teamSelectionKeyCacheBuf\n"
        ".globl teamSelectionKeyCacheEnd\n"
        ".equ   teamSelectionKeyCacheEnd, teamSelectionKeyCacheBuf + (64 * 1024) - 2\n");
