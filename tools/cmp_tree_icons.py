# -*- coding: utf-8 -*-
"""把原版与 EClone 的「程序」树 1:1 截图按节点列表逐行抠出图标，并排对照。

用法: python cmp_tree_icons.py <orig.png> <mine.png> <out.png>
节点顺序按各自树里的可见行（都是 treeinfo 展开后的状态）。
"""
import sys

from PIL import Image, ImageDraw

ORIG = [("程序数据", 0), ("启动窗口程序集", 1), ("_图形按钮1_被单击", 2), ("窗口", 1),
        ("_启动窗口", 2), ("常量表...", 1), ("资源表", 1), ("声音...", 2),
        ("图片或图片组...", 2), ("模块引用表", 1), ("外部文件记录表", 1)]
MINE = [("程序数据", 0), ("启动窗口程序集", 1), ("_启动窗口_创建完毕", 2),
        ("_图形按钮1_被单击", 2), ("加法", 2), ("窗口", 1), ("_启动窗口", 2),
        ("Dll命令", 1), ("常量表...", 1), ("资源表", 1), ("声音...", 2),
        ("图片或图片组...", 2), ("模块引用表", 1), ("外部文件记录表", 1)]


def icon_x(depth, base):
    return base + depth * 19


def crop(path, rows, base):
    im = Image.open(path).convert("RGB")
    out = []
    for i, (name, d) in enumerate(rows):
        x = icon_x(d, base)
        y = 2 + i * 16
        out.append((name, im.crop((x, y, x + 16, y + 16))))
    return out


a = crop(sys.argv[1], ORIG, 2)
b = crop(sys.argv[2], MINE, 2)
S = 5
rows = max(len(a), len(b))
sheet = Image.new("RGB", (16 * S * 2 + 200, (16 * S + 8) * rows), (255, 255, 255))
dr = ImageDraw.Draw(sheet)
for i in range(rows):
    y = i * (16 * S + 8)
    if i < len(a):
        sheet.paste(a[i][1].resize((16 * S, 16 * S), Image.NEAREST), (0, y))
        dr.text((16 * S + 4, y + 30), "O:" + a[i][0], fill=(0, 0, 0))
    if i < len(b):
        sheet.paste(b[i][1].resize((16 * S, 16 * S), Image.NEAREST), (170, y))
        dr.text((170 + 16 * S + 4, y + 30), "M:" + b[i][0], fill=(0, 0, 0))
sheet.save(sys.argv[3])
print("sheet ->", sys.argv[3], sheet.size)
