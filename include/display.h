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

/* ---- Blitter register offsets from CUSTOM_BASE ---- */
#define BLT_BLTCON0         64
#define BLT_BLTCON1         66
#define BLT_BLTAFWM         68
#define BLT_BLTALWM         70
#define BLT_BLTAPT          80
#define BLT_BLTDPT          84
#define BLT_BLTSIZE         88
#define BLT_BLTAMOD         100
#define BLT_BLTDMOD         102

/* ---- Display constants ---- */
#define COPPERLIST_SIZE_BYTES       320
#define NUM_PLANES                  4
#define NUM_PLANES_MINUS1           3
#define PLANE_STRIDE                44
#define ROW_STRIDE_BYTES            176  /* PLANE_STRIDE * NUM_PLANES */
#define SCREEN_HEIGHT               272
#define PITCH_DEST_MOD_ALIGNED      134
#define PITCH_DEST_MOD_UNALIGNED    132

/* ---- DMA / Blitter ---- */
#define DMACON_BLT_ON               33792u
#define DMACON_BLT_OFF              1024u
#define DMACONR_BLTBUSY_BIT         6
#define BLT_FWM_ALL                 0xFFFFu
#define BLTCON0_USEA_USED_LF_F0    2544u
#define BLTCON0_USED_LF_F0          496u
#define BLTSIZE_OVERLAY_PANEL       4374u
#define COLOR00_ADDR                0x00DFF180u
#define BLT_BLTADAT                 116

/* ---- Copper list byte-offset helper ---- */
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
