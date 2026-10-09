# -*- coding: utf-8 -*-
"""Is the IDE window themed (modern, thin borders) or classic (VC98-ish, thick borders)?

`SetThemeAppProperties(0)` turns theming off for the whole process, which ALSO changes the main
frame's caption/border metrics. The user reported the app looks "VC98-old with thick borders", so
measure the frame's non-client metrics and compare with the original IDE.

usage: python tools/probe_frame_metrics.py
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

ORIG = r"C:\Program Files (x86)\e\e.exe"
MINE = r"D:\eclone\bin\EClone.exe"

# GetSystemMetrics indices
SM_CXSIZEFRAME = 32
SM_CYSIZEFRAME = 33
SM_CXPADDEDBORDER = 92
SM_CYCAPTION = 4
SM_CXFRAME = 32
SM_CYFRAME = 33


def run(tag, exe, args, wait, env):
    app = drive.App(exe, args=args, wait=wait, env=env)
    f = app.frame
    r = drive.rect(f)
    # client vs window: the non-client thickness
    cx, cy = drive.client(f)
    # class styles + which theme is active for the class
    cls = drive.cls(f)
    cl = drive.u.GetClassLongW(f, -26)          # GCL_STYLE
    print("=== %s ===" % tag)
    print("  frame class=%s rect=%s client=%dx%d" % (cls, r, cx, cy))
    print("  frame border+caption: dx=%d dy=%d  (window %dx%d vs client %dx%d)"
          % (r[2] - cx, r[3] - cy, r[2], r[3], cx, cy))
    print("  class style=0x%08X  (CS_* bits)" % (cl & 0xFFFFFFFF))
    # is there a theme for this class?
    try:
        import ctypes
        ux = ctypes.windll.uxtheme
        h = ux.OpenThemeData(f, cls)
        print("  OpenThemeData(%s) = %s" % (cls, h))
    except Exception as e:
        print("  uxtheme probe failed: %s" % e)
    app.close()


if __name__ == "__main__":
    print("system metrics: SIZEFRAME=%d/%d PADDEDBORDER=%d CAPTION=%d"
          % (drive.u.GetSystemMetrics(SM_CXSIZEFRAME), drive.u.GetSystemMetrics(SM_CYSIZEFRAME),
             drive.u.GetSystemMetrics(SM_CXPADDEDBORDER), drive.u.GetSystemMetrics(SM_CYCAPTION)))
    run("orig", ORIG, [r"D:\eclone\shots\e_samples\graphic.e"], 18, None)
    run("mine", MINE, ["designer"], 7, {"ECLONE_DESIGN_DEMO": "1"})
