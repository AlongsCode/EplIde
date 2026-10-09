# -*- coding: utf-8 -*-
"""扫描 1:1 编辑器截图里「流程槽 + 流程线」区域：逐行列出非白像素的 run 和颜色分类。

用法: python flow_scan.py <png> [x0] [x1] [y0] [y1]
颜色分类: B=藏青(0,0,128系)  d=灰(128,128,128)  D=深灰(105,105,105)  g=灰底/填充  #=黑  ?=其它
"""
import sys

from PIL import Image

path = sys.argv[1]
x0 = int(sys.argv[2]) if len(sys.argv) > 2 else 0
x1 = int(sys.argv[3]) if len(sys.argv) > 3 else 200
y0 = int(sys.argv[4]) if len(sys.argv) > 4 else 0
y1 = int(sys.argv[5]) if len(sys.argv) > 5 else 400

im = Image.open(path).convert("RGB")
px = im.load()
w, h = im.size
y1 = min(y1, h)
x1 = min(x1, w)


def cls(p):
    r, g, b = p
    if p == (255, 255, 255):
        return None
    if r == g == b:
        if r == 128:
            return "d"
        if r == 105:
            return "D"
        if r >= 230:
            return "g"
        if r <= 60:
            return "#"
        return "?"
    if b > r + 40 and b > g + 40:
        return "B"
    return "?"


print("== %s  x=%d..%d y=%d..%d" % (path, x0, x1, y0, y1))
for y in range(y0, y1):
    runs = []
    cur = None
    for x in range(x0, x1):
        c = cls(px[x, y])
        if c is None:
            if cur:
                runs.append(cur)
                cur = None
            continue
        if cur and cur[2] == c and x == cur[1] + 1:
            cur[1] = x
        else:
            if cur:
                runs.append(cur)
            cur = [x, x, c]
    if cur:
        runs.append(cur)
    if runs:
        print("y=%3d " % y + " ".join("%d-%d%s" % (a, b, c) for a, b, c in runs))
