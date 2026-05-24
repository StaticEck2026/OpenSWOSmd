/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include <stdint.h>
#include "file_io.h"
#include "display.h"
#include "menus.h"
#include "audio.h"
#include "menu_system.h"
#include "string_utils.h"
#include "asset_loader.h"
#include "input_text.h"

#ifdef SWOS_SDL_BUILD
#include <string.h>
#include <stdio.h>
#include "endian_helpers.h"
#endif

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint16_t assetCurrentIndex;
extern uintptr_t assetLoaderCacheFlag;
extern uint16_t assetLoaderAltPathReady;
extern volatile uintptr_t frontbufferBasePtr;
#ifdef SWOS_SDL_BUILD
extern volatile uintptr_t frontbufferBaseAltPtr;
extern uint8_t frontbufferBaseAlt[];
#endif
extern volatile uint16_t currentGameTick;
extern void _WaitForVSync(void);   /* game_utils.c — pumps audio + events on SDL */
extern volatile uint16_t panicReasonCode;
extern uint16_t hilNumGoals;
extern uint16_t hilTeam1Goals;
extern uint16_t hilTeam2Goals;
extern uint16_t hilPitchNumber;
extern uint16_t hilMaxSubstitutes;
extern uint8_t editTacticsCurrentTactics[370]; /* TeamTactics — must match edit_tactics.c */
extern uint8_t hilFileBuffer[];

/* Save file header fields (all within careerFileBuffer/competitionFileBuffer region) */
extern uint16_t saveFileVersion;
extern uint32_t saveFileMagic1;
extern uint32_t saveFileMagic2;
extern uint32_t saveFileMagic3;
extern uint32_t saveFileAmigaType;
extern uint8_t aCj0611143807[];   /* "CJ0611143807" magic string */

/* File buffers and sizes */
extern uint16_t careerFileBuffer;
extern uint8_t competitionFileBuffer[];
extern uint16_t numSelectedTeams;
extern uint16_t numDiyTeams;

/* Error strings (all in SWOS2.s data section) */
extern uint8_t aDiskWriteProte[];
extern uint8_t aNoDiskInDrive[];
extern uint8_t aFileNotFound[];
extern uint8_t aDiskFull[];
extern uint8_t aCannotFormatGa[];
extern uint8_t aDiskError[];
extern uint8_t aCompetitionFil[];
extern uint8_t aCompetitionFil_0[];
extern uint8_t aFileSavedByDif[];
extern uint8_t aFileSavedOnDif[];

/* String constants (in SWOS2.s data section) */
extern uint8_t aContinue[];          /* "CONTINUE" */
extern uint8_t aAbort[];             /* "ABORT" */

/* Insert disk / file dialog globals */
extern uint8_t aInsertSwosDisk_0[];  /* "\x01INSERT SWOS DISK 2 IN ANY DRIVE" */
extern uint8_t aOk[];               /* "OK" */
extern uint8_t aInsertGameSave[];    /* "\x01INSERT GAME SAVE DISK IN DF0:" */
extern uint8_t aInsertBlankDis[];    /* "\x01INSERT BLANK DISK IN DF0:" */
extern uint8_t aFormatSaveDisk[];    /* "FORMAT SAVE DISK" */
extern uint8_t aDeleteFile[];        /* "DELETE FILE" */
extern uint8_t aSwosu[];            /* "SWOSU" */
extern uint8_t aSwos4Dku[];         /* "swos4.dku" */
extern uint16_t saveFileBuffer;
extern uint16_t menuMusic;

/* File selection dialog globals */
extern uint32_t extension;
extern uintptr_t menuTitle;
extern uint8_t inputSaveFilename[];
extern uint16_t numberOfFiles;
extern uint16_t totalNumberOfFiles;
extern uint16_t numMatchedFiles;
extern uint8_t foundFilenamesBuffer[];
extern uint32_t foundFilesPtrTable[];
extern uint32_t extensionsBuffer[];
extern uint16_t savingFile;
extern uint8_t selectFilesMenu[];
extern uint8_t selectedFilename[];
extern uint16_t selFilesColumn1NumItems;
extern uint16_t selFilesColumn2NumItems;
extern uint16_t selFilesColumn3NumItems;
extern uint16_t selFilesColumnStartY;
extern uint16_t selFilesColumnEndY;
extern MenuEntry menuEntries[];

/* ---- External asm functions (C-callable bridges in swos2_unit.s) ---- */
extern int16_t _CAssetSys_DispatchGetSize(uint16_t opcode, uint16_t param, void *name, void *dest, uint32_t *outFileSize);
extern int16_t _CAssetSys_Dispatch(uint16_t opcode, uint16_t param, void *name, void *dest);
extern void _RncDecompressV1(uint8_t *src, uint8_t *dest);
extern void _RncDecompressV2(uint8_t *src, uint8_t *dest);
extern int16_t _CAssetSys_DispatchWrite(uint16_t opcode, void *filename, void *src, uint32_t size);
extern void _PanicHaltLoop(void);

/* ---- External C functions (already ported) ---- */
extern int16_t _AssetCacheCopyByName(uint8_t *name, uint8_t *dest, uint32_t *outFileSize);
extern void _AssetCacheStoreFromMemory(uint8_t *name, uint8_t *src, uint32_t size);
extern void _BlitterCopyOverlayToScreen(void);

/* ---- Constants matching asm EQU ---- */
#define ASSET_OP_INIT_SOURCE0       2
#define ASSET_OP_LOAD_FILE          4
#define ASSET_OP_INIT_SOURCE2       6
#define ASSET_OP_RESOLVE_ASSET_LIST 8
#define ASSET_OP_DO_CRITICAL        10
#define ASSET_OP_OPEN_ANY_SOURCE    16
#define FPS_50HZ                    49
#define FILENAME_ENTRY_SIZE         32
#define TACTICS_SIZE                370
#include "rnc_magic.h"

/* ================================================================
 * LoadFile  (SWOS2.s line 8825)
 *
 * Loads a file into the given buffer, using the frontbuffer as a
 * staging area for decompression and overlay.
 *
 * Flow:
 * 1. If assetCurrentIndex == 4, dispatch directly and return.
 * 2. Otherwise, redirect load into frontbuffer:
 *    a. Try cache first (AssetCacheCopyByName)
 *    b. If cache miss, load from disk (AssetSys_Dispatch)
 *    c. On disk failure, retry after 50-tick delay
 *    d. On success, optionally store to cache
 * 3. Check file size vs frontbuffer capacity (panic if too large)
 * 4. Decode from frontbuffer to real dest:
 *    - RNC v1: decompress via RncDecompressV1
 *    - RNC v2: decompress via RncDecompressV2
 *    - Raw: byte-copy from frontbuffer to dest
 * 5. Restore screen overlay via BlitterCopyOverlayToScreen
 *
 * Asm register convention:
 *   in:  a0 = filename, a1 = dest buffer
 *   out: d0 = 0 (success) or non-zero (error), d1 = file size
 *        Z flag reflects d0
 *
 * outFileSize: if non-NULL, receives the file size (or decompressed
 * size for RNC files). The asm stub uses this to return d1.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
int16_t _LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize)
{
    int16_t result;
    uint32_t fileSize;
    uint8_t *frontbuf;
    uint32_t magic;
    uint32_t count;
    uint8_t *src;
    int16_t i;
    uint16_t tick;

    fileSize = 0;

    for (;;) {
        /* Direct dispatch if assetCurrentIndex == 4 */
        if (assetCurrentIndex == 4) {
            result = _CAssetSys_DispatchGetSize(ASSET_OP_INIT_SOURCE0, 0, filename, dest, &fileSize);
#ifdef SWOS_SDL_BUILD
            /* The original Amiga asset loader stripped RNC headers as
             * part of the disk pipeline, so the early-exit branch
             * received pre-decompressed bytes. Our `rob_load` returns
             * raw bytes (because _LoadRNCToBufferNoBlit also calls
             * the same dispatch and expects to see the magic). So
             * here, after a successful direct dispatch, we sniff the
             * RNC header and decompress in-place if present. */
            if (result == 0 && fileSize >= 18 &&
                dest[0] == 'R' && dest[1] == 'N' && dest[2] == 'C' &&
                (dest[3] == 0x01 || dest[3] == 0x02)) {
                static uint8_t s_rnc_scratch[256 * 1024];
                if (fileSize <= sizeof(s_rnc_scratch)) {
                    /* RNC header offset 4 = unpacked size (big-endian). */
                    uint32_t unpacked = BE_READ_U32(dest + 4);
                    memcpy(s_rnc_scratch, dest, fileSize);
                    if (dest[3] == 0x01) _RncDecompressV1(s_rnc_scratch, dest);
                    else                  _RncDecompressV2(s_rnc_scratch, dest);
                    fileSize = unpacked;
                }
            }
            /* The Amiga `_BlitterCopyOverlayToScreen` would have copied
             * the alt → primary buffer for us in the staging path. The
             * early-exit branch skipped that on Amiga because the asset
             * loader wrote directly to the displayed buffer. On SDL,
             * mirror that by stamping the displayed buffer if `dest`
             * is the alt buffer. */
            if (result == 0 && (uint8_t *)frontbufferBasePtr != dest &&
                ((uintptr_t)dest == (uintptr_t)frontbufferBaseAlt ||
                 (uintptr_t)dest == frontbufferBaseAltPtr)) {
                uint32_t cap = FRONTBUFFER_SIZE_TOTAL_BYTES;
                memcpy((void *)frontbufferBasePtr, dest,
                       fileSize < cap ? fileSize : cap);
            }
#endif
            if (outFileSize)
                *outFileSize = fileSize;
            return result;
        }

        /* Stage load through frontbuffer */
        frontbuf = (uint8_t *)frontbufferBasePtr;

        /* Try cache first */
        if (assetLoaderCacheFlag != 0) {
            if (_AssetCacheCopyByName(filename, frontbuf, &fileSize) == 0)
                goto size_check_and_decode;
        }

        /* Load from disk into frontbuffer */
        result = _CAssetSys_DispatchGetSize(ASSET_OP_INIT_SOURCE0, 0, filename, frontbuf, &fileSize);
        if (result != 0) {
            /* Load failed — retry if not index 4 */
            if (assetCurrentIndex == 4) {
                if (outFileSize)
                    *outFileSize = fileSize;
                return result;
            }

            /* Wait 50 ticks before retry */
            for (i = FPS_50HZ; i >= 0; i--) {
                _WaitForVSync();
            }
            continue; /* retry LoadFile */
        }

        /* On success, store to cache if appropriate */
        if (assetLoaderCacheFlag != 0 && assetLoaderAltPathReady != 0) {
            _AssetCacheStoreFromMemory(filename, frontbuf, fileSize);
        }

    size_check_and_decode:
        /* Check file size vs frontbuffer capacity */
        if (fileSize > FRONTBUFFER_SIZE_TOTAL_BYTES) {
            panicReasonCode = 0xE000u;
            _PanicHaltLoop();
            /* never returns */
        }

        /* Read frontbuffer pointer again */
        frontbuf = (uint8_t *)frontbufferBasePtr;
        magic = *(uint32_t *)frontbuf;

        if (magic == RNC_MAGIC_V1 || magic == RNC_MAGIC_V2) {
            /* RNC header offset 4 = unpacked size (big-endian). A
             * native uint32 load on LE would byte-swap it (e.g.
             * 0x00007AEA becomes 0xEA7A0000 ≈ 3.9 GB) and trip the
             * > 64000 sanity check at LoadTeamFile. */
            fileSize = BE_READ_U32(frontbuf + 4);
            if (magic == RNC_MAGIC_V1)
                _RncDecompressV1(frontbuf, dest);
            else
                _RncDecompressV2(frontbuf, dest);
        } else {
            /* Raw data: byte-copy from frontbuffer to dest */
            src = frontbuf;
            count = fileSize;
            while (count != 0) {
                *dest++ = *src++;
                count--;
            }
        }

        /* Restore screen overlay */
        _BlitterCopyOverlayToScreen();

        if (outFileSize)
            *outFileSize = fileSize;
        return 0;
    }
}

/* ================================================================
 * WriteFile  (SWOS2.s line 8925)
 *
 * Writes data to a file via AssetSys_Dispatch opcode 4.
 * The dispatch function uses d1 = size, which is passed via
 * the _CAssetSys_DispatchWrite bridge.
 *
 * Asm register convention:
 *   in:  a0 = filename, a1 = src buffer, d1 = size
 *   out: d0 = 0 (success) or non-zero (error), Z flag reflects d0
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _WriteFile(uint8_t *filename, uint8_t *src, uint32_t size)
{
    return _CAssetSys_DispatchWrite(ASSET_OP_LOAD_FILE, filename, src, size);
}

/* ================================================================
 * ShowFsErrorByCode  (SWOS2.s line 89147)
 *
 * Maps a filesystem error code to a human-readable error string
 * and shows it in the error menu.
 *
 * Error codes:
 *   0x1C   = DISK WRITE PROTECTED
 *   0x1D   = NO DISK IN DRIVE
 *   0xCD   = FILE NOT FOUND
 *   0xDD   = DISK FULL
 *   0xFF9C = CANNOT FORMAT GAME DISK
 *   other  = DISK ERROR (default)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ShowFsErrorByCode(uint16_t errCode)
{
    uint8_t *msg;

    if (errCode == 0x1C)
        msg = aDiskWriteProte;
    else if (errCode == 0x1D)
        msg = aNoDiskInDrive;
    else if (errCode == 0xCD)
        msg = aFileNotFound;
    else if (errCode == 0xDD)
        msg = aDiskFull;
    else if (errCode == 0xFF9C)
        msg = aCannotFormatGa;
    else
        msg = aDiskError;

    _ShowErrorMenu(msg);
}

/* ================================================================
 * SaveCompetitionFile  (SWOS2.s line 121173)
 *
 * Sets the save file header fields: amiga type, magic bytes,
 * and version number.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SaveCompetitionFile(void)
{
    uint32_t *magic = (uint32_t *)aCj0611143807;

    saveFileAmigaType = 0x100000;
    saveFileMagic1 = magic[0];
    saveFileMagic2 = magic[1];
    saveFileMagic3 = magic[2];
    saveFileVersion = 0x1200;
}

/* ================================================================
 * LoadCompetitionFile  (SWOS2.s line 121183)
 *
 * Validates the save file header: checks version, magic bytes,
 * and amiga type. Shows appropriate error if validation fails.
 *
 * Returns: 0 = valid, 1 = invalid (with error shown)
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _LoadCompetitionFile(void)
{
    uint32_t *magic = (uint32_t *)aCj0611143807;

    if (saveFileVersion == 0x500) {
        _ShowErrorMenu(aCompetitionFil_0); /* "COMPETITION FILE EXPECTS NON-AGA AMIGA" */
        return 1;
    }

    if (saveFileMagic1 != magic[0] ||
        saveFileMagic2 != magic[1] ||
        saveFileMagic3 != magic[2]) {
        _ShowErrorMenu(aFileSavedByDif); /* "FILE SAVED BY DIFFERENT S.W.O.S. VERSION" */
        return 1;
    }

    if (saveFileAmigaType != 0x100000) {
        _ShowErrorMenu(aFileSavedOnDif); /* "FILE SAVED ON DIFFERENT AMIGA MEMORY TYPE" */
        return 1;
    }

    return 0;
}

/* ================================================================
 * LoadCareerFile  (SWOS2.s line 121220)
 *
 * Loads a career file: saves current options, loads file into
 * careerFileBuffer, validates competition header, and cycles music.
 * On any error, restores options and reinitializes tactics.
 *
 * Returns: 0 = success, 1 = error
 * ================================================================ */
extern void _RebuildPointersAfterLoad(void);
extern void _TranslateAmigaAddressesAfterLoad(void);

#include "save_format_json.h"

__attribute__((used, externally_visible))
int16_t _LoadCareerFile(uint8_t *filename)
{
    int16_t result;
    uint32_t fileSize = 0;

    _SaveOptions();

    /* Probe format BEFORE the full load — JSON saves can be several
     * hundred KB (base64 expansion + key text), exceeding the
     * destination buffer that _LoadFileEx would write into. The
     * probe is a 16-byte stdio peek; SaveJson_LoadCareer then
     * re-opens the file via stdio into a heap buffer it manages. */
    if (SaveJson_FileIsJson(filename) == 1) {
        int16_t jr = SaveJson_LoadCareer(filename, NULL, 0);
        if (jr != 0) {
            _ShowErrorMenu(aFileSavedByDif);
            _RestoreOptions();
            _InitUserTactics();
            return 1;
        }
        _CycleMenuMusic();
        return 0;
    }

    result = _LoadFileEx(filename, (uint8_t *)&careerFileBuffer, &fileSize);
    if (result != 0) {
        _ShowFsErrorByCode((uint16_t)result);
        _RestoreOptions();
        _InitUserTactics();
        return 1;
    }

    if (_LoadCompetitionFile() != 0) {
        _RestoreOptions();
        _InitUserTactics();
        return 1;
    }

    /* Pointer-fixup runs in two passes. See SAVE_DATA.md
     * "Cross-build pointer LUT".
     *
     * Pass 1 — Amiga LUT translation. Rewrites every saved pointer
     * field whose value falls in the original-Amiga address range
     * [0x00100000, 0x00200000) to the current build's address of the
     * same symbol. Saves written by the original Amiga binary (or by
     * pre-LUT SDL builds whose addresses happened to overlap) get
     * remapped; saves with current-build addresses (typical SDL→SDL
     * round-trip) pass through untouched.
     *
     * Pass 2 — Field-specific re-derivation. Catches pointers the LUT
     * doesn't know how to translate (cup-record nameSource fallbacks,
     * BSS region anchors that should always point at the current
     * build's per-slot buffers, runtime DIY workspace, etc.). */
    _TranslateAmigaAddressesAfterLoad();
    _RebuildPointersAfterLoad();

    _CycleMenuMusic();
    return 0;
}

/* ================================================================
 * SaveCareerFile  (SWOS2.s line 121244)
 *
 * Saves the career file: sets competition header, computes file
 * size based on numSelectedTeams, writes to disk.
 *
 * File size = (&numSelectedTeams - &careerFileBuffer) +
 *             numSelectedTeams * 684 + 2
 *
 * Returns: 0 = success, 1 = error (after showing error menu)
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _SaveCareerFile(uint8_t *filename)
{
    /* JSON is the only career write format. The legacy raw-memory-dump
     * writer was retired because it embedded build-specific pointer
     * values and required a careerFileBuffer-shaped on-disk layout.
     * The read path still accepts legacy .CAR (see _LoadCareerFile).
     *
     * SaveJson_WriteCareer surfaces _WriteFile errors via
     * _ShowFsErrorByCode internally, so we just propagate its 0/1
     * result here. */
    return SaveJson_WriteCareer(filename);
}

/* ================================================================
 * LoadPresetFile  (SWOS2.s line 121272)
 *
 * Loads a preset/competition file into competitionFileBuffer.
 * Same pattern as LoadCareerFile but uses competitionFileBuffer.
 *
 * Returns: 0 = success, 1 = error
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _LoadPresetFile(uint8_t *filename)
{
    int16_t result;

    _SaveOptions();

    if (SaveJson_FileIsJson(filename) == 1) {
        int16_t jr = SaveJson_LoadPreset(filename, NULL, 0);
        if (jr != 0) {
            _ShowErrorMenu(aFileSavedByDif);
            _RestoreOptions();
            _InitUserTactics();
            return 1;
        }
        _CycleMenuMusic();
        return 0;
    }

    result = _LoadFileEx(filename, (uint8_t *)&competitionFileBuffer, (uint32_t *)0);
    if (result != 0) {
        _ShowFsErrorByCode((uint16_t)result);
        _RestoreOptions();
        _InitUserTactics();
        return 1;
    }

    if (_LoadCompetitionFile() != 0) {
        _RestoreOptions();
        _InitUserTactics();
        return 1;
    }

    _CycleMenuMusic();
    return 0;
}

/* ================================================================
 * SavePresetCompetitionFile  (SWOS2.s line 121296)
 *
 * Saves preset/competition file from competitionFileBuffer.
 * Uses numDiyTeams for size computation.
 *
 * File size = (&numSelectedTeams - &competitionFileBuffer) +
 *             numDiyTeams * 684 + 2
 *
 * Returns: 0 = success, 1 = error
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _SavePresetCompetitionFile(uint8_t *filename)
{
    uint32_t fileSize;
    int16_t result;

#ifdef SWOS_SAVE_FORMAT_JSON
    {
        int16_t jr = SaveJson_WritePreset(filename);
        if (jr == 0) return 0;
    }
#endif

    _SaveCompetitionFile();

    fileSize = (uint32_t)((uint8_t *)&numSelectedTeams - (uint8_t *)&competitionFileBuffer)
             + (uint32_t)numDiyTeams * 684 + 2;

    result = _WriteFile(filename, (uint8_t *)&competitionFileBuffer, fileSize);
    if (result != 0) {
        _ShowFsErrorByCode((uint16_t)result);
        return 1;
    }
    return 0;
}

/* ================================================================
 * LoadDIYFile  (SWOS2.s line 121322)
 *
 * Loads a DIY competition file into competitionFileBuffer.
 * Identical logic to LoadPresetFile.
 *
 * Returns: 0 = success, 1 = error
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _LoadDIYFile(uint8_t *filename)
{
    int16_t result;

    _SaveOptions();

    if (SaveJson_FileIsJson(filename) == 1) {
        int16_t jr = SaveJson_LoadDIY(filename, NULL, 0);
        if (jr != 0) {
            _ShowErrorMenu(aFileSavedByDif);
            _RestoreOptions();
            _InitUserTactics();
            return 1;
        }
        _CycleMenuMusic();
        return 0;
    }

    result = _LoadFileEx(filename, (uint8_t *)&competitionFileBuffer, (uint32_t *)0);
    if (result != 0) {
        _ShowFsErrorByCode((uint16_t)result);
        _RestoreOptions();
        _InitUserTactics();
        return 1;
    }

    if (_LoadCompetitionFile() != 0) {
        _RestoreOptions();
        _InitUserTactics();
        return 1;
    }

    _CycleMenuMusic();
    return 0;
}

/* ================================================================
 * SaveDIYFile  (SWOS2.s line 121344)
 *
 * Saves a DIY competition file from competitionFileBuffer.
 * Identical logic to SavePresetCompetitionFile.
 *
 * Returns: 0 = success, 1 = error
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _SaveDIYFile(uint8_t *filename)
{
    uint32_t fileSize;
    int16_t result;

#ifdef SWOS_SAVE_FORMAT_JSON
    {
        int16_t jr = SaveJson_WriteDIY(filename);
        if (jr == 0) return 0;
    }
#endif

    _SaveCompetitionFile();

    fileSize = (uint32_t)((uint8_t *)&numSelectedTeams - (uint8_t *)&competitionFileBuffer)
             + (uint32_t)numDiyTeams * 684 + 2;

    result = _WriteFile(filename, (uint8_t *)&competitionFileBuffer, fileSize);
    if (result != 0) {
        _ShowFsErrorByCode((uint16_t)result);
        return 1;
    }
    return 0;
}

/* ================================================================
 * FindFilesExtensionPresent  (SWOS2.s line 8501)
 *
 * Checks if files with the given extension exist by dispatching
 * ASSET_OP_RESOLVE_ASSET_LIST through the asset system.
 *
 * Asm callers set a1 = &numberOfFiles before calling. The asset
 * dispatch writes the directory listing there: first word = count,
 * followed by 32-byte filename entries (foundFilenamesBuffer).
 *
 * Asm register convention:
 *   in:  a0 = extension string, a1 = &numberOfFiles (dest buffer)
 *   out: d0 = result from dispatch
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _FindFilesExtensionPresent(uint8_t *ext)
{
    int16_t r = _CAssetSys_Dispatch(ASSET_OP_RESOLVE_ASSET_LIST, 0, ext, (void *)&numberOfFiles);
    return r;
}

/* ================================================================
 * ReadTacticsFile  (SWOS2.s line 121232)
 *
 * Loads a tactics file into frontbuffer, copies 370 bytes to
 * editTacticsCurrentTactics, restores screen overlay.
 *
 * Asm register convention:
 *   in:  a0 = filename
 *   out: d0 = 0 (success) or 1 (error), Z flag reflects d0
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
int16_t _ReadTacticsFile(uint8_t *filename)
{
    int16_t result;
    int16_t i;

    /* JSON path: parse directly from disk into editTacticsCurrentTactics. */
    if (SaveJson_FileIsJson(filename) == 1) {
        int16_t jr = SaveJson_LoadTactics(filename, editTacticsCurrentTactics,
                                          TACTICS_SIZE);
        if (jr != 0) {
            _ShowErrorMenu(aFileSavedByDif);
            return 1;
        }
        return 0;
    }

    result = _LoadFileEx(filename, (uint8_t *)frontbufferBasePtr, (uint32_t *)0);
    if (result != 0) {
        _BlitterCopyOverlayToScreen();
        _ShowFsErrorByCode((uint16_t)result);
        return 1;
    }

    {
        uint8_t *src = (uint8_t *)frontbufferBasePtr;
        uint8_t *dst = editTacticsCurrentTactics;
        for (i = TACTICS_SIZE - 1; i >= 0; i--) {
            *dst++ = *src++;
        }
    }

    _BlitterCopyOverlayToScreen();
    return 0;
}

/* ================================================================
 * WriteTacticsFile  (SWOS2.s line 121262)
 *
 * Writes 370 bytes from buffer to a tactics file.
 *
 * Asm register convention:
 *   in:  a0 = filename, a1 = buffer
 *   out: d0 = 0 (success) or 1 (error), Z flag reflects d0
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _WriteTacticsFile(uint8_t *filename, uint8_t *buffer)
{
    int16_t result;

#ifdef SWOS_SAVE_FORMAT_JSON
    {
        int16_t jr = SaveJson_WriteTactics(filename, buffer, TACTICS_SIZE);
        if (jr == 0)
            return 0;
        /* JSON path failed — fall through to legacy writer so we
         * still produce something on disk. */
    }
#endif

    result = _WriteFile(filename, buffer, TACTICS_SIZE);
    if (result == 0)
        return 0;

    _ShowFsErrorByCode((uint16_t)result);
    return 1;
}

/* ================================================================
 * LoadHilFile  (SWOS2.s line 121273)
 *
 * Loads a highlights file into hilFileBuffer.
 * On error, clears hilNumGoals and shows error.
 *
 * Asm register convention:
 *   in:  a0 = filename
 *   out: d0 = 0 (success) or 1 (error)
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _LoadHilFile(uint8_t *filename)
{
    int16_t result;

    if (SaveJson_FileIsJson(filename) == 1) {
        int16_t jr = SaveJson_LoadHighlights(filename, NULL, 0);
        if (jr != 0) {
            hilNumGoals = 0;
            _ShowErrorMenu(aFileSavedByDif);
            return 1;
        }
        return 0;
    }

    result = _LoadFileEx(filename, hilFileBuffer, (uint32_t *)0);
    if (result == 0) {
        /* Legacy on-disk format stores the 5 header u16s in BE (Amiga
         * m68k). The original loader left them BE in memory and
         * highlights_menu.c followed up with an in-place BE_TO_HOST
         * swap. Doing the swap right here means JSON loads — which
         * write host-native directly — can skip it, and there's a
         * single canonical place for the byte-order normalisation. */
        hilNumGoals       = BE16_TO_HOST(hilNumGoals);
        hilTeam1Goals     = BE16_TO_HOST(hilTeam1Goals);
        hilTeam2Goals     = BE16_TO_HOST(hilTeam2Goals);
        hilPitchNumber    = BE16_TO_HOST(hilPitchNumber);
        hilMaxSubstitutes = BE16_TO_HOST(hilMaxSubstitutes);
        return 0;
    }

    hilNumGoals = 0;
    _ShowFsErrorByCode((uint16_t)result);
    return 1;
}

/* ================================================================
 * DoSaveHighlights  (SWOS2.s line 121287)
 *
 * Saves highlights data. Size = hilNumGoals * 19000 + 2254 (header).
 *
 * Asm register convention:
 *   in:  a0 = filename
 *   out: d0 = 0 (success) or 1 (error)
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _DoSaveHighlights(uint8_t *filename)
{
    uint32_t size;
    int16_t result;

#ifdef SWOS_SAVE_FORMAT_JSON
    /* Dispatch BEFORE the legacy BE swap dance so the JSON writer
     * sees the live host-native u16 header values. */
    {
        int16_t jr = SaveJson_WriteHighlights(filename);
        if (jr == 0) return 0;
    }
#endif

    size = (uint32_t)hilNumGoals * 19000u + 2254u;

    /* HIL file header is big-endian on disk for Amiga compatibility.
     * Swap the 5 uint16 header fields host→BE for the duration of the
     * write, then swap back so in-memory state stays in host order for
     * the rest of the game. No-op on the Amiga BE build. */
    hilNumGoals       = HOST_TO_BE16(hilNumGoals);
    hilTeam1Goals     = HOST_TO_BE16(hilTeam1Goals);
    hilTeam2Goals     = HOST_TO_BE16(hilTeam2Goals);
    hilPitchNumber    = HOST_TO_BE16(hilPitchNumber);
    hilMaxSubstitutes = HOST_TO_BE16(hilMaxSubstitutes);

    result = _WriteFile(filename, hilFileBuffer, size);

    hilNumGoals       = BE16_TO_HOST(hilNumGoals);
    hilTeam1Goals     = BE16_TO_HOST(hilTeam1Goals);
    hilTeam2Goals     = BE16_TO_HOST(hilTeam2Goals);
    hilPitchNumber    = BE16_TO_HOST(hilPitchNumber);
    hilMaxSubstitutes = BE16_TO_HOST(hilMaxSubstitutes);

    if (result == 0)
        return 0;

    _ShowFsErrorByCode((uint16_t)result);
    return 1;
}

/* ================================================================
 * DiskMenuRet  (SWOS2.s line 90480)
 *
 * No-op callback referenced by diskMenu data (onInit/onReturn).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DiskMenuRet(void)
{
}

/* ================================================================
 * ShowInsertDisk2Dialog  (SWOS2.s line 121038)
 *
 * Loops: tries AssetSvcLoaderOp2 to check disk readiness. If not
 * ready, shows a "INSERT SWOS DISK 2" prompt with OK (no abort).
 * Loops until disk is found.
 *
 * Asm register convention:
 *   in:  a0 = header string (title for the prompt)
 *   out: none (returns when disk is ready)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ShowInsertDisk2Dialog(uint8_t *header)
{
    for (;;) {
        if (_AssetSvcLoaderOp2() == 0)
            return; /* disk ready */
        /* Show prompt — no abort button (null), always retry */
        _DoContinueAbortMenu(header, aInsertSwosDisk_0, aOk, (uint8_t *)0, 1);
    }
}

/* ================================================================
 * Asset_PromptInsertSaveDiskDf0  (SWOS2.s line 121053)
 *
 * Prompts user to insert save disk. If user clicks OK, tries to
 * open the save disk source. Retries on disk error, aborts if
 * user clicks ABORT.
 *
 * Asm register convention:
 *   in:  a0 = title string (displayed as prompt header)
 *   out: d0 = 0 (disk opened successfully) or non-zero (user aborted)
 *        Z flag reflects d0
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _Asset_PromptInsertSaveDiskDf0(uint8_t *title)
{
    int16_t result;

    for (;;) {
        result = _DoContinueAbortMenu(title, aInsertGameSave,
                                      aOk, (uint8_t *)aAbort, 0);
        if (result != 0)
            return result; /* user aborted */

        result = _CAssetSys_Dispatch(ASSET_OP_OPEN_ANY_SOURCE, 0,
                                     (void *)0, (void *)0);
        if (result == 0)
            return 0; /* disk opened successfully */
        /* disk error — retry the prompt */
    }
}

/* ================================================================
 * FormatSaveDiskMenu  (SWOS2.s line 90593)
 *
 * Menu callback: prompts to insert blank disk, formats it,
 * writes swos4.dku header, and restores music.
 *
 * Asm register convention:
 *   no params, no return (menu callback)
 * ================================================================ */
__attribute__((used, externally_visible))
void _FormatSaveDiskMenu(void)
{
    int16_t result;

    for (;;) {
        result = _DoContinueAbortMenu((uint8_t *)aFormatSaveDisk,
                                      aInsertBlankDis,
                                      (uint8_t *)aContinue,
                                      (uint8_t *)aAbort, 1);
        if (result != 0)
            return; /* user aborted */

        result = _CAssetSys_Dispatch(ASSET_OP_OPEN_ANY_SOURCE, 0,
                                     (void *)0, (void *)0);
        if (result == 0)
            break; /* disk ready */
        if (result == 0x1D)
            continue; /* no disk — retry prompt */
        _ShowFsErrorByCode((uint16_t)result);
        return;
    }

    /* Disk ready — format (DO_CRITICAL) */
    _StopMusic();
    result = _CAssetSys_Dispatch(ASSET_OP_DO_CRITICAL, 0,
                                 (void *)aSwosu, (void *)0);
    if (menuMusic != 0) {
        _TryPlayMusic();
    }
    if (result != 0) {
        _ShowFsErrorByCode((uint16_t)result);
        return;
    }

    /* Load swos4.dku save file header (2 bytes) */
    result = _CAssetSys_DispatchWrite(ASSET_OP_LOAD_FILE,
                                      (void *)aSwos4Dku,
                                      (void *)&saveFileBuffer, 2);
    if (result != 0) {
        _ShowFsErrorByCode((uint16_t)result);
    }
}

/* ================================================================
 * DeleteFileMenu  (SWOS2.s line 90638)
 *
 * Menu callback: prompts for save disk, shows file list, deletes
 * selected file.
 *
 * Asm register convention:
 *   no params, no return (menu callback)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DeleteFileMenu(void)
{
    int16_t result;

    if (_Asset_PromptInsertSaveDiskDf0(aDeleteFile) != 0)
        return;

    if (_GetFilenameAndExtension(0, aDeleteFile, aDeleteFile) != 0)
        return;

    result = _CAssetSys_Dispatch(ASSET_OP_INIT_SOURCE2, 0,
                                 (void *)0, (void *)0);
    if (result != 0) {
        _ShowFsErrorByCode((uint16_t)result);
    }
}

/* ================================================================
 * ExitDisk  (SWOS2.s line 90654)
 *
 * Menu callback: shows insert disk 2 dialog, then exits menu.
 *
 * Asm register convention:
 *   no params, no return (menu callback)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ExitDisk(void)
{
    _ShowInsertDisk2Dialog(aFormatSaveDisk);
    _SetExitMenuFlag();
}

/* ================================================================
 * SelectFileToSaveDialog  (SWOS2.s line 89141)
 *
 * Shows file selection dialog for saving. Scans file list from
 * asset system, filters by known extensions (.DIY, .PRE, .SEA,
 * .CAR, .TAC, .HIL), and shows menu for user selection.
 *
 * Asm register convention:
 *   in:  d0 = filter extension (uint32_t, e.g. '.DIY' = 0x2E444959)
 *        a0 = default filename string (copied to save dialog input)
 *        a1 = menu title string
 *   out: d0 = 0 (file selected) or 1 (error/no selection)
 *        selectedFilename global contains the chosen filename
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _SelectFileToSaveDialog(uint32_t filterExt, uint8_t *defaultFilename,
                                uint8_t *menuTitleStr)
{
    int16_t result;
    uint16_t count;
    uint8_t *fileEntry;
    uint32_t *ptrTableCursor;
    uint32_t *extTableCursor;
    uint8_t *scan;
    uint32_t ext;
    uint16_t i;

    extension = filterExt;
    menuTitle = (uintptr_t)menuTitleStr;
    _StringCopy(defaultFilename, inputSaveFilename);

    result = _FindFilesExtensionPresent((uint8_t *)0);
    if (result != 0) {
        _ShowFsErrorByCode((uint16_t)result);
        return 1;
    }

    totalNumberOfFiles = 0;
    numMatchedFiles = 0;
    count = numberOfFiles;

    if (count != 0) {
        count--;
        fileEntry = foundFilenamesBuffer;
        ptrTableCursor = foundFilesPtrTable;
        extTableCursor = extensionsBuffer;

        for (i = count; ; i--) {
            /* Find the '.' in the filename */
            scan = fileEntry;
            while (*scan != 0x2E) /* '.' */
                scan++;

            /* Build 4-byte extension from '.' + 3 chars */
            ext = ((uint32_t)scan[0] << 24) |
                  ((uint32_t)scan[1] << 16) |
                  ((uint32_t)scan[2] << 8) |
                  (uint32_t)scan[3];

            /* Check against known extensions */
            if (ext == 0x2E444959u || /* .DIY */
                ext == 0x2E505245u || /* .PRE */
                ext == 0x2E534541u || /* .SEA */
                ext == 0x2E434152u || /* .CAR */
                ext == 0x2E544143u || /* .TAC */
                ext == 0x2E48494Cu)   /* .HIL */
            {
                totalNumberOfFiles++;
                if (ext == extension) {
                    numMatchedFiles++;
                    *ptrTableCursor++ = (uint32_t)fileEntry;
                    *extTableCursor++ = ext;
                }
            }

            fileEntry += FILENAME_ENTRY_SIZE;
            if (i == 0) break;
        }
    }

    savingFile = 1;
    _ShowMenu(selectFilesMenu);

    if (selectedFilename[0] != 0)
        return 0; /* file selected */

    return 1; /* no selection */
}

/* ================================================================
 * GetFilenameAndExtension  (SWOS2.s line 89211)
 *
 * Shows file selection dialog for loading/opening. Similar to
 * SelectFileToSaveDialog but adds ALL matching files (not just
 * the filter extension), and sets savingFile=0.
 *
 * Asm register convention:
 *   in:  d0 = preferred extension (uint32_t, or 0 for "all")
 *        a0 = default filename string
 *        a1 = menu title string
 *   out: d0 = 0 (file selected) or 1 (error/no selection)
 *        selectedFilename global contains the chosen filename
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _GetFilenameAndExtension(uint32_t preferredExt, uint8_t *defaultFilename,
                                 uint8_t *menuTitleStr)
{
    int16_t result;
    uint16_t count;
    uint8_t *fileEntry;
    uint32_t *ptrTableCursor;
    uint32_t *extTableCursor;
    uint8_t *scan;
    uint32_t ext;
    uint16_t i;

    extension = preferredExt;
    menuTitle = (uintptr_t)menuTitleStr;
    _StringCopy(defaultFilename, inputSaveFilename);

    result = _FindFilesExtensionPresent((uint8_t *)0);
    if (result != 0) {
        _ShowFsErrorByCode((uint16_t)result);
        return 1;
    }

    totalNumberOfFiles = 0;
    numMatchedFiles = 0;
    count = numberOfFiles;

    if (count != 0) {
        count--;
        fileEntry = foundFilenamesBuffer;
        ptrTableCursor = foundFilesPtrTable;
        extTableCursor = extensionsBuffer;

        for (i = count; ; i--) {
            /* Find the '.' in the filename */
            scan = fileEntry;
            while (*scan != 0x2E) /* '.' */
                scan++;

            /* Build 4-byte extension from '.' + 3 chars */
            ext = ((uint32_t)scan[0] << 24) |
                  ((uint32_t)scan[1] << 16) |
                  ((uint32_t)scan[2] << 8) |
                  (uint32_t)scan[3];

            /* Check against known extensions */
            if (ext == 0x2E444959u || /* .DIY */
                ext == 0x2E505245u || /* .PRE */
                ext == 0x2E534541u || /* .SEA */
                ext == 0x2E434152u || /* .CAR */
                ext == 0x2E544143u || /* .TAC */
                ext == 0x2E48494Cu)   /* .HIL */
            {
                totalNumberOfFiles++;
                numMatchedFiles++;
                *ptrTableCursor++ = (uint32_t)fileEntry;
                *extTableCursor++ = ext;
            }

            fileEntry += FILENAME_ENTRY_SIZE;
            if (i == 0) break;
        }
    }

    savingFile = 0;
    _ShowMenu(selectFilesMenu);

    if (selectedFilename[0] != 0)
        return 0; /* file selected */

    return 1; /* no selection */
}

/* ---- Chunk 140: File Selection Helpers ---- */

/* Extern: string copy utility */
extern uint8_t *_StringCopy(const uint8_t *src, uint8_t *dst);

/* Copy filename to selectedFilename, extract 3-char extension into packed uint32_t,
   and signal menu exit. Extension is packed as: '.', c1, c2, c3 (big-endian). */
static void _CopyFilenameExtensionAndSetExitMenuFlag(uint8_t *filename) {
    _StringCopy(filename, selectedFilename);
    /* Scan for '.' in the copied filename */
    uint8_t *p = selectedFilename;
    while (*p++ != '.')
        ;
    /* Build packed 4-byte extension: '.', char1, char2, char3 */
    uint32_t ext = '.';
    ext = (ext << 8) | *p++;
    ext = (ext << 8) | *p++;
    ext = (ext << 8) | *p++;
    extension = ext;
    _SetExitMenuFlag();
}

/* Menu callback: select the save filename and exit the menu. */
__attribute__((used, externally_visible))
void _SelectFilenameToSaveAndExit(void) {
    _CopyFilenameExtensionAndSetExitMenuFlag(inputSaveFilename);
}

/* Append the stored extension to filename, copy to selectedFilename, and signal menu exit.
   Called from asm SelectFilesInputSaveFilenameOnSelect when user enters a name.
   `extension` is a packed uint32 in big-endian char order — i.e. for
   '.CAR' the value is 0x2E434152 = ('.' << 24) | ('C' << 16) | ('A' << 8) | 'R',
   so the high byte is the leading '.' and the low byte is the last
   character. Extracting via shifts is endian-safe; reading the four
   raw bytes via `(uint8_t *)&extension` would emit them in reverse on
   an LE host (e.g. "RAC." appended after the user's "TEST"). */
__attribute__((used, externally_visible))
void _AddExtension(uint8_t *filename) {
    uint8_t *p = _StringCopy(filename, selectedFilename);
    *p++ = (uint8_t)(extension >> 24); /* '.' */
    *p++ = (uint8_t)(extension >> 16);
    *p++ = (uint8_t)(extension >> 8);
    *p++ = (uint8_t)(extension);
    *p = 0;
    _SetExitMenuFlag();
}

/* Menu onSelect callback: compute file index from entry ordinal (3-column layout)
   and select that file. */
__attribute__((used, externally_visible))
void _SelFilesOnSelectCommon(MenuEntry *entry) {
    uint16_t idx = entry->ordinal;
    idx -= 1;
    if (idx >= 30) {
        idx -= 15;
        idx += selFilesColumn2NumItems;
    }
    if (idx >= 15) {
        idx -= 15;
        idx += selFilesColumn1NumItems;
    }
    uint8_t *filename = (uint8_t *)foundFilesPtrTable[idx];
    _CopyFilenameExtensionAndSetExitMenuFlag(filename);
}

/* Reposition file list menu entries: find max column height, center vertically,
   and assign Y positions for 3 columns × 15 entries each. */
__attribute__((used, externally_visible))
void _RepositionSelectFileEntries(void) {
    /* Find maximum items across all 3 columns */
    uint16_t maxItems = selFilesColumn1NumItems;
    if (selFilesColumn2NumItems > maxItems)
        maxItems = selFilesColumn2NumItems;
    if (selFilesColumn3NumItems > maxItems)
        maxItems = selFilesColumn3NumItems;

    /* Column start Y: entry[0].y, plus height if entry[0] is visible */
    uint16_t startY = menuEntries[0].y;
    if (!menuEntries[0].isInvisible)
        startY += menuEntries[0].dwHeight;
    selFilesColumnStartY = startY;

    /* Column end Y: from entry[48] if saving, entry[50] otherwise */
    if (savingFile)
        selFilesColumnEndY = menuEntries[48].y;
    else
        selFilesColumnEndY = menuEntries[50].y;

    /* Calculate centered Y start */
    uint16_t totalHeight = maxItems * 9;
    uint16_t availableHeight = selFilesColumnEndY - selFilesColumnStartY;
    uint16_t yOffset = (uint16_t)(availableHeight - totalHeight) >> 1;
    uint16_t colStartY = selFilesColumnStartY + yOffset;

    /* Position entries: 3 columns × 15 entries, starting at menuEntries[1] */
    MenuEntry *e = &menuEntries[1];
    uint16_t col;
    for (col = 0; col < 3; col++) {
        uint16_t y = colStartY;
        uint16_t row;
        for (row = 0; row < 15; row++) {
            e->y = y;
            y += 9;
            e++;
        }
    }
}

/* ---- Chunk 141: SelFilesDrawItemDescription ---- */

__attribute__((used, externally_visible))
void _SelFilesDrawItemDescription(MenuEntry *entry) {
    uint16_t idx = entry->ordinal;
    idx -= 1;
    if (idx >= 30) {
        idx -= 15;
        idx += selFilesColumn2NumItems;
    }
    if (idx >= 15) {
        idx -= 15;
        idx += selFilesColumn1NumItems;
    }

    uint32_t ext = extensionsBuffer[idx];
    MenuEntry *descEntry = &menuEntries[46];

    const uint8_t *descStr;
    if (ext == 0x2E444959)       /* '.DIY' */
        descStr = (const uint8_t *)"DIY";
    else if (ext == 0x2E505245)  /* '.PRE' */
        descStr = (const uint8_t *)"PRESET";
    else if (ext == 0x2E534541)  /* '.SEA' */
        descStr = (const uint8_t *)"SEASON";
    else if (ext == 0x2E434152)  /* '.CAR' */
        descStr = (const uint8_t *)"CAREER";
    else if (ext == 0x2E544143)  /* '.TAC' */
        descStr = (const uint8_t *)"TACT";
    else
        descStr = (const uint8_t *)"HIGH";

    descEntry->fg = (uintptr_t)descStr;
    descEntry->x = entry->x + 0x39;
    descEntry->y = entry->y;
    descEntry->isInvisible = 0;
    _DrawMenuItem(descEntry);
    descEntry->isInvisible = 1;
}

/* ---- Chunk 142: SelectFilesInputSaveFilenameOnSelect ---- */
/* SWOS2.s line 63964 — handle save filename input in file selection dialog */
extern uint16_t allowExtraCharsFlag;

__attribute__((used, externally_visible))
void _SelectFilesInputSaveFilenameOnSelect(MenuEntry *entry)
{
    if (totalNumberOfFiles == 45) {
        _ShowErrorMenu((uint8_t *)"CAN ONLY SAVE MAXIMUM 45 FILES ON DISK");
        return;
    }

    uint8_t *filename = (uint8_t *)entry->fg;
    allowExtraCharsFlag = 1;
    uint16_t result = _InputText(9, (uint8_t *)entry);
    allowExtraCharsFlag = 0;

    if (result != 0)
        return;

    /* Check if string has any non-space characters */
    uint8_t *p = filename;
    while (*p != 0) {
        if (*p != 0x20) {
            _AddExtension(filename);
            return;
        }
        p++;
    }

    /* All spaces or empty — restore default name from inputSaveFilename */
    MenuEntry *entry48 = &menuEntries[48];
    uint8_t *dst = (uint8_t *)entry48->fg;
    _StringCopyUntilChar(inputSaveFilename, dst, '.');
    _CallDrawMenuItem(entry48);
}

/* ================================================================
 * _SelectFilesOnReturn  (SWOS2.s line 57336)
 *
 * Called on return/draw for the file selection menu.
 * Sets up title, save entries, calculates 3-column layout,
 * and assigns filenames to visible menu entries.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SelectFilesOnReturn(void)
{
    /* Set menu title in entry 0 */
    menuEntries[0].fg = menuTitle;

    if (savingFile != 0) {
        /* Show save filename entry (entry 48) */
        menuEntries[48].isInvisible = 0;
        uint8_t *str = (uint8_t *)menuEntries[48].fg;
        *str = 0;
        _StringCopyUntilChar(inputSaveFilename, str, '.');

        /* Show input box (entry 47) */
        menuEntries[47].isInvisible = 0;

        if (inputSaveFilename[0] != 0) {
            /* Show save button (entry 49) */
            menuEntries[49].isInvisible = 0;
        }
    }
    /* note: the "not saving + no files" case still falls through to column calc */

    /* Calculate 3-column layout */
    if (numMatchedFiles <= 8) {
        selFilesColumn1NumItems = 0;
        selFilesColumn2NumItems = numMatchedFiles;
        selFilesColumn3NumItems = 0;
    } else {
        uint16_t perCol = numMatchedFiles / 3;
        uint16_t remainder = numMatchedFiles % 3;
        selFilesColumn1NumItems = perCol;
        selFilesColumn2NumItems = perCol;
        selFilesColumn3NumItems = perCol;
        if (remainder == 1) {
            selFilesColumn2NumItems++;
        } else if (remainder == 2) {
            selFilesColumn1NumItems++;
            selFilesColumn2NumItems++;
        }
    }

    _RepositionSelectFileEntries();
    /* Assign filenames to menu entries */
    uint32_t *filePtr = foundFilesPtrTable;
    MenuEntry *entry = &menuEntries[1]; /* entry at offset 0x38 = index 1 */
    uint16_t d6 = 0;

    for (;;) {
        entry->isInvisible = 1;

        if (d6 >= numberOfFiles)
            goto nextEntry;

        /* Find the right column slot for this file */
        for (;;) {
            uint16_t ordinal = entry->ordinal;
            int16_t idx = (int16_t)(ordinal - 1);
            uint16_t col = (uint16_t)((int32_t)idx / 15);  /* divu.w #$F */
            uint16_t row = (uint16_t)((int32_t)idx % 15);

            uint16_t colMax;
            if (col == 0) {
                colMax = selFilesColumn1NumItems;
            } else if (col == 1) {
                colMax = selFilesColumn2NumItems;
            } else {
                colMax = selFilesColumn3NumItems;
            }

            if (row >= colMax) {
                /* Entry not in visible range — skip to next entry */
                entry++;
                if (entry->ordinal == 0x2E)
                    return;
                entry->isInvisible = 1;
                continue;
            }

            /* Entry is in range — assign filename */
            entry->isInvisible = 0;
            uint8_t *filename = (uint8_t *)*filePtr++;
            uint8_t *dst = (uint8_t *)entry->fg;

            /* Copy filename until '.' */
            while (*filename != 0) {
                *dst++ = *filename++;
                if (*(filename - 1) == '.') {
                    dst--;
                    break;
                }
            }
            *dst = 0;
            break;
        }

    nextEntry:
        entry++;
        if (entry->ordinal == 0x2E)
            return;
        d6++;
    }
}

/* ================================================================
 * _SelectFilesInit  (SWOS2.s line 57279)
 *
 * Called on init for the file selection menu.
 * Sets initial cursor position, sorts files by extension/name,
 * then calls SelectFilesOnReturn to set up display.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SelectFilesInit(void)
{
    /* Set initial menu entry based on mode */
    if (savingFile != 0) {
        if (inputSaveFilename[0] != 0)
            _SetCurrentEntry(49);
        else
            _SetCurrentEntry(48);
    } else {
        if (numMatchedFiles == 0)
            _SetCurrentEntry(50);
    }

    /* Bubble sort files by extension then name */
    int16_t count = (int16_t)numMatchedFiles - 2;
    if (count >= 0) {
        uint16_t swapped;
        do {
            swapped = 0;
            uint32_t *ptrCur = foundFilesPtrTable;
            uint32_t *extCur = extensionsBuffer;

            for (int16_t i = count; i >= 0; i--) {
                uint8_t *name1 = (uint8_t *)ptrCur[0];
                uint32_t ext1 = extCur[0];
                uint32_t ext2 = extCur[1];

                if (ext1 > ext2) {
                    goto doSwap;
                } else if (ext1 == ext2) {
                    /* Extensions equal — compare names */
                    uint8_t *name2 = (uint8_t *)ptrCur[1];
                    uint8_t *p1 = name1;
                    uint8_t *p2 = name2;
                    for (;;) {
                        uint8_t c = *p1++;
                        if (c == 0) break;       /* end of name1 → no swap */
                        uint8_t c2 = *p2++;
                        if (c < c2) break;        /* name1 < name2 → no swap */
                        if (c > c2) goto doSwap;  /* name1 > name2 → swap */
                        /* equal → continue */
                    }
                }
                goto noSwap;

            doSwap:
                /* Swap pointer table entries */
                ptrCur[0] = ptrCur[1];
                ptrCur[1] = (uint32_t)name1;
                /* Swap extension entries */
                extCur[0] = ext2;
                extCur[1] = ext1;
                swapped = 1;

            noSwap:
                ptrCur++;
                extCur++;
            }
        } while (swapped);
    }

    /* Fall through to OnReturn */
    _SelectFilesOnReturn();
}
