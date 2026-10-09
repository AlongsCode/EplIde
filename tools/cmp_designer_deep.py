# -*- coding: utf-8 -*-
"""Deep designer comparison: run the same real-mouse sequence against the original IDE and our
clone, capture a 1:1 shot after every step, and print every measured rect side by side.

Covers what the user asked about:
  * the design form: must NOT move when dragged by its body/title, MUST resize from its 8 handles
  * a component: MUST move when dragged by its body, MUST resize from its 8 handles
  * the selection frame (border offset + 8 white handles) -- pixel measured from the shots

usage: python tools/cmp_designer_deep.py [orig|mine|both]
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
OUT = r"D:\eclone\shots\desdeep"
WM_MDIACTIVATE = 0x0222
os.makedirs(OUT, exist_ok=True)

# handle ids: (dx, dy) in {-1,0,1}; -1 = the low edge, +1 = the high edge
HANDLES = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]


def open_designer(app):
    mdi = app.find("MDIClient")
    d = None
    for h in drive.descendants(app.frame):
        if drive.cls(h).startswith("Afx:400000:b") and "\u7a97\u53e3" in drive.title(h):
            d = h
    if mdi and d:
        drive.u.SendMessageW(mdi, WM_MDIACTIVATE, d, 0)
        time.sleep(2.5)
    return d


def form_and_unit(app):
    form = app.find("WTWindow")
    unit = None
    if form:
        for h in drive.children(form):
            if drive.cls(h) != "AfxControlBar42s":
                unit = h
    return form, unit


def handle_point(app, target, dx, dy, pad=3):
    """screen point of the handle (dx,dy) of `target`, using the OVERLAY origin when there is one
    (our overlay is padded 6 px beyond the form; the handle centres sit on the form rect + 3)."""
    over = app.find("ECDesignOverlay")
    x, y, w, h = drive.rect(target)
    if dx < 0:
        px = x - pad
    elif dx > 0:
        px = x + w + pad
    else:
        px = x + w // 2
    if dy < 0:
        py = y - pad
    elif dy > 0:
        py = y + h + pad
    else:
        py = y + h // 2
    return px, py


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "both"
    results = {}
    for tag in (("orig", "mine") if which == "both" else (which,)):
        exe = ORIG if tag == "orig" else MINE
        args = [SAMPLE] if tag == "orig" else ["designer"]
        wait = 18 if tag == "orig" else 6
        env = None if tag == "orig" else {"ECLONE_DESIGN_DEMO": "1"}
        app = drive.App(exe, args=args, wait=wait, env=env)
        if not app.frame:
            print("%s: FAIL start" % tag)
            continue
        lines = []
        open_designer(app)
        form, unit = form_and_unit(app)
        lines.append("frame            %s" % (drive.rect(app.frame),))
        lines.append("form             %s" % (drive.rect(form),))
        lines.append("unit             %s %s" % (drive.cls(unit) if unit else None,
                                                 drive.rect(unit) if unit else None))
        app.shot(app.frame, os.path.join(OUT, "%s_00_start.png" % tag))

        # ---------------- 1. click the unit's body -> selected?
        ux, uy, uw, uh = drive.rect(unit)
        fx, fy, fw, fh = drive.rect(form)
        app.click(form, (ux - fx) + uw // 2, (uy - fy) + uh // 2)
        time.sleep(0.6)
        app.shot(form, os.path.join(OUT, "%s_01_unit_selected.png" % tag))
        lines.append("after click on unit body: shot %s_01_unit_selected.png" % tag)

        # ---------------- 2. drag the unit's body -> move
        r0 = drive.rect(unit)
        app.drag(form, (r0[0] - fx) + 20, (r0[1] - fy) + 20,
                 (r0[0] - fx) + 60, (r0[1] - fy) + 50)
        r1 = drive.rect(unit)
        lines.append("unit drag body      %s -> %s moved=%s resized=%s"
                     % (r0, r1, (r0[0] != r1[0] or r0[1] != r1[1]), (r0[2:] != r1[2:])))
        app.shot(form, os.path.join(OUT, "%s_02_unit_moved.png" % tag))

        # ---------------- 3. each handle of the unit -> resize (restore between tries)
        for dx, dy in HANDLES:
            r = drive.rect(unit)
            hx, hy = handle_point(app, unit, dx, dy)
            app.drag(form, hx - fx, hy - fy, hx - fx + 18 * (dx or 1), hy - fy + 18 * (dy or 1))
            r2 = drive.rect(unit)
            lines.append("unit handle(%+d,%+d) at (%d,%d): %dx%d -> %dx%d  sizeChanged=%s posChanged=%s"
                         % (dx, dy, hx, hy, r[2], r[3], r2[2], r2[3],
                            (r[2:] != r2[2:]), (r[:2] != r2[:2])))
            app.shot(form, os.path.join(OUT, "%s_03_unit_h%+d%+d.png" % (tag, dx, dy)))
        app.click(form, 5, 5)   # deselect (click empty form area)
        time.sleep(0.4)

        # ---------------- 4. the form: body drag must NOT move it
        f0 = drive.rect(form)
        app.drag(form, fw // 2, fh - 20, fw // 2 + 40, fh + 10)
        f1 = drive.rect(form)
        lines.append("form drag body      %s -> %s moved=%s" % (f0, f1, f0[:2] != f1[:2]))
        # click the form to select the form itself
        app.click(form, fw // 2, fh // 2)
        time.sleep(0.5)
        app.shot(form, os.path.join(OUT, "%s_04_form_selected.png" % tag))
        for dx, dy in HANDLES:
            r = drive.rect(form)
            hx, hy = handle_point(app, form, dx, dy)
            app.drag(form, hx - fx, hy - fy, hx - fx + 15 * (dx or 1), hy - fy + 15 * (dy or 1))
            r2 = drive.rect(form)
            lines.append("form handle(%+d,%+d) at (%d,%d): %dx%d -> %dx%d  sizeChanged=%s posChanged=%s"
                         % (dx, dy, hx, hy, r[2], r[3], r2[2], r2[3],
                            (r[2:] != r2[2:]), (r[:2] != r2[:2])))
            app.shot(form, os.path.join(OUT, "%s_05_form_h%+d%+d.png" % (tag, dx, dy)))
            fx, fy, fw, fh = drive.rect(form)

        io.open(os.path.join(OUT, "%s_report.txt" % tag), "w", encoding="utf-8").write(
            "\n".join(lines) + "\n")
        results[tag] = lines
        app.close()

    for tag, lines in results.items():
        print("=== %s ===" % tag)
        for l in lines:
            print("  " + l)
    if len(results) == 2:
        print("=== side by side ===")
        a = results["orig"]
        b = results["mine"]
        for i in range(max(len(a), len(b))):
            print("  O: %-90s" % (a[i] if i < len(a) else ""))
            print("  M: %-90s" % (b[i] if i < len(b) else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
