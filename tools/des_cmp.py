# -*- coding: utf-8 -*-
"""1:1 designer-page comparison: open each app on its designer document, activate it, and grab the
designer view by SCREEN (PrintWindow misses our overlay) plus the frame.

Prints:
  * the form's offset inside the designer view client area (must be (10,10) per the original)
  * the paper colour, the grid-dot colours and the area actually painted as paper
  * the title-bar pixels of the form (top row colours)
  * the component box: how many 28x28 buttons are visible and where

usage: python tools/des_cmp.py [orig|mine|both]
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
WM_MDIACTIVATE = 0x0222
os.makedirs(OUT, exist_ok=True)


def open_designer(app):
    mdi = app.find("MDIClient")
    d = None
    for h in drive.descendants(app.frame):
        if drive.cls(h).startswith("Afx:400000:b") and "\u7a97\u53e3" in drive.title(h):
            d = h
    if mdi and d:
        drive.u.SendMessageW(mdi, WM_MDIACTIVATE, d, 0)
        time.sleep(2.5)
    return d


def analyze(tag, app, view, form):
    from PIL import ImageGrab
    vx, vy, vw, vh = drive.rect(view)
    img = ImageGrab.grab(bbox=(vx, vy, vx + vw, vy + vh))
    img.save(os.path.join(OUT, "%s_view.png" % tag))
    fx, fy, fw, fh = drive.rect(form)
    # the view's client origin
    pt = drive.wt.POINT(0, 0)
    drive.u.ClientToScreen(view, drive.ctypes.byref(pt))
    lines = []
    lines.append("view rect %s client-origin (%d,%d)" % ((vx, vy, vw, vh), pt.x, pt.y))
    lines.append("form rect %s -> form offset in view client = (%d,%d)"
                 % ((fx, fy, fw, fh), fx - pt.x, fy - pt.y))
    px = img.load()
    # where does the paper start? scan row 0..40 for the first column that is paper-coloured
    paper = px[5, 5]
    lines.append("pixel at view client (5,5) = %s" % (paper,))
    cc = Counter(img.getdata())
    lines.append("top colours in the view: %s" % [(str(k), v) for k, v in cc.most_common(8)])
    # the form's title bar: first rows of the form inside the view
    ty = fy - pt.y
    tx = fx - pt.x
    for dy in (0, 2, 5, 10):
        row = [px[tx + dx, ty + dy] for dx in (1, 20, 100, 200, 379)]
        lines.append("form row dy=%2d: %s" % (dy, [str(c) for c in row]))
    return lines


def run(tag):
    exe = ORIG if tag == "orig" else MINE
    args = [SAMPLE] if tag == "orig" else ["designer"]
    wait = 18 if tag == "orig" else 6
    env = None if tag == "orig" else {"ECLONE_DESIGN_DEMO": "1"}
    app = drive.App(exe, args=args, wait=wait, env=env)
    lines = []
    d = open_designer(app)
    lines.append("designer doc = %s %s |%s|" % (d, drive.rect(d) if d else None,
                                                drive.title(d) if d else ""))
    # the design form + the view that hosts it
    form = app.find("WTWindow")
    view = None
    for h in drive.descendants(app.frame):
        if drive.cls(h).startswith("Afx:400000:3:"):
            view = h
    if view is None:
        # our chain: ECDOverlay's parent is the designer view
        over = app.find("ECDesignOverlay")
        if over:
            view = drive.u.GetParent(over)
    lines.append("view = %s %s" % (view, drive.rect(view) if view else None))
    lines.append("form = %s %s" % (form, drive.rect(form) if form else None))
    if view and form:
        lines += analyze(tag, app, view, form)
    # component box
    sel = btns = dlg = None
    for h in drive.descendants(app.frame):
        if drive.cls(h) != "#32770":
            continue
        kids = drive.children(h)
        b = [k for k in kids if drive.cls(k) == "Button" and drive.rect(k)[2] == 28]
        if len(b) >= 10:
            dlg, btns = h, b
    if dlg:
        vis = [k for k in btns if drive.u.IsWindowVisible(k)]
        lines.append("compbox dlg %s vis-buttons=%d/%d rects=%s"
                     % (drive.rect(dlg), len(vis), len(btns),
                        [(drive.rect(k)[0] - drive.rect(dlg)[0], drive.rect(k)[1] - drive.rect(dlg)[1])
                         for k in vis[:6]]))
    app.shot(app.frame, os.path.join(OUT, "%s_frame.png" % tag))
    app.close()
    return lines


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "both"
    res = {}
    for tag in (("orig", "mine") if which == "both" else (which,)):
        res[tag] = run(tag)
    for tag, lines in res.items():
        print("=== %s ===" % tag)
        for l in lines:
            print("   " + l)
    return 0


if __name__ == "__main__":
    sys.exit(main())
