# -*- coding: utf-8 -*-
"""Is the design overlay actually catching REAL mouse input? Dump the overlay's window styles /
rect / z-order and check what window sits under the cursor at a few spots on the design form.

usage: python tools/probe_overlay.py
"""
import ctypes
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

MINE = r"D:\eclone\bin\EClone.exe"
OUT = r"D:\eclone\shots\cmp"
os.makedirs(OUT, exist_ok=True)
GWL_STYLE = -16
GWL_EXSTYLE = -20
WS_EX_TRANSPARENT = 0x00000020
WS_EX_LAYERED = 0x00080000


def main():
    app = drive.App(MINE, args=["designer"], wait=6, env={"ECLONE_DESIGN_DEMO": "1"})
    if not app.frame:
        print("FAIL start")
        return 1
    form = app.find("WTWindow")
    over = app.find("ECDesignOverlay")
    print("frame=%s form=%s overlay=%s" % (app.frame, form, over))
    if over:
        st = drive.u.GetWindowLongW(over, GWL_STYLE) & 0xFFFFFFFF
        ex = drive.u.GetWindowLongW(over, GWL_EXSTYLE) & 0xFFFFFFFF
        print("overlay style=0x%08X exstyle=0x%08X transparent=%s"
              % (st, ex, bool(ex & WS_EX_TRANSPARENT)))
        print("overlay rect=%s client=%s" % (drive.rect(over), drive.client(over)))
    if form:
        fx, fy, fw, fh = drive.rect(form)
        print("form rect=%s" % ((fx, fy, fw, fh),))
        # what window is under the cursor at a few points?
        for name, cx, cy in (("centre", fw // 2, fh // 2),
                             ("SE corner", fw, fh),
                             ("SE +3", fw + 3, fh + 3),
                             ("NW -3", -3, -3)):
            app.move(form, cx, cy)
            pt = drive.wt.POINT()
            drive.u.GetCursorPos(ctypes.byref(pt))
            h = drive.u.WindowFromPoint(pt)
            print("  under cursor at %-9s (form-client %d,%d): 0x%08X %s"
                  % (name, cx, cy, h, drive.cls(h)))
    app.shot(app.frame, os.path.join(OUT, "overlay_probe.png"))
    app.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
