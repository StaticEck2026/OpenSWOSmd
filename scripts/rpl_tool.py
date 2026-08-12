#!/usr/bin/env python3
"""SWOS DOS/2020 RPL/HIL inspector.

Parses the binary replay/highlight format (little-endian) documented from the
SWOS 2020 source and validates it against real files. Future home for a
DOS/2020 -> our-engine converter; for now it just dumps the structure so we
know the sprite-index / camera coordinate ranges before writing the C loader.

Header (3626 bytes):
  0x0000  u32   magic/reserved (= 8)
  0x0004  u32   max file size (byte 5 == 0xF4 => "use current game style")
  0x0008  1704  team 1 in-game struct (name at struct+22)
  0x06B0  1704  team 2 in-game struct
  0x0D58  40    competition / league name
  0x0D80  60    reserved
  0x0DBC  100   match type string
  0x0E20  u16   number of scenes (1 for RPL, up to 10 for HIL)
  0x0E22  u16   team 1 goals
  0x0E24  u16   team 2 goals
  0x0E26  u16   pitch: low byte = pitch file number, high byte = terrain type
  0x0E28  u16   max substitutes
  0x0E2A        frame stream begins

Frame stream dwords (LE):
  bit31 set  -> frame marker: X = low 16 (signed), Y = high 15 (signed)
               followed by a goals dword and an anim-state dword
  0xFFFFFFFF -> frame/scene terminator
  bit31 clear-> sprite: [idx:11][x:10][y:10]  (x,y signed 10-bit)
"""
import struct, sys, os

HDR = 0x0E2A

def s16(v): return v - 0x10000 if v & 0x8000 else v
def s10(v): return v - 0x400 if v & 0x200 else v

def cstr(b):
    return b.split(b'\0')[0].decode('latin1', 'replace')

def parse(path):
    d = open(path, 'rb').read()
    print(f"=== {os.path.basename(path)} ({len(d)} bytes) ===")
    if len(d) < HDR:
        print("  too small for a replay header"); return
    h0, h1 = struct.unpack_from('<II', d, 0)
    numScenes, t1g, t2g, pitch, maxSubs = struct.unpack_from('<HHHHH', d, 0x0E20)
    name      = cstr(d[0x0D58:0x0D80])
    matchType = cstr(d[0x0DBC:0x0E20])
    t1name = cstr(d[8+22:8+22+16])
    t2name = cstr(d[1712+22:1712+22+16])
    print(f"  hdr: magic={h0} maxsz={h1} byte5=0x{d[5]:02x}"
          f"{'  (use-current-style)' if d[5]==0xF4 else ''}")
    print(f"  teams: '{t1name}' {t1g}-{t2g} '{t2name}'   league='{name}'  type='{matchType}'")
    print(f"  scenes={numScenes}  pitch(num={pitch & 0xFF}, type={pitch >> 8})  maxSubs={maxSubs}")

    # frame stream
    i = HDR; n = len(d)
    frames = scenes = 0
    camX = []; camY = []; sidx = []; sx = []; sy = []
    expect = 'marker'
    while i + 4 <= n:
        w = struct.unpack_from('<I', d, i)[0]; i += 4
        if w == 0xFFFFFFFF:
            scenes += 1; expect = 'marker'; continue
        if w & 0x80000000:
            v = w & 0x7FFFFFFF
            camX.append(s16(v & 0xFFFF)); camY.append(s16((v >> 16) & 0x7FFF))
            frames += 1
            i += 8   # skip goals + anim-state dwords
            continue
        sidx.append(w >> 20); sx.append(s10((w >> 10) & 0x3FF)); sy.append(s10(w & 0x3FF))
    print(f"  frames={frames}  terminators={scenes}  sprites={len(sidx)}")
    if camX:
        print(f"  camX [{min(camX)}..{max(camX)}] first={camX[0]}   camY [{min(camY)}..{max(camY)}]")
    if sidx:
        print(f"  sprite idx [{min(sidx)}..{max(sidx)}]  x [{min(sx)}..{max(sx)}]  y [{min(sy)}..{max(sy)}]")
    print()

if __name__ == '__main__':
    for p in sys.argv[1:]:
        try: parse(p)
        except Exception as e: print(f"  ERROR {p}: {e}\n")
