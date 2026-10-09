# -*- coding: utf-8 -*-
"""Dump the designer VIEW's chain of windows (class/text/rect) for both apps, so the design form's
position inside the view can be compared honestly (the user reports "background and window position
are off").

usage: python tools/des_chain.py [orig|mine|both]
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


def run(tag):
    exe = ORIG if tag == "orig" else MINE
    args = [SAMPLE] if tag == "orig" else ["designer"]
    wait = 18 if tag == "orig" else 6
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
    form = app.find("WTWindow")
    print("=== %s ===" % tag)
    # walk up from the form to the frame
    chain = []
    h = form
    while h:
        chain.append(h)
        h = drive.u.GetParent(h)
    for h in reversed(chain):
        cl = drive.cls(h)
        print("   %-38s %-40s vis=%-5s %s |%s|"
              % (cl, drive.rect(h), bool(drive.u.IsWindowVisible(h)),
                 "style=0x%08X" % (drive.u.GetWindowLongW(h, -16) & 0xFFFFFFFF), drive.title(h)))
    # siblings of the form (the desk/paper windows)
    parent = drive.u.GetParent(form)
    print("   siblings of the form (children of %s):" % drive.cls(parent))
    for k in drive.children(parent):
        print("      %-36s %s |%s|" % (drive.cls(k), drive.rect(k), drive.title(k)))
    app.shot(app.frame, os.path.join(r"D:\eclone\shots\descmp", "%s_chain.png" % tag))
    app.close()


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "both"
    for tag in (("orig", "mine") if which == "both" else (which,)):
        run(tag)
    return 0


if __name__ == "__main__":
    sys.exit(main())
