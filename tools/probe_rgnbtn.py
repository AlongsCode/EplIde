# -*- coding: utf-8 -*-
"""Compare the design-time `_EL_RgnButton` window between the original IDE and our clone:
class name, styles, extended styles, parent chain, rect. The visual bug ("our component paints a
black rectangle") has to be pinned to either window creation parameters or missing property data.

usage: python tools/probe_rgnbtn.py
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

ORIG = r"C:\Program Files (x86)\e\e.exe"
MINE = r"D:\eclone\bin\EClone.exe"
SAMPLE = r"D:\eclone\shots\e_samples\graphic.e"
WM_MDIACTIVATE = 0x0222


def desc(h):
    return ("%s |%s| rect=%s style=0x%08X ex=0x%08X id=%d parent=%s(%s)"
            % (drive.cls(h), drive.title(h), drive.rect(h),
               drive.u.GetWindowLongW(h, -16) & 0xFFFFFFFF,
               drive.u.GetWindowLongW(h, -20) & 0xFFFFFFFF,
               drive.u.GetDlgCtrlID(h),
               drive.cls(drive.u.GetParent(h)), drive.rect(drive.u.GetParent(h))))


def run(tag):
    exe = ORIG if tag == "orig" else MINE
    args = [SAMPLE] if tag == "orig" else ["designer"]
    wait = 18 if tag == "orig" else 7
    env = None if tag == "orig" else {"ECLONE_DESIGN_DEMO": "1"}
    app = drive.App(exe, args=args, wait=wait, env=env)
    mdi = app.find("MDIClient")
    d = None
    for h in drive.descendants(app.frame):
        if drive.cls(h).startswith("Afx:400000:b") and "\u7a97\u53e3" in drive.title(h):
            d = h
    if mdi and d:
        drive.u.SendMessageW(mdi, WM_MDIACTIVATE, d, 0)
        time.sleep(2.5)
    print("=== %s ===" % tag)
    found = []
    for h in drive.descendants(app.frame):
        if drive.cls(h) == "_EL_RgnButton":
            found.append(h)
    print("  _EL_RgnButton windows: %d" % len(found))
    for h in found:
        print("   " + desc(h))
    # also list children of the form so we can see what else is there
    form = app.find("WTWindow")
    if form:
        print("  form %s children:" % desc(form))
        for k in drive.children(form):
            print("     " + desc(k))
    app.shot(app.frame, os.path.join(r"D:\eclone\shots\descmp", "%s_rgnbtn.png" % tag))
    app.close()


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "both"
    for tag in (("orig", "mine") if which == "both" else (which,)):
        run(tag)
