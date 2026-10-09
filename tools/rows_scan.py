# -*- coding: utf-8 -*-
"""打印 1:1 编辑器截图里的横线 y 列表 + 每行的竖线位置（判断表头/数据行/代码行的分带）。

用法: python rows_scan.py <png> [x_off] [y_max]
x_off = 该图里编辑器客户区 x=0 对应的图像 x（原版 MDI 子框架截图要传 10）
"""
import sys

from PIL import Image

path = sys.argv[1]
XO = int(sys.argv[2]) if len(sys.argv) > 2 else 0
YMAX = int(sys.argv[3]) if len(sys.argv) > 3 else 200
GRID = (0x88, 0x88, 0x88)

im = Image.open(path).convert("RGB")
w, h = im.size
px = im.load()
print("== %s %dx%d (editor x0=%d)" % (path, w, h, XO))
ys = [y for y in range(min(h, YMAX)) if sum(1 for x in range(w) if px[x, y] == GRID) > 100]
print("hlines:", ys)
for i, y in enumerate(ys[:-1]):
    y0, y1 = y, ys[i + 1]
    mid = (y0 + y1) // 2
    v = [x - XO for x in range(XO, w) if px[x, mid] == GRID]
    # 该行有没有文字（非白非灰的点）
    ink = sum(1 for x in range(XO + 40, min(w, XO + 300)) if px[x, mid] not in ((255, 255, 255), GRID))
    print("  band y=%3d..%3d (h=%2d) vlines=%s ink=%d" % (y0, y1, y1 - y0, v[:12], ink))
