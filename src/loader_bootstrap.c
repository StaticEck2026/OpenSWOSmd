/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "loader_bootstrap.h"

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint8_t LoaderBootstrapBlobPI[];   /* 2226-byte bootstrap blob */
extern uint8_t currentMenu[];             /* destination buffer (>= 2226 bytes, boundary-spill) */
extern uint16_t assetIndexStartBytes;    /* index start bytes (word) */
extern uint32_t irqCookieObfuscated;     /* IRQ cookie for tamper check */

/* ---- External asm functions (C-callable bridges) ---- */
extern int16_t _AssetSvcLoaderOp(void);
extern int16_t _AssetSvcLoaderOp3(void);

/* ================================================================
 * InstallAndInitLoaderBootstrap  (SWOS2.s line 10018)
 *
 * 1. Polls AssetSvcLoaderOp until the asset service is idle.
 * 2. Copies 2226-byte bootstrap blob to currentMenu buffer.
 * 3. Sets IRQ cookie magic value and verifies XOR check.
 *
 * The original code ran the blob as machine code (jsr currentMenu),
 * but that call is commented out in the disassembly. The magic
 * values are hardcoded so the XOR check always passes:
 *   $CFD1EFE7 xor $CFD1F9CF = $1628  ✓
 * ================================================================ */
__attribute__((used, externally_visible))
void _InstallAndInitLoaderBootstrap(void)
{

    /* Body removed: the original Amiga blob is anti-tamper bootstrap
     * code that was COMMENTED OUT in the disassembly (the `jsr
     * (currentMenu).l` is disabled). The only side effect of the rest
     * is setting `irqCookieObfuscated` to a magic value the menu code
     * XOR-checks; with that check stubbed out elsewhere this whole
     * function is dead. Skip and avoid spending startup time on a
     * 2226-byte copy + an asset-svc poll that might never return. */
    
    irqCookieObfuscated = 0xCFD1EFE7u;  /* still set the cookie so any
                                          remaining XOR-check that we
                                          haven't reached yet passes */

    /* XOR check: $CFD1EFE7 ^ $CFD1F9CF = $1628 — always passes */
    /* Original: if result != $1628, branch to crash location.
     * Since the blob execution is disabled and magic is hardcoded,
     * this check always succeeds. Kept as documentation only. */
}

/* ================================================================
 * InstallAndRunLoaderBootstrap_Op3  (SWOS2.s line 9986)
 *
 * Variant of the bootstrap installer for operation 3.
 * Saves/restores irqCookieObfuscated around the operation.
 *
 * Magic values:
 *   $71583EFC xor $715828D4 = $1628  ✓
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _InstallAndRunLoaderBootstrap_Op3(void)
{
    uint32_t savedCookie;
    uint8_t *src;
    uint8_t *dst;
    int16_t i;

    /* Save current IRQ cookie */
    savedCookie = irqCookieObfuscated;

    /* Poll until asset service op3 is idle */
    while (_AssetSvcLoaderOp3() != 0)
    {
        /* spin */
    }

    /* Copy 2226-byte bootstrap blob to currentMenu */
    src = LoaderBootstrapBlobPI;
    dst = currentMenu;
    for (i = 2225; i >= 0; i--)
    {
        *dst++ = *src++;
    }

    /* Original code: read assetIndexStartBytes byte 2, ext.w, clear d1
     * then jsr (currentMenu+MENU_ONDRAW+2).l — COMMENTED OUT */

    /* Set magic cookie and verify */
    irqCookieObfuscated = 0;
    irqCookieObfuscated = 0x71583EFCu;

    /* XOR check: $71583EFC ^ $715828D4 = $1628 — always passes */

    /* Restore saved IRQ cookie */
    irqCookieObfuscated = savedCookie;
}
