#!/usr/bin/env python3
"""SWOS gdb-driven screen capture (Phase 1 of the test harness).

Reads the live frontbuffer (8bpp chunky) + palette from a running game over the
gdb_proxy daemon and renders a PNG — no OS screenshot tool needed, so it works
identically for the SDL build and the Amiga build under FS-UAE (each game's own
frontbuffer is read via gdb). Pairs with the gdb-driven input injection so the
harness can drive menus and "see" the result.

The target must be STOPPED (gdb all-stop) when this runs. Interrupt first:
  SDL:   pkill -INT -x SWOS.x86_64    then  gdb_proxy.py waitstop 8
  Amiga: pkill -INT -f '[m]68k-amiga-elf-gdb'  then  gdb_proxy.py waitstop 8

What it reads (matches platform_sdl.c chunky_to_rgba):
  - frame: _frontbufferBaseAltPtr's buffer (the *displayed* frame), 336x272,
    row stride 336, one byte/pixel = palette index. (_usePlaneOffsetFlag /
    _useAltBaseFlag pick a different buffer in some transient paths; we read the
    default Alt buffer, correct for menus — the harness's use case.)
  - palette: SDL s_palette[256] (RGBA). Amiga palette (from the copperlist) is
    a TODO — pass --pal-amiga once that's wired.

Usage:
  scripts/swos_screencap.py OUT.png [--proxy PATH] [--pal-fmt rgba]
"""
import argparse
import os
import struct
import subprocess
import sys
import zlib

W, H, STRIDE = 336, 272, 336
# Single source of truth for the gdb_proxy.py location (AmigaScope repo since
# the 2026-07-19 split) — see harness_lib.PROXY_CANDIDATES / AMIGASCOPE_DIR.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness_lib import DEFAULT_PROXY  # noqa: E402
FB_BIN = "/tmp/swos_fb.bin"
PAL_BIN = "/tmp/swos_pal.bin"


def gdb(proxy, command, timeout=15):
    """Run one gdb command via the proxy; return its text output."""
    out = subprocess.run(
        ["python3", proxy, "cmd", command, "--timeout", str(timeout)],
        capture_output=True, text=True)
    return out.stdout + out.stderr


def load_palette(data, fmt):
    layouts = {"rgba": (4, 0, 1, 2), "argb": (4, 1, 2, 3), "rgb": (3, 0, 1, 2)}
    step, ro, go, bo = layouts[fmt]
    pal = []
    for i in range(256):
        o = i * step
        pal.append((data[o + ro], data[o + go], data[o + bo])
                   if o + step <= len(data) else (0, 0, 0))
    return pal


def write_png(path, w, h, rgb_rows):
    def chunk(typ, data):
        body = typ + data
        return (struct.pack(">I", len(data)) + body +
                struct.pack(">I", zlib.crc32(body) & 0xffffffff))
    raw = bytearray()
    for row in rgb_rows:
        raw.append(0)            # filter type 0 (none)
        raw.extend(row)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--proxy", default=DEFAULT_PROXY)
    ap.add_argument("--pal-fmt", default="rgba")
    a = ap.parse_args(argv[1:])

    nbytes = STRIDE * H
    r = gdb(a.proxy, f'dump binary memory {FB_BIN} '
                     f'(char*)_frontbufferBaseAltPtr '
                     f'((char*)_frontbufferBaseAltPtr + {nbytes})')
    if "error" in r.lower() or "No symbol" in r:
        print("FB dump failed:\n" + r)
        return 1
    gdb(a.proxy, f'dump binary memory {PAL_BIN} &s_palette[0] &s_palette[256]')

    with open(FB_BIN, "rb") as f:
        fb = f.read()
    with open(PAL_BIN, "rb") as f:
        pal = load_palette(f.read(), a.pal_fmt)
    if len(fb) < nbytes:
        print(f"short FB read: {len(fb)}/{nbytes}")
        return 1

    rows = []
    for y in range(H):
        base = y * STRIDE
        row = bytearray()
        for x in range(W):
            r_, g_, b_ = pal[fb[base + x]]
            row += bytes((r_, g_, b_))
        rows.append(row)
    write_png(a.out, W, H, rows)
    nonzero = sum(1 for b in fb[:nbytes] if b)
    print(f"wrote {a.out} {W}x{H}  ({nonzero} non-zero px of {nbytes})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
