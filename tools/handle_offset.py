# -*- coding: utf-8 -*-
"""Brute-force the size-handle offset: for a placed component, try dragging a small probe at
several offsets around the component's edges and report which offset actually resizes it.

Why: the original's handles sit on a rect expanded by 3 px (EC_DESIGN_*), and a probe that guesses
the offset wrong reports "resize broken" when it is not.

usage: python tools/handle_offset.py [mine|orig]
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

ORIG = r"C:\Program Files (x86)\e\e.exe"
MINE = r"D:\eclone\bin\EClone.exe"
SAMPLE = r"D:\eclone\shots\e_samples\graphic.e"
OUT = r"D:\eclone\shots\cmp"
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


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "mine"
    exe, args, wait, env = (MINE, ["designer"], 6, {"ECLONE_DESIGN_DEMO": "1"}) if which == "mine" \
        else (ORIG, [SAMPLE], 18, None)
    app = drive.App(exe, args=args, wait=wait, env=env)
    if not app.frame:
        print("FAIL start")
        return 1
    open_designer(app)
    form = app.find("WTWindow")
    unit = None
    for h in drive.children(form):
        if drive.cls(h) != "AfxControlBar42s":
            unit = h
    if unit is None:
        print("no unit")
        app.close()
        return 1
    fx, fy, fw, fh = drive.rect(form)

    def unit_client():
        r = drive.rect(unit)
        return (r[0] - fx, r[1] - fy, r[2], r[3])

    print("start: unit at form-client %s" % (unit_client(),))
    for d in (0, 1, 2, 3, 4, 5, 6, -1, -2, -3):
        ux, uy, uw, uh = unit_client()
        hx = ux + uw + d
        hy = uy + uh + d
        before = (uw, uh)
        app.drag(form, hx, hy, hx + 25, hy + 20)
        ux2, uy2, uw2, uh2 = unit_client()
        changed = (uw2, uh2) != before
        print("  offset %+d drag from (%d,%d): %dx%d -> %dx%d  resized=%s"
              % (d, hx, hy, before[0], before[1], uw2, uh2, changed))
        if changed:
            break
    app.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
