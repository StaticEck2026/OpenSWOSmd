/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "asset_loader.h"
#include "display.h"

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint32_t assetLoaderWorkPtr;
extern void _WaitForVSync(void);   /* game_utils.c — pumps audio + events on SDL */
extern uint32_t assetLoaderCacheFlag;
extern volatile uint16_t panicReasonCode;
extern uint16_t assetLoaderAltPathReady;
extern uint32_t loaderMemTop;
extern uint32_t loaderBankBitmap;
extern uint8_t surnameBuf[];
extern uint8_t assetLoaderMainRegion[];
extern uint8_t assetLoaderBank0Base[];
extern uint16_t assetCurrentIndex;
extern volatile uint16_t currentGameTick;

/* ---- External asm functions (C-callable bridges in swos2_unit.s) ---- */
extern void _PanicHaltLoop(void);
extern int16_t _CAssetSys_Dispatch(uint16_t opcode, uint16_t param, void *name, void *dest);
extern int16_t _CAssetSys_DispatchGetSize(uint16_t opcode, uint16_t param, void *name, void *dest, uint32_t *outFileSize);
extern void _RncDecompressV1(uint8_t *src, uint8_t *dest);
extern int16_t _LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);

/* ---- Constants matching asm EQU ---- */
#define ASSET_OP_SELECT_AND_OPEN    14
#define ASSET_OP_INIT_SOURCE0       2
#define ASSET_OP_5_THEN_MAYBE_6     12
#define ASSET_OP_OPEN_ANY_SOURCE    16
#define FPS_50HZ                    49
#include "rnc_magic.h"

/* ---- Cache node layout constants ----
 * offset 0:  next pointer (uint32_t)
 * offset 4:  isFree flag (uint16_t) — 0 = in-use, 1 = free
 * offset 6:  blockSize (uint32_t) — allocated block size (even-aligned)
 * offset 10: dataSize (uint32_t) — actual data size stored
 * offset 14: name (char[34]) — null-terminated asset name
 * offset 48: payload data begins
 */
#define CACHE_NODE_HEADER_SIZE  48

/* ---- Forward declarations ---- */
extern void _InstallAndInitLoaderBootstrap(void);

/* ================================================================
 * assetCacheAllocInternal  (SWOS2.s line 9831, AssetCacheAlloc)
 *
 * Allocates a cache node from the free list.
 * Walks the linked list looking for a free node with enough space.
 * Splits the node if remaining space can hold another node + header.
 *
 * Returns pointer to allocated node, or NULL on failure.
 * The node's isFree flag is set to 0 (in-use), blockSize and dataSize
 * are written. assetLoaderWorkPtr is decremented.
 * ================================================================ */
static uint8_t *assetCacheAllocInternal(uint32_t size)
{
    uint8_t *node;
    uint32_t alignedSize;
    int32_t remaining;
    uint8_t *splitNode;

    /* Align size up to even */
    alignedSize = (size + 1u) & 0xFFFFFFFEu;

    node = (uint8_t *)assetLoaderCacheFlag;
    if (node == (uint8_t *)0)
        return (uint8_t *)0;

    for (;;)
    {
        /* Only consider free nodes */
        if (*(uint16_t *)(node + 4) != 0)
        {
            /* Check if block is large enough */
            if (*(uint32_t *)(node + 6) >= alignedSize)
            {
                /* Found a suitable block — claim it */
                remaining = (int32_t)(*(uint32_t *)(node + 6) - alignedSize - CACHE_NODE_HEADER_SIZE);

                *(uint32_t *)(node + 6) = alignedSize;  /* blockSize = aligned size */
                *(uint32_t *)(node + 10) = size;         /* dataSize = original size */
                *(uint16_t *)(node + 4) = 0;             /* mark in-use */

                /* Split: create a new free node from the remainder */
                if (remaining >= 0)
                {
                    splitNode = node + CACHE_NODE_HEADER_SIZE + alignedSize;
                    *(uint32_t *)splitNode = *(uint32_t *)node;   /* new->next = old->next */
                    *(uint32_t *)node = (uint32_t)splitNode;      /* old->next = new */
                    *(uint32_t *)(splitNode + 6) = (uint32_t)remaining;  /* new->blockSize */
                    *(uint16_t *)(splitNode + 4) = 1;             /* new->isFree = 1 */
                }

                assetLoaderWorkPtr -= alignedSize;
                return node;
            }
        }

        /* Advance to next node */
        if (*(uint32_t *)node == 0)
            return (uint8_t *)0;

        node = *(uint8_t **)node;
    }
}

/* ================================================================
 * cacheNodeFindByName  (internal helper)
 *
 * Searches the cache linked list for an in-use node matching name.
 * Returns pointer to the node (not payload), or NULL if not found.
 * On success, *outDataSize is set to the node's dataSize.
 * ================================================================ */
static uint8_t *cacheNodeFindByName(const uint8_t *name, uint32_t *outDataSize)
{
    uint8_t *node;
    const uint8_t *s1;
    const uint8_t *s2;

    node = (uint8_t *)assetLoaderCacheFlag;
    if (node == (uint8_t *)0)
        return (uint8_t *)0;

    for (;;)
    {
        /* Only check in-use nodes (isFree == 0) */
        if (*(uint16_t *)(node + 4) == 0)
        {
            /* Compare name at offset 14 */
            s1 = name;
            s2 = node + 14;

            for (;;)
            {
                if (*s1 != *s2)
                    break;
                if (*s1 == 0)
                {
                    /* Match found */
                    *outDataSize = *(uint32_t *)(node + 10);
                    return node;
                }
                s1++;
                s2++;
            }
        }

        /* Advance to next node */
        if (*(uint32_t *)node == 0)
            return (uint8_t *)0;

        node = *(uint8_t **)node;
    }
}

/* ================================================================
 * AssetCacheFindByName  (SWOS2.s line 9878)
 *
 * Searches the asset cache for a named entry.
 * Returns the data size on success, or -1 (0xFFFFFFFF) if not found.
 * ================================================================ */
__attribute__((used, externally_visible))
int32_t _AssetCacheFindByName(const uint8_t *name)
{
    uint32_t dataSize;
    uint8_t *node = cacheNodeFindByName(name, &dataSize);
    if (node == (uint8_t *)0)
        return (int32_t)-1;
    return (int32_t)dataSize;
}

/* ================================================================
 * AssetCacheCopyByName  (SWOS2.s line 9910)
 *
 * Finds a cached asset by name and copies its payload to dest.
 * Returns 0 on success (Z flag set), -1 on not found (Z flag clear).
 * If outFileSize is non-NULL, stores the data size there.
 *
 * The asm version uses word-copy for aligned data <= 128KB, falling
 * back to byte-copy otherwise. We use byte-copy for simplicity.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
int16_t _AssetCacheCopyByName(uint8_t *name, uint8_t *dest, uint32_t *outFileSize)
{
    uint32_t dataSize;
    uint8_t *node;
    uint8_t *src;
    uint32_t count;

    node = cacheNodeFindByName(name, &dataSize);
    if (node == (uint8_t *)0)
        return -1;

    if (outFileSize)
        *outFileSize = dataSize;

    src = node + CACHE_NODE_HEADER_SIZE;
    count = dataSize;

    /* Copy payload to destination */
    while (count != 0)
    {
        *dest++ = *src++;
        count--;
    }

    return 0;
}

/* ================================================================
 * AssetCacheLoadFromAssetSys  (SWOS2.s line 9947)
 *
 * Loads an asset from the asset system into a new cache node.
 * 1. Queries file size via ASSET_OP_5_THEN_MAYBE_6
 * 2. Allocates a cache node for the file
 * 3. Loads file data into node payload via ASSET_OP_INIT_SOURCE0
 * 4. Copies asset name into node
 *
 * Returns: 0 = success, 1 = alloc failed, 0xFF = asset system error
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
int16_t _AssetCacheLoadFromAssetSys(const uint8_t *name)
{
    uint32_t fileSize;
    int16_t result;
    uint8_t *node;
    uint8_t *payload;
    uint8_t *nameField;
    const uint8_t *srcName;

    /* Query file size */
    result = _CAssetSys_DispatchGetSize(ASSET_OP_5_THEN_MAYBE_6, 0, (void *)name, (void *)0, &fileSize);
    if (result != 0) {
#ifdef SWOS_SDL_BUILD
        extern int fprintf(void *, const char *, ...);
        extern void *stderr;
        fprintf(stderr, "[cacheload] %s: size-query failed r=%d\n", (const char *)name, result);
#endif
        return (int16_t)0xFF;
    }

    /* Allocate cache node */
    node = assetCacheAllocInternal(fileSize);
    if (node == (uint8_t *)0) {
#ifdef SWOS_SDL_BUILD
        extern int fprintf(void *, const char *, ...);
        extern void *stderr;
        fprintf(stderr, "[cacheload] %s: alloc failed for %u bytes\n", (const char *)name, fileSize);
#endif
        return 1;
    }

    /* Load file data into node payload */
    payload = node + CACHE_NODE_HEADER_SIZE;
    result = _CAssetSys_Dispatch(ASSET_OP_INIT_SOURCE0, 0, (void *)name, payload);
    if (result != 0) {
#ifdef SWOS_SDL_BUILD
        extern int fprintf(void *, const char *, ...);
        extern void *stderr;
        fprintf(stderr, "[cacheload] %s: load failed r=%d\n", (const char *)name, result);
#endif
        return (int16_t)0xFF;
    }

    /* Copy name into node (offset 14) */
    nameField = node + 14;
    srcName = name;
    do
    {
        *nameField++ = *srcName;
    } while (*srcName++ != 0);

    return 0;
}

/* ================================================================
 * AssetCacheStoreFromMemory  (SWOS2.s line 9980)
 *
 * Stores a memory buffer into a new cache node.
 * 1. Allocates a cache node for the data
 * 2. Copies payload from src to node
 * 3. Copies asset name into node
 *
 * Silently returns if allocation fails.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _AssetCacheStoreFromMemory(uint8_t *name, uint8_t *src, uint32_t size)
{
    uint8_t *node;
    uint8_t *payload;
    uint8_t *nameField;
    uint32_t count;

    node = assetCacheAllocInternal(size);
    if (node == (uint8_t *)0)
        return;

    /* Copy payload data */
    payload = node + CACHE_NODE_HEADER_SIZE;
    count = size;
    while (count != 0)
    {
        *payload++ = *src++;
        count--;
    }

    /* Copy name into node (offset 14) */
    nameField = node + 14;
    do
    {
        *nameField++ = *name;
    } while (*name++ != 0);
}

/* ================================================================
 * AssetLoaderRegisterRegion  (SWOS2.s line 10261)
 *
 * Registers a memory region in the asset cache linked list.
 * - size: total byte size of region (must be even)
 * - region: pointer to region base
 *
 * Cache node layout (48-byte header):
 *   offset 0: next pointer (uint32_t)
 *   offset 4: flags/status (uint16_t) — set to 1 for "available"
 *   offset 6: usable size (uint32_t)
 *   offset 10+: data length / payload / name
 * ================================================================ */
__attribute__((used, externally_visible))
void _AssetLoaderRegisterRegion(uint32_t size, uint8_t *region)
{
    uint8_t **cacheHead;
    uint8_t *node;

    /* Panic if size is odd */
    if (size & 1u)
    {
        panicReasonCode = 0x2332u;
        _PanicHaltLoop();
        /* never returns */
    }

    /* Subtract 48-byte node header */
    if (size <= 48u)
        return;
    size -= 48u;

    /* Accumulate total available space */
    assetLoaderWorkPtr += size;

    /* Walk linked list to find tail */
    node = (uint8_t *)assetLoaderCacheFlag;
    if (node == (uint8_t *)0)
    {
        /* First node — set cache head */
        assetLoaderCacheFlag = (uint32_t)region;
    }
    else
    {
        /* Walk to end of list */
        while (*(uint32_t *)node != 0)
        {
            node = *(uint8_t **)node;
        }
        /* Append new node */
        *(uint32_t *)node = (uint32_t)region;
    }

    /* Initialize node header */
    *(uint32_t *)region = 0;                  /* next pointer = NULL */
    *(uint32_t *)(region + 6) = size;         /* usable size */
    *(uint16_t *)(region + 4) = 1;            /* flags = available */
}

/* ================================================================
 * AssetLoaderInitAltPathAndBanks  (SWOS2.s line 10225)
 *
 * Registers memory regions for the asset cache.
 * Original game used post-BSS free memory + bank scanner.
 * Port: allocates cache memory at runtime via AllocMem since
 * BSS is hunk-allocated with no guaranteed free space after it.
 * ================================================================ */
/* Pre-allocated cache memory (set in _start before hardware takeover) */
extern uint8_t *g_earlyAllocCacheMem;
extern uint32_t g_earlyAllocCacheSize;

__attribute__((used, externally_visible))
void _AssetLoaderInitAltPathAndBanks(void)
{
    assetLoaderAltPathReady = 1;
    assetLoaderCacheFlag = 0;

    /* Register surnameBuf as placeholder (size 0 → will be skipped inside) */
    _AssetLoaderRegisterRegion(0, surnameBuf);

    /* Use cache memory that was pre-allocated in _start (before hardware takeover) */
    if (g_earlyAllocCacheMem != (uint8_t *)0 && g_earlyAllocCacheSize > 0)
    {
        _AssetLoaderRegisterRegion(g_earlyAllocCacheSize, g_earlyAllocCacheMem);
    }
}

/* ---- Precache string tables (duplicated from asm data) ---- */

static const uint8_t * const kCorePrecacheTable[] = {
    (const uint8_t *)"sound/sfx.in2",
    (const uint8_t *)GRAFS "swcpich2.map",
    (const uint8_t *)0  /* sentinel */
};

static const uint8_t * const kMenuPrecacheTable[] = {
    (const uint8_t *)GRAFS "menus.raw",
    (const uint8_t *)GRAFS "menus2.raw",
    (const uint8_t *)GRAFS "cjcgrafs.raw",
    (const uint8_t *)GRAFS "cjcbench.raw",
    (const uint8_t *)GRAFS "cjcbits.raw",
    (const uint8_t *)GRAFS "cjcteam1.raw",
    (const uint8_t *)GRAFS "cjcteam2.raw",
    (const uint8_t *)GRAFS "cjcteam3.raw",
    (const uint8_t *)GRAFS "cjcteamg.raw",
    (const uint8_t *)GRAFS "menubg.raw",
    (const uint8_t *)GRAFS "swcpich1.map",
    (const uint8_t *)GRAFS "swcpich3.map",
    (const uint8_t *)GRAFS "swcpich4.map",
    (const uint8_t *)GRAFS "swcpich5.map",
    (const uint8_t *)GRAFS "swcpich6.map",
    (const uint8_t *)"data/poolplyr.dat",
    (const uint8_t *)GRAFS "soccer_s.raw",
    (const uint8_t *)"sound/menu.sng",
    (const uint8_t *)"sound/menu.ins",
    (const uint8_t *)"sound/sfx.sng",
    (const uint8_t *)"sound/sfx.in1",
    (const uint8_t *)"data/team.008",
    (const uint8_t *)0  /* sentinel */
};

/* ================================================================
 * AssetPrecacheOneCore  (SWOS2.s line 21083)
 *
 * Precaches a single asset: if not already cached, loads it
 * from the asset system repeatedly until successful or empty.
 * ================================================================ */
__attribute__((used, externally_visible))
void _AssetPrecacheOneCore(const uint8_t *assetName)
{
    int32_t result;
    int16_t status;

    result = _AssetCacheFindByName(assetName);

    if (result != (int32_t)0xFFFFFFFFu)
        return; /* already cached */

    /* Load until cached or no more data */
    do
    {
        status = _AssetCacheLoadFromAssetSys(assetName);
    } while (status != 0);
}

/* ================================================================
 * AssetPrecacheCore  (SWOS2.s line 21069)
 *
 * Installs loader bootstrap, then precaches all entries in
 * the core precache table (sfx + sprite map).
 * ================================================================ */
__attribute__((used, externally_visible))
void _AssetPrecacheCore(void)
{
    const uint8_t * const *entry;

    _InstallAndInitLoaderBootstrap();

    if (assetLoaderCacheFlag == 0)
        return;

    entry = kCorePrecacheTable;
    while (*entry != (const uint8_t *)0)
    {
        _AssetPrecacheOneCore(*entry);
        entry++;
    }
}

/* ================================================================
 * AssetPrecacheOneMenu  (SWOS2.s line 21115)
 *
 * Same pattern as AssetPrecacheOneCore but for menu assets.
 * ================================================================ */
__attribute__((used, externally_visible))
void _AssetPrecacheOneMenu(const uint8_t *assetName)
{
    int32_t result;
    int16_t status;

    result = _AssetCacheFindByName(assetName);
    if (result != (int32_t)0xFFFFFFFFu)
        return; /* already cached */

    do
    {
        status = _AssetCacheLoadFromAssetSys(assetName);
    } while (status != 0);
}

/* ================================================================
 * AssetPrecacheMenuPack  (SWOS2.s line 21102)
 *
 * Precaches all entries in the menu precache table.
 * ================================================================ */
__attribute__((used, externally_visible))
void _AssetPrecacheMenuPack(void)
{
    const uint8_t * const *entry;

    if (assetLoaderCacheFlag == 0)
        return;

    entry = kMenuPrecacheTable;
    while (*entry != (const uint8_t *)0)
    {
        _AssetPrecacheOneMenu(*entry);
        entry++;
    }


}

/* ================================================================
 * AssetSvcLoaderOp  (SWOS2.s line 10023)
 *
 * Selects and opens asset source 1 via AssetSys_Dispatch.
 * Sets assetCurrentIndex = 1 and returns 0 on success.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _AssetSvcLoaderOp(void)
{
    return _CAssetSys_Dispatch(ASSET_OP_SELECT_AND_OPEN, 1, (void *)0, (void *)0);
}

/* ================================================================
 * AssetSvcLoaderOp2  (SWOS2.s line 10033)
 *
 * Selects and opens asset source 2 via AssetSys_Dispatch.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _AssetSvcLoaderOp2(void)
{
    return _CAssetSys_Dispatch(ASSET_OP_SELECT_AND_OPEN, 2, (void *)0, (void *)0);
}

/* ================================================================
 * AssetSvcLoaderOp3  (SWOS2.s line 10043)
 *
 * Selects and opens asset source 3 via AssetSys_Dispatch.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _AssetSvcLoaderOp3(void)
{
    return _CAssetSys_Dispatch(ASSET_OP_SELECT_AND_OPEN, 3, (void *)0, (void *)0);
}

/* ================================================================
 * PlaySong  (SWOS2.s line 9560)
 *
 * Loads an asset file with caching support and retry on failure.
 * If assetCurrentIndex == 4, dispatches directly and returns.
 * Otherwise tries cache first, then disk load.
 * On success, stores to cache if cache is active and alt path ready.
 * On error, retries after 50-tick delay unless assetCurrentIndex == 4.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _PlaySong(uint8_t *name, uint8_t *dest)
{
    uint32_t fileSize;
    int16_t result;
    int16_t i;
    uint16_t tick;

    for (;;)
    {
        if (assetCurrentIndex == 4)
        {
            return _CAssetSys_Dispatch(ASSET_OP_INIT_SOURCE0, 0, name, dest);
        }

        if (assetLoaderCacheFlag != 0)
        {
            if (_AssetCacheCopyByName(name, dest, (uint32_t *)0) == 0)
                return 0;
        }

        result = _CAssetSys_DispatchGetSize(ASSET_OP_INIT_SOURCE0, 0, name, dest, &fileSize);
        if (result == 0)
        {
            if (assetLoaderCacheFlag != 0 && assetLoaderAltPathReady != 0)
            {
                _AssetCacheStoreFromMemory(name, dest, fileSize);
            }
            return 0;
        }

        /* Error: give up if assetCurrentIndex == 4 */
        if (assetCurrentIndex == 4)
            return result;

        /* Wait 50 ticks before retry */
        for (i = FPS_50HZ; i >= 0; i--) {
            _WaitForVSync();
        }
    }
}

/* ================================================================
 * AssetEnsureReadyWithRetry  (SWOS2.s line 9603)
 *
 * Same as PlaySong but without cache store on success.
 * Loads an asset, tries cache first, retries on failure.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _AssetEnsureReadyWithRetry(uint8_t *name, uint8_t *dest)
{
    int16_t result;
    int16_t i;
    uint16_t tick;

    for (;;)
    {
        if (assetCurrentIndex == 4)
        {
            return _CAssetSys_Dispatch(ASSET_OP_INIT_SOURCE0, 0, name, dest);
        }

        if (assetLoaderCacheFlag != 0)
        {
            if (_AssetCacheCopyByName(name, dest, (uint32_t *)0) == 0)
                return 0;
        }

        result = _CAssetSys_Dispatch(ASSET_OP_INIT_SOURCE0, 0, name, dest);
        if (result == 0)
            return 0;

        /* Error: give up if assetCurrentIndex == 4 */
        if (assetCurrentIndex == 4)
            return result;

        /* Wait 50 ticks before retry */
        for (i = FPS_50HZ; i >= 0; i--) {
            _WaitForVSync();
        }
    }
}

/* ================================================================
 * LoadRNCToBufferNoBlit  (SWOS2.s line 9431)
 *
 * Loads a file via AssetSys_Dispatch and decompresses RNC v1 in-place.
 * On load failure: if assetCurrentIndex != 4, waits 50 ticks then
 * falls back to LoadFile (full file loader with blit).
 * Panics if loaded data is not RNC v1 format.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _LoadRNCToBufferNoBlit(uint8_t *name, uint8_t *dest)
{
    int16_t result;
    int16_t i;
    uint16_t tick;

    result = _CAssetSys_Dispatch(ASSET_OP_INIT_SOURCE0, 0, name, dest);
    if (result != 0)
    {
        /* Load failed */
        if (assetCurrentIndex == 4)
            return result;

        /* Wait 50 ticks then fall back to LoadFile */
        for (i = 49; i >= 0; i--) {
            _WaitForVSync();
        }
        return _LoadFileEx(name, dest, (uint32_t *)0);
    }

    /* Check for RNC v1 magic */
    if (*(uint32_t *)dest != RNC_MAGIC_V1)
    {
        panicReasonCode = 0xE002u;
        _PanicHaltLoop();
    }

    /* Decompress RNC v1 in-place */
    _RncDecompressV1(dest, dest);
    return 0;
}
