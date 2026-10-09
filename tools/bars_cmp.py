# -*- coding: utf-8 -*-
"""Control-bar skeleton comparison between the original IDE and our clone.

Two questions:
  1. how many rows do the toolbars occupy at startup, and in which order? (the original stacks
     标准条/对齐条/定位条 as three rows; ours merges 标准条+对齐条 into one "定位工具条" row)
  2. our component box auto-opens on the designer page while the original keeps it collapsed

usage: python tools/bars_cmp.py
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
os.makedirs(OUT, exist_ok=True)


def rows_of_toolbars(app):
    """map each toolbar/toolbar-bar to (y, height, rect)"""
    out = []
    for h in drive.descendants(app.frame):
        c = drive.cls(h)
        if c == "AfxControlBar42s" or c.startswith("ToolbarWindow32"):
            r = drive.rect(h)
            out.append((r[1], r[0], r[2], r[3], c, drive.title(h),
                        bool(drive.u.IsWindowVisible(h))))
    return sorted(out)


def show(tag, app):
    print("--- %s frame=%s" % (tag, drive.rect(app.frame)))
    for y, x, w, h, c, t, vis in rows_of_toolbars(app):
        print("   y=%-4d x=%-5d %4dx%-4d vis=%-5s %-20s %s" % (y, x, w, h, vis, c, t))


def main():
    for tag, exe, args, wait in (("orig", ORIG, [SAMPLE], 18),
                                 ("mine", MINE, ["designer"], 6)):
        env = {"ECLONE_DESIGN_DEMO": "1"} if exe == MINE else None
        app = drive.App(exe, args=args, wait=wait, env=env)
        if not app.frame:
            print("%s: FAIL start" % tag)
            continue
        show(tag + " startup", app)
        app.shot(app.frame, os.path.join(OUT, "bars_%s_startup.png" % tag))
        # activate the designer document, then look at the bars again
        mdi = app.find("MDIClient")
        d = None
        for h in drive.descendants(app.frame):
            if drive.cls(h).startswith("Afx:400000:b") and "\u7a97\u53e3" in drive.title(h):
                d = h
        if mdi and d:
            drive.u.SendMessageW(mdi, WM_MDIACTIVATE, d, 0)
            time.sleep(2.5)
        show(tag + " designer", app)
        app.shot(app.frame, os.path.join(OUT, "bars_%s_designer.png" % tag))
        app.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
