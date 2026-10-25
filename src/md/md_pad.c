/* md_pad.c -- three- and six-button pads on both ports.
 *
 * Read once per frame from the main loop (never from the VBlank interrupt: the six-button
 * pad counts TH edges and resets after ~1.5 ms, so the reads must not be interleaved).
 */
#include "md_hw.h"

#define PAD_DATA(p) (*(volatile uint8_t *)(0xA10003 + (p) * 2))
#define PAD_CTRL(p) (*(volatile uint8_t *)(0xA10009 + (p) * 2))

uint16_t md_pad[2], md_pad_edge[2];

void md_pad_init(void)
{
    PAD_CTRL(0) = 0x40;
    PAD_CTRL(1) = 0x40;
    PAD_DATA(0) = 0x40;
    PAD_DATA(1) = 0x40;
}

static inline uint8_t th(int port, uint8_t v)
{
    PAD_DATA(port) = v;
    __asm__ volatile ("nop\n\tnop\n\tnop\n\tnop");
    return PAD_DATA(port);
}

uint16_t md_pad_read(int port)
{
    uint8_t hi, lo, six;
    uint16_t r;

    hi = th(port, 0x40);          /* ?1CBRLDU */
    lo = th(port, 0x00);          /* ?0SA00DU */
    r = (uint16_t)(~hi & 0x3F) | (uint16_t)((~lo & 0x30) << 2);
    /* Six-button: two more TH pulses; the third low read is all zeros on the d-pad bits,
     * the high read after it carries MODE X Y Z. */
    th(port, 0x40);
    lo = th(port, 0x00);
    th(port, 0x40);
    lo = th(port, 0x00);
    if ((lo & 0x0F) == 0) {
        six = th(port, 0x40);
        r |= (uint16_t)((~six & 0x0F) << 8) | PAD_6BTN;
        th(port, 0x00);
    }
    PAD_DATA(port) = 0x40;
    return r;
}

void md_pad_poll(void)
{
    for (int p = 0; p < 2; p++) {
        uint16_t now = md_pad_read(p);
        md_pad_edge[p] = now & ~md_pad[p];
        md_pad[p] = now;
    }
}
