/* md_hw.h -- Mega Drive hardware: VDP, pads, Z80, and the platform layer's own API.
 *
 * Only the Mega Drive build (Makefile.md) includes this. docs/MEGADRIVE.md describes the
 * VRAM layout and how the shared match engine is driven from md_main.c.
 */
#ifndef MD_HW_H
#define MD_HW_H

#include <stdint.h>

#define MD_VDP_DATA   (*(volatile uint16_t *)0xC00000)
#define MD_VDP_DATA32 (*(volatile uint32_t *)0xC00000)
#define MD_VDP_CTRL   (*(volatile uint16_t *)0xC00004)
#define MD_VDP_CTRL32 (*(volatile uint32_t *)0xC00004)
#define MD_VDP_HV     (*(volatile uint16_t *)0xC00008)
#define MD_PSG        (*(volatile uint8_t  *)0xC00011)

/* VRAM layout (bytes). Tiles are 32 bytes; tile n lives at n * 32.
 * Planes sit on 8 KB boundaries; both are 64x32 cells (512x256 pixels): the pitch scrolls
 * through plane B as a ring (md_render.c), plane A holds the text (menus, the match HUD).
 *
 *   0x0000-0x7FFF  tiles    1..1023  the pitch (tile 0 stays blank: empty cells use it)
 *   0x8000-0xBFFF  tiles 1024..1535  sprite tiles, streamed per frame from ROM
 *   0xC000-0xCFFF  plane B nametable
 *   0xD000-0xDBFF  tiles 1664..1759  the font (ASCII 32..127)
 *   0xDC00-0xDFFF  tiles 1760..1791  UI pieces
 *   0xE000-0xEFFF  plane A nametable
 *   0xF000-0xF27F  sprite attribute table (80 sprites)
 *   0xF400-0xF7FF  horizontal scroll table
 *   0xF800-0xFFFF  tiles 1984..2047  UI pieces
 */
#define VRAM_PLANE_B   0xC000u
#define VRAM_PLANE_A   0xE000u
#define VRAM_WINDOW    0xE000u   /* unused (no window); points at plane A */
#define VRAM_SPRITES   0xF000u
#define VRAM_HSCROLL   0xF400u
#define PLANE_W        64
#define PLANE_H        32
#define PLANE_A_W      PLANE_W
#define PLANE_A_H      PLANE_H
#define PLANE_B_W      PLANE_W
#define PLANE_B_H      PLANE_H

#define TILE_PITCH       1
#define TILE_PITCH_MAX   1023
#define TILE_DYN_SPRITES 1024
#define TILE_DYN_COUNT   512
#define TILE_FONT        1664
#define TILE_FONT_COUNT  96
#define TILE_UI          1760
#define TILE_UI_COUNT    32
#define TILE_UI2         1984
#define TILE_UI2_COUNT   64

/* Nametable entry. */
#define TILE_ATTR(pal, prio, vflip, hflip, idx) \
    ((uint16_t)(((prio) ? 0x8000 : 0) | ((pal) << 13) | ((vflip) ? 0x1000 : 0) | ((hflip) ? 0x0800 : 0) | ((idx) & 0x7FF)))

/* Pad bits as md_pad_read returns them (1 = pressed). */
#define PAD_UP     0x0001
#define PAD_DOWN   0x0002
#define PAD_LEFT   0x0004
#define PAD_RIGHT  0x0008
#define PAD_B      0x0010
#define PAD_C      0x0020
#define PAD_A      0x0040
#define PAD_START  0x0080
#define PAD_Z      0x0100
#define PAD_Y      0x0200
#define PAD_X      0x0400
#define PAD_MODE   0x0800
#define PAD_6BTN   0x8000   /* the pad answered as a six-button pad */

/* ---- md_vdp.c ---- */
void     md_vdp_init(void);
int      md_is_pal(void);               /* 1 on a 50 Hz console */
uint16_t md_screen_h(void);             /* 224 (NTSC) or 240 (PAL) */
void     md_vdp_set_reg(uint8_t reg, uint8_t val);
void     md_vdp_display(int on);
void     md_vram_write(uint16_t addr, const void *src, uint16_t bytes);  /* CPU copy */
void     md_vram_fill(uint16_t addr, uint16_t value, uint16_t words);
void     md_vram_dma(uint16_t addr, const void *src, uint16_t bytes);    /* src in ROM or RAM, even */
void     md_cram_write(uint8_t index, const uint16_t *colors, uint8_t count);
void     md_vsram_write(uint8_t index, uint16_t value);
void     md_set_nametable_cell(uint16_t plane, uint16_t planeW, uint16_t x, uint16_t y, uint16_t attr);
void     md_wait_vblank(void);
void     md_text(uint16_t plane, uint16_t planeW, uint16_t x, uint16_t y, uint8_t pal, const char *s);
void     md_font_upload(void);

/* Deferred VDP updates, run in the next VBlank (queue in md_vdp.c). `inc` is the VDP
 * auto-increment: 2 for a run, 128 for a column of a 64-wide plane. */
#define MD_DMA_VRAM  0
#define MD_DMA_CRAM  1
#define MD_DMA_VSRAM 2
void     md_dma_queue(uint16_t vramAddr, const void *src, uint16_t bytes);
void     md_dma_queue_ex(uint8_t target, uint16_t addr, const void *src, uint16_t bytes, uint8_t inc);
void     md_dma_flush(void);                                   /* called from the VBlank IRQ */
uint16_t md_dma_queued_bytes(void);
static inline void md_irq_off(void) { __asm__ volatile ("move.w #0x2700, %%sr" ::: "memory"); }
static inline void md_irq_on(void)  { __asm__ volatile ("move.w #0x2500, %%sr" ::: "memory"); }

extern volatile uint16_t md_vblanks;     /* VBlank count since power-on */
extern volatile uint16_t md_vblank_hook_enabled;

/* ---- md_pad.c ---- */
void     md_pad_init(void);
uint16_t md_pad_read(int port);
extern uint16_t md_pad[2], md_pad_edge[2];
void     md_pad_poll(void);

/* ---- md_debug.c ---- */
void     md_log(const char *fmt, ...);
void     md_panic(const char *fmt, ...) __attribute__((noreturn));
int      md_vsnprintf(char *buf, int n, const char *fmt, __builtin_va_list ap);
int      md_snprintf(char *buf, int n, const char *fmt, ...);

/* ---- md_sound.c ---- */
void     md_sound_init(void);
void     md_sound_frame(void);
void     md_sound_stop(void);
extern uint8_t md_soundOn;      /* the OPTIONS screen's SOUND */

#endif
