# -*- coding: utf-8 -*-
"""Measure the designer's selection frame + handles pixel by pixel, in the ORIGINAL, and print the
colour runs so our clone can be drawn the same.

usage: python tools/frame_measure.py <png> <unit_x> <unit_y> <unit_w> <unit_h>
  (unit_* are the unit's WINDOW-rect origin/size relative to the FORM WINDOW rect, i.e. the same
   coordinates the shot is in)
"""
import sys
from collections import Counter
from PIL import Image


def main():
    img = Image.open(sys.argv[1]).convert("RGB")
    ux, uy, uw, uh = (int(v) for v in sys.argv[2:6])
    px = img.load()
    print("image %s %s ; unit at (%d,%d) %dx%d" % (sys.argv[1], img.size, ux, uy, uw, uh))

    # 1. horizontal scan through the unit's middle: which colours, where, how wide?
    y = uy + uh // 2
    runs = []
    prev = None
    start = 0
    for x in range(0, img.width):
        c = px[x, y]
        if c != prev:
            if prev is not None:
                runs.append((prev, start, x - 1, x - start))
            prev = c
            start = x
    runs.append((prev, start, img.width - 1, img.width - start))
    print("horizontal scan at y=%d (only x in [unit-20, unit+w+20]):" % y)
    for c, a, b, n in runs:
        if b < ux - 20 or a > ux + uw + 20:
            continue
        print("   x %4d..%-4d %-16s %d px" % (a, b, str(c), n))

    # 2. vertical scan through the unit's middle
    x = ux + uw // 2
    runs = []
    prev = None
    start = 0
    for yy in range(0, img.height):
        c = px[x, yy]
        if c != prev:
            if prev is not None:
                runs.append((prev, start, yy - 1, yy - start))
            prev = c
            start = yy
    runs.append((prev, start, img.height - 1, img.height - start))
    print("vertical scan at x=%d:" % x)
    for c, a, b, n in runs:
        if b < uy - 20 or a > uy + uh + 20:
            continue
        print("   y %4d..%-4d %-16s %d px" % (a, b, str(c), n))

    # 3. colour census in the band around the unit (frame + handles live there)
    cc = Counter()
    for yy in range(max(0, uy - 12), min(img.height, uy + uh + 13)):
        for xx in range(max(0, ux - 12), min(img.width, ux + uw + 13)):
            cc[px[xx, yy]] += 1
    print("colour census around the unit (top 10):")
    for c, n in cc.most_common(10):
        print("   %-16s %d" % (str(c), n))


if __name__ == "__main__":
    main()
