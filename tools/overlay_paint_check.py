# -*- coding: utf-8 -*-
"""Verify the overlay's painting DETERMINISTICALLY (no screen grabs).

Screen grabs are unreliable in this environment: if another window (the console) covers the app,
`ImageGrab` returns that window's pixels and every visual check silently becomes garbage. So this
script asks the overlay to paint itself via `WM_PRINT` / `PrintWindow`, which reflects the window's
own drawing regardless of what is on top, and reports:
  * how many navy (handle) / red (frame) pixels the overlay draws
  * whether the drawing is complete (all 8 handles) or partial

usage: python tools/overlay_paint_check.py [rounds]
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

MINE = r"D:\eclone\bin\EClone.exe"


def one(round_no):
    app = drive.App(MINE, args=["designer"], wait=7, env={"ECLONE_DESIGN_DEMO": "1"})
    mdi = app.find("MDIClient")
    doc = None
    for h in drive.descendants(app.frame):
        if drive.cls(h).startswith("Afx:400000:b") and "\u7a97\u53e3" in drive.title(h):
            doc = h
    if mdi and doc:
        drive.u.SendMessageW(mdi, 0x0222, doc, 0)
        time.sleep(2.5)
    form = app.find("WTWindow")
    over = app.find("ECDesignOverlay")
    unit = [k for k in drive.children(form) if drive.cls(k) != "AfxControlBar42s"][0]
    fx, fy, fw, fh = drive.rect(form)
    ux, uy, uw, uh = drive.rect(unit)
    app.click(form, (ux - fx) + uw // 2, (uy - fy) + uh // 2)
    time.sleep(0.6)
    # force a synchronous full repaint, then read back through the window's own paint
    drive.u.RedrawWindow(over, None, None, 0x0001 | 0x0400 | 0x0100 | 0x0080)
    img = app.shot(over, os.path.join(r"D:\eclone\shots\descmp", "paint_%d.png" % round_no))
    px = img.load()
    navy = sum(1 for y in range(img.height) for x in range(img.width)
               if px[x, y] == (0, 0, 128))
    red = sum(1 for y in range(img.height) for x in range(img.width)
              if px[x, y] == (255, 0, 0))
    print("round %d: overlay paints navy=%d red=%d  (size %s)" % (round_no, navy, red, img.size))
    app.close()
    return (navy, red)


if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    vals = [one(i) for i in range(n)]
    ok = all(v[0] >= 190 and v[1] > 1000 for v in vals)
    print("results: %s -> %s" % (vals, "OK (complete painting every round)" if ok else "INCOMPLETE"))
