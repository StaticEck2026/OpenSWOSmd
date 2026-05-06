#!/usr/bin/env python3
"""
extract_amiga_addresses.py — Pull (symbol, address, size) triples from
the original SWOS Amiga static-build listing.

The listing file (`SWOS2.lst`) is the output of the `vasm` listing pass
for the static-image build of the original Amiga binary. It contains
every label, the absolute post-relocation address each label resolves
to, and the bytes/directives that follow. We turn that into a flat
`(name, amiga_addr, size, section)` table that the SDL/Amiga-C ports
use to translate save-file pointers between current-build addresses
and canonical Amiga addresses.

Listing line formats (relevant subset):

  - Label line, no address:
      <whitespace><tab>NNNN: <name>:
    e.g. "                            \t123795: euroCup:"

  - Data line, has address:
      SS:HHHHHHHH BBBBBB...   <tab>NNNN: <directive> ...
    where SS = section index (00/01/02), HHHHHHHH = absolute address,
    BBBBBB = up to 8 emitted bytes, NNNN = source line, <directive>
    is `dc.b`/`dc.w`/`dc.l`/`ds.b` etc.

  - Section-pointer marker (skip):
      SS:HHHHHHHH *

We pair each label with the address of the FIRST data line that
follows it (in the same section). Labels stacked back-to-back share
the same address (next data line). Size is derived as the address
delta to the *next label in the same section*; the last label in
each section gets size 0 (caller can extend if needed).

Usage:
  python3 tools/extract_amiga_addresses.py \\
      <path/to/SWOS2.lst> \\
      [-o tools/amiga_addresses.txt] \\
      [--c-out src/generated/amiga_address_map.c] \\
      [--filter <substring>]
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Optional


# Label-only line: any leading whitespace, then "NNNN: name:" — no
# address column. The leading whitespace before "NNNN:" must NOT
# contain a "SS:HHH" token (which would mean it's a data line).
LABEL_LINE_RE = re.compile(
    r"^\s+\d+:\s+([A-Za-z_][A-Za-z0-9_]*):\s*$"
)

# Data line: leading "SS:HHHHHHHH" then optional bytes, then a tab and
# "NNNN: <directive...>".
DATA_LINE_RE = re.compile(
    r"^(?P<sec>[0-9A-Fa-f]{2}):(?P<addr>[0-9A-Fa-f]{8})"
    r"(?:\s+(?P<bytes>[0-9A-Fa-f]+))?"
    r"\s+\d+:\s*(?P<dir>.*)$"
)

# Section-pointer marker (no source line): "SS:HHHH *" — skip.
SECTION_MARKER_RE = re.compile(
    r"^[0-9A-Fa-f]{2}:[0-9A-Fa-f]{8}\s+\*\s*$"
)


@dataclass
class Symbol:
    name: str
    section: int
    addr: int
    # filled in post-pass
    size: int = 0


def parse_lst(path: str) -> list[Symbol]:
    """Single forward pass over the listing.

    State machine:
      - `pending`: list of label names that are waiting for an address.
        When we see the next data line we assign its address to all of
        them and emit one Symbol per name.
    """
    symbols: list[Symbol] = []
    pending: list[str] = []

    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for raw_line in f:
            line = raw_line.rstrip("\n")
            if not line:
                continue

            m = LABEL_LINE_RE.match(line)
            if m:
                pending.append(m.group(1))
                continue

            if SECTION_MARKER_RE.match(line):
                continue

            m = DATA_LINE_RE.match(line)
            if m:
                if pending:
                    sec = int(m.group("sec"), 16)
                    addr = int(m.group("addr"), 16)
                    for name in pending:
                        symbols.append(Symbol(name, sec, addr))
                    pending.clear()
                continue

            # Comment, EQU line, blank — leave pending intact.
            # Note: an EQU sets a constant value, not a section
            # address; we deliberately skip those (the LUT only cares
            # about section-resident symbols).
            continue

    if pending:
        # Trailing labels with no following data — drop with a warning.
        print(
            f"warn: {len(pending)} labels at EOF have no data line:"
            f" {pending[:5]}{'...' if len(pending) > 5 else ''}",
            file=sys.stderr,
        )

    return symbols


def fill_sizes(symbols: list[Symbol]) -> None:
    """Compute each symbol's size as the gap to the next symbol in
    the same section. Last symbol per section keeps size=0.

    Symbols within a section may share the same address (stacked
    labels with no data between them) — for those, every member of the
    stack reports the same size (the gap from the shared address to
    the next *different* address)."""
    # Bucket by section, then sort by address; resolve gaps.
    by_section: dict[int, list[Symbol]] = defaultdict(list)
    for s in symbols:
        by_section[s.section].append(s)

    for sec, lst in by_section.items():
        lst.sort(key=lambda s: s.addr)
        # Group consecutive same-address entries.
        i = 0
        while i < len(lst):
            j = i
            while j + 1 < len(lst) and lst[j + 1].addr == lst[i].addr:
                j += 1
            # Find next different address.
            if j + 1 < len(lst):
                gap = lst[j + 1].addr - lst[i].addr
            else:
                gap = 0
            for k in range(i, j + 1):
                lst[k].size = gap
            i = j + 1


def write_text(symbols: list[Symbol], out_path: str) -> None:
    """Plain-text dump for inspection: `name addr size section`."""
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("# name  amiga_addr  size  section\n")
        f.write("# section: 00=BSSInChip, 01=text, 02=bss\n")
        for s in symbols:
            f.write(f"{s.name}  0x{s.addr:08X}  0x{s.size:X}  {s.section:02d}\n")


C_HEADER = """\
/*
 * GENERATED FILE — do not edit by hand.
 *
 * Source:  SWOS2.lst (original Amiga static-build listing)
 * Tool:    tools/extract_amiga_addresses.py
 *
 * Each entry maps an original-Amiga symbol name → that symbol's
 * Amiga absolute address → the current-build address of the same
 * symbol. The runtime LUT in src/save_pointer_lut.c uses this to
 * translate save-file pointer fields between Amiga-canonical and
 * current-build representations.
 *
 * See SAVE_DATA.md "Cross-build pointer LUT" for the design.
 */
#include <stdint.h>
#include <stddef.h>

/* Each LUT-resident symbol is forward-declared as `extern char name[]`
 * for the sole purpose of taking its address. The actual definition
 * lives elsewhere in the codebase (often as `uint32_t` or a struct);
 * the array-of-char declaration is compatible enough for address
 * taking under our build flags (-fno-strict-aliasing). The compiler
 * never accesses the contents through this declaration. */
"""

C_BINDINGS_PREAMBLE = """\

typedef struct AmigaAddrLutEntry {
    const char *name;
    void       *current_addr;   /* &symbol in this build */
    uint32_t    amiga_addr;     /* address in original Amiga binary */
    uint32_t    size;           /* declared size of symbol (0 = unknown) */
} AmigaAddrLutEntry;

const AmigaAddrLutEntry g_amiga_addr_map[] = {
"""

C_FOOTER = """\
};

const size_t g_amiga_addr_map_count =
    sizeof(g_amiga_addr_map) / sizeof(g_amiga_addr_map[0]);
"""


def _is_filler_label(name: str) -> bool:
    """Skip auto-generated `BSS_NN_HHHH:` filler labels — the
    disassembler's 'unnamed gap' markers, not real symbols."""
    return name.startswith("BSS_") and "_" in name[4:]


def write_c(symbols: list[Symbol], out_path: str) -> None:
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(C_HEADER)

        # Forward-declare every symbol we'll reference. Use
        # `extern char name[]` — see comment in C_HEADER.
        for s in symbols:
            if _is_filler_label(s.name):
                continue
            f.write(f"extern char {s.name}[];\n")

        f.write(C_BINDINGS_PREAMBLE)

        for s in symbols:
            if _is_filler_label(s.name):
                continue
            name_esc = s.name.replace('"', '\\"')
            f.write(
                f'    {{ "{name_esc}", (void *){s.name},'
                f' 0x{s.addr:08X}u, 0x{s.size:X}u }},\n'
            )

        f.write(C_FOOTER)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("lst_path", help="Path to SWOS2.lst")
    p.add_argument(
        "-o",
        "--out",
        default="tools/amiga_addresses.txt",
        help="Plain-text dump path (default: tools/amiga_addresses.txt)",
    )
    p.add_argument(
        "--c-out",
        default=None,
        help="If set, also emit a C array at this path",
    )
    p.add_argument(
        "--filter",
        default=None,
        help="Only keep symbols whose name contains this substring",
    )
    p.add_argument(
        "--needed",
        default=None,
        help=("Path to a text file with one symbol name per line; only "
              "those names are kept. Lines starting with '#' or empty "
              "lines are ignored. Names not found in the listing emit "
              "a warning."),
    )
    args = p.parse_args()

    symbols = parse_lst(args.lst_path)
    fill_sizes(symbols)

    if args.filter:
        needle = args.filter
        symbols = [s for s in symbols if needle in s.name]

    if args.needed:
        wanted: set[str] = set()
        with open(args.needed, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                wanted.add(line)
        found = {s.name for s in symbols}
        missing = wanted - found
        if missing:
            print(
                f"warn: {len(missing)} requested symbols not in listing:\n  "
                + "\n  ".join(sorted(missing)[:20])
                + ("\n  ..." if len(missing) > 20 else ""),
                file=sys.stderr,
            )
        symbols = [s for s in symbols if s.name in wanted]

    # Sort for stable output: section, then address, then name.
    symbols.sort(key=lambda s: (s.section, s.addr, s.name))

    write_text(symbols, args.out)
    print(f"wrote {len(symbols)} symbols to {args.out}", file=sys.stderr)

    if args.c_out:
        write_c(symbols, args.c_out)
        print(f"wrote C table to {args.c_out}", file=sys.stderr)

    # Quick sanity: print a few well-known symbols.
    known = ("euroCup", "euroCupCopy", "careerFileBuffer", "aLeague_1",
             "competitionFileBuffer", "currentSeasonShowing",
             "competitionsTable", "careerCupPickList")
    name_to = {s.name: s for s in symbols}
    for k in known:
        s = name_to.get(k)
        if s:
            print(
                f"  {k:30s} 0x{s.addr:08X}  size 0x{s.size:X}  sec {s.section:02d}",
                file=sys.stderr,
            )
        else:
            print(f"  {k:30s} NOT FOUND", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
