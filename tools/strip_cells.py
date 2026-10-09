# -*- coding: utf-8 -*-
"""Render each cell of the krnln LIB_BITMAP strip as clean ASCII (thresholding away the
dithering that the PNG export shows) so the 21 category glyphs can be identified and mapped.

usage: python tools/strip_cells.py [strip.png] [cellW] [cellH]
"""
import sys

from PIL import Image

STRIP = r"D:\eclone\shots\krnln_icons\lib_bitmap_strip.png"


def main():
    p = sys.argv[1] if len(sys.argv) > 1 else STRIP
    cw = int(sys.argv[2]) if len(sys.argv) > 2 else 16
    im = Image.open(p).convert("L")
    n = im.width // cw
    print("%s  %dx%d  -> %d cells of %dpx" % (p, im.width, im.height, n, cw))
    for i in range(n):
        print("--- cell %d" % i)
        for y in range(im.height):
            row = ""
            for x in range(cw):
                v = im.getpixel((i * cw + x, y))
                row += "#" if v < 128 else ("+" if v < 220 else ".")
            print("   |" + row + "|")


main()
