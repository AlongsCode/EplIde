# -*- coding: utf-8 -*-
"""把一小块 1:1 截图按颜色打成 ASCII 图（认形状用）。

用法: python px_map.py <png> <x0> <x1> <y0> <y1>
字符: . = #F0F0F0(条底)  W = 白  # = 黑  B = 蓝(0,0,128/255)  D = #696969
      A = #A0A0A0  E = #E3E3E3  8 = #888888  ? = 其它
"""
import sys

from PIL import Image

path, x0, x1, y0, y1 = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
im = Image.open(path).convert("RGB")
px = im.load()
KEY = {(240, 240, 240): ".", (255, 255, 255): "W", (0, 0, 0): "#",
       (105, 105, 105): "D", (160, 160, 160): "A", (227, 227, 227): "E",
       (136, 136, 136): "8", (236, 236, 236): "-", (230, 237, 228): "g"}


def ch(p):
    if p in KEY:
        return KEY[p]
    r, g, b = p
    if b > r + 40 and b > g + 40:
        return "B"
    return "?"


print("    " + "".join(str(x // 100 % 10) for x in range(x0, x1)))
print("    " + "".join(str(x // 10 % 10) for x in range(x0, x1)))
print("    " + "".join(str(x % 10) for x in range(x0, x1)))
for y in range(y0, y1):
    print("%3d " % y + "".join(ch(px[x, y]) for x in range(x0, x1)))
