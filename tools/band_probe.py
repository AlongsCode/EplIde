# -*- coding: utf-8 -*-
"""Measure ONE reverse-video band of an editor shot in detail: how far the #000080 runs, what
colour the glyph pixels inside the band are, and the exact band rectangle. This is what tells
"paint the text only" apart from "paint the whole cell/row and turn the text white".

usage: python tools/band_probe.py <png> <y>
"""
import sys
from collections import Counter
from PIL import Image


def main():
    img = Image.open(sys.argv[1]).convert("RGB")
    y = int(sys.argv[2])
    px = img.load()
    w = img.width
    # the band's rows
    ys = [yy for yy in range(img.height) if px[0, yy] is not None and
          any(px[x, yy] == (0, 0, 128) for x in range(w))]
    band = [yy for yy in ys if abs(yy - y) <= 12]
    if not band:
        print("no #000080 rows near y=%d" % y)
        return
    y0, y1 = min(band), max(band)
    print("band rows y=%d..%d" % (y0, y1))
    # column extent of #000080 and of any non-white pixel
    xs_navy = []
    xs_ink = []
    for x in range(w):
        for yy in range(y0, y1 + 1):
            c = px[x, yy]
            if c == (0, 0, 128):
                xs_navy.append(x)
                break
        for yy in range(y0, y1 + 1):
            if sum(px[x, yy]) < 600:
                xs_ink.append(x)
                break
    if xs_navy:
        print("navy x = %d..%d (%d cols)" % (min(xs_navy), max(xs_navy), len(set(xs_navy))))
    if xs_ink:
        print("ink  x = %d..%d" % (min(xs_ink), max(xs_ink)))
    # colour census per row, and the glyph colour inside the navy
    for yy in (y0, (y0 + y1) // 2, y1):
        cc = Counter(px[x, yy] for x in range(w))
        print("  y=%d: %s" % (yy, [(str(k), v) for k, v in cc.most_common(5)]))
    # what colour are the "ink" pixels that sit inside the navy run?
    if xs_navy:
        xa, xb = min(xs_navy), max(xs_navy)
        inner = Counter()
        for yy in range(y0, y1 + 1):
            for x in range(xa, xb + 1):
                inner[px[x, yy]] += 1
        print("  inside navy: %s" % [(str(k), v) for k, v in inner.most_common(6)])


if __name__ == "__main__":
    main()
