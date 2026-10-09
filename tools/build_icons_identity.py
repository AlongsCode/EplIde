# -*- coding: utf-8 -*-
"""FINAL approach for the tree icons: make the mapping an IDENTITY.

EClone's support-library tree display order is now correct (verified against
shots\\live2\\frame.png, tools\\tree_img_rule.py).  So for every display position p (1-based
among the depth-1 category rows) we extract the glyph from the ORIGINAL screenshot at exactly
that row and put it in cell p.  Then the C++ side is simply `n.icon = oi + 1` -- no arithmetic
between two different orders, so nothing can get shifted.

The original's rows in shots\\live2\\frame.png: category row p is at y = 167 + 14*(p-1), x = 36
(verified: position 1 = arrow, 2 = x÷, 3 = ?, 4 = 0+1, 5 = +-, 6.. = the shared default glyph).

usage: python tools/build_icons_identity.py [--dry]
"""
import sys

from PIL import Image

ROOT = r"D:\eclone"
SHOT = ROOT + r"\shots\live2\frame.png"
DEST = ROOT + r"\src\res\tree_icons.bmp"
ICON, NCELL, BG = 16, 31, (255, 255, 255)
X = 36
ROOT_Y = 153
POS_Y0 = 167
PITCH = 14
N_CAT = 24


def clean(cell):
    px = cell.load()
    for j in range(cell.height):
        for k in range(cell.width):
            r, g, b = px[k, j]
            if r > 225 and g > 225 and b > 225:
                px[k, j] = BG
    return cell


def main():
    im = Image.open(SHOT).convert("RGB")
    strip = Image.new("RGB", (ICON * NCELL, ICON), BG)

    # the ROOT row sits right under the work pane's header separator, so its top scan line is the
    # window border (a solid black bar) -- drop it instead of pasting it into the icon.
    root = clean(im.crop((X, ROOT_Y, X + ICON, ROOT_Y + ICON)))
    px = root.load()
    for xx in range(ICON):
        px[xx, 0] = BG
    strip.paste(root, (0, 0))
    print("   cell  0 <- root (y=%d, top scan line cleared)" % ROOT_Y)

    for p in range(1, N_CAT + 1):
        y = POS_Y0 + PITCH * (p - 1)
        strip.paste(clean(im.crop((X, y, X + ICON, y + ICON))), (p * ICON, 0))
        print("   cell %2d <- display position %d (y=%d)" % (p, p, y))

    if "--dry" in sys.argv:
        print("dry run")
        return
    strip.save(DEST)
    print("wrote %s (%dx%d) -- cells 0..%d" % (DEST, strip.width, strip.height, N_CAT))


main()
