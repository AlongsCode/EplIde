# -*- coding: utf-8 -*-
"""Diagnose the flaky overlay: after startup, log the overlay's rect/size/visibility AND how many
navy pixels are on screen, several times, to see whether the overlay is misplaced or just unpainted.

usage: python tools/overlay_diag.py
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive
from PIL import ImageGrab


def navy_in(rect):
    x, y, w, h = rect
    img = ImageGrab.grab(bbox=(x, y, x + w, y + h))
    return sum(1 for yy in range(img.height) for xx in range(img.width)
               if img.getpixel((xx, yy)) == (0, 0, 128))


def main():
    app = drive.App(r"D:\eclone\bin\EClone.exe", args=["designer"], wait=7,
                    env={"ECLONE_DESIGN_DEMO": "1"})
    over = app.find("ECDesignOverlay")
    form = app.find("WTWindow")
    unit = [k for k in drive.children(form) if drive.cls(k) != "AfxControlBar42s"][0]
    fx, fy, fw, fh = drive.rect(form)
    ux, uy, uw, uh = drive.rect(unit)
    print("form   %s" % (drive.rect(form),))
    print("unit   %s" % (drive.rect(unit),))
    app.click(form, (ux - fx) + uw // 2, (uy - fy) + uh // 2)
    for i in range(6):
        time.sleep(0.4)
        ox, oy, ow, oh = drive.rect(over)
        print("t=%.1f overlay rect=%s vis=%s  navy_on_screen=%d"
              % (0.4 * (i + 1), (ox, oy, ow, oh),
                 bool(drive.u.IsWindowVisible(over)), navy_in((fx, fy, fw, fh))))
    app.shot(over, r"D:\eclone\shots\descmp\diag_overlay_pw.png")
    ImageGrab.grab(bbox=(fx, fy, fx + fw, fy + fh)).save(
        r"D:\eclone\shots\descmp\diag_form_screen.png")
    app.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
