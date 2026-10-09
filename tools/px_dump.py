# -*- coding: utf-8 -*-
"""把一个区域按颜色打成 ASCII 图，用来逐像素看细节。

用法: python px_dump.py <png> <x0> <x1> <y0> <y1> [颜色1,颜色2,...]
      颜色写 RRGGBB；不给就自动取该区域最多的 4 种颜色，每种一个符号。
      符号：'#'=第1色  'O'=第2色  'o'=第3色  '+'=第4色  '.'=白  ' '=其它
"""
import sys
from collections import Counter
from PIL import Image

SYMS = "#Oo+*x=%"


def main():
    path = sys.argv[1]
    x0, x1, y0, y1 = (int(v) for v in sys.argv[2:6])
    cols = []
    if len(sys.argv) > 6:
        for s in sys.argv[6].split(","):
            s = s.strip().lstrip("#")
            cols.append((int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16)))
    im = Image.open(path).convert("RGB")
    px = im.load()
    if not cols:
        c = Counter(px[x, y] for y in range(y0, y1) for x in range(x0, x1))
        cols = [k for k, _ in c.most_common(4)]
        print("自动取色: " + "  ".join("%s=#%02X%02X%02X" % (SYMS[i], c[0], c[1], c[2])
                                    for i, c in enumerate(cols)))
    else:
        print("指定色:   " + "  ".join("%s=#%02X%02X%02X" % (SYMS[i], c[0], c[1], c[2])
                                    for i, c in enumerate(cols)))
    print("     " + "".join(str((x // 10) % 10) for x in range(x0, x1)))
    print("     " + "".join(str(x % 10) for x in range(x0, x1)))
    for y in range(y0, y1):
        row = ""
        for x in range(x0, x1):
            p = px[x, y]
            if p in cols:
                row += SYMS[cols.index(p)]
            elif p == (255, 255, 255):
                row += "."
            else:
                row += " "
        print("%4d %s" % (y, row))


if __name__ == "__main__":
    main()
