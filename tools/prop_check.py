# -*- coding: utf-8 -*-
"""Property page check with REAL input: select a component on the design surface, switch the work
pane to its 属性 tab, and report what the property list holds (count + first rows).

usage: python tools/prop_check.py [orig|mine]
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
LVM_GETITEMCOUNT = 0x1004
LVM_GETITEMTEXT = 0x102D


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "mine"
    if which == "mine":
        exe, args, wait, env = MINE, ["designer"], 6, {"ECLONE_DESIGN_DEMO": "1"}
    else:
        exe, args, wait, env = ORIG, [SAMPLE], 18, None
    app = drive.App(exe, args=args, wait=wait, env=env)
    if not app.frame:
        print("FAIL start")
        return 1
    # activate the designer document
    mdi = app.find("MDIClient")
    d = None
    for h in drive.descendants(app.frame):
        if drive.cls(h).startswith("Afx:400000:b") and "\u7a97\u53e3" in drive.title(h):
            d = h
    if mdi and d:
        drive.u.SendMessageW(mdi, WM_MDIACTIVATE, d, 0)
        time.sleep(2.5)

    # work pane = the AfxControlBar42s titled 工作夹
    work = None
    for h in drive.descendants(app.frame):
        if drive.cls(h) == "AfxControlBar42s" and drive.title(h).startswith("\u5de5\u4f5c"):
            work = h
    tabs = [h for h in drive.descendants(work) if drive.cls(h) == "SysTabControl32"]
    print("work=%s tabs=%s" % (work, tabs))
    if tabs:
        t = tabs[0]
        w, h = drive.client(t)
        # 3 tabs at the bottom; click the 3rd one (属性)
        for idx in (2, 1):
            cx = 20 + idx * 55
            cy = h - 10
            app.click(t, cx, cy)
            time.sleep(0.8)
            sel = drive.u.SendMessageW(t, 0x130B, 0, 0)   # TCM_GETCURSEL
            print("  clicked tab x=%d -> cursel=%s" % (cx, sel))
            if sel == idx:
                break

    lists = [h for h in drive.descendants(work) if drive.cls(h) == "SysListView32"]
    print("property lists in the work pane: %s" % [(h, drive.u.IsWindowVisible(h)) for h in lists])
    for h in lists:
        n = drive.u.SendMessageW(h, LVM_GETITEMCOUNT, 0, 0)
        print("   0x%08X vis=%s rows=%d rect=%s" % (h, bool(drive.u.IsWindowVisible(h)), n,
                                                    drive.rect(h)))

    # now select the component and look again
    form = app.find("WTWindow")
    unit = [k for k in drive.children(form) if drive.cls(k) != "AfxControlBar42s"]
    if unit:
        ux, uy, uw, uh = drive.rect(unit[0])
        fx, fy, fw, fh = drive.rect(form)
        app.click(form, (ux - fx) + uw // 2, (uy - fy) + uh // 2)
        time.sleep(1.0)
    for h in lists:
        n = drive.u.SendMessageW(h, LVM_GETITEMCOUNT, 0, 0)
        print("   after selecting the component: 0x%08X vis=%s rows=%d"
              % (h, bool(drive.u.IsWindowVisible(h)), n))
    app.shot(app.frame, os.path.join(OUT, "prop_%s.png" % which))
    app.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
