/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef DISPLAY_H
#define DISPLAY_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef short int16_t;
typedef int int32_t;

/* ==================================================================
 * AGA mode switch
 *
 * Uncomment to enable AGA (8 bitplanes, 256 colors).
 * When commented out, the build uses OCS (4 bitplanes, 16 colors).
 * ================================================================== */
//#define AGA

/* ---- Graphics folder path ---- */
#ifdef AGA
#define GRAFS "grafs_aga/"
#else
#define GRAFS "grafs/"
#endif

/* ---- Hardware register addresses (for code that can't use struct Custom)
 * On the SDL host, swos_platform.h pre-defines CUSTOM_BASE to resolve
 * through the `custom` stub pointer so register pokes land in valid
 * memory instead of crashing on 0x00DFF000. */
#ifndef CUSTOM_BASE
#define CUSTOM_BASE         0x00DFF000u
#endif
#ifndef DMACONR_ADDR
#define DMACONR_ADDR        (CUSTOM_BASE + 0x002u)
#define VPOSR_ADDR          (CUSTOM_BASE + 0x004u)
#define COP1LC_ADDR         (CUSTOM_BASE + 0x080u)
#define COPJMP1_ADDR        (CUSTOM_BASE + 0x088u)
#define DMACON_ADDR         (CUSTOM_BASE + 0x096u)
#define INTENA_ADDR         (CUSTOM_BASE + 0x09Au)
#define INTREQ_ADDR         (CUSTOM_BASE + 0x09Cu)
#define COLOR00_ADDR        (CUSTOM_BASE + 0x180u)
#endif

/* ==================================================================
 * Amiga Custom Chip Register Map (struct for register-relative access)
 *
 * Using volatile struct Custom * gives the compiler struct layout info,
 * enabling efficient offset(An) addressing instead of absolute long.
 * ================================================================== */
struct Custom {
    uint16_t bltddat;       /* 0x000 */
    uint16_t dmaconr;       /* 0x002 */
    uint16_t vposr;         /* 0x004 */
    uint16_t vhposr;        /* 0x006 */
    uint16_t pad_008[4];    /* 0x008-0x00E */
    uint16_t adkconr;       /* 0x010 */
    uint16_t pad_012[5];    /* 0x012-0x01A */
    uint16_t intenar;       /* 0x01C */
    uint16_t intreqr;       /* 0x01E */
    uint16_t pad_020[16];   /* 0x020-0x03E */
    uint16_t bltcon0;       /* 0x040 */
    uint16_t bltcon1;       /* 0x042 */
    uint16_t bltafwm;       /* 0x044 */
    uint16_t bltalwm;       /* 0x046 */
    uint32_t bltcpt;        /* 0x048 */
    uint32_t bltbpt;        /* 0x04C */
    uint32_t bltapt;        /* 0x050 */
    uint32_t bltdpt;        /* 0x054 */
    uint16_t bltsize;       /* 0x058 */
    uint16_t pad_05A[3];    /* 0x05A-0x05E */
    uint16_t bltcmod;       /* 0x060 */
    uint16_t bltbmod;       /* 0x062 */
    uint16_t bltamod;       /* 0x064 */
    uint16_t bltdmod;       /* 0x066 */
    uint16_t pad_068[4];    /* 0x068-0x06E */
    uint16_t bltcdat;       /* 0x070 */
    uint16_t bltbdat;       /* 0x072 */
    uint16_t bltadat;       /* 0x074 */
    uint16_t pad_076[4];    /* 0x076-0x07C */
    uint16_t pad_07E;       /* 0x07E */
    uint32_t cop1lc;        /* 0x080 */
    uint32_t cop2lc;        /* 0x084 */
    uint16_t copjmp1;       /* 0x088 */
    uint16_t copjmp2;       /* 0x08A */
    uint16_t pad_08C[5];    /* 0x08C-0x094 */
    uint16_t dmacon;        /* 0x096 */
    uint16_t pad_098;       /* 0x098 */
    uint16_t intena;        /* 0x09A */
    uint16_t intreq;        /* 0x09C */
    uint16_t adkcon;        /* 0x09E */
    /* Audio channels 0xA0-0xDF, then 0xE0-0xFF display regs */
    uint16_t pad_0A0[112];  /* 0x0A0-0x17E */
    uint16_t color[32];     /* 0x180-0x1BE */
};

/* Global custom chip base pointer.
 * Must be volatile so reads/writes to HW registers aren't optimized away.
 * NOT packed — GCC generates byte-by-byte access for packed volatile structs.
 * The struct is naturally word-aligned (all Amiga custom regs are). */
extern volatile struct Custom *custom;

/* ---- Blitter register offsets from CUSTOM_BASE (kept for asm compat) ---- */
#define BLTCON0_OFF         0x40
#define BLTCON1_OFF         0x42
#define BLTAFWM_OFF         0x44
#define BLTALWM_OFF         0x46
#define BLTCPT_OFF          0x48
#define BLTBPT_OFF          0x4C
#define BLTAPT_OFF          0x50
#define BLTDPT_OFF          0x54
#define BLTSIZE_OFF         0x58
#define BLTCMOD_OFF         0x60
#define BLTBMOD_OFF         0x62
#define BLTAMOD_OFF         0x64
#define BLTDMOD_OFF         0x66
#define BLTCDAT_OFF         0x70
#define BLTBDAT_OFF         0x72
#define BLTADAT_OFF         0x74
#define DMACONR_OFF         0x02
#define DMACON_OFF          0x96

/* ---- DMA / Blitter control ---- */
#define DMACON_BLT_ON               33792u
#define DMACON_BLT_OFF              1024u
#define DMACONR_BLTBUSY_BIT         6
#define BLT_FWM_ALL                 0xFFFFu
#define WORD_ALIGN_EVEN_MASK        0xFFFEu

/* ---- Blitter function code (minterm) constants ---- */
#define BLTCON0_USEA_LF_FC          0x08FC
#define BLTCON0_USED_LF_00          0x0100
#define BLTCON0_USEA_USED_LF_F0     0x09F0
#define BLTCON0_USEA_USED_LF_FC     0x09FC
#define BLTCON0_USED_LF_FF          0x01FF
#define BLTCON0_USED_LF_F0          496u
#define BLTCON0_USEA_USEB_USED_LF_FC 0x0DFC
#define BLTCON0_BASE_LINKED          0x0FCA  /* D = A ? B : C (cookie cut) */
#define BLTCON0_BASE_SINGLE          0x07CA  /* D = B (straight copy, USEA=0) */
#define BLTCON0_CLEAR_MASK           0x0B0A
#define BLTCON0_OR_MASK              0x0BFA

/* ---- Blitter queue sentinel ---- */
#define BLT_BLITQ_END               0xFFFFFFFFu

/* ==================================================================
 * Bitplane layout constants
 *
 * These are the core display parameters. NUM_PLANES is set by
 * the AGA define above — all derived values update automatically.
 * ================================================================== */

/* ---- Bitplane layout (base values) ---- */
#ifdef AGA
#define NUM_PLANES                  8
#define NUM_PLANES_SHIFT            3
#define NUM_PLANES_SHIFT_PLUS1      4
#else
#define NUM_PLANES                  4
#define NUM_PLANES_SHIFT            2
#define NUM_PLANES_SHIFT_PLUS1      3
#endif

#define NUM_PLANES_MINUS1           (NUM_PLANES - 1)
#define NUM_PLANES_MINUS2           (NUM_PLANES - 2)
#define PLANE_STRIDE                44
#define PLANE_STRIDE_MINUS2         (PLANE_STRIDE - 2)      /* 42 */
#define PLANE_STRIDE_MINUS4         (PLANE_STRIDE - 4)      /* 40 */
#define PLANE_STRIDE_DIV2           (PLANE_STRIDE >> 1)     /* 22 */
#define ROW_STRIDE_BYTES            (PLANE_STRIDE * NUM_PLANES)
#define SRC_SCANLINE_BYTES          (NUM_PLANES * 2)

/* ---- Bitplane layout (multiples) ---- */
#define PLANE_STRIDE_TIMES2         (PLANE_STRIDE * 2)      /* 88 */
#define PLANE_STRIDE_TIMES3         (PLANE_STRIDE * 3)      /* 132 */
#define PLANE_STRIDE_TIMES4         (PLANE_STRIDE * 4)      /* 176 */
#define PLANE_STRIDE_TIMES5         (PLANE_STRIDE * 5)      /* 220 */
#define PLANE_STRIDE_TIMES6         (PLANE_STRIDE * 6)      /* 264 */
#define PLANE_STRIDE_TIMES7         (PLANE_STRIDE * 7)      /* 308 */

/* ---- Image stream layout ---- */
#define IMAGE_ROW_STRIDE_BYTES      (BITPLANE_ROW_BYTES * NUM_PLANES)
#define BITPLANE_ROW_BYTES          40

/* ---- Screen / display dimensions ---- */
#define SCREEN_WIDTH                336
#define SCREEN_HEIGHT               272

/* ---- Copper list layout ----
 *
 * OCS: 320 bytes (16 colors x 2 palette blocks)
 * AGA: 1316 bytes (256 colors via 8 banks x 32 colors)
 *
 * The register control section (sprite ptrs, BPLCON, modulos, DDFSTRT,
 * DIWSTRT) is at the same offsets in both templates. Only the palette
 * section and BPL pointer blocks differ.
 */
#ifdef AGA
#define COPPERLIST_SIZE_BYTES       1316
#define NUM_PAL_BANKS               8
#define COLORS_PER_BANK             32
#else
#define COPPERLIST_SIZE_BYTES       320
#define NUM_PAL_BANKS               2
#define COLORS_PER_BANK             16
#endif

/* Copper byte offsets — shared between OCS and AGA (same position) */
#define COP_SPR_B0_OFF              0x04
#define COP_BPLCON1_VAL_OFF         0x4A
#define COP_BPL1MOD_VAL_OFF         0x5A
#define COP_BPL2MOD_VAL_OFF         0x5E
#define COP_DDFSTRT_VAL_OFF         0x62

/* Copper byte offsets — differ between OCS and AGA */
#ifdef AGA
/* AGA: 8 palette banks, each preceded by BPLCON3 bank-select word pair */
#define COP_PAL_B0_C0_OFF          0x74
#define COP_PAL_B0_C0_VAL_OFF      0x76
#define COP_PAL_B1_C0_OFF          0xF8
#define COP_PAL_B1_C0_VAL_OFF      0xFA
#define COP_PAL_B2_C0_OFF          0x17C
#define COP_PAL_B2_C0_VAL_OFF      0x17E
#define COP_PAL_B3_C0_OFF          0x200
#define COP_PAL_B3_C0_VAL_OFF      0x202
#define COP_PAL_B4_C0_OFF          0x284
#define COP_PAL_B4_C0_VAL_OFF      0x286
#define COP_PAL_B5_C0_OFF          0x308
#define COP_PAL_B5_C0_VAL_OFF      0x30A
#define COP_PAL_B6_C0_OFF          0x38C
#define COP_PAL_B6_C0_VAL_OFF      0x38E
#define COP_PAL_B7_C0_OFF          0x410
#define COP_PAL_B7_C0_VAL_OFF      0x412
#define COP_BPLPTR_BLOCKA_OFF      0x498
#define COP_BPLPTR_BLOCKB_OFF      0x4E0
#define COP_WAIT_VHPOS             0x4D8
#define COP_WAIT_WAIT              0x4DC
#else
/* OCS: 2 palette blocks of 16 colors, no bank-select pairs */
#define COP_PAL_B0_C0_VAL_OFF      0x72
#define COP_PAL_B1_C0_VAL_OFF      0xB2
#define COP_BPLPTR_BLOCKA_OFF      0xF4
#define COP_BPLPTR_BLOCKB_OFF      0x11C
#define COP_WAIT_VHPOS             0x114
#define COP_WAIT_WAIT              0x118
#endif

/* ---- Frontbuffer layout ----
 * Amiga: bitplane-interleaved, NUM_PLANES planes of PLANE_STRIDE bytes per
 * scanline, 176 bytes/row for OCS.
 * SDL: 8bpp chunky, 1 byte per pixel = palette index (0..15 for OCS-style
 * palette). Row stride = SCREEN_WIDTH. Sprites store the same format
 * (byte = palette index, 0 = transparent). */
#ifdef SWOS_SDL_BUILD
/* Widescreen: the frontbuffer STRIDE (= visible width) is RUNTIME — g_screen_width is
 * 336 (normal, == SCREEN_WIDTH) or WIDESCREEN_WIDTH (widescreen), set once at startup
 * from gWidescreen. The buffers are ALLOCATED at the max stride so their array size
 * stays a link-time constant (FRONTBUFFER_ALLOC_TOTAL_BYTES); only the row-stride
 * arithmetic is runtime. SCREEN_WIDTH stays the 336 logical/centered width (menus). */
extern int g_screen_width;
/* g_screen_width (the live render width) is derived at startup from the monitor's
 * aspect ratio (main.c), so the picture fills any display with no bars. The buffer is
 * ALLOCATED at the MAX useful width = the full pitch world width (the view never
 * usefully exceeds the pitch; the camera clamp keys off the same 0x2A0). */
#define WIDESCREEN_WIDTH                480   /* 16:9 reference (1.76); the width is now adaptive */
#define PITCH_VIEW_MAX_WIDTH            672   /* = 0x2A0, pitch world width = max render width */
#define FRONTBUFFER_MAX_STRIDE_BYTES    PITCH_VIEW_MAX_WIDTH
#define FRONTBUFFER_ROW_STRIDE_BYTES    g_screen_width
#else
/* Amiga has no widescreen: g_screen_width folds to the fixed visible width at
 * compile time, so the synced per-site culls/camera math are identical to before. */
#define g_screen_width                  SCREEN_WIDTH
#define FRONTBUFFER_MAX_STRIDE_BYTES    ROW_STRIDE_BYTES
#define FRONTBUFFER_ROW_STRIDE_BYTES    ROW_STRIDE_BYTES
#endif
#define FRONTBUFFER_VISIBLE_LINES       SCREEN_HEIGHT
#define FRONTBUFFER_TOTAL_LINES         (SCREEN_HEIGHT + 1)
#define FRONTBUFFER_SIZE_VISIBLE_BYTES  (FRONTBUFFER_ROW_STRIDE_BYTES * FRONTBUFFER_VISIBLE_LINES)
#define FRONTBUFFER_SIZE_TOTAL_BYTES    (FRONTBUFFER_ROW_STRIDE_BYTES * FRONTBUFFER_TOTAL_LINES)
/* Compile-time max allocation (max stride) for the frontbuffer array declarations. */
#define FRONTBUFFER_ALLOC_TOTAL_BYTES   (FRONTBUFFER_MAX_STRIDE_BYTES * FRONTBUFFER_TOTAL_LINES)
#define FB_COPY_WORDS_MINUS1            ((FRONTBUFFER_SIZE_TOTAL_BYTES >> 1) - 1)
#define FB_BLT_MOD_AFTER_1_WORD         (FRONTBUFFER_ROW_STRIDE_BYTES - 2)

/* Widescreen menu centering: 336-wide menu content is drawn centered in the wider
 * frame by shifting the frontbuffer write base by WIDE_MARGIN. g_menu_draw_x_offset is
 * set to WIDE_MARGIN only around the menu entry-draw loop (so the full-width panel
 * background and the in-match pitch stay at offset 0). 0 / WIDE_MARGIN=0 on Amiga. */
#define WIDE_MARGIN                     ((g_screen_width - SCREEN_WIDTH) / 2)
extern int g_menu_draw_x_offset;

/* ---- Tile / scroll-blit constants ---- */
#define TILE_STRIDE                 14
#define TILE_SHIFT_X                4
#define TILE_HEIGHT                 16
#define TILE_SIZE                   (TILE_HEIGHT * SRC_SCANLINE_BYTES)
#define TILE_SIZE_SHIFT             (NUM_PLANES_SHIFT_PLUS1 + TILE_SHIFT_X)  /* log2(TILE_SIZE): 7 for OCS, 8 for AGA */
#define MAX_FAN_TILE_INDEX          22
#define MAX_FAN_TILE_OFFSET         (MAX_FAN_TILE_INDEX * TILE_SIZE)
#define TILE_MASK                   15
#define PITCH_PTR_ROW_BYTES         (PLANE_STRIDE_MINUS2 * 4)
#define PITCH_DEST_MOD_UNALIGNED    (ROW_STRIDE_BYTES - PLANE_STRIDE)
#define PITCH_DEST_MOD_ALIGNED      (ROW_STRIDE_BYTES - (PLANE_STRIDE - 2))
#define BLTDMOD_PLANE_STEP          PLANE_STRIDE_MINUS2
#define BLTSIZE_TILE_COL            (((TILE_HEIGHT * NUM_PLANES) << 6) | 1)
#define BLTSIZE_COL_16LINES_ALLPLANES  BLTSIZE_TILE_COL

/* ---- Panel overlay ---- */
#define PANEL_HEIGHT_LINES          30
#define BLTSIZE_OVERLAY_PANEL       (((PANEL_HEIGHT_LINES * NUM_PLANES) << 6) | PLANE_STRIDE_DIV2)
#define BLTSIZE_OVERLAY_TO_SCREEN   ((SCREEN_HEIGHT << 6) | PLANE_STRIDE_DIV2)

/* ---- Sprite blit ---- */
#define SPRITE_HEIGHT_LINES         15
#define SPRITE_BLIT_HEIGHT_LINES    (SPRITE_HEIGHT_LINES * NUM_PLANES)
#define SPRITE_BLTSIZE_W1           ((SPRITE_BLIT_HEIGHT_LINES << 6) | 1)
#define SPRITE_BLTSIZE_W2           ((SPRITE_BLIT_HEIGHT_LINES << 6) | 2)

/* ---- File info ---- */
#define PITCHFILE_MAX_SIZE          FRONTBUFFER_SIZE_TOTAL_BYTES

/* ---- Copper list byte-offset helper ---- */
/* NOTE: Do NOT use COPPER_W for writes — GCC decomposes volatile uint16_t
 * stores into byte-level move.b pairs. Use copper_write_word() instead. */
#define COPPER_W(offset) (*(volatile uint16_t *)((uint8_t *)copperListBuffer + (offset)))

/* ---- Function declarations ---- */
void InitDisplayAndCopper(void);
void UpdatePlanePtrsAndBlitParams(void);
void SwapBuffersAndCameraViewState(void);
void SetCameraCoordinates(int16_t targetX, int16_t targetY);
void DoubleBufferSwapSequence(void);
void BlitterCopyPanelToScreen2(void);
void ProcessSpriteBlitQueue(void);
void BlitterCopyPanelToScreen(void);
void HWSprite_LoadFromSpriteGraphics(uint16_t spriteIndex);
void FlashColor0Delay(uint32_t loopCount, uint16_t color);
void WriteCopperPalette(const uint16_t *palette);
#ifdef AGA
void _WriteCopperPitchPalette(void);
#endif

#endif
