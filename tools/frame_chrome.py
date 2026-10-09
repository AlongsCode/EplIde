# -*- coding: utf-8 -*-
"""Compare the MAIN FRAME chrome (title bar / menu band / toolbar rows / MDI tab strip) between the
original IDE and our clone, row by row, using `PrintWindow` (independent of what is on screen).

The user reported "VC98-old look with thick borders", so we need the real pixel bands.

usage: python tools/frame_chrome.py both
"""
import os
import sys
import time
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

ORIG = r"C:\Program Files (x86)\e\e.exe"
MINE = r"D:\eclone\bin\EClone.exe"
SAMPLE = r"D:\eclone\shots\e_samples\graphic.e"
OUT = r"D:\eclone\shots\descmp"
os.makedirs(OUT, exist_ok=True)


def row_profile(px, w, y):
    """dominant colour of a row, plus how many distinct colours it has"""
    cc = Counter(px[x, y] for x in range(0, w, 2))
    c, n = cc.most_common(1)[0]
    return c, len(cc), n * 2


def run(tag):
    exe = ORIG if tag == "orig" else MINE
    args = [SAMPLE] if tag == "orig" else ["designer"]
    wait = 18 if tag == "orig" else 7
    env = None if tag == "orig" else {"ECLONE_DESIGN_DEMO": "1"}
    app = drive.App(exe, args=args, wait=wait, env=env)
    img = app.shot(app.frame, os.path.join(OUT, "%s_framechrome.png" % tag))
    px = img.load()
    w, h = img.size
    print("=== %s === frame PrintWindow %s" % (tag, img.size))
    # walk the top 240 rows and report the bands (dominant colour changes)
    prev = None
    start = 0
    bands = []
    for y in range(0, min(240, h)):
        c, nd, cnt = row_profile(px, w, y)
        if c != prev:
            if prev is not None:
                bands.append((start, y - 1, prev))
            prev = c
            start = y
    bands.append((start, min(239, h - 1), prev))
    for a, b, c in bands:
        print("   y %3d..%-3d (%2d px) dominant %s" % (a, b, b - a + 1, c))
    app.close()


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "both"
    for tag in (("orig", "mine") if which == "both" else (which,)):
        run(tag)
