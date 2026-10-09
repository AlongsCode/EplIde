# -*- coding: utf-8 -*-
"""Component-box interaction comparison: locate the box anywhere in the process (hidden too),
try to show it the way the original does, click a component button, then place it on the form.

Findings this is meant to settle:
  * is the box collapsed at startup in the original (ours auto-opens on the designer page)?
  * does clicking a component button arm placement, and what does the placed component look like?
  * where is the placement preview / what cursor is used?

usage: python tools/cmp_compbox.py [orig|mine|both]
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
WM_COMMAND = 0x0111
os.makedirs(OUT, exist_ok=True)


def find_compbox(app):
    """The component box = the #32770 that holds a ~116x20 group selector plus 28x28 buttons.
    The original's selector is a Button titled 基本组件; ours is a ListBox. Detect by geometry so
    both are found."""
    for h in drive.descendants(app.frame):
        if drive.cls(h) != "#32770":
            continue
        kids = drive.children(h)
        btns = [k for k in kids if drive.cls(k) == "Button" and drive.rect(k)[2] == 28]
        if len(btns) < 10:
            continue
        sel = None
        for k in kids:
            r = drive.rect(k)
            if r[2] > 100 and r[3] <= 24:
                sel = k
                break
        return sel, btns, h
    return None, [], None


def show_compbox(app):
    sel, btns, dlg = find_compbox(app)
    if sel is not None:
        return sel, btns, dlg
    # try activating the designer document first
    mdi = app.find("MDIClient")
    design = None
    for h in drive.descendants(app.frame):
        if drive.cls(h).startswith("Afx:400000:b") and "\u7a97\u53e3" in drive.title(h):
            design = h
    if mdi and design:
        drive.u.SendMessageW(mdi, WM_MDIACTIVATE, design, 0)
        time.sleep(2.0)
    sel, btns, dlg = find_compbox(app)
    return sel, btns, dlg


def run(tag, exe, args, wait, env):
    app = drive.App(exe, args=args, wait=wait, env=env)
    if not app.frame:
        print("%s: FAIL start" % tag)
        return
    print("=== %s frame=%s ===" % (tag, drive.rect(app.frame)))
    sel, btns, dlg = find_compbox(app)
    print("  startup: selector=%s buttons=%d dlg=%s" % (sel, len(btns), drive.rect(dlg) if dlg else None))
    if sel is None:
        sel, btns, dlg = show_compbox(app)
        print("  after activating the designer doc: selector=%s buttons=%d dlg=%s"
              % (sel, len(btns), drive.rect(dlg) if dlg else None))
    if sel is None:
        print("  -> the component box is NOT in the window tree at all (lazy created?)")
        app.shot(app.frame, os.path.join(OUT, "compbox_%s_nobox.png" % tag))
        app.close()
        return
    print("  selector rect=%s vis=%s title=|%s|"
          % (drive.rect(sel), bool(drive.u.IsWindowVisible(sel)), drive.title(sel)))
    if btns:
        b0 = btns[0]
        r = drive.rect(b0)
        print("  first comp button rect=%s vis=%s title=|%s|"
              % (r, bool(drive.u.IsWindowVisible(b0)), drive.title(b0)))
    # click the first component button (client centre of the button)
    if btns and drive.u.IsWindowVisible(btns[0]):
        app.click(btns[0], 14, 14)
        time.sleep(0.8)
        app.shot(app.frame, os.path.join(OUT, "compbox_%s_armed.png" % tag))
        print("  clicked the first component button")
        # then click once on the form -> does a component appear?
        form = app.find("WTWindow")
        if form:
            fx, fy, fw, fh = drive.rect(form)
            kids_before = drive.children(form)
            print("  form=%s children=%d" % ((fx, fy, fw, fh), len(kids_before)))
            app.click(form, 60, 80)
            time.sleep(1.0)
            kids_after = drive.children(form)
            print("  after one click on the form: children=%d" % len(kids_after))
            for h in kids_after:
                print("     %-22s %s |%s|" % (drive.cls(h), drive.rect(h), drive.title(h)))
            app.shot(form, os.path.join(OUT, "compbox_%s_placed.png" % tag))
            # drag-place: second component by dragging a rectangle
            if len(btns) > 1:
                app.click(btns[1], 14, 14)
                time.sleep(0.6)
                app.drag(form, 120, 120, 220, 180)
                time.sleep(1.0)
                kids2 = drive.children(form)
                print("  after drag-place: children=%d" % len(kids2))
                for h in kids2:
                    print("     %-22s %s |%s|" % (drive.cls(h), drive.rect(h), drive.title(h)))
                app.shot(form, os.path.join(OUT, "compbox_%s_dragplaced.png" % tag))
        else:
            print("  no WTWindow to place on")
    app.close()


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "both"
    if which in ("orig", "both"):
        run("orig", ORIG, [SAMPLE], 18, None)
    if which in ("mine", "both"):
        run("mine", MINE, ["designer"], 6, {"ECLONE_DESIGN_DEMO": "1"})
    return 0


if __name__ == "__main__":
    sys.exit(main())
