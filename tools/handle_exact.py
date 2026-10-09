# -*- coding: utf-8 -*-
"""Focused designer-handle test: click a unit (does it select?), then try the SE handle at the
exact overlay geometry (overlay client = screen - overlay origin) for several offsets, and report
which one resizes.

usage: python tools/handle_exact.py [mine|orig]
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
    if which == "mine":
        exe, args, wait, env = MINE, ["designer"], 6, {"ECLONE_DESIGN_DEMO": "1"}
    else:
        exe, args, wait, env = ORIG, [SAMPLE], 18, None
    app = drive.App(exe, args=args, wait=wait, env=env)
    if not app.frame:
        print("FAIL start")
        return 1
    open_designer(app)
    form = app.find("WTWindow")
    over = app.find("ECDesignOverlay")
    unit = None
    for h in drive.children(form):
        if drive.cls(h) != "AfxControlBar42s":
            unit = h
    print("form=%s %s" % (form, drive.rect(form)))
    print("overlay=%s %s" % (over, drive.rect(over)))
    print("unit=%s %s %s" % (unit, drive.rect(unit), drive.cls(unit)))

    # 1) click the unit's middle -> does it select? (ask the property page for the unit)
    ux, uy, uw, uh = drive.rect(unit)
    fx, fy, fw, fh = drive.rect(form)
    app.click(form, (ux - fx) + uw // 2, (uy - fy) + uh // 2)
    time.sleep(0.7)
    app.shot(form, os.path.join(OUT, "handle_%s_clicked.png" % which))
    print("clicked unit middle; screenshot handle_%s_clicked.png" % which)

    # 2) SE handle: the overlay is the padded window; SE handle centre in overlay client coords
    if over:
        ox, oy, ow, oh = drive.rect(over)
        cands = []
        for dx in (-3, 0, 3, 6, 9):
            for dy in (-3, 0, 3, 6, 9):
                cands.append((dx, dy))
        for dx, dy in cands:
            sx = ux + uw + dx
            sy = uy + uh + dy
            ux0, uy0, uw0, uh0 = drive.rect(unit)
            app.drag(form, sx - fx, sy - fy, sx - fx + 26, sy - fy + 20)
            ux1, uy1, uw1, uh1 = drive.rect(unit)
            changed = (uw1, uh1) != (uw0, uh0)
            print("  handle try dx=%+d dy=%+d screen(%d,%d): %dx%d -> %dx%d resized=%s"
                  % (dx, dy, sx, sy, uw0, uh0, uw1, uh1, changed))
            if changed:
                print("  => the SE handle is at screen offset (%+d,%+d) from the unit's SE corner"
                      % (dx, dy))
                break
    app.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
