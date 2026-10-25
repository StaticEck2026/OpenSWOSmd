/*
 * mdrun -- a headless Mega Drive runner for testing the OpenSWOS Mega Drive build.
 *
 * Loads the Genesis Plus GX libretro core (scripts/md/build_harness.sh builds it),
 * runs a ROM for a number of frames with scripted pad input, and can
 *   - write screenshots (PNG) at chosen frames,
 *   - print the ROM's debug log (a ring buffer in work RAM, see src/md/md_debug.c),
 *   - dump words of work RAM at the end (the ROM's own counters: lag, frame time),
 *   - write the sound to a WAV.
 *
 *   mdrun --core build/md/harness/genesis_plus_gx_libretro.so --rom dist/SWOS-MD.bin
 *         --frames 600 [--pal] [--shot 120,300 --shotdir out/] [--input script.txt]
 *         [--log ADDR] [--peek ADDR:N ...] [--wav out.wav] [--profile FRAME:out.bin]
 *
 * Input script: one line per event, "<frame> <port> <buttons>", buttons a string of
 * U D L R A B C S X Y Z M ('-' for none); it holds until the next event for that port.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "libretro.h"

static void *core;
#define SYM(name) static __typeof__(name) *p_##name
SYM(retro_init); SYM(retro_load_game); SYM(retro_run); SYM(retro_get_memory_data);
SYM(retro_get_memory_size); SYM(retro_set_environment); SYM(retro_set_video_refresh);
SYM(retro_set_audio_sample); SYM(retro_set_audio_sample_batch); SYM(retro_set_input_poll);
SYM(retro_set_input_state); SYM(retro_get_system_av_info); SYM(retro_set_controller_port_device);
SYM(retro_deinit);

static int g_pal;
static int g_pixfmt = RETRO_PIXEL_FORMAT_0RGB1555;
static uint32_t *g_fb; static unsigned g_fbw, g_fbh;
static uint16_t g_pad[2];       /* libretro joypad mask per port */
static FILE *g_wav; static uint32_t g_wavBytes;

static void logcb(enum retro_log_level lvl, const char *fmt, ...)
{
    if (lvl < RETRO_LOG_WARN) return;
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}

static bool env(unsigned cmd, void *data)
{
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)data)->log = logcb; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        g_pixfmt = *(enum retro_pixel_format *)data; return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *v = data;
        if (!strcmp(v->key, "genesis_plus_gx_region_detect")) { v->value = g_pal ? "pal" : "ntsc-u"; return true; }
        if (!strcmp(v->key, "genesis_plus_gx_bios")) { v->value = "disabled"; return true; }
        if (!strcmp(v->key, "genesis_plus_gx_addr_error")) { v->value = "enabled"; return true; }
        if (!strcmp(v->key, "genesis_plus_gx_overscan")) { v->value = "disabled"; return true; }
        if (!strcmp(v->key, "genesis_plus_gx_no_sprite_limit")) { v->value = "disabled"; return true; }
        if (!strcmp(v->key, "genesis_plus_gx_render")) { v->value = "single field"; return true; }
        v->value = NULL;    /* the core keeps reading a stale value otherwise */
        return false;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = "."; return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool *)data = true; return true;
    default: return false;
    }
}

static void video(const void *data, unsigned w, unsigned h, size_t pitch)
{
    if (!data) return;
    if (w != g_fbw || h != g_fbh) { free(g_fb); g_fb = malloc(w * h * 4); g_fbw = w; g_fbh = h; }
    for (unsigned y = 0; y < h; y++) {
        const uint8_t *row = (const uint8_t *)data + y * pitch;
        for (unsigned x = 0; x < w; x++) {
            uint32_t c;
            if (g_pixfmt == RETRO_PIXEL_FORMAT_XRGB8888) c = ((const uint32_t *)row)[x] & 0xFFFFFF;
            else if (g_pixfmt == RETRO_PIXEL_FORMAT_RGB565) {
                uint16_t p = ((const uint16_t *)row)[x];
                c = ((p >> 11 & 31) * 255 / 31) << 16 | ((p >> 5 & 63) * 255 / 63) << 8 | (p & 31) * 255 / 31;
            } else {
                uint16_t p = ((const uint16_t *)row)[x];
                c = ((p >> 10 & 31) * 255 / 31) << 16 | ((p >> 5 & 31) * 255 / 31) << 8 | (p & 31) * 255 / 31;
            }
            g_fb[y * w + x] = c;
        }
    }
}

static void audio1(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audioN(const int16_t *d, size_t frames)
{
    if (g_wav) { fwrite(d, 4, frames, g_wav); g_wavBytes += frames * 4; }
    return frames;
}
static void poll(void) {}
static int16_t inputstate(unsigned port, unsigned dev, unsigned idx, unsigned id)
{
    (void)dev; (void)idx;
    if (port > 1) return 0;
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return (int16_t)g_pad[port];
    return (g_pad[port] >> id) & 1;
}

static void put32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void chunk(FILE *f, const char *type, const uint8_t *d, uint32_t n)
{
    uint8_t h[8]; put32(h, n); memcpy(h + 4, type, 4); fwrite(h, 1, 8, f);
    uint32_t crc = crc32(0, (const uint8_t *)type, 4); crc = crc32(crc, d, n);
    if (n) fwrite(d, 1, n, f);
    put32(h, crc); fwrite(h, 1, 4, f);
}
static void write_png(const char *path)
{
    if (!g_fb) return;
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); return; }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    uint8_t ihdr[13]; put32(ihdr, g_fbw); put32(ihdr + 4, g_fbh);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    size_t rawn = (size_t)g_fbh * (g_fbw * 3 + 1);
    uint8_t *raw = malloc(rawn), *p = raw;
    for (unsigned y = 0; y < g_fbh; y++) {
        *p++ = 0;
        for (unsigned x = 0; x < g_fbw; x++) { uint32_t c = g_fb[y * g_fbw + x]; *p++ = c >> 16; *p++ = c >> 8; *p++ = c; }
    }
    uLongf zn = compressBound(rawn); uint8_t *z = malloc(zn);
    compress2(z, &zn, raw, rawn, 6);
    chunk(f, "IDAT", z, zn); chunk(f, "IEND", NULL, 0);
    fclose(f); free(raw); free(z);
}

/* Work RAM as the 68000 sees it: GPGX keeps it in host-order 16-bit words. */
static uint8_t *g_ram;
static uint8_t ram8(uint32_t a)
{
    a &= 0xFFFF;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    a ^= 1;
#endif
    return g_ram[a];
}
static uint16_t ram16(uint32_t a) { return ram8(a) << 8 | ram8(a + 1); }
static uint32_t ram32(uint32_t a) { return (uint32_t)ram16(a) << 16 | ram16(a + 2); }

/* The ROM's log ring (md_debug.c): u16 head, then 1024 bytes of text. */
static uint16_t g_logTail;
static void drain_log(uint32_t addr)
{
    uint16_t head = ram16(addr);
    while (g_logTail != head) {
        char c = (char)ram8(addr + 2 + g_logTail);
        fputc(c, stdout);
        g_logTail = (g_logTail + 1) & 1023;
    }
}

typedef struct { int frame, port; uint16_t mask; } Ev;
static Ev g_ev[4096]; static int g_nev;
static uint16_t parse_buttons(const char *s)
{
    uint16_t m = 0;
    for (; *s; s++) switch (*s) {
        case 'U': m |= 1 << RETRO_DEVICE_ID_JOYPAD_UP; break;
        case 'D': m |= 1 << RETRO_DEVICE_ID_JOYPAD_DOWN; break;
        case 'L': m |= 1 << RETRO_DEVICE_ID_JOYPAD_LEFT; break;
        case 'R': m |= 1 << RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
        case 'A': m |= 1 << RETRO_DEVICE_ID_JOYPAD_Y; break;   /* GPGX: Y -> MD A */
        case 'B': m |= 1 << RETRO_DEVICE_ID_JOYPAD_B; break;
        case 'C': m |= 1 << RETRO_DEVICE_ID_JOYPAD_A; break;
        case 'S': m |= 1 << RETRO_DEVICE_ID_JOYPAD_START; break;
        case 'X': m |= 1 << RETRO_DEVICE_ID_JOYPAD_L; break;
        case 'Y': m |= 1 << RETRO_DEVICE_ID_JOYPAD_X; break;
        case 'Z': m |= 1 << RETRO_DEVICE_ID_JOYPAD_R; break;
        case 'M': m |= 1 << RETRO_DEVICE_ID_JOYPAD_SELECT; break;
    }
    return m;
}

int main(int argc, char **argv)
{
    const char *corep = NULL, *rom = NULL, *shots = NULL, *shotdir = ".", *inputp = NULL, *wavp = NULL;
    const char *peeks[32]; int npeek = 0;
    long profFrom = -1; const char *profOut = NULL;
    long frames = 300, logaddr = -1, shotEvery = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--core") && i + 1 < argc) corep = argv[++i];
        else if (!strcmp(argv[i], "--rom") && i + 1 < argc) rom = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = strtol(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--pal")) g_pal = 1;
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shots = argv[++i];
        else if (!strcmp(argv[i], "--shot-every") && i + 1 < argc) shotEvery = strtol(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--shotdir") && i + 1 < argc) shotdir = argv[++i];
        else if (!strcmp(argv[i], "--input") && i + 1 < argc) inputp = argv[++i];
        else if (!strcmp(argv[i], "--log") && i + 1 < argc) logaddr = strtol(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--peek") && i + 1 < argc && npeek < 32) peeks[npeek++] = argv[++i];
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc) wavp = argv[++i];
        else if (!strcmp(argv[i], "--profile") && i + 1 < argc) {
            char *colon; profFrom = strtol(argv[++i], &colon, 0);
            profOut = *colon == ':' ? colon + 1 : "profile.bin";
        }
        else { fprintf(stderr, "unknown argument %s\n", argv[i]); return 2; }
    }
    if (!corep || !rom) { fprintf(stderr, "usage: mdrun --core core.so --rom rom.bin [options]\n"); return 2; }

    if (inputp) {
        FILE *f = fopen(inputp, "r"); if (!f) { perror(inputp); return 1; }
        char line[256];
        while (fgets(line, sizeof line, f) && g_nev < 4096) {
            int fr, port; char b[64];
            if (line[0] == '#' || sscanf(line, "%d %d %63s", &fr, &port, b) != 3) continue;
            g_ev[g_nev++] = (Ev){ fr, port, parse_buttons(b) };
        }
        fclose(f);
    }

    core = dlopen(corep, RTLD_NOW);
    if (!core) { fprintf(stderr, "%s\n", dlerror()); return 1; }
#define LOAD(name) p_##name = dlsym(core, #name)
    LOAD(retro_init); LOAD(retro_load_game); LOAD(retro_run); LOAD(retro_get_memory_data);
    LOAD(retro_get_memory_size); LOAD(retro_set_environment); LOAD(retro_set_video_refresh);
    LOAD(retro_set_audio_sample); LOAD(retro_set_audio_sample_batch); LOAD(retro_set_input_poll);
    LOAD(retro_set_input_state); LOAD(retro_get_system_av_info); LOAD(retro_set_controller_port_device);
    LOAD(retro_deinit);

    p_retro_set_environment(env);
    p_retro_set_video_refresh(video);
    p_retro_set_audio_sample(audio1);
    p_retro_set_audio_sample_batch(audioN);
    p_retro_set_input_poll(poll);
    p_retro_set_input_state(inputstate);
    p_retro_init();

    FILE *rf = fopen(rom, "rb"); if (!rf) { perror(rom); return 1; }
    fseek(rf, 0, SEEK_END); long rn = ftell(rf); fseek(rf, 0, SEEK_SET);
    void *rd = malloc(rn); if (fread(rd, 1, rn, rf) != (size_t)rn) return 1; fclose(rf);
    struct retro_game_info gi = { rom, rd, (size_t)rn, NULL };
    if (!p_retro_load_game(&gi)) { fprintf(stderr, "core refused the ROM\n"); return 1; }
    /* 6-button pads on both ports (RETRO_DEVICE_JOYPAD subclass the core lists as "MD Joypad 6 Button") */
    p_retro_set_controller_port_device(0, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1));
    p_retro_set_controller_port_device(1, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1));
    g_ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);

    struct retro_system_av_info av; p_retro_get_system_av_info(&av);
    if (wavp) {
        g_wav = fopen(wavp, "wb");
        if (g_wav) fwrite(calloc(44, 1), 1, 44, g_wav);
    }

    int ev = 0;
    unsigned *profOn = p_retro_get_memory_data(0x7F03);
    for (long f = 0; f < frames; f++) {
        if (profOn && profFrom >= 0 && f == profFrom) *profOn = 1;
        while (ev < g_nev && g_ev[ev].frame <= f) { g_pad[g_ev[ev].port & 1] = g_ev[ev].mask; ev++; }
        p_retro_run();
        if (logaddr >= 0) drain_log((uint32_t)logaddr);
        int want = shotEvery && f % shotEvery == 0;
        if (shots && !want) {
            const char *s = shots;
            while (*s) { if (strtol(s, NULL, 10) == f) { want = 1; break; } s = strchr(s, ','); if (!s) break; s++; }
        }
        if (want) { char p[512]; snprintf(p, sizeof p, "%s/shot_%05ld.png", shotdir, f); write_png(p); }
    }
    if (profOut && profOn) {
        /* cycles by PC/4 below 1 MB, little-endian u32; scripts/md/profile.py reads it */
        FILE *pf = fopen(profOut, "wb");
        if (pf) { fwrite(p_retro_get_memory_data(0x7F02), 4, 0x40000, pf); fclose(pf); }
    }
    for (int i = 0; i < npeek; i++) {
        char *colon; uint32_t a = strtoul(peeks[i], &colon, 0); int n = *colon == ':' ? atoi(colon + 1) : 1;
        printf("peek %06X:", a);
        for (int k = 0; k < n; k++) printf(" %04X", ram16(a + 2 * k));
        printf("\n");
    }
    (void)ram32;
    {
        /* writes into cartridge ROM (scripts/md/gpgx_harness.patch): a table placed in
         * ROM that the game writes to -- a bug on hardware, where the write is lost */
        unsigned *n = p_retro_get_memory_data(0x7F00);
        unsigned (*log)[2] = p_retro_get_memory_data(0x7F01);
        if (n) {
            printf("rom-writes: %u\n", *n);
            for (unsigned i = 0; i < *n && i < 64; i++)
                printf("rom-write %06X from PC %06X\n", log[i][0], log[i][1]);
        }
    }
    if (g_wav) {
        uint8_t h[44]; uint32_t sr = (uint32_t)(av.timing.sample_rate + 0.5);
        memcpy(h, "RIFF", 4); uint32_t v = 36 + g_wavBytes; memcpy(h + 4, &v, 4); memcpy(h + 8, "WAVEfmt ", 8);
        v = 16; memcpy(h + 16, &v, 4); uint16_t s = 1; memcpy(h + 20, &s, 2); s = 2; memcpy(h + 22, &s, 2);
        memcpy(h + 24, &sr, 4); v = sr * 4; memcpy(h + 28, &v, 4); s = 4; memcpy(h + 32, &s, 2); s = 16; memcpy(h + 34, &s, 2);
        memcpy(h + 36, "data", 4); memcpy(h + 40, &g_wavBytes, 4);
        fseek(g_wav, 0, SEEK_SET); fwrite(h, 1, 44, g_wav); fclose(g_wav);
    }
    p_retro_deinit();
    return 0;
}
