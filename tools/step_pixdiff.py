# -*- coding: utf-8 -*-
"""Pixel-diff two interaction steps (original vs clone) and rank the differing regions.

usage:
  python tools/step_pixdiff.py                      # 全部步骤
  python tools/step_pixdiff.py 05_enter             # 单个步骤

Both shots come from tools/interact_diff.py (editor client area). Sizes differ between the two
apps, so the comparison is done on a top-left anchored, common-size crop; the report lists
connected differing rectangles sorted by area, which is what the fix tasks are keyed off.
"""
import os
import sys
from PIL import Image, ImageChops, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INT = os.path.join(ROOT, "shots", "interact")


def diff_one(name):
    pa = os.path.join(INT, "orig", name + ".png")
    pb = os.path.join(INT, "mine", name + ".png")
    if not (os.path.exists(pa) and os.path.exists(pb)):
        print("  %-14s (缺图，跳过)" % name)
        return
    a = Image.open(pa).convert("RGB")
    b = Image.open(pb).convert("RGB")
    w = min(a.width, b.width)
    h = min(a.height, b.height)
    a = a.crop((0, 0, w, h))
    b = b.crop((0, 0, w, h))
    d = ImageChops.difference(a, b).convert("L").point(lambda v: 255 if v > 24 else 0)
    px = d.load()
    total = sum(1 for y in range(h) for x in range(w) if px[x, y])
    print("  %-14s 共同区域 %dx%d  差异像素 %d (%.2f%%)" % (name, w, h, total, 100.0 * total / (w * h)))
    # 行带统计：哪几行差得最多（前 8 带，每带 12 px 高）
    bands = []
    for y0 in range(0, h, 12):
        n = sum(1 for y in range(y0, min(y0 + 12, h)) for x in range(w) if px[x, y])
        bands.append((n, y0))
    bands.sort(reverse=True)
    print("     差最多的行带: " + ", ".join("y=%d..%d:%d" % (y, y + 11, n) for n, y in bands[:6]))
    out = os.path.join(INT, "diff_%s.png" % name)
    vis = Image.new("RGB", (w, h), (255, 255, 255))
    vis.paste(a, (0, 0))
    dr = ImageDraw.Draw(vis)
    for y in range(h):
        for x in range(w):
            if px[x, y]:
                dr.point((x, y), fill=(255, 0, 255))
    vis.save(out)


def main():
    names = sys.argv[1:] or sorted(
        os.path.splitext(f)[0] for f in os.listdir(os.path.join(INT, "orig"))
        if f.endswith(".png"))
    print("== 逐像素差异（magenta 标在 diff_<step>.png）==")
    for n in names:
        diff_one(n)


if __name__ == "__main__":
    main()
