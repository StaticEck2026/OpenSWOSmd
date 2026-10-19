/*
 * dos2amiga.h -- rebuild the Amiga game's 320x256 sprite banks from the PC
 * game's sprites (SwosGfx -dos2amiga).
 *
 * The Amiga game cuts its sprites out of 320x256 banks (CHARSET.RAW,
 * CJCGRAFS.RAW, ...) by running "image command scripts": a load opcode names
 * the bank, every draw opcode after it says which sprite slot is cut from
 * which (x, y) of the bank, and how big. Run the same scripts the other way --
 * for every draw, put the PC game's version of that sprite where the Amiga
 * game will cut it from -- and you have banks with the Amiga layout and the PC
 * pixels: the 8-bit banks OpenSWOS's AGA tier (bin/grafs_aga) is built from.
 *
 * The scripts are the game's own (src/intro_menu_image_script_cmds.inc and
 * friends, compiled in here), word for word the arrays SwosGfx interprets.
 *
 * Which PC sprite fills an Amiga slot:
 *   - gameplay banks (menuColorAndTextScript): PC index = Amiga index + 114,
 *     except the kit banks, whose slot 227+n is sprite n of TEAMn.DAT;
 *   - CHARSET, MENUS, MENUS2: PC index = Amiga index, slots >= 227 (the font
 *     extension) have no PC sprite and are left empty -- Sd_FontExtend draws them.
 * SOCCER_S (the big R/S sprites) is drawn in the game palette's 0x70..0x7F.
 */
#ifndef SWOSDATA_DOS2AMIGA_H
#define SWOSDATA_DOS2AMIGA_H

#include <stdint.h>

#include "dos_sprite.h"
#include "sd_image.h"

#define SD_BANK_W 320
#define SD_BANK_H 256

typedef enum {
    SD_BANK_CJCGRAFS, SD_BANK_CJCTEAM1, SD_BANK_CJCTEAM2, SD_BANK_CJCTEAM3,
    SD_BANK_CJCTEAMG, SD_BANK_CJCBITS, SD_BANK_SOCCER_S, SD_BANK_CJCBENCH,
    SD_BANK_CHARSET, SD_BANK_MENUS, SD_BANK_MENUS2,
    SD_BANK_COUNT
} SdBank;

/* "CJCGRAFS", ... -- the bank's file name without ".RAW". */
extern const char *const Sd_BankNames[SD_BANK_COUNT];

/* ---- the image-command scripts and their interpreter ------------------ */

typedef enum {
    SD_SCRIPT_INTRO,            /* introMenuImageScript: CHARSET + font extension */
    SD_SCRIPT_MENU_BG,          /* menuBGImageCmdStream: MENUS, MENUS2 */
    SD_SCRIPT_MENU_COLOR_TEXT,  /* menuColorAndTextScript: the gameplay banks */
    SD_SCRIPT_COUNT
} SdScript;

/* The script's words (bank-load pointer words are 0). */
const uint16_t *Sd_ScriptWords(SdScript script, int32_t *count);

/* Where the interpreter says a sprite slot is cut from. */
typedef struct {
    int32_t amigaIndex;         /* sprite slot, opcode & 0x0FFF */
    int32_t x0, y0;             /* top left in the bank (x rounded down to 16) */
    int32_t winW, winH;         /* the cut window: width rounded up to 16 */
} SdPlacement;

/* Every draw after one load opcode, in script order. */
typedef struct {
    int32_t      loadOffset;    /* word index of the 0x7000/0x8000 opcode */
    int32_t      count, cap;
    SdPlacement *items;
} SdPlacementList;

typedef struct {
    int32_t         numLoads;
    SdPlacementList loads[16];
} SdScriptPlacements;

/* Walk a script (SwosGfx DosToAmiga.Interpret, after the game's
 * ProcessImageCommandStream). 0 on success; -1 on an unknown opcode. */
int32_t Sd_ScriptInterpret(const uint16_t *words, int32_t count, SdScriptPlacements *out, char *err);
void    Sd_ScriptPlacementsFree(SdScriptPlacements *p);
/* The draws after the load opcode at word `loadOffset`, or NULL. */
const SdPlacementList *Sd_ScriptFindLoad(const SdScriptPlacements *p, int32_t loadOffset);

/* How each bank is built from the scripts. */
typedef enum { SD_MAP_GAMEPLAY_PLUS114, SD_MAP_MENU_IDENTITY, SD_MAP_TEAM } SdBankMap;

typedef struct {
    SdScript     script;
    int32_t      loadOffset;    /* word index of the bank's load opcode */
    SdBankMap    map;
    int32_t      orMask;        /* OR'd into every pixel drawn (0x70 for SOCCER_S) */
    int32_t      gamePalette;   /* 1: Sd_DosSpritePalGame, 0: Sd_DosSpritePalMenu */
    const char  *teamFile;      /* SD_MAP_TEAM: the container to draw from */
} SdBankInfo;

extern const SdBankInfo Sd_BankInfo[SD_BANK_COUNT];

/* Draw a decoded sprite (spr x sprH, 0 = transparent) into a bank at a
 * placement, clipped to its window and the bank, each pixel OR orMask. */
void Sd_BankBlit(SdImage *bank, const uint8_t *px, int32_t sprW, int32_t sprH,
                 const SdPlacement *p, int32_t orMask);

/* Decode a PC sprite and Sd_BankBlit it. 0 on success, -1 when the sprite's
 * record is broken. A NULL or empty sprite draws nothing. */
int32_t Sd_BankBlitDosSprite(SdImage *bank, const SdDosSprite *spr, const SdPlacement *p, int32_t orMask);

/* Build all eleven banks from the PC game in `dosDir`: each SdImage is set up
 * here (320x256, the PC palette it was drawn in) and freed by the caller with
 * SdImage_Free. CHARSET is returned WITHOUT the font extension (Sd_FontExtend).
 * 0 on success; on failure nothing is left allocated and `err` says why. */
int32_t Sd_DosToAmigaBanks(const char *dosDir, SdImage banks[SD_BANK_COUNT], char *err);

#endif
