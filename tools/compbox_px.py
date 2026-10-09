# -*- coding: utf-8 -*-
"""Colour census of the component box buttons: which pixels does each 28x28 button show?

Written because "the component box looks blank" needs a data answer: PrintWindow of the pane
shows gray for an owner-draw button that never paints, but a drawn button shows its 3D border
(white top-left / dark bottom-right) plus the icon's colours.

usage: python tools/compbox_px.py <png> [x0 y0]
"""
import sys
from collections import Counter
from PIL import Image


def main():
    path = sys.argv[1]
    # the component-box dialog's buttons start at dialog client (5,34); the crop passed in
    # should already be the dialog's client rect.
    im = Image.open(path).convert("RGB")
    print("image %s %s" % (path, im.size))
    c = Counter(im.getdata())
    for col, n in c.most_common(8):
        print("  %-16s %d" % (str(col), n))
    w, h = im.size
    cell = 28 + 1
    for row in range(4):
        y0 = 34 + row * cell
        if y0 + 28 > h:
            break
        for col in range(3):
            x0 = 5 + col * cell
            if x0 + 28 > w:
                break
            sub = im.crop((x0, y0, x0 + 28, y0 + 28))
            cc = Counter(sub.getdata())
            top = cc.most_common(3)
            ink = sum(n for c2, n in cc.items() if sum(c2) < 600)
            print("  cell r%d c%d x=%d y=%d top=%s ink=%d" %
                  (row, col, x0, y0, [(str(k), v) for k, v in top], ink))


if __name__ == "__main__":
    main()
