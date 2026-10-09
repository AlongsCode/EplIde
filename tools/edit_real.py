# -*- coding: utf-8 -*-
"""Real-input check of the code editor: click into a table cell, double-click it (in-place edit),
click on the code line, right-click (context menu), and report the caret + what happened.

usage: python tools/edit_real.py [orig|mine]
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
os.makedirs(OUT, exist_ok=True)


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "mine"
    if which == "mine":
        exe, args, wait, env = MINE, [], 6, {}
    else:
        exe, args, wait, env = ORIG, [SAMPLE], 18, None
    app = drive.App(exe, args=args, wait=wait, env=env)
    if not app.frame:
        print("FAIL start")
        return 1
    ed = None
    best = 0
    for h in drive.descendants(app.frame):
        if ":8:" in drive.cls(h) and "\u7f16\u8f91" not in drive.title(h):
            r = drive.rect(h)
            if r[2] * r[3] > best and r[2] > 600:
                best = r[2] * r[3]
                ed = h
    print("editor=%s %s" % (ed, drive.rect(ed) if ed else None))
    if not ed:
        app.close()
        return 1
    # clicks: table header cell, table data cell, a flag cell, the code line
    for tag, cx, cy in (("hdr_name", 60, 12), ("data_name", 70, 33), ("flag_cell", 200, 12),
                        ("code", 90, 106)):
        app.click(ed, cx, cy)
        app.shot(ed, os.path.join(OUT, "edit_%s_%s.png" % (which, tag)))
        kids = [(drive.cls(k), drive.rect(k)) for k in drive.children(ed)]
        print("  click %-10s (%3d,%3d) -> children=%s" % (tag, cx, cy, kids))
    # double click a data cell -> in-place edit?
    app.click(ed, 70, 33, dbl=True)
    time.sleep(0.8)
    kids = [(drive.cls(k), drive.rect(k), drive.title(k)) for k in drive.children(ed)]
    print("  double-click data cell -> children=%s" % kids)
    app.shot(ed, os.path.join(OUT, "edit_%s_dblclick.png" % which))
    # right-click -> context menu? (the popup menu is a TOP-LEVEL #32768 window, not a child)
    app.click(ed, 90, 106, right=True)
    time.sleep(0.9)
    pops = [(hex(h), drive.rect(h)) for h in drive.tops(app.pid) if drive.cls(h) == "#32768"
            and drive.u.IsWindowVisible(h)]
    print("  right-click -> visible popup menus (top-level): %s" % pops)
    app.shot(app.frame, os.path.join(OUT, "edit_%s_ctxmenu.png" % which))
    app.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
