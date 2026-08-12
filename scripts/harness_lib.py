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
  <game>_plugin  — game knowledge (symbols, menu layout, nav/_fire injection).
  <game>_harness — thin CLI.

Memory/register WRITES require a write-capable gdb stub. Native SDL gdb is fine;
the FS-UAE stub needed a patch to add M/P/G packets (see
docs/HARNESS.md "FS-UAE write support"). write_check() verifies a write stuck —
use it to fail loudly on a read-only target instead of silently no-op'ing.
"""
import os
import re
import struct
import subprocess
import sys
import time
import zlib

# gdb_proxy.py + amiga_gdb.sh live in the AmigaScope repo (split out of
# SWOS_C/scripts/amiga on 2026-07-19 — github.com/benbaker76/AmigaScope). Set
# AMIGASCOPE_DIR if your checkout is elsewhere; the old in-tree paths are kept
# as fallbacks so a stale checkout still resolves. swos_plugin.py derives
# amiga_gdb.sh from dirname(proxy), so pointing this at the repo fixes both.
PROXY_CANDIDATES = [
    os.path.join(os.environ["AMIGASCOPE_DIR"], "gdb_proxy.py")
    if os.environ.get("AMIGASCOPE_DIR") else None,
    "/mnt/c/Projects/AI/AmigaScope/gdb_proxy.py",                  # standalone repo
    "/mnt/c/Projects/AI/SWOS/SWOS_C/scripts/amiga/amigascope/gdb_proxy.py",  # submodule
    "/mnt/c/Projects/AI/SWOS/SWOS_C/scripts/amiga/gdb_proxy.py",   # pre-split
]
DEFAULT_PROXY = next(
    (p for p in PROXY_CANDIDATES if p and os.path.exists(p)),
    PROXY_CANDIDATES[1])

# A single `continue` to the next breakpoint should be sub-second with TURBO
# (and at most ~1 s without it). When the WSL2/gdb session degrades, the
# inferior's per-frame timer thread starves and conts balloon to 10-120 s
# (reads stay fast — it's the inferior frame, not command latency). Surface
# that the MOMENT it starts, not after a 40-step stuck-out, so the operator
# can act (verify TURBO is on; `wsl --shutdown` to reset a long-_up VM).
CONT_SLOW_WARN_S = 4.0


class GdbHarness:
    def __init__(self, proxy=DEFAULT_PROXY, timeout=8):
        if not os.path.exists(proxy):
            # Fail LOUDLY here: without this every cmd() silently returns
            # python3's "can't open file" on stderr, which parses as "no
            # symbol / not stopped" and looks like a wedged target.
            raise FileNotFoundError(
                "gdb_proxy.py not found at %r. It lives in the AmigaScope repo "
                "(github.com/benbaker76/AmigaScope) since the 2026-07-19 split "
                "out of SWOS_C/scripts/amiga. Set AMIGASCOPE_DIR to your "
                "checkout, or pass proxy=<path>." % proxy)
        self.proxy = proxy
        self.timeout = timeout
        self._slow_conts = 0   # consecutive slow conts (degradation tripwire)

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
        """True iff the target is cleanly STOPPED (booted + at a bp/signal, so
        memory is readable). The proxy verdict is 'OK stopped PC=...' ONLY when
        cleanly stopped; a not-booted OR running target reports
        'DEGRADED target not cleanly stopped'. A naive `"stopped" in health`
        false-positives on that DEGRADED string (it contains the substring
        'stopped'), which made ensure_stopped() believe a fresh, never-run daemon
        was already stopped and skip the cold-start boot — the recurring harness
        hang. Key on the OK verdict instead. Empty/ambiguous health -> False, so
        the caller (safely) primes/boots."""
        h = self.health()
        return h.startswith("OK") and "stopped" in h

    def waitstop(self, seconds=20):
        return self._simple("waitstop", str(seconds))

    # ---- value reads -------------------------------------------------------
    def value(self, expr, timeout=None):
        """`print expr` -> the _right-hand side string, or None."""
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
        """Set a global. Robust to this codebase's stub-global pattern where a
        logical scalar is DEFINED as a 1-element array (e.g.
        `uint16_t _fire[1]` / `_shortFire[1]` / `_menuControlsDirection[1]` in
        swos_amiga_data_imports.c) but extern'd as a plain scalar in the C. gdb
        types those from the DEFINITION (array), so `set var X = v` fails
        'Invalid cast' and the write silently no-ops — which dropped every menu
        _fire and hung the drivers. Retry `X[0] = v` (same memory). Genuine
        scalars (e.g. the `unsigned char` _up/_down/_left/_right) take the first
        form unchanged."""
        r = self.cmd(f"set variable {name} = {value}")
        if "Invalid cast" in r or "not an lvalue" in r:
            self.cmd(f"set variable {name}[0] = {value}")

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
        t0 = time.monotonic()
        out = self.cmd("continue", timeout)
        dt = time.monotonic() - t0
        if dt > CONT_SLOW_WARN_S:
            self._slow_conts += 1
            sys.stderr.write(
                f"[harness] SLOW CONT {dt:.1f}s (>{CONT_SLOW_WARN_S:.0f}s; "
                f"{self._slow_conts} in a row) — likely WSL2/gdb timer-thread "
                f"starvation. Check SWOS_HARNESS_TURBO is set on the inferior; "
                f"if it persists, `wsl --shutdown` to reset the VM.\n")
            sys.stderr.flush()
        else:
            self._slow_conts = 0
        return out

    def run_to(self, location, timeout=120, fresh=True):
        """Set a breakpoint at `location` and continue until it is hit."""
        if fresh:
            self.delete_breakpoints()
        self.break_at(location)
        self.cmd("continue", timeout=3)
        return self.waitstop(timeout)

    def inject(self, assignments, settle_timeout=20):
        """Apply {var: value} at the current stop, then continue one cycle.

        The caller is responsible for being stopped at the _right injection
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
