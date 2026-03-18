/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/*
 * resload.c — C port of Resload.s
 *
 * AmigaOS library calls use inline asm with register-specified variables.
 * Data (S_DosBase, S_VBR, etc.) stays in Resload.s (Phase 1).
 */
#include "resload.h"
#include "display.h"

/* ---- Hardware register addresses (resload-specific, not in display.h) ---- */
#define ADKCON_ADDR    0x00DFF09Eu
#define ADKCONR_ADDR   0x00DFF010u
#define INTENAR_ADDR   0x00DFF01Cu
#define COP2LC_ADDR    0x00DFF084u
#define BPLCON0_OFF    0x100u
#define CIAA_SDR_ADDR  0x00BFEC01u

/* ---- Interrupt vector offsets ---- */
#define LEVEL_1_VEC    0x64u
#define LEVEL_2_VEC    0x68u
#define LEVEL_3_VEC    0x6Cu
#define LEVEL_4_VEC    0x70u
#define LEVEL_5_VEC    0x74u

/* ---- AmigaOS constants ---- */
#define MODE_OLDFILE   1005u
#define MODE_NEWFILE   1006u
#define MEMF_PUBLIC    1u
#define MEMF_FAST      4u
#define DOS_FIB        2u
#define ACCESS_READ    (-2)

#define RNC_TYPE1_MAGIC 0x524E4301u
#define RNC_TYPE2_MAGIC 0x524E4302u

/* ---- GfxBase struct offsets ---- */
#define GFX_ACTIVIEW   0x22u
#define GFX_COPINIT    0x26u
#define GFX_LOFLIST    0x32u

/* ---- Exec struct offsets ---- */
#define EXEC_ATTNFLAGS 296u

/* ---- External asm data (Resload.s) ---- */
extern uint32_t S_DosBase;
extern uint32_t S_GfxBase;
extern uint16_t S_FromWB;
extern uint32_t S_WBMsg;
extern uint32_t S_VBR;
extern uint32_t S_SavedView;
extern uint16_t S_SavedDmaCon;
extern uint16_t S_SavedIntEna;
extern uint16_t S_SavedAdkCon;
extern uint32_t S_SavedSP;
extern uint32_t S_SavedInt1;
extern uint32_t S_SavedInt2;
extern uint32_t S_SavedInt3;
extern uint32_t S_SavedInt4;
extern uint32_t S_SavedInt5;
extern uint32_t size;       /* file size global used by rob_* functions */
extern uint16_t g_quitToDos;
extern uint8_t _keyexit;   /* DEL key code = 0x46 */

/* ---- External asm functions ---- */
extern void _S_GetVBR(void);          /* asm: MOVEC VBR,D0; RTE */
extern void Int2KeyboardHandler(void); /* game keyboard interrupt handler */

/* ---- External C functions ---- */
extern void _RncDecompressV1(uint8_t *src, uint8_t *dst);
extern void _RncDecompressV2(uint8_t *src, uint8_t *dst);

/* ---- Local data ---- */
static uint8_t data_path_buf[256];
static const uint8_t progdir_prefix[] = "PROGDIR:";
/* save_path_buf: "save/" prefix (5 bytes) + filename (up to 14 bytes).
 * fix_filename() copies the filename starting at offset 5 and returns
 * the buffer start, producing "save/FILENAME.EXT".
 * Original asm had these as two adjacent labels but in the wrong order
 * (_save_filename before _save_filepath), which was a latent bug.  */
static uint8_t save_path_buf[5 + 14] = { 's','a','v','e','/' };
static const uint8_t savedir[] = "save";
static uint8_t listdir_buf[2048 + 4];

static const uint32_t ext_table[] = {
    0x2E444959u, /* ".DIY" */
    0x2E505245u, /* ".PRE" */
    0x2E534541u, /* ".SEA" */
    0x2E434152u, /* ".CAR" */
    0x2E544143u, /* ".TAC" */
    0x2E48494Cu, /* ".HIL" */
    0            /* terminator */
};

/* RobNorthenLoader d1 return value (read by asm bridge) */
uint32_t _rnl_d1_return;

/* ================================================================
 * AmigaOS inline asm call helpers
 *
 * IMPORTANT: AmigaOS library calls trash d0, d1, a0, a1.
 * Any input register that is also trashed MUST use "+r" (read-write)
 * constraint, NOT "r" (read-only). This tells GCC the register is
 * modified, preventing it from keeping other values in that register
 * across the asm call. A trashed register must NOT also appear in
 * the clobber list (GCC forbids overlap between operands and clobbers).
 * ================================================================ */

static inline void *exec_base(void)
{
    return *(void **)4;
}

/* --- Exec library calls --- */

/* OldOpenLibrary: a1=name, d0=version → d0=base */
static inline void *os_OldOpenLibrary(const char *name, uint32_t version)
{
    register void *_a6 __asm("a6") = exec_base();
    register const char *_a1 __asm("a1") = name;
    register uint32_t _d0 __asm("d0") = version;
    __asm volatile (
        "jsr -408(%%a6)"
        : "+r"(_d0), "+r"(_a1)
        : "r"(_a6)
        : "d1", "a0", "cc", "memory"
    );
    return (void *)_d0;
}

/* FindTask: a1=name (NULL=self) → d0=task */
static inline void *os_FindTask(void *name)
{
    register void *_a6 __asm("a6") = exec_base();
    register void *_a1 __asm("a1") = name;
    register void *_d0 __asm("d0");
    __asm volatile (
        "jsr -294(%%a6)"
        : "=r"(_d0), "+r"(_a1)
        : "r"(_a6)
        : "d1", "a0", "cc", "memory"
    );
    return _d0;
}

/* WaitPort: a0=port */
static inline void os_WaitPort(void *port)
{
    register void *_a6 __asm("a6") = exec_base();
    register void *_a0 __asm("a0") = port;
    __asm volatile (
        "jsr -384(%%a6)"
        : "+r"(_a0)
        : "r"(_a6)
        : "d0", "d1", "a1", "cc", "memory"
    );
}

/* GetMsg: a0=port → d0=msg */
static inline void *os_GetMsg(void *port)
{
    register void *_a6 __asm("a6") = exec_base();
    register void *_a0 __asm("a0") = port;
    register void *_d0 __asm("d0");
    __asm volatile (
        "jsr -372(%%a6)"
        : "=r"(_d0), "+r"(_a0)
        : "r"(_a6)
        : "d1", "a1", "cc", "memory"
    );
    return _d0;
}

/* Supervisor: a5=func → d0=result (a5 is callee-saved, not trashed) */
static inline uint32_t os_Supervisor(void (*func)(void))
{
    register void *_a6 __asm("a6") = exec_base();
    register void (*_a5)(void) __asm("a5") = func;
    register uint32_t _d0 __asm("d0");
    __asm volatile (
        "jsr -30(%%a6)"
        : "=r"(_d0)
        : "r"(_a6), "r"(_a5)
        : "d1", "a0", "a1", "cc", "memory"
    );
    return _d0;
}

/* ReplyMsg: a1=msg */
static inline void os_ReplyMsg(void *msg)
{
    register void *_a6 __asm("a6") = exec_base();
    register void *_a1 __asm("a1") = msg;
    __asm volatile (
        "jsr -378(%%a6)"
        : "+r"(_a1)
        : "r"(_a6)
        : "d0", "d1", "a0", "cc", "memory"
    );
}

/* CloseLibrary: a1=lib */
static inline void os_CloseLibrary(void *lib)
{
    register void *_a6 __asm("a6") = exec_base();
    register void *_a1 __asm("a1") = lib;
    __asm volatile (
        "jsr -414(%%a6)"
        : "+r"(_a1)
        : "r"(_a6)
        : "d0", "d1", "a0", "cc", "memory"
    );
}

/* AllocMem: d0=size, d1=flags → d0=addr */
static inline void *os_AllocMem(uint32_t sz, uint32_t flags)
{
    register void *_a6 __asm("a6") = exec_base();
    register uint32_t _d0 __asm("d0") = sz;
    register uint32_t _d1 __asm("d1") = flags;
    __asm volatile (
        "jsr -198(%%a6)"
        : "+r"(_d0), "+r"(_d1)
        : "r"(_a6)
        : "a0", "a1", "cc", "memory"
    );
    return (void *)_d0;
}

/* FreeMem: a1=addr, d0=size */
static inline void os_FreeMem(void *addr, uint32_t sz)
{
    register void *_a6 __asm("a6") = exec_base();
    register void *_a1 __asm("a1") = addr;
    register uint32_t _d0 __asm("d0") = sz;
    __asm volatile (
        "jsr -210(%%a6)"
        : "+r"(_d0), "+r"(_a1)
        : "r"(_a6)
        : "d1", "a0", "cc", "memory"
    );
}

/* --- DOS library calls (a6=dosBase) --- */

/* Open: d1=name, d2=mode → d0=handle */
static inline uint32_t os_DosOpen(void *dosBase, const uint8_t *name, uint32_t mode)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d1 __asm("d1") = (uint32_t)name;
    register uint32_t _d2 __asm("d2") = mode;
    register uint32_t _d0 __asm("d0");
    __asm volatile (
        "jsr -30(%%a6)"
        : "=r"(_d0), "+r"(_d1)
        : "r"(_a6), "r"(_d2)
        : "a0", "a1", "cc", "memory"
    );
    return _d0;
}

/* Close: d1=handle */
static inline void os_DosClose(void *dosBase, uint32_t handle)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d1 __asm("d1") = handle;
    __asm volatile (
        "jsr -36(%%a6)"
        : "+r"(_d1)
        : "r"(_a6)
        : "d0", "a0", "a1", "cc", "memory"
    );
}

/* Read: d1=handle, d2=buf, d3=len → d0=bytesRead */
static inline int32_t os_DosRead(void *dosBase, uint32_t handle, void *buf, uint32_t len)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d1 __asm("d1") = handle;
    register uint32_t _d2 __asm("d2") = (uint32_t)buf;
    register uint32_t _d3 __asm("d3") = len;
    register int32_t _d0 __asm("d0");
    __asm volatile (
        "jsr -42(%%a6)"
        : "=r"(_d0), "+r"(_d1)
        : "r"(_a6), "r"(_d2), "r"(_d3)
        : "a0", "a1", "cc", "memory"
    );
    return _d0;
}

/* Write: d1=handle, d2=buf, d3=len → d0=bytesWritten */
static inline int32_t os_DosWrite(void *dosBase, uint32_t handle, const void *buf, uint32_t len)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d1 __asm("d1") = handle;
    register uint32_t _d2 __asm("d2") = (uint32_t)buf;
    register uint32_t _d3 __asm("d3") = len;
    register int32_t _d0 __asm("d0");
    __asm volatile (
        "jsr -48(%%a6)"
        : "=r"(_d0), "+r"(_d1)
        : "r"(_a6), "r"(_d2), "r"(_d3)
        : "a0", "a1", "cc", "memory"
    );
    return _d0;
}

/* Seek: d1=handle, d2=offset, d3=mode → d0=oldPos */
static inline int32_t os_DosSeek(void *dosBase, uint32_t handle, int32_t offset, int32_t mode)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d1 __asm("d1") = handle;
    register int32_t _d2 __asm("d2") = offset;
    register int32_t _d3 __asm("d3") = mode;
    register int32_t _d0 __asm("d0");
    __asm volatile (
        "jsr -66(%%a6)"
        : "=r"(_d0), "+r"(_d1)
        : "r"(_a6), "r"(_d2), "r"(_d3)
        : "a0", "a1", "cc", "memory"
    );
    return _d0;
}

/* DeleteFile: d1=name → d0=success */
static inline uint32_t os_DosDeleteFile(void *dosBase, const uint8_t *name)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d1 __asm("d1") = (uint32_t)name;
    register uint32_t _d0 __asm("d0");
    __asm volatile (
        "jsr -72(%%a6)"
        : "=r"(_d0), "+r"(_d1)
        : "r"(_a6)
        : "a0", "a1", "cc", "memory"
    );
    return _d0;
}

/* Lock: d1=name, d2=mode → d0=lock */
static inline uint32_t os_DosLock(void *dosBase, const uint8_t *name, int32_t mode)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d1 __asm("d1") = (uint32_t)name;
    register int32_t _d2 __asm("d2") = mode;
    register uint32_t _d0 __asm("d0");
    __asm volatile (
        "jsr -84(%%a6)"
        : "=r"(_d0), "+r"(_d1)
        : "r"(_a6), "r"(_d2)
        : "a0", "a1", "cc", "memory"
    );
    return _d0;
}

/* UnLock: d1=lock */
static inline void os_DosUnLock(void *dosBase, uint32_t lock)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d1 __asm("d1") = lock;
    __asm volatile (
        "jsr -90(%%a6)"
        : "+r"(_d1)
        : "r"(_a6)
        : "d0", "a0", "a1", "cc", "memory"
    );
}

/* Examine: d1=lock, d2=fib → d0=success */
static inline uint32_t os_DosExamine(void *dosBase, uint32_t lock, void *fib)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d1 __asm("d1") = lock;
    register uint32_t _d2 __asm("d2") = (uint32_t)fib;
    register uint32_t _d0 __asm("d0");
    __asm volatile (
        "jsr -102(%%a6)"
        : "=r"(_d0), "+r"(_d1)
        : "r"(_a6), "r"(_d2)
        : "a0", "a1", "cc", "memory"
    );
    return _d0;
}

/* ExNext: d1=lock, d2=fib → d0=success */
static inline uint32_t os_DosExNext(void *dosBase, uint32_t lock, void *fib)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d1 __asm("d1") = lock;
    register uint32_t _d2 __asm("d2") = (uint32_t)fib;
    register uint32_t _d0 __asm("d0");
    __asm volatile (
        "jsr -108(%%a6)"
        : "=r"(_d0), "+r"(_d1)
        : "r"(_a6), "r"(_d2)
        : "a0", "a1", "cc", "memory"
    );
    return _d0;
}

/* IoErr: → d0=error */
static inline uint32_t os_DosIoErr(void *dosBase)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d0 __asm("d0");
    __asm volatile (
        "jsr -132(%%a6)"
        : "=r"(_d0)
        : "r"(_a6)
        : "d1", "a0", "a1", "cc", "memory"
    );
    return _d0;
}

/* AllocDosObject: d1=type, d2=tags → d0=obj */
static inline void *os_DosAllocDosObject(void *dosBase, uint32_t type, void *tags)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d1 __asm("d1") = type;
    register uint32_t _d2 __asm("d2") = (uint32_t)tags;
    register void *_d0 __asm("d0");
    __asm volatile (
        "jsr -228(%%a6)"
        : "=r"(_d0), "+r"(_d1)
        : "r"(_a6), "r"(_d2)
        : "a0", "a1", "cc", "memory"
    );
    return _d0;
}

/* FreeDosObject: d1=type, d2=obj */
static inline void os_DosFreeDosObject(void *dosBase, uint32_t type, void *obj)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d1 __asm("d1") = type;
    register uint32_t _d2 __asm("d2") = (uint32_t)obj;
    __asm volatile (
        "jsr -234(%%a6)"
        : "+r"(_d1)
        : "r"(_a6), "r"(_d2)
        : "d0", "a0", "a1", "cc", "memory"
    );
}

/* PutStr: d1=string */
static inline void os_DosPutStr(void *dosBase, const char *str)
{
    register void *_a6 __asm("a6") = dosBase;
    register uint32_t _d1 __asm("d1") = (uint32_t)str;
    __asm volatile (
        "jsr -948(%%a6)"
        : "+r"(_d1)
        : "r"(_a6)
        : "d0", "a0", "a1", "cc", "memory"
    );
}

/* --- Graphics library calls (a6=gfxBase) --- */

/* LoadView: a1=view */
static inline void os_GfxLoadView(void *gfxBase, void *view)
{
    register void *_a6 __asm("a6") = gfxBase;
    register void *_a1 __asm("a1") = view;
    __asm volatile (
        "jsr -222(%%a6)"
        : "+r"(_a1)
        : "r"(_a6)
        : "d0", "d1", "a0", "cc", "memory"
    );
}

/* WaitTOF */
static inline void os_GfxWaitTOF(void *gfxBase)
{
    register void *_a6 __asm("a6") = gfxBase;
    __asm volatile (
        "jsr -270(%%a6)"
        :
        : "r"(_a6)
        : "d0", "d1", "a0", "a1", "cc", "memory"
    );
}


/* ================================================================
 * Internal helpers
 * ================================================================ */

static void print_file_error(const uint8_t *filename)
{
    void *dosBase = (void *)S_DosBase;
    if (!dosBase) return;
    os_DosPutStr(dosBase, "Error - Could not open file: ");
    os_DosPutStr(dosBase, (const char *)filename);
    os_DosPutStr(dosBase, "\n");
}

/* resload_MakeDataPath: prepend "PROGDIR:" if name has no colon */
static uint8_t *make_data_path(uint8_t *name)
{
    uint8_t *p;
    const uint8_t *src;
    uint8_t *dst;

    if (*name == 0) return name;

    /* Scan for colon */
    for (p = name; *p; p++) {
        if (*p == ':') return name; /* absolute/device path */
    }

    /* Build "PROGDIR:" + name into data_path_buf */
    dst = data_path_buf;
    for (src = progdir_prefix; *src; )
        *dst++ = *src++;
    for (src = name; ; ) {
        *dst = *src++;
        if (*dst == 0) break;
        dst++;
    }
    return data_path_buf;
}

/* Swap interrupt vector 2 to OS (restore saved) */
static void swap_int2_to_os(void)
{
    volatile uint32_t *vec = (volatile uint32_t *)(S_VBR + LEVEL_2_VEC);
    *vec = S_SavedInt2;
}

/* Swap interrupt vector 2 back to game handler */
static void swap_int2_to_game(void)
{
    volatile uint32_t *vec = (volatile uint32_t *)(S_VBR + LEVEL_2_VEC);
    *vec = (uint32_t)Int2KeyboardHandler;
}

/* Wait for blitter idle */
static inline __attribute__((always_inline)) void wait_blitter(void)
{
    (void)(custom->dmaconr & (1u << 14));
    while (custom->dmaconr & (1u << 14))
        ;
}


/* ================================================================
 * SafeStart
 * ================================================================ */
__attribute__((used, externally_visible))
void _SafeStart(void)
{
    /* Open graphics.library (optional) */
    S_GfxBase = (uint32_t)os_OldOpenLibrary("graphics.library", 0);

    S_FromWB = 0;

    /* Find our process */
    void *task = os_FindTask((void *)0);
    if (task) {
        /* Check CLI vs Workbench: pr_CLI at offset 172 */
        uint32_t cli = *(uint32_t *)((uint8_t *)task + 172);
        if (cli == 0) {
            /* Workbench launch */
            S_FromWB = 1;
            void *msgPort = (void *)((uint8_t *)task + 92); /* pr_MsgPort */
            os_WaitPort(msgPort);
            S_WBMsg = (uint32_t)os_GetMsg(msgPort);
        }
    }

    S_VBR = 0;

    /* Check for 68010+ (AttnFlags bit 0) */
    uint8_t *execPtr = (uint8_t *)exec_base();
    if (execPtr[EXEC_ATTNFLAGS + 1] & 1) {
        /* 68010+ : get VBR via Supervisor mode */
        S_VBR = os_Supervisor(_S_GetVBR);
    }

    /* Save original interrupt vectors */
    volatile uint32_t *vbr = (volatile uint32_t *)S_VBR;
    S_SavedInt1 = vbr[LEVEL_1_VEC / 4];
    S_SavedInt2 = vbr[LEVEL_2_VEC / 4];
    S_SavedInt3 = vbr[LEVEL_3_VEC / 4];
    S_SavedInt4 = vbr[LEVEL_4_VEC / 4];
    S_SavedInt5 = vbr[LEVEL_5_VEC / 4];
}


/* ================================================================
 * DetachOSDisplay
 * ================================================================ */
__attribute__((used, externally_visible))
void _DetachOSDisplay(void)
{
    if (S_FromWB == 0) return;

    void *gfxBase = (void *)S_GfxBase;
    if (!gfxBase) return;

    /* Save current View from gfxbase */
    S_SavedView = *(uint32_t *)((uint8_t *)gfxBase + GFX_ACTIVIEW);

    /* LoadView(NULL) — detach WB display */
    os_GfxLoadView(gfxBase, (void *)0);
    os_GfxWaitTOF(gfxBase);
    os_GfxWaitTOF(gfxBase);

    /* Save DMACONR/INTENAR/ADKCONR with SET bit for later restore */
    S_SavedDmaCon = custom->dmaconr | 0x8000u;
    S_SavedIntEna = custom->intenar | 0x8000u;
    S_SavedAdkCon = custom->adkconr | 0x8000u;

    /* Wait for blitter idle */
    wait_blitter();

    /* Disable all DMA and interrupts */
    custom->dmacon = 0x01FFu;
    custom->intena = 0x3FFFu;
}


/* ================================================================
 * LoadDosLibrary
 * ================================================================ */
__attribute__((used, externally_visible))
void _LoadDosLibrary(void)
{
    if (S_DosBase != 0) return; /* already loaded */
    S_DosBase = (uint32_t)os_OldOpenLibrary("dos.library", 39);
}


/* ================================================================
 * CloseDosLibrary
 * ================================================================ */
__attribute__((used, externally_visible))
void _CloseDosLibrary(void)
{
    if (S_DosBase == 0) return;
    os_CloseLibrary((void *)S_DosBase);
}


/* ================================================================
 * AllocMemAny — C-callable wrapper for Exec AllocMem(MEMF_PUBLIC)
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_AllocMemAny(uint32_t sz)
{
    return (uint8_t *)os_AllocMem(sz, MEMF_PUBLIC);
}


/* ================================================================
 * ExitToDos — restore hardware, close libs, reply WB msg
 * NOTE: SP restore is handled by _startShim in asm.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ExitToDos(void)
{
    /* Restore interrupt vectors */
    volatile uint32_t *vbr = (volatile uint32_t *)S_VBR;
    vbr[LEVEL_1_VEC / 4] = S_SavedInt1;
    vbr[LEVEL_2_VEC / 4] = S_SavedInt2;
    vbr[LEVEL_3_VEC / 4] = S_SavedInt3;
    vbr[LEVEL_4_VEC / 4] = S_SavedInt4;
    vbr[LEVEL_5_VEC / 4] = S_SavedInt5;

    /* Wait for blitter idle */
    wait_blitter();

    /* Restore copper list from gfx base */
    void *gfxBase = (void *)S_GfxBase;
    custom->cop1lc = *(uint32_t *)((uint8_t *)gfxBase + GFX_COPINIT);
    custom->cop2lc = *(uint32_t *)((uint8_t *)gfxBase + GFX_LOFLIST);
    custom->copjmp1 = 0;

    /* Restore DMA / interrupts / ADKCON */
    custom->dmacon = S_SavedDmaCon;
    custom->intena = S_SavedIntEna;
    custom->adkcon = S_SavedAdkCon;

    /* Restore original view via graphics.library */
    void *savedView = (void *)S_SavedView;
    if (savedView) {
        os_GfxLoadView(gfxBase, savedView);
        gfxBase = (void *)S_GfxBase; /* reload after call */
        os_GfxWaitTOF(gfxBase);
        os_GfxWaitTOF(gfxBase);
    }

    if (S_FromWB == 0) return;

    /* Reply WB message */
    if (S_WBMsg != 0) {
        os_ReplyMsg((void *)S_WBMsg);
    }

    /* Close graphics.library */
    if (S_GfxBase != 0) {
        os_CloseLibrary((void *)S_GfxBase);
    }
}


/* ================================================================
 * resload file I/O — internal C implementations
 * ================================================================ */

/* GetFileSize: open, seek to end, seek back, close */
static uint32_t resload_getfilesize(uint8_t *name)
{
    name = make_data_path(name);
    void *dosBase = (void *)S_DosBase;

    uint32_t handle = os_DosOpen(dosBase, name, MODE_OLDFILE);
    if (handle == 0) return 0;

    /* Seek to end */
    os_DosSeek(dosBase, handle, 0, 1);  /* OFFSET_END = 1 */
    /* Seek back to beginning — returns old pos = file size */
    int32_t fileSize = os_DosSeek(dosBase, handle, 0, -1); /* OFFSET_BEGINNING = -1 */

    os_DosClose(dosBase, handle);
    return (uint32_t)fileSize;
}

/* LoadFile: open, read, close */
static uint32_t resload_loadfile(uint8_t *name, uint8_t *dest, uint32_t *outError)
{
    name = make_data_path(name);
    void *dosBase = (void *)S_DosBase;

    uint32_t handle = os_DosOpen(dosBase, name, MODE_OLDFILE);
    if (handle == 0) {
        custom->color[0] = 0x0F00u; /* red flash */
        print_file_error(name);
        if (outError) *outError = 1;
        return 0;
    }

    int32_t bytesRead = os_DosRead(dosBase, handle, dest, 0x7FFFFFFFu);
    os_DosClose(dosBase, handle);

    if (bytesRead <= 0) {
        custom->color[0] = 0x0F00u;
        print_file_error(name);
        if (outError) *outError = 1;
        return 0;
    }

    if (outError) *outError = 0;
    return (uint32_t)bytesRead;
}

/* LoadFileDEC: load to temp buffer, RNC decompress or copy raw, free temp */
static uint32_t resload_loadfiledec(uint8_t *name, uint8_t *dest, uint32_t *outError)
{
    name = make_data_path(name);

    /* Get file size */
    uint32_t fileSize = resload_getfilesize(name);
    if (fileSize == 0) {
        if (outError) *outError = 1;
        return 0;
    }

    /* Allocate temporary FastMem buffer */
    uint8_t *tempBuf = (uint8_t *)os_AllocMem(fileSize, MEMF_FAST);
    if (!tempBuf) {
        if (outError) *outError = 1;
        return 0;
    }

    /* Open and read file into temp buffer */
    void *dosBase = (void *)S_DosBase;
    uint32_t handle = os_DosOpen(dosBase, name, MODE_OLDFILE);
    if (handle == 0) {
        os_FreeMem(tempBuf, fileSize);
        if (outError) *outError = 1;
        return 0;
    }

    int32_t bytesRead = os_DosRead(dosBase, handle, tempBuf, fileSize);
    os_DosClose(dosBase, handle);

    if ((uint32_t)bytesRead != fileSize) {
        os_FreeMem(tempBuf, fileSize);
        if (outError) *outError = 1;
        return 0;
    }

    /* Check for RNC magic and decompress */
    uint32_t magic = *(uint32_t *)tempBuf;
    uint32_t decompSize;

    if (magic == RNC_TYPE2_MAGIC) {
        decompSize = *(uint32_t *)(tempBuf + 4);
        _RncDecompressV2(tempBuf, dest);
    } else if (magic == RNC_TYPE1_MAGIC) {
        decompSize = *(uint32_t *)(tempBuf + 4);
        _RncDecompressV1(tempBuf, dest);
    } else {
        /* Raw copy */
        uint32_t i;
        for (i = 0; i < fileSize; i++)
            dest[i] = tempBuf[i];
        decompSize = fileSize;
    }

    os_FreeMem(tempBuf, fileSize);
    if (outError) *outError = 0;
    return decompSize;
}

/* Deletefile: dos_DeleteFile */
static uint32_t resload_deletefile(uint8_t *name, uint32_t *outError)
{
    name = make_data_path(name);
    void *dosBase = (void *)S_DosBase;

    uint32_t success = os_DosDeleteFile(dosBase, name);
    if (success == 0) {
        if (outError) *outError = 1;
        return 0;
    }
    if (outError) *outError = 0;
    return success;
}

/* SaveFile: create/truncate, write, close */
static uint32_t resload_savefile(uint32_t fileSize, uint8_t *name, uint8_t *src, uint32_t *outError)
{
    name = make_data_path(name);
    void *dosBase = (void *)S_DosBase;

    uint32_t handle = os_DosOpen(dosBase, name, MODE_NEWFILE);
    if (handle == 0) {
        if (outError) *outError = 1;
        return 0;
    }

    int32_t bytesWritten = os_DosWrite(dosBase, handle, src, fileSize);
    if ((uint32_t)bytesWritten != fileSize) {
        os_DosClose(dosBase, handle);
        if (outError) *outError = 1;
        return 0;
    }

    os_DosClose(dosBase, handle);
    if (outError) *outError = 0;
    return 1; /* success */
}

/* ListFiles: lock dir, examine/exnext loop, copy names to buffer */
static uint32_t resload_listfiles(uint32_t buffersize, uint8_t *dirname, uint8_t *buffer, uint32_t *outError)
{
    dirname = make_data_path(dirname);
    void *dosBase = (void *)S_DosBase;
    uint32_t count = 0;
    uint32_t dirLock = 0;
    uint8_t *fib = (uint8_t *)0;

    if ((int32_t)buffersize <= 0) {
        if (outError) *outError = 1;
        return 0;
    }

    /* Lock directory */
    dirLock = os_DosLock(dosBase, dirname, ACCESS_READ);
    if (dirLock == 0) {
        if (outError) *outError = os_DosIoErr(dosBase);
        return 0;
    }

    /* Allocate FileInfoBlock */
    fib = (uint8_t *)os_DosAllocDosObject(dosBase, DOS_FIB, (void *)0);
    if (!fib) {
        os_DosUnLock(dosBase, dirLock);
        if (outError) *outError = 1;
        return 0;
    }

    /* Prime with Examine */
    if (os_DosExamine(dosBase, dirLock, fib) == 0) {
        os_DosFreeDosObject(dosBase, DOS_FIB, fib);
        os_DosUnLock(dosBase, dirLock);
        if (outError) *outError = 1;
        return 0;
    }

    /* Iterate with ExNext */
    uint32_t remaining = buffersize;
    uint8_t *writePtr = buffer;
    count = 0;

    while (os_DosExNext(dosBase, dirLock, fib) != 0) {
        /* Skip directories: fib_DirEntryType at offset 4, >0 = directory */
        int32_t entryType = *(int32_t *)(fib + 4);
        if (entryType > 0) continue;

        /* Get filename from FIB (at offset 8) */
        volatile uint8_t *fname = (volatile uint8_t *)(fib + 8);
        uint32_t nameLen = 0;
        while (fname[nameLen]) nameLen++;

        /* Need nameLen + 1 bytes (name + null) */
        uint32_t needed = nameLen + 1;
        if (needed > remaining) break; /* no more room */

        /* Copy name + null */
        uint32_t i;
        for (i = 0; i < needed; i++)
            *writePtr++ = (uint8_t)fname[i];

        remaining -= needed;
        count++;
    }

    os_DosFreeDosObject(dosBase, DOS_FIB, fib);
    os_DosUnLock(dosBase, dirLock);
    if (outError) *outError = 0;
    return count;
}


/* ================================================================
 * File I/O wrappers (swap interrupt vectors around DOS calls)
 * ================================================================ */

static void file_GetFileSize(uint8_t *name)
{
    swap_int2_to_os();
    uint32_t sz = resload_getfilesize(name);
    swap_int2_to_game();
    /* d0 = sz, d1 = error — not used by callers, size stored by caller */
    size = sz;
}

static void file_LoadFile(uint8_t *name, uint8_t *dest)
{
    swap_int2_to_os();
    uint32_t len = resload_loadfile(name, dest, (uint32_t *)0);
    swap_int2_to_game();
    size = len;
}

static void file_SaveFile(uint32_t fileSize, uint8_t *name, uint8_t *src)
{
    swap_int2_to_os();
    resload_savefile(fileSize, name, src, (uint32_t *)0);
    swap_int2_to_game();
    size = fileSize;
}

static void file_Deletefile(uint8_t *name)
{
    swap_int2_to_os();
    resload_deletefile(name, (uint32_t *)0);
    swap_int2_to_game();
}

static void file_ListFiles(uint32_t buffersize, uint8_t *dirname, uint8_t *buffer)
{
    swap_int2_to_os();
    uint32_t count = resload_listfiles(buffersize, dirname, buffer, (uint32_t *)0);
    swap_int2_to_game();
    /* Store count as word in high 16 bits of size (big-endian) — matching asm behavior */
    *(uint16_t *)&size = (uint16_t)count;
}


/* ================================================================
 * FixFilename — detect save file extensions, prepend "save/" path
 * ================================================================ */
static uint8_t *fix_filename(uint8_t *name)
{
    uint8_t *a0 = name;

    /* Skip "DF0:" prefix if present */
    if (a0[3] == ':')
        a0 += 4;

    /* Find end of string */
    uint8_t *end = a0;
    while (*end) end++;

    /* Need at least 4 chars for extension check */
    if (end - a0 < 4)
        return a0;

    /* Read last 4 chars as big-endian uint32_t */
    uint32_t ext = ((uint32_t)end[-4] << 24) | ((uint32_t)end[-3] << 16) |
                   ((uint32_t)end[-2] << 8) | end[-1];

    /* Check against extension table */
    const uint32_t *ep;
    for (ep = ext_table; *ep; ep++) {
        if (*ep == ext) {
            /* Save extension matched — copy filename after "save/" prefix */
            uint8_t *src = a0;
            uint8_t *dst = save_path_buf + 5; /* after "save/" */
            while ((*dst++ = *src++) != 0)
                ;
            return save_path_buf;
        }
    }

    return a0; /* no match, return (possibly DF0:-stripped) name */
}


/* ================================================================
 * rob_* command handlers (called by RobNorthenLoader dispatch)
 * ================================================================ */

static uint32_t rob_load(uint8_t *name, uint8_t *dest)
{
    name = fix_filename(name);
    file_LoadFile(name, dest);
    _rnl_d1_return = size;
    return 0;
}

static uint32_t rob_save(uint8_t *name, uint8_t *src, uint32_t fileSize)
{
    name = fix_filename(name);
    file_SaveFile(fileSize, name, src);
    _rnl_d1_return = size;
    return 0;
}

static uint32_t rob_delete(uint8_t *name)
{
    name = fix_filename(name);
    file_Deletefile(name);
    _rnl_d1_return = 0;
    return 0;
}

static uint32_t rob_format_disk(void)
{
    _rnl_d1_return = 0;
    return 0;
}

static uint32_t rob_get_file_size(uint8_t *name)
{
    name = fix_filename(name);
    file_GetFileSize(name);
    /* d0 = file size (non-zero = success), d1 = file size */
    _rnl_d1_return = size;
    return size; /* DO NOT clear d0 — AssetSys needs d0!=0 for success */
}

static uint32_t rob_list_directory(uint8_t *name, uint8_t *callerBuf)
{
    /* Skip drive prefix (colon) from name */
    uint32_t i = 0;
    while (name[i]) {
        if (name[i] == ':') {
            name += i + 1;
            break;
        }
        i++;
    }

    /* Always use "save" directory */
    uint8_t *dirname = (uint8_t *)savedir;

    /* List files into our scratch buffer */
    file_ListFiles(2048, dirname, listdir_buf);

    /* Get count from high word of size (big-endian) */
    uint16_t count = *(uint16_t *)&size;
    if (count == 0) {
        _rnl_d1_return = (uint32_t)count;
        return 0;
    }

    /* Copy names from listdir_buf to caller's buffer in rob format */
    uint8_t *src = listdir_buf;
    uint8_t *dst = callerBuf;
    uint16_t n;
    for (n = 0; n < count; n++) {
        /* Copy filename to entry at offset +2, measure length */
        uint32_t nameLen = 0;
        while (*src) {
            dst[2 + nameLen] = *src++;
            nameLen++;
        }
        dst[2 + nameLen] = 0; /* null terminate */
        src++; /* skip source null */

        /* Set name length at offset +1 */
        dst[1] = (uint8_t)nameLen;
        /* Set marker byte at offset +0 */
        dst[0] = 0xFD;
        /* Advance to next 32-byte entry */
        dst += 0x20;
    }

    /* Return: d0=0, d1=count (as word from size) */
    _rnl_d1_return = (uint32_t)count;
    return 0;
}


/* ================================================================
 * RobNorthenLoader — C dispatcher (called via asm bridge)
 * ================================================================ */
__attribute__((used, externally_visible))
uint32_t _RobNorthenLoader_C(uint32_t cmd, uint32_t arg1, uint8_t *name, uint8_t *buf)
{
    switch (cmd) {
    case 0: return rob_load(name, buf);
    case 1: return rob_save(name, buf, arg1);
    case 2: return rob_delete(name);
    case 3: return rob_list_directory(name, buf);
    case 4: return rob_format_disk();
    case 5: return rob_get_file_size(name);
    default:
        _rnl_d1_return = 0;
        return 0;
    }
}


/* ================================================================
 * Hardware helper functions
 * ================================================================ */

__attribute__((used, externally_visible))
void _FixSnoopBug(void)
{
    /* BPLCON0 is at offset 0x100, not in struct Custom — use absolute address */
    *(volatile uint16_t *)(CUSTOM_BASE + BPLCON0_OFF) = 0x0200u;
}

__attribute__((used, externally_visible))
void _FixMemoryRoutine(void)
{
    /* no-op */
}

/* KBHook: read keyboard scan code, check for quit key */
__attribute__((used, externally_visible))
uint8_t _KBHook(void)
{
    uint8_t raw = *(volatile uint8_t *)CIAA_SDR_ADDR;
    uint8_t keycode = ~raw;

    /* ROR.B #1 equivalent */
    keycode = (keycode >> 1) | (keycode << 7);

    if (keycode == _keyexit) {
        g_quitToDos = 1;
    }

    return raw;
}
