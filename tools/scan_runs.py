# -*- coding: utf-8 -*-
"""Per-row run-length scan of an image region, filtered to given colours.

This is how the original editor's flow-control lines are measured: every row of
pixels is reduced to runs of the target colours with exact x ranges.

usage: python scan_runs.py <png> <x0> <y0> <x1> <y1> <RRGGBB,RRGGBB,...>
"""
import sys
from collections import defaultdict

from PIL import Image


def main(png, x0, y0, x1, y1, colours):
    want = set()
    for c in colours.split(","):
        c = c.strip().lstrip("#")
        want.add((int(c[0:2], 16), int(c[2:4], 16), int(c[4:6], 16)))
    im = Image.open(png).convert("RGB")
    px = im.load()
    x0, y0, x1, y1 = int(x0), int(y0), int(x1), int(y1)
    total = defaultdict(int)
    for y in range(y0, y1):
        runs = []
        for x in range(x0, x1):
            if px[x, y] in want:
                total[px[x, y]] += 1
                if runs and runs[-1][2] == x - 1 and runs[-1][3] == px[x, y]:
                    runs[-1][2] = x
                else:
                    runs.append([x, y, x, px[x, y]])
        if runs:
            desc = " ".join("%d-%d(%02X%02X%02X)" % (r[0], r[2], r[3][0], r[3][1], r[3][2])
                            for r in runs)
            print("y=%3d  %s" % (y - y0, desc))
    print("--- totals ---")
    for c, n in sorted(total.items(), key=lambda kv: -kv[1]):
        print("  #%02X%02X%02X  %d px" % (c[0], c[1], c[2], n))


if __name__ == "__main__":
    main(*sys.argv[1:])
