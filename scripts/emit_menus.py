#!/usr/bin/env python3
"""JSON -> C emitter for SWOS menus — the inverse of export_menus.py.

Regenerates each `Menu_<Type> <name> = { ... };` initializer in menu_data.c from
bin/data_json/menus/*.json, recombining the split colors byte (frame/background nibbles).
Emits FULLY-FLAT initializers: the C compiler fills each bespoke per-menu struct
field-by-field in declaration order, so one uniform form works for all 102 menu types
(continueMenu's flat layout, buyOtherPlayers' wrapped entries, ...). menu_data.h (the
types) and the file preamble/stubs are kept; only the `{ ... }` bodies are replaced.

Verifies via the export round-trip (emit -> splice -> re-export -> diff the JSON). Final
byte-correctness is the compile + SWOS_MENU_JSON_VERIFY check (raw mode, 47/47). `--out
PATH` writes the regenerated file.
"""
import json, os, re, sys, glob, tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import export_menus as exp

MENU_DIR = exp.OUT_DIR
REV = {jo: (cmd, fields) for cmd, (jo, fields) in exp.OPCODES.items()}   # json_op -> (MENU_CMD_*, fields)


def _ptr(v, key=None):
    # int sentinel and dotted name-buffer refs only ever land in a char* text-pointer field,
    # so they need a (char *) cast — without it the Amiga build (GCC 15) errors on the
    # int8_t(*)[36] name buffers and int sentinels. func/table/sprite operands are symbols
    # (never int/dotted) and the header funcs come through key=None, so neither is affected.
    if v is None:
        return "0"
    if isinstance(v, int):
        return "(char *)%d" % v     # int in a pointer field = char* text sentinel
    if "." in v:                    # dotted field ref = name buffer used as char* text
        return "(char *)&" + v
    if key == "sprite":             # MENU_CMD_SPRITE_COPY operand is a struct -> &
        return "&" + v
    return v                    # function / string / table name decays to a pointer


def _cmd_toks(cmd):
    name, fields = REV[cmd["op"]]
    toks = [name]
    for key, knd in fields:
        if knd == "colorpair":
            b = ((cmd.get("frame_color", 0) & 0xF) << 4) | (cmd.get("background_color", 0) & 0xF)
            toks.append("0x%X" % b)
        elif knd == "int":
            toks.append(str(cmd[key]))
        elif knd == "flags":
            toks.append(cmd[key] if isinstance(cmd[key], str) else "0x%X" % cmd[key])
        else:                                   # ptr
            toks.append(_ptr(cmd[key], key))
    return toks


def _item_toks(it):
    k = it["kind"]
    if k == "coords_mode":
        return ["MENU_MARK_WIDE_COORDS" if it["wide"] else "MENU_MARK_BYTE_COORDS"]
    if k == "set_origin":
        return ["MENU_MARK_START_MENU", str(it["x"]), str(it["y"])]
    if k == "reset_template":
        return ["MENU_MARK_INIT_ENTRY"]
    if k == "start_template":
        return ["MENU_MARK_START_TEMPLATE"]
    if k == "entry":
        c = it["coords"]
        toks = [str(c["x"]), str(c["y"]), str(c["w"]), str(c["h"])]
        for cmd in it["commands"]:
            toks += _cmd_toks(cmd)
        toks.append("MENU_CMD_END")
        return toks
    raise ValueError("unknown item kind %r" % k)


def menu_flat_tokens(menu):
    """The full flat leaf-value sequence for a menu (header + items + end marker), with
    pointer casts/& applied. Same order export_menus.flatten() yields from the C."""
    toks = [_ptr(menu["onInit"]), _ptr(menu["onReturn"]), _ptr(menu["onDraw"]), str(menu["defaultEntry"])]
    for it in menu["items"]:
        toks += _item_toks(it)
    toks.append("MENU_MARK_END_MENU")
    return toks


def parse_braces(init):
    """Parse a C initializer `{ ... }` into a nested list: each `{...}` is a sub-list, each
    comma-separated value a leaf string. Comments stripped. Captures the brace STRUCTURE
    (which the bespoke per-menu type dictates) so we can re-emit hand-written-looking C."""
    s = exp.strip_comments(init)
    i = s.index("{")

    def group():
        nonlocal i
        i += 1                      # skip '{'
        out, cur = [], ""
        while i < len(s):
            ch = s[i]
            if ch == "{":
                out.append(group())
            elif ch == "}":
                i += 1
                if cur.strip():
                    out.append(cur.strip())
                return out
            elif ch == ",":
                if cur.strip():
                    out.append(cur.strip())
                cur = ""; i += 1
            else:
                cur += ch; i += 1
        return out

    return group()


def _leaf_count(tree):
    return sum(_leaf_count(x) if isinstance(x, list) else 1 for x in tree)


# A leading C cast like "(char *)" on the original leaf. The brace template discards the
# original leaf's value, but we must keep its cast — without it the Amiga build (GCC 15,
# no -Wno-incompatible-pointer-types/-Wint-conversion) errors on the name-buffer text
# pointers (int8_t(*)[36]) and int sentinels. SDL suppresses those warnings; Amiga doesn't.
_CAST_RE = re.compile(r'^\(\s*[A-Za-z_][\w ]*\*\s*\)')

def _substitute(tree, vals):
    out = []
    for x in tree:
        if isinstance(x, list):
            out.append(_substitute(x, vals))
        else:
            new = next(vals)
            m = _CAST_RE.match(x)             # original leaf token (may carry a (TYPE *) cast)
            if m and new != "0" and not new.startswith("("):
                new = m.group(0) + new        # restore the cast the JSON value dropped
            out.append(new)
    return out


def _emit_braced(node, indent):
    if isinstance(node, str):
        return node
    parts = [_emit_braced(x, indent + 1) for x in node]
    if any(isinstance(x, list) for x in node):                 # a group with sub-groups -> block
        ipad = "    " * (indent + 1)
        return "{\n" + ",\n".join(ipad + p for p in parts) + "\n" + "    " * indent + "}"
    return "{ " + ", ".join(parts) + " }"                      # leaf-only group -> inline


def emit_menu(menu, orig_init):
    """Regenerate a menu initializer: take the brace STRUCTURE from the original C and the
    VALUES from JSON. Value edits reproduce the hand-written shape; a structural edit (leaf
    count change) raises (it needs a menu_data.h type change too)."""
    tree = parse_braces(orig_init)
    vals = menu_flat_tokens(menu)
    if _leaf_count(tree) != len(vals):
        raise ValueError("%s: template has %d leaves but JSON has %d values (structural edit "
                         "needs a menu_data.h type change)" % (menu["name"], _leaf_count(tree), len(vals)))
    return _emit_braced(_substitute(tree, iter(vals)), 0)


def splice_menu(text, menu):
    m = re.search(r'\b' + re.escape(menu["name"]) + r'\s*=\s*\{', text)
    if not m:
        return text, 0
    i = text.index('{', m.start()); depth = 0; j = i
    while j < len(text):
        if text[j] == '{':
            depth += 1
        elif text[j] == '}':
            depth -= 1
            if depth == 0:
                break
        j += 1
    init = emit_menu(menu, text[i:j + 1])            # original {...} is the brace template
    return text[:i] + init + text[j + 1:], 1


def _diff(a, b, path=""):
    if isinstance(a, dict) and isinstance(b, dict):
        for k in set(a) | set(b):
            if k not in a or k not in b:
                print("    %s/%s only on one side" % (path, k))
            else:
                _diff(a[k], b[k], path + "/" + str(k))
    elif isinstance(a, list) and isinstance(b, list):
        if len(a) != len(b):
            print("    len %s: %d != %d" % (path, len(a), len(b)))
        for i in range(min(len(a), len(b))):
            _diff(a[i], b[i], path + "[%d]" % i)
    elif a != b:
        print("    %s: %r != %r" % (path, a, b))


def main():
    menus = []
    for f in sorted(glob.glob(os.path.join(MENU_DIR, "*.json"))):
        d = json.load(open(f))
        if "items" in d:                        # skip the manifest
            menus.append(d)

    text = open(exp.SRC, encoding="utf-8", errors="replace").read()
    n = 0
    for menu in menus:
        text, k = splice_menu(text, menu)
        if not k:
            print("  WARN: menu %s not found in menu_data.c" % menu["name"])
        n += k
    print("emitted/spliced %d of %d menus" % (n, len(menus)))

    if "--out" in sys.argv:
        open(sys.argv[sys.argv.index("--out") + 1], "w").write(text)
        return 0                         # build/regen mode: skip the slow round-trip verify

    with tempfile.TemporaryDirectory() as td:
        gen = os.path.join(td, "menu_data.c")
        open(gen, "w").write(text)
        out = os.path.join(td, "out"); os.makedirs(out)
        exp.SRC = gen; exp.OUT_DIR = out
        exp.main()
        bad = 0
        for menu in menus:
            name = menu["name"]
            o = json.load(open(os.path.join(MENU_DIR, name + ".json")))
            r = json.load(open(os.path.join(out, name + ".json")))
            if o != r:
                bad += 1
                print("  DIFF in %s" % name)
                _diff(o, r)
        print("\nround-trip: %s" % ("FAITHFUL (0 diff)" if bad == 0 else "%d menu(s) differ" % bad))

    if "--out" in sys.argv:
        out_path = sys.argv[sys.argv.index("--out") + 1]
        open(out_path, "w").write(text)
        print("wrote regenerated C -> %s" % out_path)
    return bad


if __name__ == "__main__":
    sys.exit(1 if main() else 0)
