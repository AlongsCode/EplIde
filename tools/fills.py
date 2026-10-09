# -*- coding: utf-8 -*-
"""Report exact solid-colour fills in an image: colour -> pixel count + bbox.

Used to read the original editor's palette straight off a 1:1 screenshot
(header fills, grid lines, flow band) without guessing.

usage: python fills.py <png> [maxcolors]
"""
import sys
from collections import defaultdict

from PIL import Image


def main(png, top=30):
    im = Image.open(png).convert("RGB")
    w, h = im.size
    px = im.load()
    cnt = defaultdict(int)
    box = {}
    for y in range(h):
        for x in range(w):
            c = px[x, y]
            cnt[c] += 1
            b = box.get(c)
            if b is None:
                box[c] = [x, y, x, y]
            else:
                if x < b[0]:
                    b[0] = x
                if y < b[1]:
                    b[1] = y
                if x > b[2]:
                    b[2] = x
                if y > b[3]:
                    b[3] = y
    print("%s %dx%d  distinct=%d" % (png, w, h, len(cnt)))
    for c, n in sorted(cnt.items(), key=lambda kv: -kv[1])[:top]:
        b = box[c]
        print("  #%02X%02X%02X  n=%-8d bbox=(%d,%d)-(%d,%d)" % (c[0], c[1], c[2], n, b[0], b[1], b[2], b[3]))


if __name__ == "__main__":
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 30)
