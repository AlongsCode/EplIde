# -*- coding: utf-8 -*-
"""Report the *rectangles* of a select-all shot: for each band of rows, the x runs that contain
either #000080 (selection background) or white-on-navy glyphs -- i.e. the full painted extent of
each table row/cell. This is how "the original selects the whole row" can be checked from data.

usage: python tools/sel_rects.py <png>
"""
import sys
from PIL import Image


def main():
    img = Image.open(sys.argv[1]).convert("RGB")
    px = img.load()
    w, h = img.size
    # a pixel belongs to the selection if it is navy, or white and horizontally flanked by navy
    sel = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if px[x, y] == (0, 0, 128):
                sel[y][x] = True
    # grow white glyph pixels that sit inside a navy neighbourhood
    for y in range(h):
        x = 0
        while x < w:
            if sel[y][x]:
                x += 1
                continue
            if px[x, y] == (255, 255, 255):
                x0 = x
                while x < w and px[x, y] == (255, 255, 255) and not sel[y][x]:
                    x += 1
                x1 = x
                left = any(sel[y][xx] for xx in range(max(0, x0 - 3), x0))
                right = any(sel[y][xx] for xx in range(x1, min(w, x1 + 3)))
                if left and right:
                    for xx in range(x0, x1):
                        sel[y][xx] = True
                continue
            x += 1

    # bands of rows that have any selection
    rows = [y for y in range(h) if any(sel[y])]
    bands = []
    cur = None
    for y in rows:
        if cur is None:
            cur = [y, y]
        elif y - cur[1] <= 1:
            cur[1] = y
        else:
            bands.append(cur)
            cur = [y, y]
    if cur:
        bands.append(cur)

    print("%s: %d selection bands" % (sys.argv[1], len(bands)))
    for y0, y1 in bands:
        cols = [x for x in range(w) if any(sel[y][x] for y in range(y0, y1 + 1))]
        # split the columns into runs (gap > 4 px = separate rectangle)
        runs = []
        st = prev = cols[0]
        for x in cols[1:]:
            if x - prev > 4:
                runs.append((st, prev))
                st = x
            prev = x
        runs.append((st, prev))
        desc = " ".join("%d..%d" % r for r in runs[:8])
        print("  y %3d..%-3d (%2d rows) x %3d..%-3d  runs: %s%s"
              % (y0, y1, y1 - y0 + 1, cols[0], cols[-1], desc,
                 " ..." if len(runs) > 8 else ""))


if __name__ == "__main__":
    main()
