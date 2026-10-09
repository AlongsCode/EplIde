# -*- coding: utf-8 -*-
"""扫出编辑器 1:1 截图里流程槽的折叠标记（- / +）与流程箭头。

用法: python marker_scan.py <png> [x0] [x1]
打印每个标记的包围盒、点阵（# = 前景）、前景色、左右两侧的底色。
"""
import sys
from collections import Counter

from PIL import Image

path = sys.argv[1]
X0 = int(sys.argv[2]) if len(sys.argv) > 2 else 16
X1 = int(sys.argv[3]) if len(sys.argv) > 3 else 44


def is_ink(p):
    r, g, b = p
    return b > 60 and b > r + 30 and b > g + 30 and r < 120


im = Image.open(path).convert("RGB")
w, h = im.size
px = im.load()
pts = [(x, y) for y in range(h) for x in range(X0, min(X1, w)) if is_ink(px[x, y])]

# 简单聚类：按 y 相邻（<=3px）分组
clusters = []
for p in sorted(pts, key=lambda t: (t[1], t[0])):
    for c in clusters:
        if abs(p[1] - c["y1"]) <= 3 and c["x0"] - 4 <= p[0] <= c["x1"] + 4:
            c["pts"].append(p)
            c["x0"] = min(c["x0"], p[0]); c["x1"] = max(c["x1"], p[0])
            c["y0"] = min(c["y0"], p[1]); c["y1"] = max(c["y1"], p[1])
            break
    else:
        clusters.append({"pts": [p], "x0": p[0], "x1": p[0], "y0": p[1], "y1": p[1]})

print("== %s  %dx%d  ink=%d clusters=%d" % (path, w, h, len(pts), len(clusters)))
for c in clusters:
    x0, x1, y0, y1 = c["x0"], c["x1"], c["y0"], c["y1"]
    cols = Counter(px[x, y] for x, y in c["pts"])
    fg = cols.most_common(1)[0][0]
    S = set(c["pts"])
    art = []
    for y in range(y0, y1 + 1):
        art.append("".join("#" if (x, y) in S else "." for x in range(x0, x1 + 1)))
    bg_l = px[max(0, x0 - 3), (y0 + y1) // 2]
    bg_r = px[min(w - 1, x1 + 3), (y0 + y1) // 2]
    print("  bbox=(%d,%d)-(%d,%d) %dx%d fg=%s bgL=%s bgR=%s colors=%s"
          % (x0, y0, x1, y1, x1 - x0 + 1, y1 - y0 + 1, fg, bg_l, bg_r, cols.most_common(3)))
    for a in art:
        print("     " + a)
