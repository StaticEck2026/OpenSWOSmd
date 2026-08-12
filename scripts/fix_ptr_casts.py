#!/usr/bin/env python3
"""Mechanically widen (uint32_t)<ptr-expr> casts to (uintptr_t).

Parses gcc Wpointer-to-int-cast warnings out of a captured build log,
and at each warning's exact column replaces the `(uint32_t)` token
with `(uintptr_t)`. On 32-bit hosts uintptr_t == uint32_t so the result
is identical; on 64-bit the cast no longer truncates a pointer.

Wint-to-pointer-cast warnings are *reported* but never auto-fixed,
since the fix usually requires retyping the source variable's storage
declaration (which the warning column can't reach).

Default mode is --dry-run: shows the planned edit per warning, no
files are written. Pass --apply to actually rewrite the files (with
a .bak alongside each rewritten source file).

Usage:
    # capture warnings:
    make ARM64=1 SWOS_SAVE_FORMAT_JSON=1 -j$(nproc) &> /tmp/build.log

    # see what would change:
    python3 scripts/fix_ptr_casts.py --build-log /tmp/build.log

    # apply for real:
    python3 scripts/fix_ptr_casts.py --build-log /tmp/build.log --apply
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

# gcc emits:  src/foo.c:NN:CC: warning: cast from pointer to integer of different _size [-Wpointer-to-int-cast]
WARN_RE = re.compile(
    r"^(?P<file>[^\s:]+):(?P<line>\d+):(?P<col>\d+):\s*warning:\s*"
    r"(?P<msg>cast (?:from pointer to integer|to pointer from integer) of different _size)\s*"
    r"\[-W(?P<kind>pointer-to-int-cast|int-to-pointer-cast)\]"
)


def parse_warnings(log_path: Path):
    """Yield (file, line, col, kind) tuples for each pointer-cast warning."""
    seen = set()
    with log_path.open(encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            m = WARN_RE.match(raw.strip())
            if not m:
                continue
            key = (m["file"], int(m["line"]), int(m["col"]), m["kind"])
            # Some files compile twice (-j overlap) — dedupe identical hits.
            if key in seen:
                continue
            seen.add(key)
            yield key


def plan_edit(line: str, col: int):
    """Return (new_line, before_token, after_token) or None if no edit.

    col is 1-based (gcc convention). We look for `(uint32_t)` ending at
    or starting near col, and replace it with `(uintptr_t)`. Because
    gcc points at the *expression being cast* (_right after the cast),
    the token usually sits in columns col-len("(uint32_t)")..col-1.
    """
    target = "(uint32_t)"
    repl = "(uintptr_t)"

    # Try cast that ends just before column (most common: warning points
    # at the operand). Search backwards from col-1 for the token.
    idx = col - 1  # 0-based index of warning marker
    # Find the (uint32_t) token whose closing paren is at or before idx,
    # and whose operand starts at idx exactly.
    start = idx - len(target)
    if 0 <= start <= len(line) - len(target) and line[start:start + len(target)] == target:
        return (line[:start] + repl + line[start + len(target):], target, repl)

    # Fall back: search the line for *any* (uint32_t) closest to col.
    candidates = [m.start() for m in re.finditer(re.escape(target), line)]
    if not candidates:
        return None
    candidates.sort(key=lambda s: abs(s - idx))
    s = candidates[0]
    return (line[:s] + repl + line[s + len(target):], target, repl)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build-log", required=True, type=Path)
    ap.add_argument("--apply", action="store_true",
                    help="rewrite files (default = dry-run / show diff)")
    ap.add_argument("--root", type=Path, default=Path("."),
                    help="project root (file paths in the log are resolved here)")
    ap.add_argument("--limit", type=int, default=0,
                    help="cap the number of edits (0 = no cap)")
    args = ap.parse_args(argv)

    if not args.build_log.is_file():
        sys.exit(f"build log not found: {args.build_log}")

    # Group planned edits per file. Each entry: (line_no_1based, col, kind)
    per_file: dict[Path, list[tuple[int, int, str]]] = defaultdict(list)
    int_to_ptr_followups: list[tuple[str, int, int]] = []

    for fpath, line_no, col, kind in parse_warnings(args.build_log):
        if kind == "int-to-pointer-cast":
            int_to_ptr_followups.append((fpath, line_no, col))
            continue
        per_file[args.root / fpath].append((line_no, col, kind))

    if not per_file and not int_to_ptr_followups:
        print("No pointer-cast warnings parsed — nothing to do.")
        return 0

    total_edits = 0
    skipped_no_token = 0
    skipped_already_done = 0

    edited_files: list[Path] = []
    for fpath in sorted(per_file):
        if not fpath.is_file():
            print(f"!! missing {fpath}, skipping {len(per_file[fpath])} edits")
            continue
        text = fpath.read_text(encoding="utf-8", errors="replace")
        lines = text.splitlines(keepends=True)

        # Group warnings by line so we can apply multiple edits per line
        # _right-to-_left (so earlier-column edits don't shift later ones).
        by_line: dict[int, list[int]] = defaultdict(list)
        for ln, col, _ in per_file[fpath]:
            by_line[ln].append(col)

        file_edits = 0
        for ln in sorted(by_line):
            if ln < 1 or ln > len(lines):
                continue
            cols = sorted(set(by_line[ln]), reverse=True)
            new_line = lines[ln - 1]
            line_edits = 0
            for col in cols:
                if args.limit and total_edits + line_edits >= args.limit:
                    break
                planned = plan_edit(new_line, col)
                if planned is None:
                    skipped_no_token += 1
                    continue
                edited, _before, _after = planned
                if edited == new_line:
                    skipped_already_done += 1
                    continue
                new_line = edited
                line_edits += 1

            if line_edits == 0:
                continue

            print(f"--- {fpath}:{ln}")
            print(f"-    {lines[ln - 1].rstrip()}")
            print(f"+    {new_line.rstrip()}")
            lines[ln - 1] = new_line
            file_edits += line_edits
            total_edits += line_edits

            if args.limit and total_edits >= args.limit:
                print(f"(limit {args.limit} reached, stopping)")
                break

        if file_edits and args.apply:
            backup = fpath.with_suffix(fpath.suffix + ".bak")
            if not backup.exists():
                backup.write_text(text, encoding="utf-8")
            fpath.write_text("".join(lines), encoding="utf-8")
            edited_files.append(fpath)
            print(f"#  wrote {fpath} ({file_edits} edit{'s' if file_edits != 1 else ''}), "
                  f"backup → {backup}")
        elif file_edits:
            print(f"#  would write {fpath} ({file_edits} edit{'s' if file_edits != 1 else ''})")

        if args.limit and total_edits >= args.limit:
            break

    # Footer
    mode = "APPLIED" if args.apply else "DRY-RUN"
    print()
    print(f"==== {mode} summary ====")
    print(f"  Wpointer-to-int-cast edits planned : {total_edits}")
    print(f"  warnings skipped (no token at col) : {skipped_no_token}")
    print(f"  warnings skipped (already widened) : {skipped_already_done}")
    print(f"  Wint-to-pointer-cast for manual    : {len(int_to_ptr_followups)}")
    print(f"  files touched                      : {len(edited_files) if args.apply else len(per_file)}")
    if not args.apply:
        print("  re-run with --apply to write changes.")

    if int_to_ptr_followups:
        followup_path = Path("/tmp/fix_ptr_casts_int_to_ptr_followups.txt")
        with followup_path.open("w") as fh:
            for fpath, ln, col in int_to_ptr_followups:
                fh.write(f"{fpath}:{ln}:{col}\n")
        print(f"  int-to-pointer sites recorded at: {followup_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
