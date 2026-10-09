# -*- coding: utf-8 -*-
"""Compare the ORIGINAL IDE and our clone on one interaction scenario: launch both, take the same
action, then report the same measurements side by side.

Scenarios are selected with --case. Every scenario prints a normalized report so the two runs can
be diffed by eye or by tools/diff_sel.py for the screenshots.

usage:
  python tools/cmp_interact.py --case designer_geom
  python tools/cmp_interact.py --case compbox_place
  python tools/cmp_interact.py --case edit_click_cell
  python tools/cmp_interact.py --case prop_page
"""
import io
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


def open_designer(app):
    """make the designer document the active MDI child (both apps: activate the child that owns
    a WTWindow)."""
    mdi = app.find("MDIClient")
    design = None
    for h in drive.descendants(app.frame):
        if drive.cls(h).startswith("Afx:400000:b") and "\u7a97\u53e3" in drive.title(h):
            design = h
    if mdi and design:
        drive.u.SendMessageW(mdi, WM_MDIACTIVATE, design, 0)
        time.sleep(2.0)
    return design


def report(tag, lines):
    p = os.path.join(OUT, tag + ".txt")
    io.open(p, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print("--- %s ---" % tag)
    for l in lines:
        print("  " + l)


def case_designer_geom(app, tag):
    """where is the design form, its view chain, and the component box?"""
    lines = ["frame rect   = %s |%s|" % (drive.rect(app.frame), drive.title(app.frame))]
    for h in drive.descendants(app.frame):
        c = drive.cls(h)
        if c in ("MDIClient",) or c.startswith("AfxMDIFrame42s") or c.startswith("Afx:400000:b") \
           or c == "WTWindow" or c == "ECDesignOverlay" or c.startswith("Afx:400000:8") \
           or c == "AfxControlBar42s" or c == "_EL_RgnButton":
            lines.append("%-34s vis=%-5s %s |%s|" % (c, bool(drive.u.IsWindowVisible(h)),
                                                     drive.rect(h), drive.title(h)))
    report(tag, lines)
    app.shot(app.frame, os.path.join(OUT, tag + ".png"))


def case_compbox_place(app, tag):
    """open the component box, click one component button, then click once on the form: does a
    component appear, and where?"""
    open_designer(app)
    cb = app.find_titled("AfxControlBar42s", "\u7ec4\u4ef6\u7bb1")   # 窗口组件箱
    lines = []
    if cb:
        lines.append("comp bar %s vis=%s" % (drive.rect(cb), bool(drive.u.IsWindowVisible(cb))))
        inner = [h for h in drive.descendants(cb)]
        btns = [h for h in inner if drive.cls(h) == "Button"]
        lines.append("buttons=%d" % len(btns))
        # the first 28x28 button is the first component
        comp = [h for h in btns if drive.rect(h)[2] == 28 and drive.rect(h)[3] == 28]
        if comp:
            lines.append("first comp button %s |%s|" % (drive.rect(comp[0]), drive.title(comp[0])))
            app.click(comp[0], 14, 14)
            time.sleep(0.6)
            app.shot(cb, os.path.join(OUT, tag + "_armed.png"))
            lines.append("clicked the first component button")
    form = app.find("WTWindow")
    if form:
        before = len([h for h in drive.descendants(form) if drive.cls(h).startswith("_EL_") or
                      drive.cls(h) in ("Button", "Edit", "Static", "ComboBox")])
        fx, fy, fw, fh = drive.rect(form)
        lines.append("form %s children-before=%d" % ((fx, fy, fw, fh), before))
        app.click(form, 60, 80)
        time.sleep(0.8)
        after = [h for h in drive.descendants(form)]
        lines.append("form children-after=%d" % len(after))
        for h in after:
            lines.append("   %-24s %s |%s|" % (drive.cls(h), drive.rect(h), drive.title(h)))
        app.shot(app.frame, os.path.join(OUT, tag + "_placed.png"))
    else:
        lines.append("no WTWindow")
    report(tag, lines)


def case_edit_click_cell(app, tag):
    """click a table cell in the code editor: what gets painted / where does the caret go?"""
    ed = app.find("Afx:00000000:8") or app.find(":8:10003")
    if ed is None:
        for h in drive.descendants(app.frame):
            if drive.cls(h).startswith("Afx:") and ":8:" in drive.cls(h) and \
               drive.rect(h)[2] > 600:
                ed = h
    lines = ["editor = %s %s" % (ed, drive.rect(ed) if ed else None)]
    if ed:
        for tag2, cx, cy in (("asmname", 60, 12), ("subname", 60, 65), ("code", 90, 106)):
            app.click(ed, cx, cy)
            app.shot(ed, os.path.join(OUT, tag + "_" + tag2 + ".png"))
            lines.append("clicked (%d,%d) -> shot %s_%s.png" % (cx, cy, tag, tag2))
    report(tag, lines)


CASES = {
    "designer_geom": (case_designer_geom, True),
    "compbox_place": (case_compbox_place, True),
    "edit_click_cell": (case_edit_click_cell, False),
}


def main():
    case = "designer_geom"
    for i, a in enumerate(sys.argv):
        if a == "--case":
            case = sys.argv[i + 1]
    fn, with_sample = CASES[case]
    for tag, exe in (("orig", ORIG), ("mine", MINE)):
        args = []
        if with_sample:
            args = [SAMPLE]
        if exe == MINE:
            args = ([SAMPLE] if with_sample else []) + ["designer"]
        app = drive.App(exe, args=args, wait=18 if exe == ORIG else 6,
                        env={"ECLONE_DESIGN_DEMO": "1"} if exe == MINE else None)
        if app.frame is None:
            print("FAIL: %s did not start" % tag)
            continue
        try:
            fn(app, case + "_" + tag)
        finally:
            app.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
