# -*- coding: utf-8 -*-
"""Does the app render with the v6 (themed) common controls / non-client chrome, or classic?

`SetThemeAppProperties(0)` (or linking without a manifest) switches an app to classic rendering.
The original 易语言 IDE is classic -- its design form's title bar is the flat grey 3D one with
dark-grey caption buttons, while a themed app paints the Aero gradient.

usage: python tools/probe_theme.py   (start both and print the form's title-bar profile)
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

ORIG = r"C:\Program Files (x86)\e\e.exe"
MINE = r"D:\eclone\bin\EClone.exe"
SAMPLE = r"D:\eclone\shots\e_samples\graphic.e"
OUT = r"D:\eclone\shots\descmp"
WM_MDIACTIVATE = 0x0222


def run(tag, exe, args, wait, env):
    app = drive.App(exe, args=args, wait=wait, env=env)
    mdi = app.find("MDIClient")
    d = None
    for h in drive.descendants(app.frame):
        if drive.cls(h).startswith("Afx:400000:b") and "\u7a97\u53e3" in drive.title(h):
            d = h
    if mdi and d:
        drive.u.SendMessageW(mdi, WM_MDIACTIVATE, d, 0)
        time.sleep(2.5)
    form = app.find("WTWindow")
    from PIL import ImageGrab
    fx, fy, fw, fh = drive.rect(form)
    img = ImageGrab.grab(bbox=(fx, fy, fx + fw, fy + 30))
    img.save(os.path.join(OUT, "%s_titlebar.png" % tag))
    px = img.load()
    print("=== %s === form=%s style=0x%08X exstyle=0x%08X"
          % (tag, (fx, fy, fw, fh),
             drive.u.GetWindowLongW(form, -16) & 0xFFFFFFFF,
             drive.u.GetWindowLongW(form, -20) & 0xFFFFFFFF))
    print("   title row y=2 : %s" % [str(px[x, 2]) for x in (2, 40, 120, 240, 378)])
    print("   title row y=12: %s" % [str(px[x, 12]) for x in (2, 40, 120, 240, 378)])
    print("   title row y=23: %s" % [str(px[x, 23]) for x in (2, 40, 120, 240, 378)])
    print("   class=%s title=|%s| parent-class=%s"
          % (drive.cls(form), drive.title(form), drive.cls(drive.u.GetParent(form))))
    app.close()


def main():
    run("orig", ORIG, [SAMPLE], 18, None)
    run("mine", MINE, ["designer"], 6, {"ECLONE_DESIGN_DEMO": "1"})
    return 0


if __name__ == "__main__":
    sys.exit(main())
