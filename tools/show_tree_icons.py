# -*- coding: utf-8 -*-
"""Render src\\res\\tree_icons.bmp as one wide PNG with the cell numbers drawn under it, so the
sheet can be eyeballed instead of ASCII-guessed.

usage: python tools/show_tree_icons.py [bmp] [out.png]
"""
import sys

from PIL import Image, ImageDraw

P = sys.argv[1] if len(sys.argv) > 1 else r"D:\eclone\src\res\tree_icons.bmp"
OUT = sys.argv[2] if len(sys.argv) > 2 else r"D:\eclone\shots\c_tree_icons_sheet.png"
S = 5
ICON = 16


def main():
    im = Image.open(P).convert("RGB")
    n = im.width // ICON
    big = im.resize((im.width * S, im.height * S), Image.NEAREST)
    sheet = Image.new("RGB", (big.width, big.height + 60), (255, 0, 255))
    sheet.paste(big, (0, 0))
    d = ImageDraw.Draw(sheet)
    for i in range(n):
        d.text((i * ICON * S + 8, big.height + 6), str(i), fill=(255, 255, 255))
        d.line([(i * ICON * S, 0), (i * ICON * S, big.height)], fill=(255, 0, 255))
    sheet.save(OUT)
    print("wrote %s  (%d cells)" % (OUT, n))


main()
