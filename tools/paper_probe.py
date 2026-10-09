# -*- coding: utf-8 -*-
"""Measure the design "paper" (the #F0F0F0 client area of the design form) and the grid dots in a
designer-view screenshot, and compare them with the form's own rect.

usage: python tools/paper_probe.py <view.png> <form_x> <form_y> <form_w> <form_h>
"""
import sys
from PIL import Image

PAPER = (240, 240, 240)
DESK = (160, 160, 160)


def main():
    path = sys.argv[1]
    fx, fy, fw, fh = (int(v) for v in sys.argv[2:6])
    im = Image.open(path).convert("RGB")
    px = im.load()
    w, h = im.size
    print("%s %s ; form at (%d,%d) %dx%d" % (path, im.size, fx, fy, fw, fh))
    # paper bbox: #F0F0F0 pixels, but only those connected to the form's client area
    minx, miny = 10 ** 9, 10 ** 9
    maxx = maxy = -1
    for y in range(h):
        for x in range(w):
            if px[x, y] == PAPER:
                if x < minx: minx = x
                if y < miny: miny = y
                if x > maxx: maxx = x
                if y > maxy: maxy = y
    print("all #F0F0F0 bbox = (%d,%d)-(%d,%d)  %dx%d" % (minx, miny, maxx, maxy,
                                                         maxx - minx + 1, maxy - miny + 1))
    # dots: isolated black pixels inside the paper area
    dots = 0
    for y in range(miny, maxy + 1):
        for x in range(minx, maxx + 1):
            if sum(px[x, y]) < 150:
                dots += 1
    print("black pixels inside the paper bbox = %d" % dots)
    # the first few isolated dots in the top-left corner of the paper
    found = []
    for y in range(miny, min(miny + 40, h)):
        for x in range(minx, min(minx + 40, w)):
            if sum(px[x, y]) < 150:
                found.append((x, y))
    print("dots in the paper's top-left 40x40: %s" % found[:20])
    # horizontal transition scan through the middle of the form
    y = fy + fh // 2
    prev = None
    tr = []
    for x in range(w):
        c = px[x, y]
        k = "P" if c == PAPER else ("D" if c == DESK else ".")
        if k != prev:
            tr.append((x, k))
            prev = k
    print("row y=%d transitions: %s" % (y, tr[:14]))


if __name__ == "__main__":
    main()
