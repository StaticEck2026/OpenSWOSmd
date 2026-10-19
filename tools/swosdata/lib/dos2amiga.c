/*
 * dos2amiga.c -- rebuild the Amiga game's sprite banks from the PC game's
 * sprites. A port of SwosGfx DosToAmiga.cs; see dos2amiga.h.
 *
 * The opcodes (the game's src/image_cmd.c, ProcessImageCommandStream). The top
 * nibble is the opcode, the low 12 bits the sprite slot; "byte x/y" packs x in
 * the high byte of one word and y in the low, "w/h" likewise:
 *
 *   7xxx, 8xxx   load a bank: 2 words of file name pointer follow
 *   Cxxx         byte x/y, w/h
 *   1xxx, 5xxx   word x, word y, w/h
 *   Dxxx         byte x/y, w/h, 1 word dst x/y
 *   2xxx, Axxx   word x, word y, w/h, 1 word dst x/y
 *   Exxx, Fxxx   byte x/y, w/h, dst x/y, mask select   (2 words)
 *   3xxx, Bxxx   word x, word y, w/h, dst x/y, mask select
 *   9xxx         draw from the saved pointer: 2 words, no source pixels
 *   FFFF         end
 *
 * A draw cuts from byte ((x >> 3) & ~1) of the line, i.e. x rounded down to a
 * multiple of 16, and a window ((w + 15) / 16) * 16 wide and h tall.
 */
#include "dos2amiga.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the scripts, word for word what the game runs -------------------- */

static const uint16_t kIntroScript[] = {
    0x7000, 0, 0,
#include "intro_menu_image_script_cmds.inc"
#include "font_ext_script.inc"
    0xFFFF
};

static const uint16_t kMenuBgScript[] = {
#include "menu_bg_image_script_cmds.inc"
};

static const uint16_t kMenuColorTextScript[] = {
#include "menu_color_text_script_cmds.inc"
};

const uint16_t *Sd_ScriptWords(SdScript script, int32_t *count)
{
    switch (script) {
    case SD_SCRIPT_INTRO:
        *count = (int32_t)(sizeof kIntroScript / sizeof kIntroScript[0]);
        return kIntroScript;
    case SD_SCRIPT_MENU_BG:
        *count = (int32_t)(sizeof kMenuBgScript / sizeof kMenuBgScript[0]);
        return kMenuBgScript;
    case SD_SCRIPT_MENU_COLOR_TEXT:
        *count = (int32_t)(sizeof kMenuColorTextScript / sizeof kMenuColorTextScript[0]);
        return kMenuColorTextScript;
    default:
        *count = 0;
        return NULL;
    }
}

const char *const Sd_BankNames[SD_BANK_COUNT] = {
    "CJCGRAFS", "CJCTEAM1", "CJCTEAM2", "CJCTEAM3", "CJCTEAMG", "CJCBITS",
    "SOCCER_S", "CJCBENCH", "CHARSET", "MENUS", "MENUS2",
};

/* Load-opcode word offsets, matched to the banks by the game's patch table
 * (swos_globals.c). All three kit banks share the team1-slot layout at 405;
 * the team2-slot block is the same layout for the other side and not a bank. */
const SdBankInfo Sd_BankInfo[SD_BANK_COUNT] = {
    { SD_SCRIPT_MENU_COLOR_TEXT,    0, SD_MAP_GAMEPLAY_PLUS114, 0,    1, NULL },        /* CJCGRAFS */
    { SD_SCRIPT_MENU_COLOR_TEXT,  405, SD_MAP_TEAM,             0,    1, "TEAM1.DAT" }, /* CJCTEAM1 */
    { SD_SCRIPT_MENU_COLOR_TEXT,  405, SD_MAP_TEAM,             0,    1, "TEAM2.DAT" }, /* CJCTEAM2 */
    { SD_SCRIPT_MENU_COLOR_TEXT,  405, SD_MAP_TEAM,             0,    1, "TEAM3.DAT" }, /* CJCTEAM3 */
    { SD_SCRIPT_MENU_COLOR_TEXT, 3418, SD_MAP_GAMEPLAY_PLUS114, 0,    1, NULL },        /* CJCTEAMG */
    { SD_SCRIPT_MENU_COLOR_TEXT, 4547, SD_MAP_GAMEPLAY_PLUS114, 0,    1, NULL },        /* CJCBITS  */
    { SD_SCRIPT_MENU_COLOR_TEXT, 4694, SD_MAP_GAMEPLAY_PLUS114, 0x70, 1, NULL },        /* SOCCER_S */
    { SD_SCRIPT_MENU_COLOR_TEXT, 4889, SD_MAP_GAMEPLAY_PLUS114, 0,    1, NULL },        /* CJCBENCH */
    { SD_SCRIPT_INTRO,              0, SD_MAP_MENU_IDENTITY,    0,    0, NULL },        /* CHARSET  */
    { SD_SCRIPT_MENU_BG,            0, SD_MAP_MENU_IDENTITY,    0,    0, NULL },        /* MENUS    */
    { SD_SCRIPT_MENU_BG,          392, SD_MAP_MENU_IDENTITY,    0,    0, NULL },        /* MENUS2   */
};

#define GAMEPLAY_OFFSET   114   /* PC index = Amiga index + 114 */
#define MENU_SPRITE_LIMIT 227   /* identity-mapped PC sprites exist below this */
#define TEAM_BLOCK_BASE   227   /* first Amiga slot of the team1 kit block */

/* ---- interpreter ------------------------------------------------------- */

static int32_t PushPlacement(SdPlacementList *l, const SdPlacement *p)
{
    if (l->count == l->cap) {
        int32_t cap = l->cap ? l->cap * 2 : 64;
        SdPlacement *n = realloc(l->items, (size_t)cap * sizeof *n);
        if (!n)
            return -1;
        l->items = n;
        l->cap = cap;
    }
    l->items[l->count++] = *p;
    return 0;
}

void Sd_ScriptPlacementsFree(SdScriptPlacements *p)
{
    for (int32_t i = 0; i < p->numLoads; i++)
        free(p->loads[i].items);
    memset(p, 0, sizeof *p);
}

int32_t Sd_ScriptInterpret(const uint16_t *s, int32_t n, SdScriptPlacements *out, char *err)
{
    SdPlacementList *cur = NULL;
    int32_t i = 0;

    memset(out, 0, sizeof *out);

    for (;;) {
        int32_t op, t, spr, need;
        int32_t xOff, yOff, w, h, v, skip = 0;
        int32_t byteXY;
        SdPlacement p;

        if (i >= n)
            break;
        op = s[i++];
        if (op == 0xFFFF)
            break;
        t = op & 0xF000;
        spr = op & 0x0FFF;

        if (t == 0x7000 || t == 0x8000) {
            if (out->numLoads == (int32_t)(sizeof out->loads / sizeof out->loads[0])) {
                snprintf(err, 256, "image script: too many bank loads");
                Sd_ScriptPlacementsFree(out);
                return -1;
            }
            cur = &out->loads[out->numLoads++];
            cur->loadOffset = i - 1;
            i += 2;                     /* the file name pointer */
            continue;
        }

        switch (t) {
        case 0xC000: byteXY = 1; skip = 0; break;
        case 0x1000:
        case 0x5000: byteXY = 0; skip = 0; break;
        case 0xD000: byteXY = 1; skip = 1; break;
        case 0x2000:
        case 0xA000: byteXY = 0; skip = 1; break;
        case 0xE000:
        case 0xF000: byteXY = 1; skip = 2; break;
        case 0x3000:
        case 0xB000: byteXY = 0; skip = 2; break;
        case 0x9000:
            i += 2;
            continue;
        default:
            snprintf(err, 256, "image script: unknown opcode 0x%04X at word %d", (unsigned)op, (int)(i - 1));
            Sd_ScriptPlacementsFree(out);
            return -1;
        }

        need = (byteXY ? 2 : 3) + skip;
        if (i + need > n) {
            snprintf(err, 256, "image script: opcode 0x%04X at word %d runs past the end", (unsigned)op, (int)(i - 1));
            Sd_ScriptPlacementsFree(out);
            return -1;
        }
        if (byteXY) {
            v = s[i++];
            xOff = (v >> 8) & 0xFF;
            yOff = v & 0xFF;
        } else {
            xOff = s[i++];
            yOff = s[i++];
        }
        v = s[i++];
        w = (v >> 8) & 0xFF;
        h = v & 0xFF;
        i += skip;

        if (!cur)
            continue;                   /* a draw before any load */

        p.amigaIndex = spr;
        p.x0 = ((xOff >> 3) & 0xFFFE) * 8;
        p.y0 = yOff;
        p.winW = ((w + 15) >> 4) * 16;
        p.winH = h;
        if (PushPlacement(cur, &p) != 0) {
            snprintf(err, 256, "out of memory");
            Sd_ScriptPlacementsFree(out);
            return -1;
        }
    }
    return 0;
}

const SdPlacementList *Sd_ScriptFindLoad(const SdScriptPlacements *p, int32_t loadOffset)
{
    for (int32_t i = 0; i < p->numLoads; i++)
        if (p->loads[i].loadOffset == loadOffset)
            return &p->loads[i];
    return NULL;
}

/* ---- drawing ------------------------------------------------------------ */

void Sd_BankBlit(SdImage *bank, const uint8_t *px, int32_t sprW, int32_t sprH,
                 const SdPlacement *p, int32_t orMask)
{
    int32_t limW = sprW < p->winW ? sprW : p->winW;
    int32_t limH = sprH < p->winH ? sprH : p->winH;

    for (int32_t y = 0; y < limH; y++) {
        int32_t by = p->y0 + y;
        if (by < 0 || by >= bank->h)
            continue;
        for (int32_t x = 0; x < limW; x++) {
            uint8_t v = px[y * sprW + x];
            int32_t bx = p->x0 + x;
            if (v == 0 || bx < 0 || bx >= bank->w)
                continue;               /* 0 is transparent */
            bank->px[by * bank->w + bx] = (uint8_t)(v | orMask);
        }
    }
}

int32_t Sd_BankBlitDosSprite(SdImage *bank, const SdDosSprite *spr, const SdPlacement *p, int32_t orMask)
{
    uint8_t *px;

    if (!spr || spr->width <= 0 || spr->height <= 0)
        return 0;
    px = malloc((size_t)spr->width * (size_t)spr->height);
    if (!px)
        return -1;
    if (Sd_DosSpriteDecode(spr, px) != 0) {
        free(px);
        return -1;
    }
    Sd_BankBlit(bank, px, spr->width, spr->height, p, orMask);
    free(px);
    return 0;
}

/* ---- the banks ---------------------------------------------------------- */

static int32_t RenderBank(const SdBankInfo *info, const char *name, const SdPlacementList *places,
                          const SdDosSprites *model, const SdDosDat *team, SdImage *bank, char *err)
{
    if (SdImage_Init(bank, SD_BANK_W, SD_BANK_H) != 0) {
        snprintf(err, 256, "out of memory");
        return -1;
    }
    SdImage_SetPaletteArgb(bank, info->gamePalette ? Sd_DosSpritePalGame : Sd_DosSpritePalMenu, 256);

    for (int32_t k = 0; k < places->count; k++) {
        const SdPlacement *p = &places->items[k];
        const SdDosSprite *spr;
        int32_t idx;

        switch (info->map) {
        case SD_MAP_TEAM:
            idx = p->amigaIndex - TEAM_BLOCK_BASE;
            if (idx < 0 || idx >= team->count)
                continue;
            spr = &team->spr[idx];
            break;
        case SD_MAP_MENU_IDENTITY:
            if (p->amigaIndex >= MENU_SPRITE_LIMIT)
                continue;               /* font extension: no PC source */
            idx = p->amigaIndex;
            if (idx >= SD_DOS_NUM_SPRITES)
                continue;
            spr = model->spr[idx];
            break;
        default:
            idx = p->amigaIndex + GAMEPLAY_OFFSET;
            if (idx < 0 || idx >= SD_DOS_NUM_SPRITES)
                continue;
            spr = model->spr[idx];
            break;
        }
        if (Sd_BankBlitDosSprite(bank, spr, p, info->orMask) != 0) {
            snprintf(err, 256, "%s: PC sprite %d (for slot %d) is broken", name, (int)idx, (int)p->amigaIndex);
            SdImage_Free(bank);
            return -1;
        }
    }
    return 0;
}

int32_t Sd_DosToAmigaBanks(const char *dosDir, SdImage banks[SD_BANK_COUNT], char *err)
{
    SdScriptPlacements scripts[SD_SCRIPT_COUNT];
    SdDosSprites *model;
    SdDosDat team;
    int32_t rc = -1, done = 0;

    memset(scripts, 0, sizeof scripts);
    memset(&team, 0, sizeof team);
    for (int32_t b = 0; b < SD_BANK_COUNT; b++)
        memset(&banks[b], 0, sizeof banks[b]);

    model = calloc(1, sizeof *model);
    if (!model) {
        snprintf(err, 256, "out of memory");
        return -1;
    }
    for (int32_t sc = 0; sc < SD_SCRIPT_COUNT; sc++) {
        int32_t n;
        const uint16_t *w = Sd_ScriptWords((SdScript)sc, &n);
        if (Sd_ScriptInterpret(w, n, &scripts[sc], err) != 0)
            goto out;
    }
    if (Sd_DosSpritesLoad(model, dosDir, err) != 0)
        goto out;

    for (done = 0; done < SD_BANK_COUNT; done++) {
        const SdBankInfo *info = &Sd_BankInfo[done];
        const char *name = Sd_BankNames[done];
        const SdPlacementList *places = Sd_ScriptFindLoad(&scripts[info->script], info->loadOffset);

        if (!places) {
            snprintf(err, 256, "%s: no bank load at word %d of its script", name, (int)info->loadOffset);
            goto out;
        }
        if (info->map == SD_MAP_TEAM &&
            Sd_DosDatLoad(&team, dosDir, info->teamFile, SD_DOS_TEAM_SPRITES, err) != 0)
            goto out;
        if (RenderBank(info, name, places, model, &team, &banks[done], err) != 0)
            goto out;
        Sd_DosDatFree(&team);
    }
    rc = 0;

out:
    if (rc != 0)
        for (int32_t b = 0; b < done; b++)
            SdImage_Free(&banks[b]);
    Sd_DosDatFree(&team);
    for (int32_t sc = 0; sc < SD_SCRIPT_COUNT; sc++)
        Sd_ScriptPlacementsFree(&scripts[sc]);
    Sd_DosSpritesFree(model);
    free(model);
    return rc;
}
