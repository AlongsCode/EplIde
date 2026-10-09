# -*- coding: utf-8 -*-
"""Compare two 1:1 editor shots (the original's and ours) pixel by pixel: report every differing
region with its colours, and per-row 反白 extents. Used to judge "select all" against the
original instead of by eye.

usage: python tools/diff_sel.py <A.png> <B.png> [out.png]
"""
import sys
from PIL import Image, ImageChops


def bbox_blocks(diff, minrun=1):
    """Group differing pixels into coarse rectangular blocks by scanning rows."""
    w, h = diff.size
    px = diff.load()
    rows = {}
    for y in range(h):
        xs = [x for x in range(w) if px[x, y] != (0, 0, 0)]
        if xs:
            rows[y] = (min(xs), max(xs), len(xs))
    blocks = []
    cur = None
    for y in sorted(rows):
        x0, x1, n = rows[y]
        if cur is None:
            cur = [y, y, x0, x1, n]
        elif y - cur[1] <= 1:
            cur[1] = y
            cur[2] = min(cur[2], x0)
            cur[3] = max(cur[3], x1)
            cur[4] += n
        else:
            blocks.append(cur)
            cur = [y, y, x0, x1, n]
    if cur:
        blocks.append(cur)
    return blocks, rows


def main():
    pa, pb = sys.argv[1], sys.argv[2]
    out = sys.argv[3] if len(sys.argv) > 3 else None
    a = Image.open(pa).convert("RGB")
    b = Image.open(pb).convert("RGB")
    w = min(a.width, b.width)
    h = min(a.height, b.height)
    if a.size != b.size:
        print("size differs: %s vs %s -> comparing %dx%d" % (a.size, b.size, w, h))
    a = a.crop((0, 0, w, h))
    b = b.crop((0, 0, w, h))
    diff = ImageChops.difference(a, b).convert("RGB")
    if out:
        diff.save(out)
    blocks, rows = bbox_blocks(diff)
    total = sum(r[2] for r in rows.values())
    print("differing pixels = %d / %d (%.2f%%) over %d rows" %
          (total, w * h, 100.0 * total / (w * h), len(rows)))
    print("blocks (y0..y1, x0..x1, px):")
    for y0, y1, x0, x1, n in sorted(blocks, key=lambda r: -r[4])[:20]:
        print("  y %3d..%-3d x %3d..%-3d  %5d px" % (y0, y1, x0, x1, n))

    def bars(img, tag):
        print("%s reverse-video (#000080) rows:" % tag)
        px = img.load()
        run = None
        for y in range(h):
            n = sum(1 for x in range(w) if px[x, y] == (0, 0, 128))
            if n > 0 and run is None:
                run = [y, y, n]
            elif n > 0:
                run[1] = y
                run[2] = max(run[2], n)
            elif run is not None:
                print("   y %3d..%-3d maxwidth %d" % (run[0], run[1], run[2]))
                run = None
        if run is not None:
            print("   y %3d..%-3d maxwidth %d" % (run[0], run[1], run[2]))

    bars(a, "A(original)")
    bars(b, "B(mine)")


if __name__ == "__main__":
    main()
