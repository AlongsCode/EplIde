# -*- coding: utf-8 -*-
"""Designer interaction comparison (original vs clone): select / drag / resize / delete a placed
component, and check the design form's own drag+resize behaviour.

Everything is measured from window rects, so the two runs are directly comparable.

usage: python tools/cmp_designer.py [orig|mine|both]
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
VK_DELETE = 0x2E
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


def unit_kids(form):
    """the components on the form (skip the empty menu bar)"""
    out = []
    for h in drive.children(form):
        c = drive.cls(h)
        if c == "AfxControlBar42s":
            continue
        out.append((c, drive.rect(h), drive.title(h)))
    return out


def run(tag, exe, args, wait, env):
    app = drive.App(exe, args=args, wait=wait, env=env)
    if not app.frame:
        print("%s: FAIL start" % tag)
        return
    print("=== %s ===" % tag)
    open_designer(app)
    form = app.find("WTWindow")
    if not form:
        print("  no WTWindow")
        app.close()
        return
    fx, fy, fw, fh = drive.rect(form)
    print("  form %s  units=%s" % ((fx, fy, fw, fh), unit_kids(form)))
    app.shot(form, os.path.join(OUT, "des_%s_0start.png" % tag))

    unit = None
    for h in drive.children(form):
        if drive.cls(h) != "AfxControlBar42s":
            unit = h
    if unit is None:
        print("  (no component on the form to play with)")
        app.close()
        return
    ux, uy, uw, uh = drive.rect(form)
    # unit rect in form client coords
    cx, cy, cw, ch = drive.rect(unit)
    px0, py0 = cx - ux, cy - uy
    print("  unit %s at form-client (%d,%d) %dx%d" % (drive.cls(unit), px0, py0, cw, ch))

    # 1) drag the unit's middle -> should move
    app.drag(form, px0 + cw // 2, py0 + ch // 2, px0 + cw // 2 + 40, py0 + ch // 2 + 25)
    cx2, cy2, cw2, ch2 = drive.rect(unit)
    print("  after drag middle: at (%d,%d) %dx%d  moved=%s resized=%s"
          % (cx2 - ux, cy2 - uy, cw2, ch2, (cx2 != cx or cy2 != cy),
             (cw2 != cw or ch2 != ch)))
    app.shot(form, os.path.join(OUT, "des_%s_1dragged.png" % tag))

    # 2) drag the SE handle (+3 px outside the rect) -> should resize, not move
    hx, hy = cx2 + cw2 + 3, cy2 + ch2 + 3
    app.drag(form, hx - ux, hy - uy, hx - ux + 30, hy - uy + 20)
    cx3, cy3, cw3, ch3 = drive.rect(unit)
    print("  after SE-handle drag: at (%d,%d) %dx%d  resized=%s topLeftMoved=%s"
          % (cx3 - ux, cy3 - uy, cw3, ch3,
             (cw3 != cw2 or ch3 != ch2), (cx3 != cx2 or cy3 != cy2)))
    app.shot(form, os.path.join(OUT, "des_%s_2resized.png" % tag))

    # 3) the design form itself: drag its middle -> must NOT move; drag its handle -> resize
    fx1, fy1, fw1, fh1 = drive.rect(form)
    mcx = fw1 // 2
    mcy = fh1 - 20
    app.drag(form, mcx, mcy, mcx + 40, mcy + 30)
    fx2, fy2, fw2, fh2 = drive.rect(form)
    print("  form after inside-drag: moved=%s resized=%s"
          % ((fx2 != fx1 or fy2 != fy1), (fw2 != fw1 or fh2 != fh1)))
    app.drag(form, fw2, fh2, fw2 + 40, fh2 + 30)
    fx3, fy3, fw3, fh3 = drive.rect(form)
    print("  form after handle-drag: resized=%s topLeftMoved=%s (now %dx%d)"
          % ((fw3 != fw2 or fh3 != fh2), (fx3 != fx2 or fy3 != fy2), fw3, fh3))
    app.shot(form, os.path.join(OUT, "des_%s_3form.png" % tag))
    app.close()


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "both"
    if which in ("orig", "both"):
        run("orig", ORIG, [SAMPLE], 18, None)
    if which in ("mine", "both"):
        run("mine", MINE, ["designer"], 6, {"ECLONE_DESIGN_DEMO": "1"})
    return 0


if __name__ == "__main__":
    sys.exit(main())
