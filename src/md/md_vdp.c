/* md_vdp.c -- the VDP: set-up, VRAM/CRAM/VSRAM writes, DMA and the VBlank queue.
 *
 * Everything the game changes on screen while the display is on goes through the DMA
 * queue: md_dma_queue() records a transfer, the VBlank interrupt runs the lot
 * (md_dma_flush) while the beam is off screen. A transfer queued after the frame's
 * budget waits for the next VBlank rather than tearing.
 */
#include "md_hw.h"

volatile uint16_t md_vblanks;
volatile uint16_t md_vblank_hook_enabled;

static uint8_t s_reg[24];
static volatile uint32_t s_dmaCmd;
static uint8_t s_pal;

#define CTRL_VRAM_WRITE(a)  (0x40000000u | (((uint32_t)(a) & 0x3FFF) << 16) | (((uint32_t)(a) >> 14) & 3))
#define CTRL_CRAM_WRITE(a)  (0xC0000000u | (((uint32_t)(a) & 0x3FFF) << 16))
#define CTRL_VSRAM_WRITE(a) (0x40000010u | (((uint32_t)(a) & 0x3FFF) << 16))

void md_vdp_set_reg(uint8_t reg, uint8_t val)
{
    s_reg[reg] = val;
    MD_VDP_CTRL = 0x8000 | (reg << 8) | val;
}

int md_is_pal(void) { return s_pal; }
uint16_t md_screen_h(void) { return s_pal ? 240 : 224; }

void md_vdp_init(void)
{
    (void)MD_VDP_CTRL;
    s_pal = (*(volatile uint8_t *)0xA10001 & 0x40) ? 1 : 0;

    md_vdp_set_reg(0, 0x04);                          /* HInt off, normal colour */
    md_vdp_set_reg(1, 0x04 | (s_pal ? 0x08 : 0) | 0x10 | 0x20); /* display off, VInt on, DMA on, V30 on PAL */
    md_vdp_set_reg(2, VRAM_PLANE_A >> 10);
    md_vdp_set_reg(3, VRAM_WINDOW >> 10);
    md_vdp_set_reg(4, VRAM_PLANE_B >> 13);
    md_vdp_set_reg(5, VRAM_SPRITES >> 9);
    md_vdp_set_reg(6, 0);
    md_vdp_set_reg(7, 0x00);                          /* backdrop: palette 0 colour 0 */
    md_vdp_set_reg(10, 0xFF);
    md_vdp_set_reg(11, 0x00);                         /* full-screen H and V scroll */
    md_vdp_set_reg(12, 0x81);                         /* H40, no interlace, no shadow/highlight */
    md_vdp_set_reg(13, VRAM_HSCROLL >> 10);
    md_vdp_set_reg(14, 0);
    md_vdp_set_reg(15, 2);                            /* auto-increment 2 */
    md_vdp_set_reg(16, 0x01);                         /* planes 64x32 */
    md_vdp_set_reg(17, 0x00);                         /* no window columns */
    md_vdp_set_reg(18, 0x00);                         /* no window rows */

    md_vram_fill(0, 0, 0x8000);                       /* all 64 KB */
    {
        static const uint16_t black[64];
        md_cram_write(0, black, 64);
    }
    for (int i = 0; i < 40; i++) md_vsram_write((uint8_t)i, 0);
}

void md_vdp_display(int on)
{
    md_vdp_set_reg(1, on ? (s_reg[1] | 0x40) : (s_reg[1] & ~0x40));
}

void md_vram_write(uint16_t addr, const void *src, uint16_t bytes)
{
    const uint16_t *p = src;
    MD_VDP_CTRL32 = CTRL_VRAM_WRITE(addr);
    for (uint16_t n = bytes >> 1; n; n--) MD_VDP_DATA = *p++;
}

void md_vram_fill(uint16_t addr, uint16_t value, uint16_t words)
{
    MD_VDP_CTRL32 = CTRL_VRAM_WRITE(addr);
    while (words--) MD_VDP_DATA = value;
}

void md_cram_write(uint8_t index, const uint16_t *colors, uint8_t count)
{
    MD_VDP_CTRL32 = CTRL_CRAM_WRITE(index * 2);
    while (count--) MD_VDP_DATA = *colors++;
}

void md_vsram_write(uint8_t index, uint16_t value)
{
    MD_VDP_CTRL32 = CTRL_VSRAM_WRITE(index * 2);
    MD_VDP_DATA = value;
}

void md_set_nametable_cell(uint16_t plane, uint16_t planeW, uint16_t x, uint16_t y, uint16_t attr)
{
    MD_VDP_CTRL32 = CTRL_VRAM_WRITE(plane + (y * planeW + x) * 2);
    MD_VDP_DATA = attr;
}

/* VRAM DMA from 68000 memory. The source must not cross a 128 KB boundary; md_vram_dma
 * splits the transfer where it would. */
static void dma_raw(uint32_t ctrl, const void *src, uint16_t bytes)
{
    uint32_t s = (uint32_t)src >> 1;
    uint16_t words = bytes >> 1;
    volatile uint32_t *c32 = &MD_VDP_CTRL32;
    /* registers 19-23 (length, source) two to a longword write */
    *c32 = 0x93009400u | ((uint32_t)(words & 0xFF) << 16) | (words >> 8);
    *c32 = 0x95009600u | ((s & 0xFF) << 16) | ((s >> 8) & 0xFF);
    MD_VDP_CTRL = (uint16_t)(0x9700 | ((s >> 16) & 0x7F));
    /* The command that starts a 68000->VDP DMA is written from a RAM copy: on hardware
     * the bus is taken as the second word lands, and the next instruction fetch must not
     * be from ROM (a known Mega Drive DMA hazard; SGDK does the same). */
    s_dmaCmd = ctrl | 0x80;
    *c32 = s_dmaCmd;
}

void md_vram_dma(uint16_t addr, const void *src, uint16_t bytes)
{
    while (bytes) {
        uint32_t room = 0x20000 - ((uint32_t)src & 0x1FFFF);
        uint16_t n = bytes;
        if (n > room) n = (uint16_t)room;
        dma_raw(CTRL_VRAM_WRITE(addr), src, n);
        src = (const uint8_t *)src + n;
        addr += n;
        bytes -= n;
    }
}

/* ---- the VBlank queue ---- */
#define DMAQ_MAX 64
typedef struct { const void *src; uint16_t addr; uint16_t bytes; uint8_t target, inc; } DmaJob;
static DmaJob s_q[DMAQ_MAX];
static volatile uint16_t s_qn;
static uint16_t s_qBytes;

/* What the VDP can move in one VBlank: ~7.6 KB on an NTSC console in H40 (37 lines at
 * 205 bytes), more on PAL (the frame has 72 more lines of blank). The renderer keeps its
 * per-frame uploads under md_dma_budget() and defers the rest. */
void md_dma_queue_ex(uint8_t target, uint16_t addr, const void *src, uint16_t bytes, uint8_t inc)
{
    if (!bytes) return;
    if (s_qn >= DMAQ_MAX) md_panic("DMA queue full");
    s_q[s_qn].src = src;
    s_q[s_qn].addr = addr;
    s_q[s_qn].bytes = bytes;
    s_q[s_qn].target = target;
    s_q[s_qn].inc = inc;
    s_qn++;
    s_qBytes += bytes;
}

void md_dma_queue(uint16_t vramAddr, const void *src, uint16_t bytes)
{
    md_dma_queue_ex(MD_DMA_VRAM, vramAddr, src, bytes, 2);
}

uint16_t md_dma_queued_bytes(void) { return s_qBytes; }

static uint32_t ctrl_for(uint8_t target, uint16_t addr)
{
    if (target == MD_DMA_CRAM) return CTRL_CRAM_WRITE(addr);
    if (target == MD_DMA_VSRAM) return CTRL_VSRAM_WRITE(addr);
    return CTRL_VRAM_WRITE(addr);
}

void md_dma_flush(void)
{
    for (uint16_t i = 0; i < s_qn; i++) {
        DmaJob *j = &s_q[i];
        if (j->bytes <= 8) {            /* a few words: the CPU is quicker than a DMA set-up */
            const uint16_t *w = j->src;
            if (j->inc != 2) md_vdp_set_reg(15, j->inc);
            MD_VDP_CTRL32 = ctrl_for(j->target, j->addr);
            for (uint16_t n = j->bytes >> 1; n; n--) MD_VDP_DATA = *w++;
            if (j->inc != 2) md_vdp_set_reg(15, 2);
            continue;
        }
        if (j->inc != 2) md_vdp_set_reg(15, j->inc);
        {
            const uint8_t *src = j->src;
            uint16_t addr = j->addr, bytes = j->bytes;
            while (bytes) {
                uint32_t room = 0x20000 - ((uint32_t)src & 0x1FFFF);
                uint16_t n = bytes;
                if (n > room) n = (uint16_t)room;
                dma_raw(ctrl_for(j->target, addr), src, n);
                src += n;
                addr += (uint16_t)(j->inc == 2 ? n : (n / 2) * j->inc);
                bytes -= n;
            }
        }
        if (j->inc != 2) md_vdp_set_reg(15, 2);
    }
    s_qn = 0;
    s_qBytes = 0;
}

void md_wait_vblank(void)
{
    uint16_t v = md_vblanks;
    while (md_vblanks == v) {}
}

extern const uint32_t md_font_tiles[96 * 8];
void md_font_upload(void)
{
    md_vram_write(TILE_FONT * 32, md_font_tiles, 96 * 32);
}

void md_text(uint16_t plane, uint16_t planeW, uint16_t x, uint16_t y, uint8_t pal, const char *s)
{
    MD_VDP_CTRL32 = CTRL_VRAM_WRITE(plane + (y * planeW + x) * 2);
    for (; *s; s++) {
        uint8_t c = (uint8_t)*s;
        if (c < 32 || c > 127) c = '?';
        MD_VDP_DATA = TILE_ATTR(pal, 1, 0, 0, TILE_FONT + c - 32);
    }
}
