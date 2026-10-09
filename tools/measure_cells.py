# -*- coding: utf-8 -*-
"""Report, for an original-IDE table page, the dominant text colour of each cell column.

usage: python tools/measure_cells.py <png> [xmax]

Columns are taken from the #888888 vertical grid lines (threshold on the count of grid
pixels in the column). Row bands come from the horizontal grid lines. For each band the
dominant non-background colour inside each column range is printed, which is how the
per-column text colours (name / type / value / comment) were read off the original.
"""
import sys
from collections import Counter

from PIL import Image

GRID = (0x88, 0x88, 0x88)
WHITE = (0xFF, 0xFF, 0xFF)
BG = ((0xEC, 0xEC, 0xEC), (0xE6, 0xED, 0xE4), (0xD9, 0xE3, 0xF0), WHITE)


def dominant(px, x0, x1, y0, y1):
    c = Counter()
    for y in range(y0, y1):
        for x in range(x0, x1):
            v = px[x, y]
            if v == GRID or v in BG:
                continue
            c[v] += 1
    return c


def main():
    path = sys.argv[1]
    xmax = int(sys.argv[2]) if len(sys.argv) > 2 else 700
    im = Image.open(path).convert("RGB")
    W, H = im.size
    px = im.load()

    # horizontal first: every x inside the table also crosses every horizontal line, so the
    # vertical-line threshold has to be well above the number of horizontal lines.
    rows = [y for y in range(0, min(H, 500))
            if sum(1 for x in range(0, min(xmax, W)) if px[x, y] == GRID) > (min(xmax, W) * 0.3)]
    thr = max(12, len(rows) * 2)
    cols = [x for x in range(0, min(xmax, W))
            if sum(1 for y in range(0, min(H, 500)) if px[x, y] == GRID) > thr]
    print("vertical grid:", cols)
    print("horizontal grid:", rows)

    bounds = [-1] + rows + [min(H, 500)]
    for i in range(len(bounds) - 1):
        y0, y1 = bounds[i] + 1, bounds[i + 1]
        if y1 - y0 < 5:
            continue
        out = []
        for j in range(len(cols) - 1):
            x0, x1 = cols[j] + 1, cols[j + 1]
            c = dominant(px, x0, x1, y0, y1)
            top = c.most_common(1)
            if top:
                (r, g, b), n = top[0]
                out.append("x%d-%d:#%02X%02X%02X(%d)" % (x0, x1, r, g, b, n))
        print("y %3d..%3d  %s" % (y0, y1 - 1, "  ".join(out)))


main()
