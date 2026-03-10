#ifndef DISPLAY_H
#define DISPLAY_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef short int16_t;
typedef long int32_t;

/* ---- Hardware register addresses ---- */
#define CUSTOM_BASE         0x00DFF000u
#define DMACONR_ADDR        0x00DFF002u
#define VPOSR_ADDR          0x00DFF004u
#define COP1LC_ADDR         0x00DFF080u
#define COPJMP1_ADDR        0x00DFF088u
#define DMACON_ADDR         0x00DFF096u
#define INTENA_ADDR         0x00DFF09Au
#define COLOR00_ADDR        0x00DFF180u

/* ---- Blitter register offsets from CUSTOM_BASE ---- */
#define BLTCON0_OFF         64
#define BLTCON1_OFF         66
#define BLTAFWM_OFF         68
#define BLTALWM_OFF         70
#define BLTCPT_OFF          72
#define BLTBPT_OFF          76
#define BLTAPT_OFF          80
#define BLTDPT_OFF          84
#define BLTSIZE_OFF         88
#define BLTCMOD_OFF         96
#define BLTBMOD_OFF         98
#define BLTAMOD_OFF         100
#define BLTDMOD_OFF         102
#define BLTBDAT_OFF         114
#define BLTADAT_OFF         116

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
 * These are the core display parameters. For AGA support, change
 * NUM_PLANES and PLANE_STRIDE here — all derived values update
 * automatically.
 * ================================================================== */

/* ---- Bitplane layout (base values) ---- */
#define NUM_PLANES                  4
#define NUM_PLANES_MINUS1           (NUM_PLANES - 1)        /* 3 */
#define NUM_PLANES_MINUS2           (NUM_PLANES - 2)        /* 2 */
#define PLANE_STRIDE                44
#define PLANE_STRIDE_MINUS2         (PLANE_STRIDE - 2)      /* 42 */
#define PLANE_STRIDE_MINUS4         (PLANE_STRIDE - 4)      /* 40 */
#define PLANE_STRIDE_DIV2           (PLANE_STRIDE >> 1)     /* 22 */
#define ROW_STRIDE_BYTES            (PLANE_STRIDE * NUM_PLANES)         /* 176 */
#define SRC_SCANLINE_BYTES          (NUM_PLANES * 2)                    /* 8 */

/* ---- Bitplane layout (bit shifts) ---- */
#define NUM_PLANES_SHIFT            2
#define NUM_PLANES_SHIFT_PLUS1      3

/* ---- Bitplane layout (multiples) ---- */
#define PLANE_STRIDE_TIMES2         (PLANE_STRIDE * 2)      /* 88 */
#define PLANE_STRIDE_TIMES3         (PLANE_STRIDE * 3)      /* 132 */
#define PLANE_STRIDE_TIMES4         (PLANE_STRIDE * 4)      /* 176 */
#define PLANE_STRIDE_TIMES5         (PLANE_STRIDE * 5)      /* 220 */
#define PLANE_STRIDE_TIMES6         (PLANE_STRIDE * 6)      /* 264 */
#define PLANE_STRIDE_TIMES7         (PLANE_STRIDE * 7)      /* 308 */

/* ---- Image stream layout ---- */
#define IMAGE_ROW_STRIDE_BYTES      160
#define BITPLANE_ROW_BYTES          40

/* ---- Screen / display dimensions ---- */
#define SCREEN_WIDTH                336
#define SCREEN_HEIGHT               272
#define COPPERLIST_SIZE_BYTES       320

/* ---- Frontbuffer layout ---- */
#define FRONTBUFFER_ROW_STRIDE_BYTES    ROW_STRIDE_BYTES                            /* 176 */
#define FRONTBUFFER_VISIBLE_LINES       SCREEN_HEIGHT                               /* 272 */
#define FRONTBUFFER_TOTAL_LINES         (SCREEN_HEIGHT + 1)                         /* 273 */
#define FRONTBUFFER_SIZE_VISIBLE_BYTES  (FRONTBUFFER_ROW_STRIDE_BYTES * FRONTBUFFER_VISIBLE_LINES)   /* 47872 */
#define FRONTBUFFER_SIZE_TOTAL_BYTES    (FRONTBUFFER_ROW_STRIDE_BYTES * FRONTBUFFER_TOTAL_LINES)     /* 48048 */
#define FB_COPY_WORDS_MINUS1            ((FRONTBUFFER_SIZE_VISIBLE_BYTES >> 1) - 1)                  /* 23935 */
#define FB_BLT_MOD_AFTER_1_WORD         (FRONTBUFFER_ROW_STRIDE_BYTES - 2)          /* 174 */

/* ---- Tile / scroll-blit constants ---- */
#define TILE_STRIDE                 14
#define TILE_SHIFT_X                4
#define TILE_HEIGHT                 16
#define TILE_SIZE                   (TILE_HEIGHT * SRC_SCANLINE_BYTES)               /* 128 */
#define TILE_MASK                   15
#define PITCH_PTR_ROW_BYTES         (PLANE_STRIDE_MINUS2 * 4)                       /* 168 */
#define PITCH_DEST_MOD_UNALIGNED    (ROW_STRIDE_BYTES - PLANE_STRIDE)               /* 132 */
#define PITCH_DEST_MOD_ALIGNED      (ROW_STRIDE_BYTES - (PLANE_STRIDE - 2))         /* 134 */
#define BLTDMOD_PLANE_STEP          PLANE_STRIDE_MINUS2                             /* 42 */
#define BLTSIZE_TILE_COL            (((TILE_HEIGHT * NUM_PLANES) << 6) | 1)         /* 4097 */
#define BLTSIZE_COL_16LINES_ALLPLANES  BLTSIZE_TILE_COL                             /* 4097 */

/* ---- Panel overlay ---- */
#define PANEL_HEIGHT_LINES          30
#define BLTSIZE_OVERLAY_PANEL       (((PANEL_HEIGHT_LINES * NUM_PLANES) << 6) | PLANE_STRIDE_DIV2)  /* 4374 */

/* ---- File info ---- */
#define PITCHFILE_MAX_SIZE          FRONTBUFFER_SIZE_TOTAL_BYTES                     /* 48048 */

/* ---- Copper list byte-offset helper ---- */
/* NOTE: Do NOT use COPPER_W for writes — GCC decomposes volatile uint16_t
 * stores into byte-level move.b pairs. Use copper_write_word() instead. */
#define COPPER_W(offset) (*(volatile uint16_t *)((uint8_t *)copperListBuffer + (offset)))

/* ---- Function declarations ---- */
void _InitDisplayAndCopper(void);
void _UpdatePlanePtrsAndBlitParams(void);
void _SwapBuffersAndCameraViewState(void);
void _SetCameraCoordinates(int16_t targetX, int16_t targetY);
void _DoubleBufferSwapSequence(void);
void _BlitterCopyPanelToScreen2(void);
void _ProcessSpriteBlitQueue(void);
void _BlitterCopyPanelToScreen(void);
void _HWSprite_LoadFromSpriteGraphics(uint16_t spriteIndex);
void _FlashColor0Delay(uint32_t loopCount, uint16_t color);

#endif
