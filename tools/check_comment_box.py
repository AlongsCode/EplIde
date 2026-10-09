# -*- coding: utf-8 -*-
"""Check the multi-line comment green box geometry on an editor screenshot.

usage: python tools/check_comment_box.py <png>

The rules were measured from the original IDE
(shots\\live_fold_jy\\view_before.png, 11 comment lines = 5 text + 6 empty):
  colour #008000
  horizontal borders: 2px tall, 11-on/1-off dash, 36px apart (one at the top of every
                      EMPTY comment line), left edge = text column + 17
  vertical borders:   2px wide, one 12px segment per comment line (6px gaps)
Run with no threshold tuning: borders are found as the rows/columns that carry far more
green ink than the text itself.
Exit code 0 = all checks passed.
"""
import sys
from PIL import Image

GREEN = (0, 128, 0)


def groups(vals):
    out = []
    for v in vals:
        if out and v - out[-1][-1] <= 1:
            out[-1].append(v)
        else:
            out.append([v])
    return out


def main():
    path = sys.argv[1]
    pitch = int(sys.argv[2]) if len(sys.argv) > 2 else 18
    im = Image.open(path).convert("RGB")
    W, H = im.size
    px = im.load()
    rc = [sum(1 for x in range(W) if px[x, y] == GREEN) for y in range(H)]
    cc = [sum(1 for y in range(H) if px[x, y] == GREEN) for x in range(W)]
    mr, mc = max(rc), max(cc)
    if mr == 0 or mc == 0:
        print("FAIL: no green pixels at all")
        sys.exit(1)

    hrows = groups([y for y in range(H) if rc[y] >= mr * 0.5])
    bcols = groups([x for x in range(W) if cc[x] >= mc * 0.5])
    fails = []

    def chk(name, cond, extra=""):
        print(("  OK   " if cond else "  FAIL ") + name + ("  " + extra if extra else ""))
        if not cond:
            fails.append(name)

    print("horizontal borders:", [g[0] for g in hrows])
    print("vertical borders  :", [g[0] for g in bcols])

    chk("at least 3 horizontal borders", len(hrows) >= 3, str(len(hrows)))
    chk("each horizontal border is 2px tall", all(len(g) == 2 for g in hrows),
        str([len(g) for g in hrows]))
    gaps = [hrows[i + 1][0] - hrows[i][0] for i in range(len(hrows) - 1)]
    chk("horizontal borders 2*linepitch apart", all(g == 2 * pitch for g in gaps),
        str(gaps) + " pitch=" + str(pitch))

    y0 = hrows[0][0]
    xs = [x for x in range(W) if px[x, y0] == GREEN]
    runs = []
    s = p = xs[0]
    for x in xs[1:]:
        if x == p + 1:
            p = x
        else:
            runs.append(p - s + 1); s = p = x
    runs.append(p - s + 1)
    body = runs[1:-1] if len(runs) > 3 else runs
    chk("horizontal dash = 11 on / 1 off", max(body) == 11 and set(body) <= {10, 11, 12},
        str(runs[:6]))

    chk("a vertical border exists", len(bcols) >= 1, str([g[0] for g in bcols]))
    chk("the left vertical border is 2px wide", len(bcols[0]) == 2, str(bcols[0]))

    x0 = bcols[0][0]
    ys = [y for y in range(H) if px[x0, y] == GREEN]
    segs = groups(ys)
    print("left-border segments:", [(a[0], a[-1]) for a in segs])
    gaps2 = [segs[i + 1][0] - segs[i][0] for i in range(len(segs) - 1)]
    chk("vertical border is dashed (>=3 segments)", len(segs) >= 3, str(len(segs)))
    if len(segs) >= 4:
        chk("one segment per comment line (linepitch apart)",
            all(abs(g - pitch) <= 1 for g in gaps2[1:]), str(gaps2))
        inner = [len(g) for g in segs[1:-1]]
        chk("inner segments are ~12px", all(11 <= l <= 12 for l in inner), str(inner))

    print(("%d passed, %d failed" % (7 - len(fails), len(fails))))
    sys.exit(1 if fails else 0)


main()
