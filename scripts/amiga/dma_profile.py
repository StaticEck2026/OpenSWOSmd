#!/usr/bin/env python3
"""Headless Amiga DMA profiler for the bartman fs-uae gdb harness.

Drives `monitor profile <N> "" <out>` on the modified fs-uae binary (which has a
cycle-exact DMA recorder), then parses the per-frame DMA record grid
(227 hpos x 313 vpos, one entry per colour-clock per scanline) and reports which
DMA channel owned each cycle. The point: SEE, cycle-exact, whether the blitter is
running on the same scanlines the bitplanes are being fetched — i.e. real
blitter-vs-display DMA contention — instead of guessing from screenshots.

Record + file format reverse-engineered from the authoritative parser in
  vscode-amiga-debug/src/backend/profile.ts   (sizeofDmaRec = 58)
  vscode-amiga-debug/src/client/dma.ts        (DmaTypes / DmaEvents)

Usage:
  dma_profile.py profile <N> <out.prof>   # capture N frames via the running harness
  dma_profile.py parse <out.prof>         # parse + report contention per frame
  dma_profile.py <N>                      # profile N frames to a temp file, then parse
"""
import subprocess, sys, os, struct

HERE = os.path.dirname(os.path.abspath(__file__))
PROXY = os.path.join(HERE, "gdb_proxy.py")
TMP = os.environ.get("CLAUDE_JOB_DIR", "/tmp")
if os.path.isdir(TMP + "/tmp"): TMP = TMP + "/tmp"

NR_HPOS, NR_VPOS = 227, 313
SIZEOF_DMAREC = 58
DMACOUNT = NR_HPOS * NR_VPOS            # 71051
MARKER = struct.pack("<II", SIZEOF_DMAREC, DMACOUNT)   # dmaLen, dmaCount

# DmaTypes (dma.ts)
T = {1: "refresh", 2: "cpu", 3: "copper", 4: "audio", 5: "BLITTER",
     6: "bitplane", 7: "sprite", 8: "disk", 9: "CONFLICT"}
# DmaEvents (dma.ts) — contention-relevant bits of the per-cell evt word
EV_NOONEGETS       = 256
EV_CPUBLITTERSTEAL = 512
EV_CPUBLITTERSTOLEN = 1024
EV_VB              = 0x10000       # vertical blank
EV_VDIW            = 0x1000000     # inside vertical display window (a display line)

def gdb(cmd, t=60):
    return subprocess.run(["python3", PROXY, "cmd", cmd], capture_output=True,
                          text=True, timeout=t).stdout

def do_profile(n, out):
    if os.path.exists(out): os.remove(out)
    # target must be free-running to profile frames; monitor profile resumes it,
    # profiles N frames, writes the file, then replies.
    gdb(f'monitor profile {n} "" "{out}"', t=90)
    return os.path.exists(out)

def parse(path):
    with open(path, "rb") as f:
        buf = f.read()
    # find every DMA block (one per profiled frame) by its (dmaLen,dmaCount) marker
    blocks = []
    off = 0
    while True:
        i = buf.find(MARKER, off)
        if i < 0: break
        start = i + len(MARKER)
        end = start + SIZEOF_DMAREC * DMACOUNT
        if end <= len(buf):
            blocks.append((start, buf[start:end]))
        off = i + 1
    print(f"{os.path.basename(path)}: {len(blocks)} DMA frame block(s)")
    for fi, (start, blk) in enumerate(blocks):
        analyze_frame(fi, blk)

def analyze_frame(fi, blk):
    # per-scanline channel tallies
    bpl_lines = set(); blt_lines = set(); conflict_cells = 0
    steal = 0; blitter_cells = 0; bitplane_cells = 0
    blt_on_display_line = {}   # vpos -> list of hpos where blitter ran on a bitplane line
    per_line_bpl = [0]*NR_VPOS
    per_line_blt = [0]*NR_VPOS
    for idx in range(DMACOUNT):
        b = idx * SIZEOF_DMAREC
        reg  = blk[b] | (blk[b+1] << 8)
        evt  = struct.unpack_from("<I", blk, b+16)[0]
        typ  = struct.unpack_from("<h", blk, b+29)[0]
        vpos = idx // NR_HPOS
        hpos = idx % NR_HPOS
        if typ == 6:  # bitplane
            bitplane_cells += 1; bpl_lines.add(vpos); per_line_bpl[vpos]+=1
        elif typ == 5:  # blitter
            blitter_cells += 1; blt_lines.add(vpos); per_line_blt[vpos]+=1
        elif typ == 9:  # conflict
            conflict_cells += 1
        if evt & (EV_CPUBLITTERSTEAL | EV_CPUBLITTERSTOLEN):
            steal += 1
    # display lines = lines with bitplane DMA. contention lines = those w/ blitter too.
    contention_lines = sorted(l for l in bpl_lines if per_line_blt[l] > 0)
    disp_lo = min(bpl_lines) if bpl_lines else -1
    disp_hi = max(bpl_lines) if bpl_lines else -1
    blt_lo = min(blt_lines) if blt_lines else -1
    blt_hi = max(blt_lines) if blt_lines else -1
    print(f"  frame {fi}: bitplane_cells={bitplane_cells} blitter_cells={blitter_cells} "
          f"conflict_cells={conflict_cells} steal_evt={steal}")
    print(f"    display lines (bitplane DMA): vpos {disp_lo}..{disp_hi} "
          f"({len(bpl_lines)} lines)")
    if blt_lines:
        print(f"    blitter active:            vpos {blt_lo}..{blt_hi} "
              f"({len(blt_lines)} lines)")
        if contention_lines:
            print(f"    >>> CONTENTION: blitter runs on {len(contention_lines)} "
                  f"DISPLAY lines (blitter + bitplane same scanline)")
            print(f"        contention vpos: {contention_lines[:12]}"
                  f"{'...' if len(contention_lines)>12 else ''}")
        else:
            print(f"    blitter stays OUT of display lines (clean: blanking-only blit)")
    else:
        print(f"    no blitter DMA this frame (no tile-cross)")

# custom-register offsets used for blit reconstruction
R_BLTCON0=0x40; R_BLTAFWM=0x44; R_BLTALWM=0x46
R_BLTDPTH=0x54; R_BLTSIZE=0x58; R_BLTSIZV=0x5C; R_BLTSIZH=0x5E
DISPLAY_VLO, DISPLAY_VHI = 36, 307     # SWOS AGA pitch DIW vertical window

def blit_blocks(path):
    with open(path, "rb") as f: buf = f.read()
    out=[]; off=0
    while True:
        i = buf.find(MARKER, off)
        if i < 0: break
        s = i+len(MARKER); e = s+SIZEOF_DMAREC*DMACOUNT
        if e<=len(buf): out.append(buf[s:e])
        off=i+1
    return out

def analyze_blits(fi, blk):
    """Reconstruct each blit from the DMA stream (port of dma.ts GetBlits):
    track custom-reg writes, a BLTSIZE/BLTSIZH write starts a blit, a BLITIRQ
    (evt bit0) ends the last one. Report raster span + size + channels + dest."""
    reg_shadow = {}
    blits = []
    for idx in range(DMACOUNT):
        b = idx*SIZEOF_DMAREC
        reg  = blk[b] | (blk[b+1]<<8)
        dat  = struct.unpack_from("<H", blk, b+2)[0]     # low word = written value
        addr = struct.unpack_from("<I", blk, b+12)[0]
        evt  = struct.unpack_from("<I", blk, b+16)[0]
        vpos = idx // NR_HPOS; hpos = idx % NR_HPOS
        # resolve a register write (by reg field or by 0xdffxxx addr), dma.ts-style
        r = reg if (reg != 0xffff and reg < 0x200) else \
            (addr-0xdff000 if 0xdff000<=addr<0xdff200 else None)
        if r is not None:
            reg_shadow[r] = dat
            if r in (R_BLTSIZE, R_BLTSIZH):
                if r == R_BLTSIZE:                      # OCS-style pack
                    w = dat & 0x3f or 64; h = (dat>>6) & 0x3ff or 1024
                else:                                    # ECS/AGA
                    w = dat & 0x7ff or 2048; h = reg_shadow.get(R_BLTSIZV,0) & 0x7fff or 32768
                con0 = reg_shadow.get(R_BLTCON0, 0)
                chan = "".join(c for k,c in enumerate("ABCD") if con0 & (1<<(11-k)))
                dptr = (reg_shadow.get(R_BLTDPTH,0)<<16) | reg_shadow.get(R_BLTDPTH+2,0)
                blits.append({"v0":vpos,"h0":hpos,"w":w,"h":h,"chan":chan,
                              "dst":dptr & 0x1ffffe,"v1":None,"h1":None})
        if (evt & 1) and blits and blits[-1]["v1"] is None:  # BLITIRQ
            blits[-1]["v1"]=vpos; blits[-1]["h1"]=hpos
    if not blits:
        print(f"  frame {fi}: no blits"); return
    print(f"  frame {fi}: {len(blits)} blit(s)")
    for bl in blits:
        v1 = bl["v1"] if bl["v1"] is not None else "?"
        # a blit overruns if its raster span touches the display window
        over = ""
        if isinstance(v1,int) and v1 >= DISPLAY_VLO and bl["v0"] <= DISPLAY_VHI:
            first_disp = max(bl["v0"], DISPLAY_VLO)
            over = f"  <<< OVERRUNS DISPLAY (blitter on display lines {first_disp}..{min(v1,DISPLAY_VHI)})"
        print(f"    L{bl['v0']:>3}->L{str(v1):>3}  {bl['w']*16:>3}x{bl['h']:<4}px  "
              f"chan={bl['chan']:<4} D=${bl['dst']:06x}{over}")

def parse_blits(path):
    blks = blit_blocks(path)
    print(f"{os.path.basename(path)}: {len(blks)} frame block(s)  "
          f"[display window vpos {DISPLAY_VLO}..{DISPLAY_VHI}]")
    for fi,blk in enumerate(blks): analyze_blits(fi, blk)

def main():
    a = sys.argv[1:]
    if a and a[0] == "profile":
        n = int(a[1]); out = a[2]
        print("OK" if do_profile(n, out) else "FAILED"); return
    if a and a[0] == "parse":
        parse(a[1]); return
    if a and a[0] == "blits":                 # per-blit raster-span view (overrun finder)
        parse_blits(a[1]); return
    n = int(a[0]) if a else 10
    out = f"{TMP}/dma_{n}f.prof"
    if do_profile(n, out): parse(out); parse_blits(out)
    else: print("profile FAILED")

main()
