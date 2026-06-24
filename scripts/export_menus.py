#!/usr/bin/env python3
"""Export src/menu_data.c menu instances to JSON (menu data-jailbreak bootstrap).

See docs/MENUS.md. This is the Python bootstrap exporter described in §7a: it
parses the 46 hand-written `Menu_Foo foo = { ... };` static initializers in
src/menu_data.c and emits one JSON file per menu to bin/data_json/menus/, plus a
`_symbols.json` listing every distinct callback / string / table / sprite-copy /
buffer name the menus reference (the _seed for the generated menu_symbols.c
registry).

Why a flat re-parse instead of brace-walking: the C initializers are the IDA
"one struct per instance" layout (Menu_SWOSMainMenu, MenuEntry_Byte_* ...). The
brace nesting is irregular (some entries are wrapped, some flattened inline), so
we IGNORE braces entirely: strip comments, drop all `{`/`}`, split the remaining
values on commas, then walk the flat token stream with the SAME state machine as
UnpackMenu (src/menu_system.c) / the IDA beautifiers (sws_menu*.{py,idc}):

    header(4) -> { markers | entries }* -> end-menu

Every pointer becomes a SYMBOL NAME string (never an address) per the CLAUDE.md
"pointers as semantic identity" rule; numeric flag words are emitted as hex
strings for readability. The end-menu marker is implicit (the emitter re-appends
it); coords-mode flips are explicit `coords_mode` items so the byte stream
round-trips exactly.

Run from the repo root:  python3 scripts/export_menus.py
"""
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, "src", "menu_data.c")
OUT_DIR = os.path.join(REPO, "bin", "data_json", "menus")

# ----------------------------------------------------------------- opcode table
# name -> (json_op, [field-spec, ...]) where each field-spec is (key, kind):
#   kind 'int'  -> decoded integer (coords/nav/number/skip/sprite index)
#   kind 'flags'-> hex string (color/string flags/control mask)
#   kind 'ptr'  -> symbol name string | null | numeric literal
# The trailing MENU_CMD_END (0) closes the entry and is handled by the walker.
OPCODES = {
    "MENU_CMD_INVISIBLE":     ("invisible",     []),
    "MENU_CMD_NAV":           ("nav",           [("_left", "int"), ("_right", "int"),
                                                  ("_up", "int"), ("_down", "int")]),
    "MENU_CMD_BG_FUNC":       ("bg_func",       [("func", "ptr")]),
    "MENU_CMD_COLORS":        ("colors",        [("value", "colorpair")]),
    "MENU_CMD_BG_SPRITE":     ("sprite_bg",     [("index", "int")]),
    "MENU_CMD_FG_FUNC":       ("fg_func",       [("func", "ptr")]),
    "MENU_CMD_TEXT_PTR":      ("text",          [("flags", "flags"), ("str", "ptr")]),
    "MENU_CMD_SPRITE_FG":     ("sprite_fg",     [("index", "int")]),
    "MENU_CMD_STRING_TABLE":  ("string_table",  [("flags", "flags"), ("table", "ptr")]),
    "MENU_CMD_MULTILINE":     ("multiline",     [("flags", "flags"), ("str", "ptr")]),
    "MENU_CMD_NUMBER":        ("number",        [("flags", "flags"), ("value", "int")]),
    "MENU_CMD_ONSELECT_FIRE": ("onselect_fire", [("func", "ptr")]),
    "MENU_CMD_INPUT_FUNC":    ("input_func",    [("mask", "flags"), ("func", "ptr")]),
    "MENU_CMD_BEFORE_DRAW":   ("before_draw",   [("func", "ptr")]),
    "MENU_CMD_AFTER_DRAW":    ("after_draw",    [("func", "ptr")]),
    "MENU_CMD_UP_SKIP":       ("up_skip",       [("skip", "int"), ("dir", "int")]),
    "MENU_CMD_DOWN_SKIP":     ("down_skip",     [("skip", "int"), ("dir", "int")]),
    "MENU_CMD_LEFT_SKIP":     ("left_skip",     [("skip", "int"), ("dir", "int")]),
    "MENU_CMD_RIGHT_SKIP":    ("right_skip",    [("skip", "int"), ("dir", "int")]),
    "MENU_CMD_SPRITE_COPY":   ("sprite_copy",   [("sprite", "ptr")]),
    "MENU_CMD_TEXT_NO_PTR":   ("text_no_ptr",   [("flags", "flags")]),
}
# the JSON op key a pointer field rides on -> registry kind for that symbol
PTR_FIELD_KIND = {
    "func": "functions", "str": "strings", "table": "tables",
    "sprite": "sprite_copies",
}

CAST_RE = re.compile(r"^\(\s*[A-Za-z_][A-Za-z0-9_ ]*\*\s*\)\s*")
MENU_RE = re.compile(r"^(?:static\s+)?Menu_\w+\s+(\w+)\s*=\s*\{", re.M)


# ------------------------------------------------------------------- token prep
def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def menu_body(text, open_brace_idx):
    """Return the body string between the menu's outer { } via brace balancing."""
    depth = 0
    for i in range(open_brace_idx, len(text)):
        c = text[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[open_brace_idx + 1:i]
    raise ValueError("unbalanced braces starting at %d" % open_brace_idx)


def flatten(body):
    """Drop all braces, split on commas -> list of value tokens (no empties)."""
    body = body.replace("{", " ").replace("}", " ")
    return [t.strip() for t in body.split(",") if t.strip()]


# -------------------------------------------------------------- value decoders
def parse_int(tok):
    t = tok.strip().rstrip("uUlL")
    return int(t, 0)


def parse_flags(tok):
    return "0x%X" % (parse_int(tok) & 0xFFFFFFFF)


def parse_ptr(tok, symbols, kind):
    """A pointer slot -> None | symbol-name str | numeric literal int.

    Casts ((char*)/(int8_t*)/...) and a leading '&' are stripped; dotted
    field refs (_diyCompetitionStart.base.competitionName) are kept whole and
    treated as buffers. Collects discovered symbols into `symbols`."""
    t = CAST_RE.sub("", tok.strip())
    had_cast = t != tok.strip()
    t = t.lstrip("&").strip()
    if t in ("0", "0x0", "NULL"):
        return None
    # pure numeric literal (e.g. deferred 0xFFFFFFFF) -> emit as int
    if re.fullmatch(r"[+-]?(?:0[xX][0-9A-Fa-f]+|\d+)", t):
        return parse_int(t)
    bucket = "buffers" if (had_cast or "." in t) else PTR_FIELD_KIND[kind]
    symbols.setdefault(bucket, {})[t] = None
    return t


# ------------------------------------------------------------------- the walker
class Walker:
    def __init__(self, toks, name):
        self.t = toks
        self.i = 0
        self.name = name

    def peek(self):
        return self.t[self.i]

    def next(self):
        v = self.t[self.i]
        self.i += 1
        return v

    def at_end(self):
        return self.i >= len(self.t)


def parse_entry(w, symbols, is_template):
    x = parse_int(w.next())
    y = parse_int(w.next())
    ww = parse_int(w.next())
    h = parse_int(w.next())
    entry = {"kind": "entry", "coords": {"x": x, "y": y, "w": ww, "h": h}}
    if is_template:
        entry["template"] = True
    cmds = []
    while True:
        op = w.next()
        if op == "MENU_CMD_END":
            break
        if op not in OPCODES:
            raise ValueError("%s: unexpected opcode token %r at %d"
                             % (w.name, op, w.i - 1))
        json_op, fields = OPCODES[op]
        cmd = {"op": json_op}
        for key, knd in fields:
            tok = w.next()
            if knd == "int":
                cmd[key] = parse_int(tok)
            elif knd == "flags":
                cmd[key] = parse_flags(tok)
            elif knd == "colorpair":   # split the colors byte: hi nibble=frame, lo=background
                b = parse_int(tok) & 0xFF
                cmd["frame_color"] = (b >> 4) & 0x0F
                cmd["background_color"] = b & 0x0F
            else:  # ptr
                cmd[key] = parse_ptr(tok, symbols, key)
        cmds.append(cmd)
    entry["commands"] = cmds
    return entry


def parse_menu(name, body, symbols):
    toks = flatten(body)
    w = Walker(toks, name)
    menu = {
        "name": name,
        "onInit":   parse_ptr(w.next(), symbols, "func"),
        "onReturn": parse_ptr(w.next(), symbols, "func"),
        "onDraw":   parse_ptr(w.next(), symbols, "func"),
        "defaultEntry": parse_int(w.next()),
        "items": [],
    }
    items = menu["items"]
    pending_template = False
    while not w.at_end():
        tok = w.peek()
        if tok == "MENU_MARK_END_MENU":
            w.next()
            break
        if tok == "MENU_MARK_BYTE_COORDS":
            w.next(); items.append({"kind": "coords_mode", "wide": False}); continue
        if tok == "MENU_MARK_WIDE_COORDS":
            w.next(); items.append({"kind": "coords_mode", "wide": True}); continue
        if tok == "MENU_MARK_START_MENU":
            w.next()
            items.append({"kind": "set_origin",
                          "x": parse_int(w.next()), "y": parse_int(w.next())})
            continue
        if tok == "MENU_MARK_INIT_ENTRY":
            w.next(); items.append({"kind": "reset_template"}); continue
        if tok == "MENU_MARK_START_TEMPLATE":
            w.next(); items.append({"kind": "start_template"})
            pending_template = True
            continue
        # anything else begins an entry
        items.append(parse_entry(w, symbols, pending_template))
        pending_template = False
    if not w.at_end():
        leftover = w.t[w.i:w.i + 6]
        print("  WARN %s: %d tokens after end-menu: %r"
              % (name, len(w.t) - w.i, leftover))
    return menu


# -------------------------------------------------------------------------- run
def main():
    with open(SRC, encoding="utf-8", errors="replace") as f:
        text = strip_comments(f.read())

    os.makedirs(OUT_DIR, exist_ok=True)
    symbols = {}
    menus = []
    for m in MENU_RE.finditer(text):
        name = m.group(1)
        open_idx = text.index("{", m.start())
        body = menu_body(text, open_idx)
        menu = parse_menu(name, body, symbols)
        menus.append(menu)
        path = os.path.join(OUT_DIR, name + ".json")
        with open(path, "w", encoding="utf-8") as out:
            json.dump(menu, out, indent=2)
            out.write("\n")

    # symbol manifest (sorted names per bucket) for the menu_symbols.c generator
    manifest = {b: sorted(symbols.get(b, {}))
                for b in ("functions", "strings", "tables",
                          "sprite_copies", "buffers")}
    with open(os.path.join(OUT_DIR, "_symbols.json"), "w", encoding="utf-8") as out:
        json.dump(manifest, out, indent=2)
        out.write("\n")

    n_entries = sum(1 for mu in menus for it in mu["items"]
                    if it["kind"] == "entry")
    print("Exported %d menus, %d entries -> %s"
          % (len(menus), n_entries, os.path.relpath(OUT_DIR, REPO)))
    for b, names in manifest.items():
        print("  %-14s %d" % (b, len(names)))
    if len(menus) != 47:
        print("  WARN expected 47 menus, got %d" % len(menus), file=sys.stderr)


if __name__ == "__main__":
    main()
