#!/usr/bin/env python3
"""Export the compiled UI strings to a per-language translation table.

Scans src/strings_data.c + src/swos_amiga_strings.c for `char NAME[] = "TEXT";`
definitions and writes bin/data_json/translations/en.json — a { symbol -> text }
map that the runtime tr() lookup (src/translations.c) resolves menu/UI text
through. en.json IS the English baseline (text identical to the compiled
strings); other languages are copies of it with the values translated.

This is the string analogue of the menu/competition JSON: the data files carry
the symbol NAME (e.g. "aDivision1"), never the literal text, and the language
file maps name -> text. bin/data_json is gitignored/regenerated; this script is
the source of truth.

Run from the repo root:  python3 scripts/export_translations.py
"""
import os
import re
import json

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRCS = [os.path.join(REPO, "src", "strings_data.c"),
        os.path.join(REPO, "src", "swos_amiga_strings.c")]
OUT_DIR = os.path.join(REPO, "bin", "data_json", "translations")

# `[const] char NAME[opt-size] = "BODY";` on one line. BODY allows C escapes.
DEF = re.compile(r'^\s*(?:const\s+)?char\s+([A-Za-z_]\w*)\s*\[\s*\d*\s*\]\s*=\s*'
                 r'"((?:[^"\\]|\\.)*)"\s*;', re.M)


def decode_c(body: str) -> str:
    """Decode the handful of C escapes that show up in SWOS strings."""
    return (body.replace('\\"', '"').replace('\\\\', '\\')
                .replace('\\n', '\n').replace('\\t', '\t').replace('\\r', '\r'))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    strings = {}
    dups = 0
    for src in SRCS:
        text = open(src, encoding="utf-8", errors="replace").read()
        for m in DEF.finditer(text):
            name, body = m.group(1), decode_c(m.group(2))
            if name in strings:
                dups += 1
                continue            # first definition wins
            strings[name] = body

    out = {"language": "en", "strings": dict(sorted(strings.items()))}
    path = os.path.join(OUT_DIR, "en.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print("wrote %s: %d strings (%d duplicate names skipped)"
          % (os.path.relpath(path, REPO), len(strings), dups))


if __name__ == "__main__":
    main()
