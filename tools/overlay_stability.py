# -*- coding: utf-8 -*-
"""Is our design-time overlay (paper/grid/handles) reliably repainted, or does the form window
cover it?  This reproduces the flaky symptom and reports it honestly.

Run it a few times: a healthy build should print the SAME navy-handle pixel count every time.

usage: python tools/overlay_stability.py [rounds]
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive
from PIL import ImageGrab

MINE = r"D:\eclone\bin\EClone.exe"


def one(round_no):
    app = drive.App(MINE, args=["designer"], wait=7, env={"ECLONE_DESIGN_DEMO": "1"})
    # ⚠️⚠️ 必须先**激活设计器页**再做任何测量。不激活的话，MDI 子窗口还没显示出来，
    #    `GetWindowRect(窗体)` 拿到的是"上一次运行留下的"位置/尺寸，抓屏抓到的是
    #    别的页面（实测抓到过代码编辑器页）—— 那会伪装成"覆盖层时好时坏"。
    mdi = app.find("MDIClient")
    doc = None
    for h in drive.descendants(app.frame):
        if drive.cls(h).startswith("Afx:400000:b") and "\u7a97\u53e3" in drive.title(h):
            doc = h
    if mdi and doc:
        drive.u.SendMessageW(mdi, 0x0222, doc, 0)      # WM_MDIACTIVATE
        time.sleep(2.5)
    form = app.find("WTWindow")
    unit = [k for k in drive.children(form) if drive.cls(k) != "AfxControlBar42s"][0]
    fx, fy, fw, fh = drive.rect(form)
    ux, uy, uw, uh = drive.rect(unit)
    # click the unit -> selection frame + handles must appear
    app.click(form, (ux - fx) + uw // 2, (uy - fy) + uh // 2)
    time.sleep(0.6)
    img = ImageGrab.grab(bbox=(fx, fy, fx + fw, fy + fh))
    navy = sum(1 for y in range(img.height) for x in range(img.width)
               if img.getpixel((x, y)) == (0, 0, 128))
    red = sum(1 for y in range(img.height) for x in range(img.width)
              if img.getpixel((x, y)) == (255, 0, 0))
    dots = sum(1 for y in range(30, img.height - 5) for x in range(5, 60)
               if sum(img.getpixel((x, y))) < 150)
    img.save(os.path.join(r"D:\eclone\shots\descmp", "stab_%d.png" % round_no))
    print("round %d: navy=%d red=%d dots=%d   (form %s)" % (round_no, navy, red, dots, (fx, fy, fw, fh)))
    app.close()
    return navy


if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    vals = [one(i) for i in range(n)]
    print("navy counts: %s  -> %s" % (vals, "STABLE" if len(set(vals)) == 1 else "FLAKY"))
