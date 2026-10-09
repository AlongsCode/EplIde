# -*- coding: utf-8 -*-
"""Print one e.exe RT_DIALOG as an indented containment tree.

The DLGTEMPLATE is flat; the visual panels come from geometry, so a control's
panel = the smallest earlier rect that contains it (groupboxes, tab panes).

usage: python dlg_tree.py <exe> <id>
"""
import struct
import sys

import pefile

sys.path.insert(0, __file__.rsplit("\\", 1)[0])
from dump_dialogs import load_strings, parse_dialog  # noqa: E402


def parse_items(data, strings):
    ex = struct.unpack_from("<H", data, 0)[0] == 1 and struct.unpack_from("<H", data, 2)[0] == 0xFFFF
    if ex:
        style = struct.unpack_from("<I", data, 8)[0]
        cdit, cx, cy = struct.unpack_from("<Hhh", data, 16)
        off = 26
    else:
        style = struct.unpack_from("<I", data, 0)[0]
        cdit, x, y, cx, cy = struct.unpack_from("<Hhhhh", data, 8)
        off = 18
    off = parse_dialog.__globals__["read_str"](data, off, strings)[1]
    off = parse_dialog.__globals__["read_str"](data, off, strings)[1]
    off = parse_dialog.__globals__["read_str"](data, off, strings)[1]
    if style & 0x40:
        off += 6 + len(parse_dialog.__globals__["read_str"](data, off + 6, strings)[0]) * 0
        off = parse_dialog.__globals__["read_str"](data, off, strings)[1]
    items = []
    for i in range(cdit):
        while off % 4:
            off += 1
        if ex:
            cstyle = struct.unpack_from("<I", data, off + 8)[0]
            ix, iy, icx, icy = struct.unpack_from("<hhhh", data, off + 12)
            cid = struct.unpack_from("<I", data, off + 20)[0]
            off += 24
        else:
            cstyle, cex, ix, iy, icx, icy, cid = struct.unpack_from("<IIhhhhH", data, off)
            off += 18
        ccls, off = parse_dialog.__globals__["read_str"](data, off, strings)
        ctext, off = parse_dialog.__globals__["read_str"](data, off, strings)
        extra = struct.unpack_from("<H", data, off)[0]
        off += 2 + extra
        items.append(dict(cls=ccls, text=ctext, id=cid, x=ix, y=iy, cx=icx, cy=icy,
                          style=cstyle, kind=cstyle & 0xF, ord=i))
    return cx, cy, items


def main(exe, want):
    want = int(want)
    pe = pefile.PE(exe, fast_load=True)
    pe.parse_data_directories()
    img = pe.get_memory_mapped_image()
    strings = load_strings(pe, img)
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if t.struct.Id != 5:
            continue
        for rid in t.directory.entries:
            if rid.struct.Id != want:
                continue
            for lang in rid.directory.entries:
                off, size = lang.data.struct.OffsetToData, lang.data.struct.Size
                cx, cy, items = parse_items(img[off:off + size], strings)
                print("DIALOG id=%d  %dx%d  items=%d  (dialog units)" % (want, cx, cy, len(items)))
                for i, it in enumerate(items):
                    print("  [%02d] id=%-5d %-16s %4d,%-4d %4dx%-4d %s%s" % (
                        i, it["id"], it["cls"], it["x"], it["y"], it["cx"], it["cy"],
                        ("GROUPBOX " if it["kind"] == 7 and it["cls"] == "Button" else ""),
                        repr(it["text"])[:64]))
                print("\n--- containment tree ---")
                order = sorted(range(len(items)), key=lambda k: items[k]["cx"] * items[k]["cy"])
                par = {}
                for k, it in enumerate(items):
                    best, bestA = None, None
                    for c in order:
                        o = items[c]
                        if c == k or o["cx"] * o["cy"] <= it["cx"] * it["cy"]:
                            continue
                        if (o["x"] <= it["x"] and o["y"] <= it["y"] and
                                o["x"] + o["cx"] >= it["x"] + it["cx"] and
                                o["y"] + o["cy"] >= it["y"] + it["cy"]):
                            a = o["cx"] * o["cy"]
                            if bestA is None or a < bestA:
                                best, bestA = c, a
                    par[k] = best
                for k, it in enumerate(items):
                    depth, q = 0, par[k]
                    while q is not None:
                        depth += 1
                        q = par[q]
                    print("%s%-16s id=%-5d %3dx%-4d @%d,%d %r" % ("  " * depth, it["cls"], it["id"],
                                                                 it["cx"], it["cy"], it["x"], it["y"],
                                                                 it["text"][:56]))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
