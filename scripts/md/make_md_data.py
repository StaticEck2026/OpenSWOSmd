#!/usr/bin/env python3
"""Build the Mega Drive ROM's game data: sprites, pitches, teams.

    make_md_data.py --out build/md/md_data.c [--export DIR]

The cartridge has no disk: everything a match draws is in ROM, already in the VDP's
format. This script makes it, from one of two sources:

  --export DIR   the player's own SWOS data, as the SDL build decodes it
                 (`SWOS --md-export=DIR`, src/md_export.c): every in-match sprite image
                 by index with its metrics, the kit sheets, the six pitches and every
                 team record. That is the original game's artwork, cut by the original
                 image scripts -- byte for byte what the Amiga game draws.
  (no --export, or an empty DIR)
                 stand-in art drawn here: little footballers for every pose and direction
                 the engine's animation tables name (src/sprite_data.c), a pitch drawn to
                 the engine's own line coordinates (docs/GAMEPLAY.md §1.1), and made-up
                 teams. The ROM builds and plays without anyone's game data.

Either way the output is the same C file (docs/MEGADRIVE.md "ROM data"):

  md_spriteTable[1310]   the engine's sprite-graphics table (_spriteGraphicsPtr points at
                         it): a SpriteGraphics per image, sprData -> an MdImage
  MdImage                n hardware-sprite pieces + their 4bpp tiles, ready to DMA
  md_kitImage[3][606]    the player images of each kit sheet (cjcteam1/2/3)
  md_pitch*              per pitch: 8x8 tiles (deduplicated, flips folded), per 16x16
                         Amiga tile its four nametable cells, and the 42x55 map
  md_teams               684-byte TeamFileEx records, as on the Amiga disk (big-endian)
"""
import argparse
import os
import random
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))

NUM_IMAGES = 1310
TEAM1_FIRST, TEAM2_FIRST, GK1_FIRST, GK2_FIRST, BENCH_FIRST = 227, 530, 833, 949, 1065
TEAM_RANGE = TEAM2_FIRST - TEAM1_FIRST          # 303: three skins of 101
KIT_RANGE = (TEAM1_FIRST, GK1_FIRST)            # the images a kit sheet supplies
# Sprite-graphics entries the engine rewrites at run time (adverts.c, animated_patterns.c):
# they live in RAM on the cartridge.
MUTABLE = list(range(113, 137)) + [137]
ADVERT_FIRST, ADVERT_COUNT, ADVERT_LINES = 113, 24, 13

# Palette indices of the OCS game palette (src/swos_amiga_data_imports.c _gamePalette):
# 0 pitch green, 1 grey, 2 white, 3 black, 4 brown, 5 tan, 6 skin, 7/8/9 greens,
# 10 red, 11 blue, 12 dark red, 13 light blue, 14 green, 15 yellow. Player sprites use
# 10 (stripes), 11 (shirt), 14 (shorts), 15 (socks) for the kit: the engine's colour
# tables map those to each team's colours (color_sprites.c), the cartridge's palette
# lines do the same (md_render.c).
C_GREY, C_WHITE, C_BLACK, C_BROWN, C_TAN, C_SKIN = 1, 2, 3, 4, 5, 6
C_DKGREEN, C_DKGREEN2, C_GREEN = 7, 8, 9
C_STRIPES, C_SHIRT, C_DKRED, C_LTBLUE, C_SHORTS, C_SOCKS = 10, 11, 12, 13, 14, 15

DIRS = ["Up", "UpRight", "Right", "DownRight", "Down", "DownLeft", "Left", "UpLeft"]


class Img:
    def __init__(self, w, h, cx, cy, pix=None):
        self.w, self.h, self.cx, self.cy = w, h, cx, cy
        self.pix = pix if pix is not None else [[0] * w for _ in range(h)]

    def put(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.pix[y][x] = c

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.put(x, y, c)

    def key(self):
        return (self.w, self.h, tuple(tuple(r) for r in self.pix))


# ---------------------------------------------------------------- stand-in art

def catalogue():
    """index -> (pose, direction, team) from the engine's own frame lists."""
    src = open(os.path.join(ROOT, "src", "sprite_data.c")).read()
    cat = {}
    for name, body in re.findall(r"int16_t\s+(_\w+)\s*\[\d*\]\s*=\s*\{([^}]*)\}", src):
        frames = [int(v, 0) for v in re.findall(r"-?0x[0-9a-fA-F]+|-?\d+", body)]
        frames = [f for f in frames if 0 <= f < NUM_IMAGES]
        d = None
        for i, dn in sorted(enumerate(DIRS), key=lambda t: -len(t[1])):
            if re.search(dn + r"(Team|Frames|$)", name) or ("Facing" + dn) in name:
                d = i
                break
        if "FacingSide" in name:
            d = 2
        low = name.lower()
        if "tackled" in low or "down" in low and "dive" not in low and "facingdown" not in low and d is None:
            pose = "down"
        elif "tackl" in low:
            pose = "tackle"
        elif "dive" in low or "jumping" in low:
            pose = "dive"
        elif "header" in low:
            pose = "header"
        elif "throw" in low:
            pose = "throw"
        elif "catch" in low or "deflect" in low:
            pose = "catch"
        elif "card" in low or "winning" in low or "losing" in low or "celebr" in low:
            pose = "cheer"
        elif "injured" in low:
            pose = "down"
        elif "running" in low or "coming" in low:
            pose = "run"
        else:
            pose = "stand"
        for k, f in enumerate(frames):
            cat.setdefault(f, (pose, d if d is not None else 4, k, name))
    return cat


def draw_player(pose, d, phase, kind):
    """A 12x15 footballer (pixWidth 12, nlines 15, centre (4,12): docs/GAMEPLAY.md §5's
    typical running frame), or lying/diving 16x9."""
    if kind == "gk":
        shirt, stripe, shorts, socks = C_GREEN, C_GREEN, C_BLACK, C_GREEN
    elif kind == "ref":
        shirt, stripe, shorts, socks = C_BLACK, C_BLACK, C_BLACK, C_BLACK
    else:
        shirt, stripe, shorts, socks = C_SHIRT, C_STRIPES, C_SHORTS, C_SOCKS
    if pose in ("down", "tackle", "dive"):
        im = Img(16, 9, 8, 6)
        left = d in (5, 6, 7) or (pose == "down" and phase % 2)
        body = range(3, 13)
        for x in body:
            im.rect(x, 3, x, 6, shirt if x < 9 else shorts)
        for x in range(3, 9, 3):
            im.rect(x, 3, x, 6, stripe)
        im.rect(13, 4, 15, 5, C_SKIN)
        im.rect(0, 3, 2, 6, C_SKIN)
        im.rect(0, 3, 1, 3, C_BROWN)
        if left:
            im.pix = [list(reversed(r)) for r in im.pix]
        return im
    im = Img(12, 15, 4, 12)
    jump = 3 if pose == "header" and phase % 2 else 0
    def P(x, y, c):
        im.put(x, y - jump, c)
    # head
    for y in range(0, 4):
        for x in range(3, 7):
            P(x, y, C_SKIN)
    hair = C_BROWN if kind != "ref" else C_BLACK
    for x in range(3, 7):
        P(x, 0, hair)
    if d in (7, 0, 1):              # facing away: the back of the head
        for x in range(3, 7):
            P(x, 1, hair)
    elif d in (3, 4, 5):            # facing us: eyes
        P(4, 2, C_BLACK); P(6, 2, C_BLACK) if d != 5 else None
    elif d == 2:
        P(6, 2, C_BLACK)
    elif d == 6:
        P(3, 2, C_BLACK)
    # shirt with a stripe, arms
    for y in range(4, 9):
        for x in range(2, 8):
            P(x, y, shirt)
        P(4, y, stripe)
        P(6, y, stripe)
    arms_up = pose in ("throw", "cheer") or (pose == "header" and phase % 2)
    if arms_up:
        P(1, 1, C_SKIN); P(1, 2, shirt); P(1, 3, shirt)
        P(8, 1, C_SKIN); P(8, 2, shirt); P(8, 3, shirt)
    elif pose == "catch":
        P(1, 5, shirt); P(0, 5, C_SKIN); P(8, 5, shirt); P(9, 5, C_SKIN)
    else:
        swing = (phase % 2) if pose == "run" else 0
        P(1, 5 + swing, shirt); P(1, 7 + swing, C_SKIN)
        P(8, 6 - swing, shirt); P(8, 8 - swing, C_SKIN)
    # shorts, legs, socks, boots
    for y in range(9, 11):
        for x in range(2, 8):
            P(x, y, shorts)
    stride = [0, 1, 0, -1][phase % 4] if pose == "run" else 0
    for leg, lx in ((0, 3), (1, 6)):
        off = stride if leg == 0 else -stride
        for y in range(11, 13):
            P(lx + (off if y == 12 else 0), y, C_SKIN)
        P(lx + off, 13, socks)
        P(lx + off, 14, C_BLACK)
        P(lx + off + (1 if d in (1, 2, 3) else -1 if d in (5, 6, 7) else 0), 14, C_BLACK)
    return im


def standin_images():
    cat = catalogue()
    images = {}
    def kind_of(i):
        if TEAM1_FIRST <= i < GK1_FIRST:
            return "pl"
        if GK1_FIRST <= i < BENCH_FIRST:
            return "gk"
        if 1159 <= i <= 1172:
            return "ref"
        return None
    # player ranges: every skin copy of a frame draws like the frame it copies
    for i in range(TEAM1_FIRST, BENCH_FIRST):
        base = i
        if i < GK1_FIRST:
            off = (i - TEAM1_FIRST) % TEAM_RANGE
            base = TEAM1_FIRST + off % 101
            if base not in cat:
                base2 = TEAM2_FIRST + off % 101
                base = base2 if base2 in cat else base
        else:
            off = (i - GK1_FIRST) % (GK2_FIRST - GK1_FIRST)
            base = GK1_FIRST + off % 58
        pose, d, phase, _ = cat.get(base, cat.get(i, ("stand", 4, 0, "")))
        images[i] = draw_player(pose, d, phase, kind_of(i))
    for i in range(1159, 1173):
        pose, d, phase, _ = cat.get(i, ("stand", 4, 0, ""))
        images[i] = draw_player(pose if pose != "cheer" else "throw", d, phase, "ref")
    # the ball, spinning (1065..1068), and its shadow
    for k, i in enumerate(range(1065, 1069)):
        im = Img(5, 5, 2, 2)
        for y in range(5):
            for x in range(5):
                if (x - 2) ** 2 + (y - 2) ** 2 <= 5:
                    im.pix[y][x] = C_WHITE
        im.pix[(k + 1) % 4 + 1 if k < 3 else 2][1 + k % 3] = C_BLACK
        images[i] = im
    sh = Img(5, 3, 2, 1)
    for x in range(5):
        sh.pix[1][x] = C_DKGREEN2
    for x in range(1, 4):
        sh.pix[0][x] = sh.pix[2][x] = C_DKGREEN2
    images[1069] = sh
    # the goals: the top goal's crossbar, the bottom goal's net in front of the players
    top = Img(73, 7, 0, 23)
    top.rect(0, 0, 72, 1, C_WHITE)
    top.rect(0, 0, 1, 6, C_WHITE); top.rect(71, 0, 72, 6, C_WHITE)
    images[1091] = top
    bot = Img(73, 28, 0, 27)
    for y in range(28):
        for x in range(73):
            if (x + y) % 4 == 0 or (x - y) % 4 == 0:
                bot.pix[y][x] = C_GREY
    bot.rect(0, 0, 72, 1, C_WHITE)
    bot.rect(0, 0, 1, 27, C_WHITE); bot.rect(71, 0, 72, 27, C_WHITE)
    images[1092] = bot
    # the corner flags (138, 1090 and around: small pennants)
    for i in (138, 1090, 1094):
        f = Img(6, 12, 0, 11)
        f.rect(0, 0, 0, 11, C_WHITE)
        f.rect(1, 0, 4, 3, C_DKRED if i % 2 else C_SOCKS)
        images[i] = f
    # the player marker and the "S"
    m = Img(5, 3, 2, 8)
    m.rect(0, 0, 4, 0, C_SOCKS); m.rect(1, 1, 3, 1, C_SOCKS); m.put(2, 2, C_SOCKS)
    images[1170] = m
    # adverts (113..136): boards of 16x16 with a word in them
    for k in range(ADVERT_COUNT):
        a = Img(16, 16, 0, 0)
        a.rect(0, 0, 15, 15, [C_SHIRT, C_DKRED, C_SOCKS][k // 8])
        for x in range(0, 16, 3):
            a.rect(x, 4 + (k % 3), x + 1, 10 + (k % 3), C_WHITE)
        images[ADVERT_FIRST + k] = a
    images[137] = Img(16, 16, 0, 0)
    # the bench (bench_subs.c): the coaches (1182.., 7 a team), the substitutes sitting
    # (1196.., 12 a team), the faces of the substitutes menu (1179..1181), its cards
    # (139/140), the injury cross (1176) and the SUBSTITUTES / FORMATION icons (1174/1175)
    for i in range(1182, 1196):
        images[i] = draw_player("stand", 4, i % 2, "ref")
    for i in range(1196, 1226):
        sit = Img(10, 11, 4, 9)
        sit.rect(2, 0, 6, 2, C_SKIN)
        sit.rect(1, 3, 7, 7, C_SHIRT)
        sit.rect(1, 8, 8, 9, C_SHORTS)
        sit.rect(7, 9, 9, 10, C_SOCKS)
        images[i] = sit
    for k, i in enumerate(range(1179, 1182)):
        f = Img(8, 8, 0, 0)
        f.rect(1, 0, 6, 1, [C_BROWN, C_BLACK, C_TAN][k])
        f.rect(1, 2, 6, 7, [C_SKIN, C_BROWN, C_SKIN][k])
        f.put(2, 4, C_BLACK); f.put(5, 4, C_BLACK)
        images[i] = f
    for i, c in ((139, C_DKRED), (140, C_SOCKS)):
        card = Img(5, 7, 0, 0)
        card.rect(0, 0, 4, 6, c)
        images[i] = card
    cross = Img(7, 7, 0, 0)
    cross.rect(2, 0, 4, 6, C_DKRED); cross.rect(0, 2, 6, 4, C_DKRED)
    images[1176] = cross
    for i in (1174, 1175):
        icon = Img(16, 16, 0, 0)
        icon.rect(0, 0, 15, 15, C_GREY)
        icon.rect(2, 2, 13, 13, C_WHITE if i == 1174 else C_GREEN)
        images[i] = icon
    # the statistics' grid square: DarkenRectangle reads its size (md_overlay.c darkens
    # the cells under it), the pixels are never drawn
    images[1093] = Img(16, 16, 0, 0)
    # everything else the table has a slot for: an empty 8x8 (HUD sprites the cartridge
    # draws itself, menu art)
    for i in range(NUM_IMAGES):
        images.setdefault(i, Img(8, 8, 0, 0))
    return images


def standin_kits(images):
    """The three kit sheets: plain/vertical stripes (1), coloured sleeves (2), hoops (3)."""
    kits = {1: {}, 2: {}, 3: {}}
    for i in range(*KIT_RANGE):
        base = images[i]
        for sheet in (1, 2, 3):
            im = Img(base.w, base.h, base.cx, base.cy, [list(r) for r in base.pix])
            for y in range(im.h):
                for x in range(im.w):
                    c = im.pix[y][x]
                    if c not in (C_SHIRT, C_STRIPES):
                        continue
                    if sheet == 2:          # sleeves: the outer columns in the second colour
                        im.pix[y][x] = C_STRIPES if x in (1, 2, 7, 8) else C_SHIRT
                    elif sheet == 3:        # hoops
                        im.pix[y][x] = C_STRIPES if y % 2 else C_SHIRT
            kits[sheet][i] = im
    return kits


def standin_pitch():
    W, H = 672, 880
    pic = [[0] * W for _ in range(H)]
    for y in range(H):
        band = C_DKGREEN if (y // 32) % 2 else C_GREEN
        row = pic[y]
        for x in range(W):
            row[x] = band
    def rect(x0, y0, x1, y1, c):
        for y in range(max(0, y0), min(H, y1 + 1)):
            for x in range(max(0, x0), min(W, x1 + 1)):
                pic[y][x] = c
    def line_h(y, x0, x1):
        rect(x0, y, x1, y + 1, C_WHITE)
    def line_v(x, y0, y1):
        rect(x, y0, x + 1, y1, C_WHITE)
    # surround: stands and advert boards
    rect(0, 0, W - 1, 95, C_DKGREEN2)
    rect(0, 805, W - 1, H - 1, C_DKGREEN2)
    rect(0, 0, 47, H - 1, C_DKGREEN2)
    rect(624, 0, W - 1, H - 1, C_DKGREEN2)
    rect(48, 88, 623, 95, C_GREY)
    rect(48, 802, 623, 809, C_GREY)
    # the markings, at the engine's coordinates
    line_v(81, 129, 769); line_v(589, 129, 769)
    line_h(129, 81, 590); line_h(768, 81, 590)
    line_h(448, 81, 590)
    for a in range(0, 3600):
        import math
        t = a * math.pi / 1800
        x = int(round(336 + 56 * math.cos(t))); y = int(round(449 + 48 * math.sin(t)))
        rect(x, y, x + 1, y, C_WHITE)
        if 187 + 48 * math.sin(t) > 216:
            yy = int(round(187 + 48 * math.sin(t))); rect(x, yy, x + 1, yy, C_WHITE)
        if 711 - 48 * math.sin(t) < 682:
            yy = int(round(711 - 48 * math.sin(t))); rect(x, yy, x + 1, yy, C_WHITE)
    for (y0, y1) in ((129, 216), (682, 769)):
        line_v(193, y0, y1); line_v(477, y0, y1)
        line_h(y1 if y0 == 129 else y0, 193, 478)
    for (y0, y1) in ((129, 158), (740, 769)):
        line_v(273, y0, y1); line_v(397, y0, y1)
        line_h(y1 if y0 == 129 else y0, 273, 398)
    for (cx, cy) in ((336, 187), (336, 711), (336, 449)):
        rect(cx - 1, cy - 1, cx + 1, cy + 1, C_WHITE)
    # the top goal's net, behind the line (the bottom goal's is a sprite, 1092)
    for y in range(113, 129):
        for x in range(302, 371):
            if (x + y) % 4 == 0 or (x - y) % 4 == 0:
                pic[y][x] = C_GREY
    rect(300, 113, 301, 129, C_WHITE); rect(369, 113, 370, 129, C_WHITE)

    # cut into 16x16 tiles; the crowd rows (1-3, 51-53) are the fan tiles the engine
    # animates (animated_patterns.c): tile pairs 2k-1 (still) / 2k (cheering), k = 1..11
    tiles = [[C_DKGREEN2] * 256]
    for k in range(11):
        rnd = random.Random(k)
        still = [C_DKGREEN2] * 256
        cheer = [C_DKGREEN2] * 256
        for n in range(6):
            fx, fy = rnd.randrange(1, 14), rnd.randrange(4, 13)
            col = rnd.choice([C_SHIRT, C_DKRED, C_SOCKS, C_WHITE, C_LTBLUE, C_TAN])
            for (dx, dy) in ((0, 0), (1, 0), (0, 1), (1, 1), (0, 2), (1, 2)):
                still[(fy + dy) * 16 + fx + dx] = col
                cheer[(fy + dy - 3) * 16 + fx + dx] = col
            still[(fy - 1) * 16 + fx] = C_SKIN
            cheer[(fy - 4) * 16 + fx] = C_SKIN
            cheer[(fy - 5) * 16 + fx - 1 if fx else 0] = C_SKIN
        tiles.append(still)
        tiles.append(cheer)
    index = {tuple(t): i for i, t in enumerate(tiles)}
    pmap = []
    rnd = random.Random(1996)
    for ty in range(55):
        for tx in range(42):
            if ty in (1, 2, 3, 51, 52, 53) and 3 <= tx <= 38:
                pmap.append(2 * rnd.randrange(11) + 1)
                continue
            t = tuple(pic[ty * 16 + y][tx * 16 + x] for y in range(16) for x in range(16))
            if t not in index:
                index[t] = len(tiles)
                tiles.append(list(t))
            pmap.append(index[t])
    return tiles, pmap


SYLL = ["ar", "ben", "cas", "dor", "el", "fen", "gar", "hol", "is", "jor", "kel", "lan", "mor",
        "nor", "ol", "par", "quin", "ros", "sal", "tor", "ul", "ver", "wil", "yan", "zan"]


def standin_teams():
    rnd = random.Random(1994)
    names = ["REDCASTLE UTD", "NORTH ATHLETIC", "BLUEHAVEN", "IRONBRIDGE", "PORT VALLEY",
             "KINGS CROSS", "ST ALDA", "WESTFORD CITY", "GREENFIELD", "OLD BRIDGE",
             "SILVERTON", "HIGHLAND", "LAKESIDE", "EAST ROVERS", "MILLWOOD", "STONEHILL"]
    # (shirt type, stripes, basic, shorts, socks): kit colour IDS, 0..9 -- grey, white,
    # black, orange, red, blue, dark red, light blue, green, yellow (team_colors.c
    # _shirtColorTable maps them to palette entries). Shirt types: 0 plain, 1 vertical
    # stripes, 2 coloured sleeves, 3 hoops (the order of _teamFilenamesTable).
    kits = [(0, 4, 4, 1, 4), (1, 5, 1, 5, 1), (0, 1, 1, 2, 1), (3, 2, 9, 2, 9),
            (2, 1, 4, 1, 1), (0, 7, 7, 1, 7), (1, 2, 1, 2, 2), (0, 9, 9, 5, 9),
            (3, 4, 1, 4, 4), (0, 8, 8, 1, 8), (2, 5, 1, 1, 5), (0, 0, 0, 2, 0),
            (1, 4, 9, 2, 4), (3, 5, 7, 5, 7), (0, 3, 3, 2, 3), (0, 6, 6, 1, 6)]
    positions = [0, 1, 2, 3, 3, 4, 5, 6, 6, 7, 7, 0, 3, 6, 7, 4]
    recs = []
    for t, name in enumerate(names):
        shirtType, stripes, basic, shorts, socks = kits[t]
        strength = 2 + t % 5
        hdr = struct.pack(">BBhB17sBBBB", 0x50, t, t, 1, name.encode()[:16].ljust(17, b"\0"),
                          0, 0, 0, 0)
        hdr += bytes([shirtType, stripes, basic, shorts, socks, 0, 2, 2, 3, 2])
        hdr += ("COACH " + name.split()[0]).encode()[:22].ljust(23, b"\0")
        hdr += bytes([1]) + bytes(range(16))
        assert len(hdr) == 76, len(hdr)
        pl = b""
        for p in range(16):
            first = rnd.choice("ABCDEFGHJKLMNPRSTW")
            sur = (rnd.choice(SYLL) + rnd.choice(SYLL) + rnd.choice(["", "son", "ez", "ini", "ov"])).upper()
            nm = ("%s. %s" % (first, sur))[:22]
            pos = positions[p]
            face = rnd.randrange(3)
            def sk():
                return max(0, min(7, strength + rnd.randrange(-2, 3)))
            rec = bytes([0x50, 0, p + 1]) + nm.encode().ljust(23, b"\0")
            rec += bytes([(pos << 5) | (face << 3), 0,
                          sk(), (sk() << 4) | sk(), (sk() << 4) | sk(), (sk() << 4) | sk(),
                          min(0x31, 8 + strength * 4 + rnd.randrange(6)), 0, 0, 0, 0, 0])
            assert len(rec) == 38
            pl += rec
        recs.append(hdr + pl)
    return recs


# ---------------------------------------------------------------- real data (--export)

def read_export(d):
    """The SDL build's --md-export dump (src/md_export.c); None if DIR has none."""
    p = os.path.join(d, "sprites.bin")
    if not os.path.exists(p):
        return None
    def read_images(path):
        data = open(path, "rb").read()
        assert data[:4] == b"MDSP", path
        n, = struct.unpack(">H", data[4:6])
        pos, out = 6, {}
        for _ in range(n):
            idx, w, h, cx, cy = struct.unpack(">Hhhhh", data[pos:pos + 10]); pos += 10
            pix = [list(data[pos + y * w: pos + (y + 1) * w]) for y in range(h)]
            pos += w * h
            out[idx] = Img(w, h, cx, cy, pix)
        return out
    images = read_images(p)
    for i in range(NUM_IMAGES):
        images.setdefault(i, Img(8, 8, 0, 0))
    kits = {}
    for sheet in (1, 2, 3):
        kp = os.path.join(d, "kit%d.bin" % sheet)
        kits[sheet] = read_images(kp) if os.path.exists(kp) else {}
        for i in range(*KIT_RANGE):
            kits[sheet].setdefault(i, images[i])
    pitches = []
    for n in range(1, 7):
        pp = os.path.join(d, "pitch%d.bin" % n)
        if not os.path.exists(pp):
            continue
        data = open(pp, "rb").read()
        assert data[:4] == b"MDPT"
        ntiles, = struct.unpack(">H", data[4:6])
        tiles = [list(data[6 + k * 256: 6 + (k + 1) * 256]) for k in range(ntiles)]
        mp = list(struct.unpack(">%dH" % (42 * 55), data[6 + ntiles * 256: 6 + ntiles * 256 + 42 * 55 * 2]))
        pitches.append((tiles, mp))
    teams = []
    tp = os.path.join(d, "teams.bin")
    if os.path.exists(tp):
        data = open(tp, "rb").read()
        teams = [data[k:k + 684] for k in range(0, len(data) - 683, 684)]
    return images, kits, pitches, teams


# ---------------------------------------------------------------- packing

def tile_bytes(px):
    """8x8 palette indices -> 32 bytes of VDP 4bpp."""
    out = bytearray()
    for y in range(8):
        for x in range(0, 8, 2):
            out.append((px[y][x] & 15) << 4 | (px[y][x + 1] & 15))
    return bytes(out)


def pack_image(im):
    """An image as VDP sprites: pieces of up to 4x4 tiles, blank ones dropped.
    MdImage: u16 nTiles, u8 nPieces, u8 0, then per piece {s8 dx, s8 dy, u8 size, u8 tile},
    then the tiles, column-major within each piece as the VDP reads a sprite."""
    tw = (im.w + 7) // 8
    th = (im.h + 7) // 8
    def cell(cx, cy):
        return [[im.pix[cy * 8 + y][cx * 8 + x] if cy * 8 + y < im.h and cx * 8 + x < im.w else 0
                 for x in range(8)] for y in range(8)]
    pieces, tiles = [], []
    for py in range(0, th, 4):
        for px in range(0, tw, 4):
            pw, ph = min(4, tw - px), min(4, th - py)
            cells = [cell(px + x, py + y) for x in range(pw) for y in range(ph)]
            if not any(any(r) for c in cells for r in c):
                continue
            pieces.append((px * 8, py * 8, ((pw - 1) << 2) | (ph - 1), len(tiles)))
            tiles.extend(tile_bytes(c) for c in cells)
    blob = struct.pack(">HBB", len(tiles), len(pieces), 0)
    for (dx, dy, size, t) in pieces:
        blob += struct.pack(">bbBB", dx, dy, size, t)
    return blob + b"".join(tiles)


def flips(t):
    rows = [t[y * 8:(y + 1) * 8] for y in range(8)]
    h = [r[::-1] for r in rows]
    v = rows[::-1]
    hv = [r[::-1] for r in rows[::-1]]
    f = lambda rr: tuple(c for r in rr for c in r)
    return [(f(rows), 0), (f(h), 0x0800), (f(v), 0x1000), (f(hv), 0x1800)]


def pack_pitch(tiles16, pmap, first_vram_tile):
    """16x16 Amiga tiles -> deduplicated 8x8 VDP tiles + four nametable cells each."""
    found, vt, cells = {}, [], []
    blank = tuple([0] * 64)
    for t in tiles16:
        quads = []
        for (qx, qy) in ((0, 0), (8, 0), (0, 8), (8, 8)):
            q = tuple(t[(qy + y) * 16 + qx + x] for y in range(8) for x in range(8))
            entry = None
            for (fq, fl) in flips(q):
                if fq in found:
                    entry = found[fq] | fl
                    break
            if entry is None:
                n = first_vram_tile + len(vt)
                found[q] = n
                vt.append(q)
                entry = n
            quads.append(entry)
        cells.append(quads)
    return vt, cells


def c_bytes(name, data, static=True, align=2):
    s = "%sconst uint8_t %s[%d] __attribute__((aligned(%d))) = {\n" % ("static " if static else "", name, len(data), align)
    for k in range(0, len(data), 24):
        s += "  " + ",".join("%d" % b for b in data[k:k + 24]) + ",\n"
    return s + "};\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--export", help="directory written by SWOS --md-export")
    ap.add_argument("--pitch-first-tile", type=int, default=1)
    a = ap.parse_args()

    src = read_export(a.export) if a.export else None
    if src:
        images, kits, pitches, teams = src
        if not pitches:
            pitches = [standin_pitch()]
        if not teams:
            teams = standin_teams()
        origin = "the player's SWOS data (%s)" % a.export
    else:
        images = standin_images()
        kits = standin_kits(images)
        pitches = [standin_pitch()]
        teams = standin_teams()
        origin = "stand-in art (no --export given)"

    out = ["/* Generated by scripts/md/make_md_data.py from %s -- do not edit. */" % origin,
           '#include "md_data.h"', ""]
    blobs = {}
    def blob_name(im):
        k = im.key()
        if k not in blobs:
            blobs[k] = ("md_img_%d" % len(blobs), pack_image(im))
        return blobs[k][0]

    names = [blob_name(images[i]) for i in range(NUM_IMAGES)]
    kitnames = {s: [blob_name(kits[s][i]) for i in range(*KIT_RANGE)] for s in (1, 2, 3)}
    # adverts: each board image cut to the 6-line window the engine scrolls through it
    advnames = []
    for k in range(ADVERT_COUNT):
        im = images[ADVERT_FIRST + k]
        row = []
        for line in range(ADVERT_LINES):
            win = Img(im.w, 6, 0, 0, [list(im.pix[line + y]) if line + y < im.h else [0] * im.w for y in range(6)])
            row.append(blob_name(win))
        advnames.append(row)

    total = 0
    for (nm, data) in blobs.values():
        out.append(c_bytes(nm, data))
        total += len(data)

    def sg(i, im, nm):
        wq = (im.w + 15) // 16
        return "{ (uint8_t *)%s, 0, 0, %d, %d, %d, %d, %d, %d, %d }" % (
            nm, im.w, im.h, wq, im.cx, im.cy, (im.h << 6) + 2 * wq, i)

    out.append("const SpriteGraphics md_sgRom[%d] = {" % NUM_IMAGES)
    out += ["  " + sg(i, images[i], names[i]) + "," for i in range(NUM_IMAGES)]
    out.append("};")
    out.append("SpriteGraphics md_sgRam[%d];" % len(MUTABLE))
    out.append("const uint16_t md_sgRamIndex[%d] = { %s };" % (len(MUTABLE), ", ".join(map(str, MUTABLE))))
    out.append("const SpriteGraphics *const md_spriteTable[%d] = {" % NUM_IMAGES)
    for i in range(NUM_IMAGES):
        out.append("  &md_sgRam[%d]," % MUTABLE.index(i) if i in MUTABLE else "  &md_sgRom[%d]," % i)
    out.append("};")
    out.append("const uint8_t *const md_kitImage[3][%d] = {" % (KIT_RANGE[1] - KIT_RANGE[0]))
    for s in (1, 2, 3):
        out.append("  { " + ", ".join(kitnames[s]) + " },")
    out.append("};")
    out.append("const uint8_t *const md_advertImage[%d][%d] = {" % (ADVERT_COUNT, ADVERT_LINES))
    for row in advnames:
        out.append("  { " + ", ".join(row) + " },")
    out.append("};")

    # pitches: up to six; the engine numbers them 0..5 (SetPitchTypeAndNumber)
    packed = []
    for n, (tiles16, pmap) in enumerate(pitches):
        vt, cells = pack_pitch(tiles16, pmap, a.pitch_first_tile)
        data = b"".join(tile_bytes([list(q[y * 8:(y + 1) * 8]) for y in range(8)]) for q in vt)
        cellw = b"".join(struct.pack(">4H", *c) for c in cells)
        out.append(c_bytes("md_pitch%d_tiles" % n, data))
        out.append(c_bytes("md_pitch%d_cells" % n, cellw))
        out.append(c_bytes("md_pitch%d_map" % n, struct.pack(">%dH" % len(pmap), *pmap)))
        total += len(data) + len(cellw) + 2 * len(pmap)
        packed.append((len(vt), len(tiles16)))
        sys.stderr.write("pitch %d: %d tiles 16x16 -> %d VDP tiles\n" % (n, len(tiles16), len(vt)))
    out.append("const MdPitch md_pitches[6] = {")
    for n in range(6):
        k = n if n < len(pitches) else 0
        out.append("  { md_pitch%d_tiles, (const uint16_t *)md_pitch%d_cells, (const uint16_t *)md_pitch%d_map, %d, %d }," % (
            k, k, k, packed[k][0], packed[k][1]))
    out.append("};")

    tdata = b"".join(teams)
    out.append(c_bytes("md_teamsData", tdata, static=False))
    out.append("const uint16_t md_numTeams = %d;" % len(teams))
    total += len(tdata)
    open(a.out, "w").write("\n".join(out) + "\n")
    sys.stderr.write("md data: %d images (%d distinct), %d teams, %d KB of ROM, from %s\n" % (
        NUM_IMAGES, len(blobs), len(teams), total // 1024, origin))


if __name__ == "__main__":
    main()
