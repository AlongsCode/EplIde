# -*- coding: utf-8 -*-
"""Side-by-side dump of every e-language .clr to derive the file layout.

usage: python clr_dump2.py
"""
import glob
import os

D = r"C:\Program Files (x86)\e\clr"


def main():
    files = sorted(glob.glob(os.path.join(D, "*.clr")))
    data = {}
    for f in files:
        nm = os.path.basename(f)[:-4]
        # name is GBK on disk? os.listdir gives unicode already on py3
        raw = open(f, "rb").read()
        data[nm] = raw
    n = max(len(v) // 4 for v in data.values())
    cols = list(data)
    print("cols: " + " | ".join("%-14s" % c for c in cols))
    print("n=%d" % n)
    for i in range(n):
        row = []
        for c in cols:
            raw = data[c]
            v = int.from_bytes(raw[i * 4:i * 4 + 4], "little") if (i + 1) * 4 <= len(raw) else None
            if v is None:
                row.append("%-14s" % "-")
            else:
                b, g, r = v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF
                row.append("%-14s" % ("%02X%02X%02X" % (r, g, b)))
        print("[%2d] %s" % (i, " | ".join(row)))


if __name__ == "__main__":
    main()
