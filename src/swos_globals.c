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

#include <string.h>  /* memcpy for image_script_emit_ptr */

#include "swos_platform.h"
#include "swos_sections.h"   /* SWOS_CHIP_BSS — chip-RAM placement for DMA */
#include "display.h"
#include "game_types.h"
#include "menu_system.h"     /* MENU_HEADER_SIZE — used by menuEntries alias */

#include "swos_types.h"

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

/* "Front" set (visible this frame) + "alt" set (drawn into).
 * On Amiga: must live in CHIP RAM because the copper/bitplane DMA can
 * only fetch from chip memory. SWOS_CHIP_BSS routes these into the
 * .BSSInChip.MEMF_CHIP section the linker maps to chip RAM. On SDL the
 * attribute is a no-op (regular BSS). */
SWOS_CHIP_BSS uint8_t bitPlane0Base[BITPLANE_BYTES];
SWOS_CHIP_BSS uint8_t bitPlane1Base[BITPLANE_BYTES];
SWOS_CHIP_BSS uint8_t bitPlane2Base[BITPLANE_BYTES];
SWOS_CHIP_BSS uint8_t bitPlane3Base[BITPLANE_BYTES];
#ifdef AGA
SWOS_CHIP_BSS uint8_t bitPlane4Base[BITPLANE_BYTES];
SWOS_CHIP_BSS uint8_t bitPlane5Base[BITPLANE_BYTES];
SWOS_CHIP_BSS uint8_t bitPlane6Base[BITPLANE_BYTES];
SWOS_CHIP_BSS uint8_t bitPlane7Base[BITPLANE_BYTES];
#endif
SWOS_CHIP_BSS uint8_t bitPlane0BaseAlt[BITPLANE_BYTES];
SWOS_CHIP_BSS uint8_t bitPlane1BaseAlt[BITPLANE_BYTES];
SWOS_CHIP_BSS uint8_t bitPlane2BaseAlt[BITPLANE_BYTES];
SWOS_CHIP_BSS uint8_t bitPlane3BaseAlt[BITPLANE_BYTES];
#ifdef AGA
SWOS_CHIP_BSS uint8_t bitPlane4BaseAlt[BITPLANE_BYTES];
SWOS_CHIP_BSS uint8_t bitPlane5BaseAlt[BITPLANE_BYTES];
SWOS_CHIP_BSS uint8_t bitPlane6BaseAlt[BITPLANE_BYTES];
SWOS_CHIP_BSS uint8_t bitPlane7BaseAlt[BITPLANE_BYTES];
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
/* Frontbuffers also need chip RAM on Amiga — copper-list BPLPT entries
 * point at these for the main display area. */
/* Sized at the MAX (widescreen) stride so the array is big enough whatever the
 * runtime g_screen_width is; the live frame uses g_screen_width as its row stride. */
SWOS_CHIP_BSS uint8_t frontbufferBase[FRONTBUFFER_ALLOC_TOTAL_BYTES];
SWOS_CHIP_BSS uint8_t frontbufferBaseAlt[FRONTBUFFER_ALLOC_TOTAL_BYTES];
SWOS_CHIP_BSS uint8_t frontbufferMaskBuffer[FRONTBUFFER_ALLOC_TOTAL_BYTES];
#ifdef SWOS_SDL_BUILD
/* Live frontbuffer stride / visible width: SCREEN_WIDTH (336) or WIDESCREEN_WIDTH
 * (384), set once at startup from gWidescreen (main.c). 336 = unchanged behaviour. */
int g_screen_width = SCREEN_WIDTH;
#endif
/* These store live pointers to frontbuffer memory. On 32-bit they
 * are word-sized for layout compatibility with the Amiga port (where
 * the asm reads them as long words). On 64-bit they must be wide
 * enough to hold a real heap/BSS pointer — uintptr_t keeps both
 * targets working without retyping every consumer. */
volatile uintptr_t frontbufferBasePtr;
volatile uintptr_t frontbufferBaseAltPtr;

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

/* Copper list (OCS 320 bytes / AGA 1316 bytes). Must be in chip RAM
 * because the copper hardware fetches from it via DMA. */
SWOS_CHIP_BSS uint8_t  copperListBuffer[COPPERLIST_SIZE_BYTES];
SWOS_CHIP_BSS uint32_t copperZeroLong;

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

uint16_t introMenuImageScript[] = {
    0x7000, 0, 0,            /* filename ptr halves filled at init */
#include "intro_menu_image_script_cmds.inc"
#include "font_ext_script.inc"   /* accented / full-ASCII / $ glyph extracts */
    0xFFFF
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
extern uintptr_t team1Filename;
extern uintptr_t team2Filename;

#include "swos_types.h"

/* image_cmd.c interprets 0x7000/0x8000 file-load opcodes by reading a
 * pointer from the script stream at the current cursor — but the
 * stream stores pointers as two uint16 halves (32 bits total). On
 * 32-bit hosts that works directly; on 64-bit it truncates the high
 * 32 bits of the address and the load handler hands garbage to RNC,
 * which overruns and trips a cygwin/libc assert in memcpy.
 *
 * Fix: keep the on-stream layout (two uint16 halves) the same on
 * both targets, but on 64-bit treat the stored value as an INDEX
 * into the side table below rather than as a raw pointer. Init code
 * here populates the side table and writes the index back into the
 * stream slot; image_cmd.c reads the slot, looks up the table on
 * 64-bit, dereferences directly on 32-bit. Bitness-conditional via
 * UINTPTR_MAX so the 32-bit code path is unchanged. */
const void *imageScriptPtrTable[32];

static uint16_t s_imageScriptPtrCount = 0;
static uint32_t image_script_register_ptr(const void *p)
{
    /* 32-bit: encode pointer directly (low/high halves). */
    /* 64-bit: register in side table, return the slot index. */
#if UINTPTR_MAX == 0xFFFFFFFFu
    return (uint32_t)(uintptr_t)p;
#else
    if (s_imageScriptPtrCount >= sizeof(imageScriptPtrTable) /
                                 sizeof(imageScriptPtrTable[0])) {
        /* Out of slots — bump the table size in the header. */
        return 0xFFFFFFFFu;
    }
    imageScriptPtrTable[s_imageScriptPtrCount] = p;
    return (uint32_t)s_imageScriptPtrCount++;
#endif
}

static void image_script_emit_ptr(uint16_t *stream, uint32_t off, const void *p)
{
    uint32_t enc = image_script_register_ptr(p);
    /* The stream is read back as a native 32-bit pointer at byte offset
     * &stream[off] via `*(uint8_t * const *)a0`. The bytes must therefore
     * end up in host byte order. Splitting `enc` into "low half" then
     * "high half" stored as uint16_t works on LE (the lower uint16
     * naturally lands at the lower address = low byte of the 32-bit
     * load) but produces SWAPPED HALVES on BE (m68k Amiga): the BE
     * 32-bit load reads the low-half uint16 as the *high* word of the
     * pointer. Use memcpy with a host-native uint32 source so the bytes
     * land in correct host order regardless of endianness. */
    memcpy(&stream[off], &enc, sizeof(enc));
}

void _SwosSDL_InitImageScripts(void)
{
    /* Reset the side-table cursor on each init call (only matters on
     * 64-bit; the 32-bit path doesn't touch the table). */
    s_imageScriptPtrCount = 0;

    image_script_emit_ptr(introMenuImageScript,    1, aGrafsCharsetRa);
    image_script_emit_ptr(menuBGImageCmdStream,    1, aGrafsMenusRaw);
    image_script_emit_ptr(menuBGImageCmdStream,  393, aGrafsMenus2Raw);

    /* menuColorAndTextScript pointer slots. The .inc file lists the
     * word offsets as comments. */
    struct { uint32_t off; const void *val; } mcts[] = {
        {    1, aGrafsCjcgrafsR },
        {  406, &team1Filename  },
        { 1877, &team2Filename  },
        { 3419, aGrafsCjcteamgR },
        { 4548, aGrafsCjcbitsRa },
        { 4695, aGrafsSoccerSRa },
        { 4890, aGrafsCjcbenchR },
    };
    for (size_t i = 0; i < sizeof(mcts) / sizeof(mcts[0]); i++) {
        image_script_emit_ptr(menuColorAndTextScript, mcts[i].off, mcts[i].val);
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
extern uintptr_t team1Filename;
extern uintptr_t team2Filename;
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
/* The alias offset must match MENU_HEADER_SIZE — 22 on 32-bit, 38 on
 * 64-bit (Menu has 4 pointer fields). Stringify the literal so the
 * inline asm picks up the correct value for the build. */
#define _MENU_HDR_STR_HELPER(x) #x
#define _MENU_HDR_STR(x) _MENU_HDR_STR_HELPER(x)
/* Emit storage + labels via inline asm rather than a C array so the
 * menuEntries alias becomes a real BSS label (resolvable on Mach-O —
 * .equ alias = ext_carrier + offset is silently dropped there). */
/* Leave the assembler in .data (NOT .text) at the end of this block. GCC tracks
 * the "current section" lazily and emits a section directive only when it thinks
 * the section changed; the opaque file-scope asm switches the section behind its
 * back. A trailing `.text` desynced the two: GCC then emitted the next
 * initialized global (stadiumMenuLittlePlayerSpriteCopy, just below) believing it
 * was in .data and emitting no directive, but the assembler was still in .text —
 * so that global landed in read-only .text and any runtime write to it SIGSEGV'd
 * (the stadium pre-match little-player sprite-copy crash). Ending in `.data`
 * matches the initialized global that follows.
 *
 * NOTE: must use plain `.section`/`.data` here, NOT `.pushsection`/`.popsection`
 * or `.previous` — the Cygwin/Windows COFF/PE assembler backend doesn't implement
 * those section-stack pseudo-ops (it errors "unknown pseudo-op"). The zero-init
 * globals further down (offerOrdinals, goalieFaces) emit their own .bss/.comm
 * directive and resync regardless. */
__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 2\n"
    ".globl " _ASM_PFX "currentMenu\n"
    _ASM_PFX "currentMenu:\n"
    ".skip " _MENU_HDR_STR(MENU_HEADER_SIZE) "\n"
    ".globl " _ASM_PFX "menuEntries\n"
    _ASM_PFX "menuEntries:\n"
    ".skip " _MENU_HDR_STR((8 * 1024 + 80 * 1024) - MENU_HEADER_SIZE) "\n"
    ".data\n"
);

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
/* UNTANGLED (.equ -> struct): same 36-byte MENU_CMD_SPRITE_COPY descriptor as the
 * editTeams kit previews (EditTeamsSpriteCopy), replacing the uint16_t[18] backing
 * buffer + `.equ stadiumMenuLittlePlayer / littlePlayerSpriteGeneric /
 * littlePlayersColorTable` adjacency aliases. Already host-native (the old
 * byte-array init's LE bytes panic'd 0x982 on the BE Amiga — fixed earlier by the
 * uint16[] typing, preserved here). srcSprite (+0) is set at runtime; dstSprite
 * (+2) = 0xD6 (was littlePlayerSpriteGeneric); colTable identity 0..15. */
__attribute__((used, aligned(2)))
EditTeamsSpriteCopy stadiumMenuLittlePlayerSpriteCopy = {
    0x0000, 0x00D6, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
};

/* ---- Transfer-offer ordinals ---------------------------------------
 *
 * UNTANGLED (.equ -> struct): SWOS2.s 96267..96280 laid 7 `dc.w` globals
 * back-to-back (teamOffering, offerPrimaryPlayerOrdinal, numExtraPlayers-
 * Wanted, offerSwapPlayerOrdinal, offerExtraPlayerOrdinal, offerAmount,
 * offerNegotiationStep1Amount). The SDL port previously merged them into
 * a uint8_t[14] backing buffer with `.equ` aliases because
 * `offerAfterDrawCommonShared` walks the four player-ordinal slots as a
 * contiguous int16_t array from `&offerPrimaryPlayerOrdinal[0..3]` (which
 * read garbage when they were 7 separate linker-placed globals).
 *
 * Now a typed OfferOrdinals (game_types.h): host-native uint16/int16
 * (the original `dc.w`), byte-identical on SDL/LE and Amiga/BE. The four
 * ordinal slots stay contiguous so the walk still works — see the struct
 * comment. transfer_market.c is the sole consumer (transient menu state,
 * not serialized). */
__attribute__((used, aligned(2)))
OfferOrdinals offerOrdinals = {0};

/* ---- Goalkeeper face / skin state --------------------------------
 *
 * Untangled (roadmap step 3) from the goalieFacesBackingBuf 6-symbol
 * .equ adjacency overlay into a typed GoalieFaceState (game_types.h).
 * The two old "team2*"-labelled bytes (+7 reserve-of-team1, +9
 * reserve-of-team2) had no real code consumers — they were only ever
 * reached via team1Faces[1] / team2Faces[1] — so they fold into the
 * 2-byte face arrays and the misleading labels disappear. Layout is
 * pinned byte-identical to the old backing buffer by the offsetof
 * static asserts on the struct. */
__attribute__((used, aligned(2)))
GoalieFaceState goalieFaces = {0};

/* ---- Memory boundary marker -----------------------------------------
 * Amiga BSS boundary symbol. _ShowLoaderIntroSequence does a vestigial
 * `if ((uintptr_t)&memBoundaryMarker > 0x100000u) { }` layout sanity check
 * (empty body — a no-op on both ports). At -O2 the dead check is eliminated
 * and the symbol is never referenced, but at -O0 the `&memBoundaryMarker`
 * reference survives and the link fails undefined unless the symbol exists.
 * Define it so the build links at any optimization level. */
__attribute__((used))
uint32_t memBoundaryMarker;

/* ---- Sprite blit queues (each = sentinel + 3000-byte buffer) ----
 *
 * SWOS2.s lines 2790-2796:
 *   kSpriteBlitQueueSentinel_A:  ds.b $2      ; sentinel marker
 *   BSS_00_2CED4:                ds.b $BB6    ; 3000-byte queue buffer A
 *   kSpriteBlitQueueSentinel_B:  ds.b $2
 *   BSS_00_2DA8C:                ds.b $BB6    ; 3000-byte queue buffer B
 *
 * Shared between SDL and Amiga because `_DrawSprite_entry2` (in
 * sprites.c) is compiled for both targets and does a 4-byte
 * `*(uint32_t *)savedQueue = BLT_BLITQ_END;` against what was a
 * 2-byte uint16 — that overflow silently corrupted whatever
 * follows the sentinel symbol on SDL too. On Amiga the sprite-write
 * paths additionally fill the queue with 12-byte entries; without
 * the 3000-byte buffer those entries overflow into bootTemp0 /
 * bottomTeamInGame, _ProcessSpriteBlitQueue reads garbage as srcPtr
 * and faults. */
#define SPRITE_BLIT_QUEUE_BYTES   (2 + 0xBB6)
__attribute__((used, aligned(4)))
uint8_t spriteBlitQueueA_backing[SPRITE_BLIT_QUEUE_BYTES] = {0};
__attribute__((used, aligned(4)))
uint8_t spriteBlitQueueB_backing[SPRITE_BLIT_QUEUE_BYTES] = {0};
__asm__(".globl " _ASM_PFX "kSpriteBlitQueueSentinel_A\n"
        ".equ   " _ASM_PFX "kSpriteBlitQueueSentinel_A, " _ASM_PFX "spriteBlitQueueA_backing\n"
        ".globl " _ASM_PFX "kSpriteBlitQueueSentinel_B\n"
        ".equ   " _ASM_PFX "kSpriteBlitQueueSentinel_B, " _ASM_PFX "spriteBlitQueueB_backing\n");

/* trialistNameBufTerminator overflow check: handled directly in
 * transfer_market.c via `&trialistNameBuffer[22]`. The `.equ`
 * cross-translation-unit alias trick we use for stadiumMenuLittlePlayer
 * etc. doesn't work here because trialistNameBuffer lives in a
 * different .o (swos_data_bulk_imports.o) — m68k-amiga-elf's assembler
 * can't resolve `.equ X, Y+22` when Y is external. The fix is local
 * to the bounds-check site instead. */

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
 * UNTANGLED (.equ -> struct, 2026-06-11): each descriptor is now an
 * EditTeamsSpriteCopy (game_types.h) with HOST-NATIVE uint16 fields,
 * replacing the uint8_t[36] + EDIT_TEAMS_BUF LE-byte macro + the
 * `.equ editTeams* / editTeams*ColTable` aliases. The old macro wrote
 * explicit LE bytes (head `(SRC)&0xFF` first; colTable `N,0`), which the
 * MENU_CMD_SPRITE_COPY host-native `*(uint16_t*)` read byte-swaps on the BE
 * Amiga (vs SWOS2.s `dc.w` head + `dc.b` {0,N} BE-word colTable) — a latent
 * BE/Amiga bug. Native uint16 is correct on BOTH targets; this is what the
 * prior "store the final int16 values directly" note intended but the
 * LE-byte macro never delivered. Sprite ids: SPR_BIG_{SHIRT,SHORTS,SOCKS}=
 * 190/191/192, SPR_EDIT_TEAMS_*=193..198. Colour tables = identity 0..15 with
 * palette holes punched to 0 (shirt: slots 2,10,11,14; shorts/socks: 11). */

#define COL_SHIRT  { 0, 1, 0, 3, 4, 5, 6, 7, 8, 9, 0, 0, 12, 13, 0, 15 }
#define COL_SHORTS { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 12, 13, 14, 15 }

__attribute__((used, aligned(2))) EditTeamsSpriteCopy editTeamsPrimaryShirts   = { 190, 193, COL_SHIRT };
__attribute__((used, aligned(2))) EditTeamsSpriteCopy editTeamsShorts          = { 191, 194, COL_SHORTS };
__attribute__((used, aligned(2))) EditTeamsSpriteCopy editTeamsSocks           = { 192, 195, COL_SHORTS };
__attribute__((used, aligned(2))) EditTeamsSpriteCopy editTeamsSecondaryShirt  = { 190, 196, COL_SHIRT };
__attribute__((used, aligned(2))) EditTeamsSpriteCopy editTeamsSecondaryShorts = { 191, 197, COL_SHORTS };
__attribute__((used, aligned(2))) EditTeamsSpriteCopy editTeamsSecondarySocks  = { 192, 198, COL_SHORTS };

#undef COL_SHIRT
#undef COL_SHORTS

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
/* selTeamsPtr / selTeamIsComputerFlag / selTeamNameBase / selTeamImportField /
 * selTeamSocksColor / selTeamPlayerIndices / editTeamsBuffer:
 * moved into swos_globals_career_overlay.c at careerFileBufferBuf + 98478
 * onwards. On Amiga these labels sit immediately after numSelectedTeams in
 * the same BSS run, so writing &careerFileBuffer for the full save size
 * captures the team-data array automatically. The old standalone
 * selTeamsBackingBuf left bytes 98478..120365 of the save file as zeros. */

/* ---- Highlight file buffer + scene ring buffer (one BSS run)
 *
 * On Amiga these are 10 adjacent BSS labels (SWOS2.s line 3034..3056):
 *   hilFileBuffer:         $7FC bytes  (2044 — sig + 2 teams)
 *   hilGameName:           $64 bytes   (100)
 *   hilGameRound:          $64 bytes   (100)
 *   hilNumGoals:           $2 bytes
 *   hilTeam1Goals:         $2 bytes
 *   hilTeam2Goals:         $2 bytes
 *   hilPitchNumber:        $2 bytes
 *   hilMaxSubstitutes:     $2 bytes
 *   hilStartOfGoalsBuffer: $4A38 bytes (19000 — one "goal" scene slot)
 *   inGameHilBuffer:       $29BF8 bytes (171000 — 9 additional slots)
 *
 * Total = 2254 + 19000 + 171000 = 192254 bytes (= K_HIL_FILE_MAXSZ).
 *
 * `_LoadHilFile` calls `_LoadFileEx(hilFileBuffer, …)` which writes the
 * entire HIL file starting at `hilFileBuffer`. The first 8 bytes are the
 * magic-sig and max-size, the next 2036 are the two teams, the next 200
 * are gameName+gameRound, the next 10 bytes are the per-match header
 * (numGoals/team1Goals/team2Goals/pitchNumber/maxSubstitutes), and the
 * remaining bytes are the goal scenes. The Amiga build relies on these
 * being one contiguous BSS run — separate stubs leave hilNumGoals at
 * zero after load, disabling the VIEW HIGHLIGHTS entry.
 *
 * The highlight system also addresses hilStartOfGoalsBuffer/inGameHilBuffer
 * as one combined pool:
 *   _SaveHighlightScene writes 19000-byte scenes at
 *     `hilStartOfGoalsBuffer + N * 19000` for N in 0..HIL_MAX_SCENES-1, and
 *   game_loop.c:1003 sets goalBasePtr to the same offset for replay. */
#define HIL_BACKING_BUF_SIZE 192254
/* Emit storage + labels via inline asm so each alias is a real BSS
 * label (Mach-O can't resolve .equ alias = ext_BSS_carrier + offset).
 * Layout (matches the Amiga BSS run):
 *   +0     hilFileBuffer        (alias of hilBackingBuf)
 *   +2044  hilGameName
 *   +2144  hilGameRound
 *   +2244  hilNumGoals
 *   +2246  hilTeam1Goals
 *   +2248  hilTeam2Goals
 *   +2250  hilPitchNumber
 *   +2252  hilMaxSubstitutes
 *   +2254  hilStartOfGoalsBuffer
 *   +21254 inGameHilBuffer        (= 2254 + 0x4A38) */
__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#elif defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)
    ".section .bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 2\n"
    ".globl " _ASM_PFX "hilBackingBuf\n"
    _ASM_PFX "hilBackingBuf:\n"
    ".globl " _ASM_PFX "hilFileBuffer\n"
    _ASM_PFX "hilFileBuffer:\n"
    ".skip 2044\n"
    ".globl " _ASM_PFX "hilGameName\n"
    _ASM_PFX "hilGameName:\n"
    ".skip 100\n"
    ".globl " _ASM_PFX "hilGameRound\n"
    _ASM_PFX "hilGameRound:\n"
    ".skip 100\n"
    ".globl " _ASM_PFX "hilNumGoals\n"
    _ASM_PFX "hilNumGoals:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "hilTeam1Goals\n"
    _ASM_PFX "hilTeam1Goals:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "hilTeam2Goals\n"
    _ASM_PFX "hilTeam2Goals:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "hilPitchNumber\n"
    _ASM_PFX "hilPitchNumber:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "hilMaxSubstitutes\n"
    _ASM_PFX "hilMaxSubstitutes:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "hilStartOfGoalsBuffer\n"
    _ASM_PFX "hilStartOfGoalsBuffer:\n"
    ".skip 19000\n"               /* 0x4A38 */
    ".globl " _ASM_PFX "inGameHilBuffer\n"
    _ASM_PFX "inGameHilBuffer:\n"
    ".skip 171000\n"              /* 192254 - 21254 */
    ".text\n"
);
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
SWOS_CHIP_BSS uint8_t menuInsBank [64 * 1024];
uint8_t sfxSongData [8  * 1024];

/* ---- SFX instrument bank (IN1 + IN2 contiguous in CHIP RAM) ----
 *
 * The Amiga Paula audio DMA can only fetch sample data from CHIP RAM,
 * AND the RJP1 player treats sound/sfx.in1 + sound/sfx.in2 as a single
 * contiguous instrument blob — each instrument record carries a 32-bit
 * offset relative to the START of IN1, and offsets above IN1's size
 * are expected to land inside IN2. SWOS2.s satisfies both: it places
 * `poolplyrDatBuffer` (which holds IN1 once SFX is loaded) and
 * `sfxInsBank2` exactly 71500 bytes apart in `BSSInChip`.
 *
 * Our previous build had `poolplyrDatBuffer` in plain .bss (FAST RAM,
 * unreachable by Paula DMA) AND non-adjacent to `sfxInsBank2`, so SFX
 * either played silence or the wrong samples (the user described it as
 * "SFX not playing correctly").
 *
 * Single CHIP_BSS backing buffer with .equ aliases, sized to hold both
 * banks contiguously (71500 + 81068 = 152568 bytes today, round to
 * 192 KB for headroom). poolplyrDatBuffer also serves as the
 * destination for `data/poolplyr.dat` (52200 bytes) — that's smaller
 * than IN1 and lives at the same offset so it overlays cleanly when
 * SFX isn't loaded (the original SWOS overlay behaviour, tracked by
 * `audioBankLoaded`). */
#define SFX_BANK_BACKING_BYTES   (192 * 1024)
#define SFX_BANK_IN2_OFFSET      71500u
SWOS_CHIP_BSS uint8_t sfxInsBankBacking[SFX_BANK_BACKING_BYTES];
__asm__(".globl " _ASM_PFX "poolplyrDatBuffer\n"
        ".equ   " _ASM_PFX "poolplyrDatBuffer, " _ASM_PFX "sfxInsBankBacking\n"
        ".globl " _ASM_PFX "sfxInsBank2\n"
        ".equ   " _ASM_PFX "sfxInsBank2, "      _ASM_PFX "sfxInsBankBacking + 71500\n");

/* ---- Menu font tables (small + big) --------------------------------
 * Each CharTable is a 14-byte header (unk1, charHeight, unk2, charSpacing,
 * spaceWidth, spriteIndexOffset) + a 224-entry ASCII→sprite conversion table.
 * _DrawMenuTextCentered reads charHeight/spaceWidth and indexes into
 * spriteGraphicsPtr via `conversionTable[c] + spriteIndexOffset`. Values
 * transcribed verbatim from the Amiga C port's Data.c. conversionTable is
 * uint16 (was uint8) so glyph slots can exceed 255 — needed for the font
 * extension, whose glyphs live above the game sprites at slots 256+. This is
 * in-memory data (not a file format), so uint16 is native-endian on both the
 * 68k (BE) and x86 (LE) — no byteswap helpers needed; the renderers read it as
 * uint16 too. The 2-entry `charTables` pointer table picks the small/big font. */
typedef struct CharTable {
    uint16_t unk1;
    uint16_t charHeight;
    uint32_t unk2;
    uint16_t charSpacing;
    uint16_t spaceWidth;
    uint16_t spriteIndexOffset;
    uint16_t conversionTable[224];
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

/* Apply the conversionTable entries for the font-extension glyphs (accented
 * capitals, full ASCII punctuation, $), the lowercase a-z -> uppercase-glyph
 * mappings, and the Latin-1 remap of the existing Ä/Ö/Ü. Called once after the
 * charset image script runs so the new glyph sprites already exist. Generated
 * mappings live in font_ext_conv.inc (from SWOS_Font/integration.txt). */
void FontExt_ApplyConv(void)
{
#define SB(i, s, b) do { smallCharsTable.conversionTable[(i)] = (uint16_t)(s); \
                         bigCharsTable.conversionTable[(i)]   = (uint16_t)(b); } while (0)
#include "font_ext_conv.inc"
#undef SB
}

/* 2-entry pointer table read by _DrawMenuItem etc:
 *   charTables[0] = &smallCharsTable
 *   charTables[1] = &bigCharsTable
 * Index is derived from the menu entry's stringFlags (see menu_system.c). */
void *charTables[2] = {
    &smallCharsTable,
    &bigCharsTable
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
SWOS_CHIP_BSS uint8_t pitchDatBuffer[64 * 1024];
uint8_t pitchChunkyBuffer[PITCH_POINTER_COUNT * PITCH_CHUNKY_TILE_BYTES];

void *kGoalieSkillTables[8] = {
    kGoalieSkill0Table,
    kGoalieSkill1Table,
    kGoalieSkill2Table,
    kGoalieSkill3Table,
    kGoalieSkill4Table,
    kGoalieSkill5Table,
    kGoalieSkill6Table,
    kGoalieSkill7Table,
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
/* primaryQualifiedTeamGrids / tertiary / euroCupTitleHolderGrid /
 * cupWinnersCupTitleHolderGrid / uefaCupTitleHolderGrid sit
 * consecutively in Amiga BSS (SWOS2.s lines 2534-2555):
 *
 *   primaryQualifiedTeamGrids: ds.b $1AB8  (10 × 684 = 10 grid slots)
 *   BSS_00_695C:               ds.b $558   (2 unnamed grid slots)
 *   tertiaryQualifiedTeamGrid: ds.b $2AC   (1 grid slot)
 *   euroCupTitleHolderGrid:    ds.b $2AC
 *   cupWinnersCupTitleHolderGrid: ds.b $2AC
 *   uefaCupTitleHolderGrid:    ds.b $2AC
 *
 * qualifyTeamsForUefaCup walks `a5` forward through these slots and
 * relies on `a5 == euroCupTitleHolderGrid` as the wrap-to-primary[1]
 * boundary. On SDL the linker scattered the grids (primary in one
 * BSS region, the four title-holder/tertiary grids in another at
 * 0x2C0-byte stride), so the walker stepped past tertiary into
 * unrelated globals (diyCompetitionStartBuf etc.), read garbage as
 * team ids, and tripped _Career_AppendTeamToTmdTeamPool's 0x3443
 * panic. Pack the whole region into one buffer matching the Amiga
 * BSS run and expose each slot via inline-asm .equ alias. */
__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#elif defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)
    ".section .bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 2\n"
    ".globl " _ASM_PFX "qualifiedTeamGridsBlock\n"
    _ASM_PFX "qualifiedTeamGridsBlock:\n"
    ".globl " _ASM_PFX "primaryQualifiedTeamGrids\n"
    _ASM_PFX "primaryQualifiedTeamGrids:\n"
    ".skip 8208\n"
    ".globl " _ASM_PFX "tertiaryQualifiedTeamGrid\n"
    _ASM_PFX "tertiaryQualifiedTeamGrid:\n"
    ".skip 684\n"
    ".globl " _ASM_PFX "euroCupTitleHolderGrid\n"
    _ASM_PFX "euroCupTitleHolderGrid:\n"
    ".skip 684\n"
    ".globl " _ASM_PFX "cupWinnersCupTitleHolderGrid\n"
    _ASM_PFX "cupWinnersCupTitleHolderGrid:\n"
    ".skip 684\n"
    ".globl " _ASM_PFX "uefaCupTitleHolderGrid\n"
    _ASM_PFX "uefaCupTitleHolderGrid:\n"
    ".skip 684\n"
    ".text\n"
);


/* gameRound: on Amiga the 1-byte gameRound label is followed by an
 * unnamed 99-byte BSS region (BSS_00_CD54 in SWOS2.s) that the
 * round-name string buffer extends into. _StringCopy(roundStr, &gameRound)
 * in PlayDIYLeagueMatchAndCacheResult writes "FIRST ROUND" / "QUARTER FINAL"
 * etc., overrunning the 1-byte typed definition and stomping magicValue
 * (which sits adjacent on SDL). highlight.c also iterates 40 bytes
 * between gameRound and hilGameRound. Provide a 100-byte buffer to
 * match the Amiga round-string capacity. */
__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#elif defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)
    ".section .bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 2\n"
    ".globl " _ASM_PFX "gameRoundBuf\n"
    _ASM_PFX "gameRoundBuf:\n"
    ".globl " _ASM_PFX "gameRound\n"
    _ASM_PFX "gameRound:\n"
    ".skip 100\n"
    ".text\n"
);


/* diyFileBufferCopy + diyCompetitionStart share one BSS run on Amiga:
 *
 *   2830 diyFileBufferCopy:    ds.b $60     ; +0x000
 *   2833 BSS_00_2DB6A:         ds.b $3A     ; +0x060
 *   2836 diyTournamentTeamIds: ds.b $386    ; +0x09A
 *   2840 diyCompetitionStart:  ds.b $60     ; +0x420
 *   2842 diyMinSubstitutes ... diyTeamShuffleOrder ... audioInstrumentBank
 *        — the "extended" diyCompetitionStart region runs to +0x420 + 0x734
 *
 * The 1450-word (2900-byte = 0xB54) DIY_CopyDiyFileBufferCopyToCompetitionFileBuffer
 * copy, which the tournament-preset save flow uses, deliberately covers
 * BOTH buffers because they're contiguous: writes to diyCompetitionStart
 * + 0x2B4 (league standings) land at diyFileBufferCopy + 0x6D4, and that
 * offset is within the 0xB54-byte copy.
 *
 * If we keep diyFileBufferCopyBuf and diyCompetitionStartBuf as TWO
 * standalone arrays, the standings live in a region the Copy never
 * touches — the saved .PRE file ends up with zeros where the Group A
 * standings should be (Colombia 1W lost on save → loads as fresh
 * competition). Mirror the Amiga BSS adjacency by aliasing
 * diyCompetitionStartBuf to diyFileBufferCopyBuf + 0x420; one big
 * backing buffer holds both, the existing aliases (diyLeagueTeamIds at
 * +0x12E, diyCupTeamIds at +0x24E, etc.) all resolve through, and
 * Copy's reach naturally includes the standings region.
 *
 * Sizing: 0x420 (diyFileBufferCopy region) + 0x734 (diyCompetitionStart
 * extended region) = 0xB54 minimum. Round up to 0x2000 (8192) for
 * defensive headroom — the 1450-word copy reaches 0xB54 anyway. */
/* ---------------------------------------------------------------------
 * STEP-3 UNTANGLE: diyFileBufferCopy is now a typed struct
 * (DIYTournamentState, game_types.h) instead of the legacy raw-offset
 * overlay. The old layout placed the 64-byte team-index list at byte
 * 0x5A, deliberately overlapping the DIYCompetition header tail
 * (numRounds/subRound/playEach on Amiga). On 64-bit the namePtr widening
 * (4->8) grew DIYCompetition by +4 and slid the *used* header fields
 * competitionFinished (->0x5A) and currentRoundIndex (->0x5C) on top of
 * the list, so a routine header reset zeroed team entries [0..3] (panic
 * 0x8034, ROMANIA x5, inflated PL column). As a named struct field
 * (`teamIndexOrder`, placed by the compiler AFTER the header) the list
 * can no longer be reached by a header write on any target.
 *
 * The former companions are now struct fields, not separate symbols:
 *   diyTournamentTeamIds  -> diyFileBufferCopy.teamIds
 *   diyTournamentCompetition -> diyFileBufferCopy.header (title alias)
 *
 * diyCompetitionStart is its OWN standalone buffer now (the old
 * diyFileBufferCopyBuf + 0x420 adjacency is retired): the four
 * tournament <-> competitionFileBuffer copies field-map explicitly, and
 * _FindTeamInDIY reaches the current-stage record by symbol, so no
 * contiguity is required. Its league standings table still extends past
 * sizeof(DIYCompetitionEx) into the aliased sub-regions below
 * (clearLeagueTableStats writes to +0x734), so the backing buffer stays
 * oversized.
 * --------------------------------------------------------------------- */
DIYTournamentState diyFileBufferCopy;

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 2\n"
    ".globl " _ASM_PFX "diyCompetitionStartBuf\n"
    _ASM_PFX "diyCompetitionStartBuf:\n"
    ".skip 7136\n"
    ".text\n"
);

__asm__(".globl " _ASM_PFX "diyCompetitionStart\n"
        ".equ   " _ASM_PFX "diyCompetitionStart, " _ASM_PFX "diyCompetitionStartBuf\n");

/* competitionFileBuffer: overlays careerFileBufferBuf at offset 93308.
 * Aliased in swos_globals_career_overlay.c. The 4 KB standalone backing
 * buffer that used to live here meant DIY/preset writes never reached
 * the career save body — and `_LoadPresetFile` would load a 4 KB file
 * into a buffer that overflows past everything else. The overlay puts
 * it back inside the 256 KB careerFileBufferBuf at the right offset. */

/* The diy* overlay aliases — both the scalars (above) and the four ARRAY
 * aliases that used to live here — are ALL retired. Every consumer now uses
 * typed DIYCompetitionEx field access (game_types.h), which lands on the
 * same bytes natively on 32- and 64-bit, so the .equ aliases and the
 * DIY_POST_BASE_SHIFT namePtr-growth offset machinery they needed are gone:
 *   diyLeagueTeamIds           @0x12E -> diyCompetitionStart.matchData.league.leagueTeamIds
 *   diyTeamEntryOffsetsByIndex @0x6E  -> diyCompetitionStart.matchData.league.teamEntryOffsets
 *   diyTeamShuffleIndices      @0xEE  -> diyCompetitionStart.matchData.league.shuffleAndPad
 *   diyCupTeamIds              @0x24E -> diyCompetitionStart.cupTeamIds (top-level field) */

/* The diy* SCALAR overlay aliases at parent offsets 0x60..0x68 and
 * 0x1BE..0x1C4 are GONE — all converted to typed diyCompetitionStart
 * field access (DIYCompetitionEx, game_types.h):
 *   0x60  diyMinSubstitutes       -> .minSubstitutes
 *   0x62  diyPointsForAWin        -> .pointsForAWin
 *   0x64  diyLeagueMinSubstitutes -> .matchData.league.leagueMinSubstitutes
 *   0x66  diyLeagueMaxSubstitutes -> .matchData.league.leagueMaxSubstitutes
 *   0x68  diyHasGroups            -> .matchData.league.hasGroupsDiy
 *   0x1C0 diyLeagueCurrentSlot    -> .leagueCurrentSlot
 *   0x1C2 diyLeagueMatchProgress  -> .leagueMatchProgress
 *   0x1C4 diyLeagueTotalMatches   -> .leagueTotalMatches
 * (diyMatchesPlayedCount@0x1BE / diyCompetitionFormat@0x14A /
 * diyRoundFormat@0x14C were dead — dropped earlier.)
 *
 * Field access IS the same memory the writers already used (e.g.
 * comp->matchData.league.leagueMinSubstitutes), so the reader/writer now
 * share one lvalue — the auto-play-loop phase-boundary inconsistency the old
 * comment warned about is gone by construction. The diy* ARRAY aliases
 * (diyLeagueTeamIds / diyCupTeamIds / diyTeamEntryOffsetsByIndex /
 * diyTeamShuffleIndices) remain above, pending their own conversion. */

/* diyPresetNameOfCompetition: aliased in swos_globals_career_overlay.c
 * at careerFileBufferBuf + 93312 (== competitionFileBuffer + 4 in the
 * Amiga overlay). The standalone alias here was redundant with the
 * generated overlay block. */

/* careerFileBufferBuf + the entire ~98 KB careerFileBuffer overlay
 * (careerFileBuffer through numSelectedTeams, plus all 332 named
 * globals in between) is now defined in swos_globals_career_overlay.c.
 *
 * The buffer must live in the same TU as the .equ aliases or the
 * assembler emits no symbol for them: `.equ alias, undefined+offset`
 * stays a relocatable expression that never lands in the .o symbol
 * table (gcc/binutils — verified with `nm`). */

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
__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#elif defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)
    ".section .bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 2\n"
    ".globl " _ASM_PFX "numberOfFilesBuf\n"
    _ASM_PFX "numberOfFilesBuf:\n"
    ".globl " _ASM_PFX "numberOfFiles\n"
    _ASM_PFX "numberOfFiles:\n"
    ".skip 4\n"
    ".globl " _ASM_PFX "foundFilenamesBuffer\n"
    _ASM_PFX "foundFilenamesBuffer:\n"
    ".skip 1918\n"
    ".text\n"
);


/* careerTeam: aliased in swos_globals_career_overlay.c to
 * careerFileBufferBuf + 56190. On Amiga it lives inside the career
 * save run as a 1292-byte (TeamFile 76 + 32 × PlayerFile 38) region.
 * `teamInit(&careerTeam, 31)` reaches index 31 — the C TeamFileEx
 * struct only declares 16 players, but the extended player slots are
 * accessed via pointer arithmetic the compiler doesn't bounds-check. */

/* Slot 1/2/3/4 competition context buffers: aliased in
 * swos_globals_career_overlay.c into careerFileBufferBuf at
 *   competitionContext           +63340
 *   tertiaryCompetitionContext   +66240
 *   diyCompetitionContext        +67332
 *   euroCupsCompetitionContext   +68424
 * On Amiga each label sits inside the career save run; the contexts
 * span up to ~2900 bytes (type=2+), and the next named global doesn't
 * appear until ~2900 bytes later — so the 4 KB standalone buffers we
 * used before were just emulating the Amiga BSS spacing. The overlay
 * now puts them back inside careerFileBufferBuf, which both restores
 * save round-trips and matches the original adjacency. */

/* teamSelectionKeyCache cluster: EXTRACTED from the careerFileBuffer overlay
 * (Tier 2 untangle). On Amiga these labels sat at careerFileBufferBuf + 73292..
 * +93306 with a 20014-byte gap that the cache shared with
 * `careerForeignMarketPlayers` (offset 75162) — memory reuse that let a
 * career-mode cache overflow stomp player data. They now live in their own
 * files: the cache + cursors + end marker in career_key_cache.c (dedicated
 * 8192-byte buffer — tightened from the original 20014-byte InsertSpaceSorted
 * bound span to an honest bound now that nothing shares the slack, ~7x the
 * measured ~1188 B HWM), and the market pool in career_foreign_market.c
 * (MarketPlayer[432]) — no shared memory. (The careerFileBufferBuf overlay
 * these were lifted from was itself subsequently retired in Part B, so there
 * is no longer a .skip hole to keep.) careerJobOfferPhaseBias (was cache byte
 * 1) is now a standalone scalar.
 * See docs/KEYCACHE_CLUSTER_REWORK.md / project_keycache_cluster_design. */
