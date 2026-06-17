#!/usr/bin/env python3
"""Generic gdb-driven test-harness primitives over the gdb_proxy daemon.

This module is GAME-AGNOSTIC — it knows nothing about SWOS. It is the reusable
"harness framework" layer on top of the generic gdb_proxy.py transport, so it
can be lifted to any project that drives a target through gdb_proxy. A
game-specific PLUGIN (e.g. swos_plugin.py) supplies the symbol names, struct
layouts, injection points, and screen geometry.

Layers (see docs/HARNESS.md):
  gdb_proxy.py   — generic transport: one persistent gdb session, cmd/waitstop/
                   health, SDL (native) or Amiga (FS-UAE remote) mode.
  harness_lib.py — THIS: generic primitives (read/write vars, breakpoints,
                   input injection = break+set+continue, framebuffer capture).
  <game>_plugin  — game knowledge (symbols, menu layout, nav/fire injection).
  <game>_harness — thin CLI.

Memory/register WRITES require a write-capable gdb stub. Native SDL gdb is fine;
the FS-UAE stub needed a patch to add M/P/G packets (see
docs/HARNESS.md "FS-UAE write support"). write_check() verifies a write stuck —
use it to fail loudly on a read-only target instead of silently no-op'ing.
"""
import re
import struct
import subprocess
import zlib

DEFAULT_PROXY = "/mnt/c/Projects/AI/SWOS/SWOS_C/gdb_proxy.py"


class GdbHarness:
    def __init__(self, proxy=DEFAULT_PROXY, timeout=8):
        self.proxy = proxy
        self.timeout = timeout

    # ---- transport ---------------------------------------------------------
    def cmd(self, command, timeout=None):
        """Send one gdb command; return its combined stdout+stderr text."""
        r = subprocess.run(
            ["python3", self.proxy, "cmd", command,
             "--timeout", str(timeout if timeout is not None else self.timeout)],
            capture_output=True, text=True)
        return r.stdout + r.stderr

    def _simple(self, *args):
        return subprocess.run(["python3", self.proxy, *args],
                              capture_output=True, text=True).stdout

    def health(self):
        return self._simple("health").strip()

    def is_stopped(self):
        return "stopped" in self.health()

    def waitstop(self, seconds=20):
        return self._simple("waitstop", str(seconds))

    # ---- value reads -------------------------------------------------------
    def value(self, expr, timeout=None):
        """`print expr` -> the right-hand side string, or None."""
        m = re.search(r"\$\d+\s*=\s*(.+)", self.cmd(f"print {expr}", timeout))
        return m.group(1).strip() if m else None

    def int_value(self, expr, timeout=None):
        """`print expr` parsed as an int (handles 0x.., decimals, char fmt)."""
        v = self.value(expr, timeout)
        if v is None:
            return None
        m = re.search(r"0x[0-9a-fA-F]+", v)
        if m:
            return int(m.group(0), 16)
        m = re.search(r"-?\d+", v)
        return int(m.group(0)) if m else None

    def symbol(self, expr, timeout=None):
        """Resolve a function-pointer expr to its <symbol> name, or None."""
        m = re.search(r"<([^>+]+)", self.cmd(f"print {expr}", timeout))
        return m.group(1) if m else None

    # ---- writes ------------------------------------------------------------
    def set_var(self, name, value):
        self.cmd(f"set variable {name} = {value}")

    def write_check(self, name, value):
        """set var, read back; return (ok, readback). Detects read-only stubs."""
        self.set_var(name, value)
        rb = self.int_value(name)
        return (rb == value, rb)

    def write_bytes(self, addr_expr, data):
        """Write raw bytes to memory via per-byte sets (no inferior malloc)."""
        for i, b in enumerate(data):
            self.cmd(f"set {{unsigned char}}(({addr_expr})+{i}) = {b}")

    # ---- breakpoints / execution ------------------------------------------
    def break_at(self, location):
        self.cmd(f"break {location}")

    def delete_breakpoints(self):
        self.cmd("delete")

    def cont(self, timeout=20):
        return self.cmd("continue", timeout)

    def run_to(self, location, timeout=120, fresh=True):
        """Set a breakpoint at `location` and continue until it is hit."""
        if fresh:
            self.delete_breakpoints()
        self.break_at(location)
        self.cmd("continue", timeout=3)
        return self.waitstop(timeout)

    def inject(self, assignments, settle_timeout=20):
        """Apply {var: value} at the current stop, then continue one cycle.

        The caller is responsible for being stopped at the right injection
        breakpoint first (the value reaches whatever reads it before the next
        breakpoint hit). Returns the continue output."""
        for var, val in assignments.items():
            self.set_var(var, val)
        return self.cont(settle_timeout)

    # ---- raw memory dump ---------------------------------------------------
    def dump_mem(self, addr_expr, nbytes, path="/tmp/harness_dump.bin"):
        """gdb `dump binary memory` of nbytes from addr_expr; return the bytes."""
        r = self.cmd(f"dump binary memory {path} (char*)({addr_expr}) "
                     f"((char*)({addr_expr}) + {nbytes})", timeout=20)
        if "error" in r.lower() or "No symbol" in r:
            raise RuntimeError(f"memory dump failed ({addr_expr}):\n" + r)
        data = open(path, "rb").read()
        if len(data) < nbytes:
            raise RuntimeError(f"short dump: {len(data)}/{nbytes} from {addr_expr}")
        return data

    # ---- framebuffer capture (generic chunky 8bpp + palette -> PNG) --------
    def screencap(self, out_png, buffer_expr, w, h, stride,
                  palette_expr=None, pal_fmt="rgba"):
        """Dump an 8bpp chunky framebuffer + palette over gdb and write a PNG.

        buffer_expr : gdb expr for the framebuffer base (char*).
        palette_expr: gdb expr for a 256-entry palette table, or None (greyscale).
        """
        fb_bin, pal_bin = "/tmp/harness_fb.bin", "/tmp/harness_pal.bin"
        n = stride * h
        r = self.cmd(f"dump binary memory {fb_bin} (char*)({buffer_expr}) "
                     f"((char*)({buffer_expr}) + {n})", timeout=20)
        if "error" in r.lower() or "No symbol" in r:
            raise RuntimeError("framebuffer dump failed:\n" + r)
        pal = None
        if palette_expr:
            self.cmd(f"dump binary memory {pal_bin} "
                     f"(char*)({palette_expr}) ((char*)({palette_expr}) + 1024)")
            try:
                pal = _load_palette(open(pal_bin, "rb").read(), pal_fmt)
            except OSError:
                pal = None
        fb = open(fb_bin, "rb").read()
        if len(fb) < n:
            raise RuntimeError(f"short framebuffer read: {len(fb)}/{n}")
        rows = []
        for y in range(h):
            base = y * stride
            row = bytearray()
            for x in range(w):
                idx = fb[base + x]
                row += bytes(pal[idx]) if pal else bytes((idx, idx, idx))
            rows.append(row)
        _write_png(out_png, w, h, rows)
        nonzero = sum(1 for b in fb[:n] if b)
        return f"wrote {out_png} {w}x{h} ({nonzero} non-zero px of {n})"


def _load_palette(data, fmt):
    layouts = {"rgba": (4, 0, 1, 2), "argb": (4, 1, 2, 3), "rgb": (3, 0, 1, 2)}
    step, ro, go, bo = layouts[fmt]
    pal = []
    for i in range(256):
        o = i * step
        pal.append((data[o + ro], data[o + go], data[o + bo])
                   if o + step <= len(data) else (0, 0, 0))
    return pal


def _write_png(path, w, h, rgb_rows):
    def chunk(typ, data):
        body = typ + data
        return (struct.pack(">I", len(data)) + body +
                struct.pack(">I", zlib.crc32(body) & 0xffffffff))
    raw = bytearray()
    for row in rgb_rows:
        raw.append(0)
        raw.extend(row)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)
