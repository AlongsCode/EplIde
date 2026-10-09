# -*- coding: utf-8 -*-
"""Measure an original-IDE table page: grid lines, band fills, text colors.

usage: python tools/measure_page.py <png> [xmax]

Prints the vertical/horizontal #888888 grid lines, the fill colour of every horizontal
band (sampled at a blank x), and the distinct non-background colours per band.
"""
import sys
from collections import Counter

from PIL import Image

GRID = (0x88, 0x88, 0x88)


def main():
    path = sys.argv[1]
    xmax = int(sys.argv[2]) if len(sys.argv) > 2 else 520
    im = Image.open(path).convert("RGB")
    W, H = im.size
    px = im.load()

    print("== %s %dx%d  (x< %d) ==" % (path, W, H, xmax))

    # vertical grid lines: long runs of grid colour
    vs = []
    for x in range(0, min(xmax, W)):
        n = sum(1 for y in range(0, min(H, 400)) if px[x, y] == GRID)
        if n > 40:
            vs.append((x, n))
    print("vertical grid x:", [x for x, _ in vs])

    # horizontal grid lines: rows that are mostly grid colour
    hs = []
    for y in range(0, min(H, 400)):
        n = sum(1 for x in range(0, min(xmax, W)) if px[x, y] == GRID)
        if n > (min(xmax, W) * 0.5):
            hs.append(y)
    print("horizontal grid y:", hs)

    # bands between horizontal lines
    print("-- bands (top..bottom): fill at x=xmax-4, colours in the band --")
    bounds = [-1] + hs + [min(H, 400)]
    for i in range(len(bounds) - 1):
        y0, y1 = bounds[i] + 1, bounds[i + 1]
        if y1 - y0 <= 1:
            continue
        fill = px[min(xmax, W) - 4, (y0 + y1) // 2]
        c = Counter()
        for y in range(y0, y1):
            for x in range(0, min(xmax, W)):
                v = px[x, y]
                if v != fill and v != GRID:
                    c[v] += 1
        top = ", ".join("#%02X%02X%02X x%d" % (r, g, b, n) for (r, g, b), n in c.most_common(5))
        print("  y %3d..%3d h=%2d fill=#%02X%02X%02X  %s" % (y0, y1 - 1, y1 - y0, fill[0], fill[1], fill[2], top))


main()
