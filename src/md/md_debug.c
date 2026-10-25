/* md_debug.c -- the debug log, panics and a small printf.
 *
 * md_log writes into a 1 KB ring at the start of work RAM (section .md_debuglog, placed
 * first by md.ld, so at 0xFF0000) that scripts/md/mdrun.c --log 0xFF0000 prints as the
 * ROM runs. On a console nothing reads it; it costs 1 KB of RAM and the formatting.
 * md_panic and the exception handler put the message on screen and stop.
 */
#include <stdarg.h>
#include "md_hw.h"

#define LOG_SIZE 1024
typedef struct { volatile uint16_t head; char buf[LOG_SIZE]; } LogRing;
LogRing md_logring __attribute__((section(".md_debuglog"), used));

static void putc_ring(char c)
{
    md_logring.buf[md_logring.head] = c;
    md_logring.head = (md_logring.head + 1) & (LOG_SIZE - 1);
}

static int fmt_num(char *out, uint32_t v, int base, int neg, int width, char pad)
{
    char tmp[12]; int n = 0, len = 0;
    do { uint32_t d = v % (uint32_t)base; tmp[n++] = (char)(d < 10 ? '0' + d : 'A' + d - 10); v /= (uint32_t)base; } while (v);
    if (neg) tmp[n++] = '-';
    while (n + len < width) out[len++] = pad;
    while (n) out[len++] = tmp[--n];
    return len;
}

int md_vsnprintf(char *buf, int size, const char *fmt, va_list ap)
{
    int o = 0;
    char num[24];
    for (; *fmt; fmt++) {
        if (*fmt != '%') { if (o < size - 1) buf[o] = *fmt; o++; continue; }
        fmt++;
        char pad = ' '; int width = 0, lng = 0;
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        while (*fmt == 'l' || *fmt == 'h') { lng = *fmt == 'l'; fmt++; }
        (void)lng;
        int n = 0;
        const char *s = num;
        switch (*fmt) {
        case 'd': case 'i': { int32_t v = va_arg(ap, int32_t); n = fmt_num(num, v < 0 ? (uint32_t)-v : (uint32_t)v, 10, v < 0, width, pad); break; }
        case 'u': n = fmt_num(num, va_arg(ap, uint32_t), 10, 0, width, pad); break;
        case 'x': case 'X': case 'p': n = fmt_num(num, va_arg(ap, uint32_t), 16, 0, width, pad); break;
        case 'c': num[0] = (char)va_arg(ap, int); n = 1; break;
        case 's': s = va_arg(ap, const char *); if (!s) s = "(null)"; while (s[n]) n++; break;
        case '%': num[0] = '%'; n = 1; break;
        default: num[0] = '?'; n = 1; break;
        }
        for (int i = 0; i < n; i++) { if (o < size - 1) buf[o] = s[i]; o++; }
    }
    if (size > 0) buf[o < size ? o : size - 1] = 0;
    return o;
}

int md_snprintf(char *buf, int n, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = md_vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

void md_log(const char *fmt, ...)
{
    char line[160];
    va_list ap; va_start(ap, fmt);
    md_vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    for (char *p = line; *p; p++) putc_ring(*p);
    putc_ring('\n');
}

extern void md_font_upload(void);

static void __attribute__((noreturn)) panic_screen(const char *msg)
{
    static const uint16_t pal[16] = { 0x0008, 0, 0x0EEE, 0x0000 };
    __asm__ volatile ("move.w #0x2700, %sr");
    md_vdp_set_reg(1, 0x04 | (md_is_pal() ? 0x08 : 0) | 0x10);   /* display off, no VInt */
    md_vdp_set_reg(17, 0);
    md_vdp_set_reg(18, 0);
    md_vdp_set_reg(11, 0);
    md_vram_fill(VRAM_PLANE_A, 0, PLANE_A_W * PLANE_A_H);
    md_vram_fill(VRAM_PLANE_B, 0, PLANE_B_W * PLANE_B_H);
    md_vram_fill(VRAM_SPRITES, 0, 4);
    md_vram_fill(VRAM_HSCROLL, 0, 2);
    md_vsram_write(0, 0); md_vsram_write(1, 0);
    md_font_upload();
    md_cram_write(0, pal, 16);
    md_text(VRAM_PLANE_A, PLANE_A_W, 1, 1, 0, "OPENSWOS MEGA DRIVE: PANIC");
    {
        int x = 1, y = 3; char one[2] = { 0, 0 };
        for (const char *p = msg; *p && y < 26; p++) {
            if (*p == '\n' || x >= 39) { x = 1; y++; if (*p == '\n') continue; }
            one[0] = *p;
            md_text(VRAM_PLANE_A, PLANE_A_W, (uint16_t)x++, (uint16_t)y, 0, one);
        }
    }
    md_vdp_set_reg(1, 0x44 | (md_is_pal() ? 0x08 : 0) | 0x10);
    for (;;) {}
}

void md_panic(const char *fmt, ...)
{
    char msg[200];
    va_list ap; va_start(ap, fmt);
    md_vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    md_log("PANIC: %s", msg);
    panic_screen(msg);
}

/* From md_crt0.s: vector 2/3 bus/address error, 4 illegal, 5 divide by zero, 8 privilege,
 * -1 anything else. */
void md_exception(int32_t vec, uint32_t pc, uint32_t addr, uint32_t sr)
{
    static const char *const names[] = { "?", "?", "BUS ERROR", "ADDRESS ERROR", "ILLEGAL INSTRUCTION",
                                         "DIVIDE BY ZERO", "?", "?", "PRIVILEGE VIOLATION" };
    md_panic("%s\nPC %06X ADDR %06X SR %04X", vec >= 0 && vec <= 8 ? names[vec] : "EXCEPTION",
             pc, addr, sr & 0xFFFF);
}

/* The shared engine's panic (_PanicHaltLoop and friends) ends up here. */
void md_engine_panic(uint16_t code)
{
    md_panic("ENGINE PANIC %04X", code);
}
