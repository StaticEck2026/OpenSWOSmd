#!/usr/bin/env python3
"""
make_whdload_icons.py -- the WHDLoad package's two icons, from OpenSWOS's own.

    scripts/amiga/make_whdload_icons.py

Writes bin/setup/whdload/ (committed; the setup's SwosData_MakeWhdload reads it):

    OpenSWOSAGA-NE.info  the package's drawer: packaging/amiga/SWOS.info, the SWOS
                         drawer's icon, as it is
    OpenSWOS.info        the Slave's: packaging/amiga/SWOS/SWOS.info, the picture you
                         double-click in the SWOS drawer, as a project whose default
                         tool is WHDLoad and whose tooltypes name OpenSWOS.slave

Rerun it after changing either source icon.

The .info layout (a DiskObject): 78 bytes of header (its Gadget inside), the
DrawerData for a drawer, the Gadget's image and selected image (a 20-byte Image
header and its planes each), then the default tool and the tooltypes as
length-prefixed strings. Only the strings change here.
"""
import os
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SLAVE_TOOLTYPES = ["SLAVE=OpenSWOS.slave", "PRELOAD"]


def image_size(data, pos):
    """Bytes of one Image struct and its planes at pos."""
    _, _, w, h, depth, ptr = struct.unpack(">hhhhhI", data[pos:pos + 14])
    planes = ((w + 15) // 16) * 2 * h * depth if ptr else 0
    return 20 + planes


def read_string(data, pos):
    n = struct.unpack(">I", data[pos:pos + 4])[0]
    return data[pos + 4:pos + 4 + n], pos + 4 + n


def pack_string(s):
    b = s.encode("latin-1") + b"\0"
    return struct.pack(">I", len(b)) + b


def as_whdload_project(src):
    """The project icon `src`, its default tool WHDLoad, its tooltypes the Slave's."""
    data = bytearray(open(src, "rb").read())
    magic, version = struct.unpack(">HH", data[0:4])
    if magic != 0xE310:
        sys.exit(f"{src}: not an icon")
    render, select = struct.unpack(">II", data[22:30])
    kind = data[48]
    deftool, tooltypes, _, _, drawer, toolwin = struct.unpack(">IIiiII", data[50:74])
    if kind != 4 or drawer:
        sys.exit(f"{src}: not a project icon")
    if bytes(data[78:]).find(b"FORM") >= 0:
        sys.exit(f"{src}: has a colour (OS 3.5) icon, which this does not rewrite")
    pos = 78
    if render:
        pos += image_size(data, pos)
    if select:                             # the selected image
        pos += image_size(data, pos)
    images = bytes(data[:pos])
    if deftool:
        tool, pos = read_string(data, pos)
        if not tool.endswith(b"\0") or not all(32 <= c < 127 for c in tool[:-1]):
            sys.exit(f"{src}: its images are not where they were expected")
    if tooltypes:
        count = struct.unpack(">I", data[pos:pos + 4])[0] // 4 - 1
        pos += 4
        for _ in range(count):
            _, pos = read_string(data, pos)
    if toolwin:
        _, pos = read_string(data, pos)
    rest = bytes(data[pos:])

    head = bytearray(images)
    struct.pack_into(">II", head, 50, 1, 1)          # default tool, tooltypes: present
    struct.pack_into(">I", head, 70, 0)              # no tool window
    out = bytes(head) + pack_string("WHDLoad")
    out += struct.pack(">I", (len(SLAVE_TOOLTYPES) + 1) * 4)
    out += b"".join(pack_string(t) for t in SLAVE_TOOLTYPES)
    return out + rest


def main():
    out = os.path.join(ROOT, "bin", "setup", "whdload")
    os.makedirs(out, exist_ok=True)
    drawer = os.path.join(ROOT, "packaging", "amiga", "SWOS.info")
    project = os.path.join(ROOT, "packaging", "amiga", "SWOS", "SWOS.info")
    with open(os.path.join(out, "OpenSWOSAGA-NE.info"), "wb") as f:
        f.write(open(drawer, "rb").read())
    with open(os.path.join(out, "OpenSWOS.info"), "wb") as f:
        f.write(as_whdload_project(project))
    print(f"wrote {out}/OpenSWOSAGA-NE.info, OpenSWOS.info")


if __name__ == "__main__":
    main()
