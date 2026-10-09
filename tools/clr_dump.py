# -*- coding: utf-8 -*-
"""Decode e-language .clr color-scheme files and map DWORDs to the item-name
table extracted from e.exe at VA 0x65E048..0x65E470.

usage: python clr_dump.py [clrdir]
"""
import glob
import os
import struct
import sys

import pefile

EXE = r"C:\Program Files (x86)\e\e.exe"
NAME_LO, NAME_HI = 0x65E048, 0x65E470


def item_names():
    pe = pefile.PE(EXE, fast_load=True)
    img = pe.get_memory_mapped_image()
    base = pe.OPTIONAL_HEADER.ImageBase
    p, q = NAME_LO - base, NAME_HI - base
    out = []
    while p < q:
        e = img.index(b"\x00", p)
        if e > q:
            break
        out.append(bytes(img[p:e]).decode("gbk", "replace"))
        p = e + 1
    return out


def main(d):
    names = item_names()
    print("item names: %d" % len(names))
    for i, n in enumerate(names):
        print("  [%2d] %s" % (i, n))
    for f in sorted(glob.glob(os.path.join(d, "*.clr"))):
        raw = open(f, "rb").read()
        n = len(raw) // 4
        vals = struct.unpack("<%dI" % n, raw[:n * 4])
        print("\n=== %s  %d bytes -> %d dwords ===" % (os.path.basename(f), len(raw), n))
        for i, v in enumerate(vals):
            b, g, r = v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF
            a = (v >> 24) & 0xFF
            nm = names[i] if i < len(names) else "?"
            print("  [%2d] %08X  RGB(%3d,%3d,%3d) a=%3d  %s" % (i, v, r, g, b, a, nm))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else r"C:\Program Files (x86)\e\clr")
