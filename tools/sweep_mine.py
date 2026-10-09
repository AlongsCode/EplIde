# -*- coding: utf-8 -*-
"""Real-input interaction sweep for OUR build: click through the main widgets and report what
changed. Every action uses a real mouse/key event, so a hit-testing/transparency bug shows up
(the designer overlay bug of round 26 was invisible to the PostMessage-based smokes).

usage: python tools/sweep_mine.py [--keep]
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

MINE = r"D:\eclone\bin\EClone.exe"
OUT = r"D:\eclone\shots\sweep"
os.makedirs(OUT, exist_ok=True)
VK_DELETE = 0x2E
VK_F9 = 0x78


def main():
    app = drive.App(MINE, args=["designer"], wait=6, env={"ECLONE_DESIGN_DEMO": "1"})
    if not app.frame:
        print("FAIL start")
        return 1
    form = app.find("WTWindow")
    over = app.find("ECDesignOverlay")
    unit = [h for h in drive.children(form) if drive.cls(h) != "AfxControlBar42s"]
    print("form=%s %s" % (form, drive.rect(form)))
    print("overlay=%s %s" % (over, drive.rect(over)))
    unit = unit[0] if unit else None
    print("unit=%s %s" % (unit, drive.rect(unit) if unit else None))
    fails = []

    def check(name, ok, extra=""):
        print("  [%s] %s %s" % ("PASS" if ok else "FAIL", name, extra))
        if not ok:
            fails.append(name)

    def unit_rect():
        return drive.rect(unit) if unit else None

    # ---------------------------------------------------------------- 1. click to select
    if unit:
        ux, uy, uw, uh = drive.rect(unit)
        fx, fy, fw, fh = drive.rect(form)
        app.click(form, (ux - fx) + uw // 2, (uy - fy) + uh // 2)
        time.sleep(0.5)
        app.shot(form, os.path.join(OUT, "1_selected.png"))
        # the handles are painted by the overlay -> they only show up in a SCREEN grab
        from PIL import ImageGrab
        sx, sy, sw, sh = drive.rect(form)
        img = ImageGrab.grab(bbox=(sx, sy, sx + sw, sy + sh))
        navy = sum(1 for yy in range(img.height) for xx in range(img.width)
                   if img.getpixel((xx, yy)) == (0, 0, 128))
        check("clicking a component selects it (navy handles px=%s)" % navy, navy > 100)

    # ---------------------------------------------------------------- 2. drag to move
    if unit:
        r0 = unit_rect()
        app.drag(form, r0[0] - fx + 20, r0[1] - fy + 20, r0[0] - fx + 60, r0[1] - fy + 50)
        r1 = unit_rect()
        check("dragging a component moves it", (r1[0] != r0[0] or r1[1] != r0[1]),
              "%s -> %s" % (r0, r1))

    # ---------------------------------------------------------------- 3. resize via SE handle
    if unit and over:
        ox, oy, ow, oh = drive.rect(over)
        r0 = unit_rect()
        hx = (r0[0] + r0[2]) - ox + 3
        hy = (r0[1] + r0[3]) - oy + 3
        app.drag(over, hx, hy, hx + 25, hy + 20)
        r1 = unit_rect()
        check("dragging the SE handle resizes", (r1[2] != r0[2] or r1[3] != r0[3]),
              "%s -> %s" % (r0, r1))
        app.shot(form, os.path.join(OUT, "3_resized.png"))

    # ---------------------------------------------------------------- 4. Delete removes it
    if unit:
        app.key(VK_DELETE)
        time.sleep(0.5)
        gone = not drive.u.IsWindow(unit)
        check("Delete removes the selected component", gone)

    # ---------------------------------------------------------------- 5. the form itself
    ox, oy, ow, oh = drive.rect(over)
    fx2, fy2, fw2, fh2 = drive.rect(form)
    app.drag(form, fw2 // 2, fh2 - 20, fw2 // 2 + 40, fh2 + 10)
    fx3, fy3, fw3, fh3 = drive.rect(form)
    check("the design form cannot be dragged", (fx3 == fx2 and fy3 == fy2))
    app.drag(over, ow - 6 + 3, oh - 6 + 3, ow + 20, oh + 15)
    fx4, fy4, fw4, fh4 = drive.rect(form)
    check("the design form resizes from its SE handle", (fw4 != fw3 or fh4 != fh3),
          "%dx%d -> %dx%d" % (fw3, fh3, fw4, fh4))
    app.shot(app.frame, os.path.join(OUT, "5_form.png"))

    # ---------------------------------------------------------------- 6. code editor clicks
    ed = app.find(":10005:")
    if ed:
        app.click(ed, 60, 12)
        app.shot(ed, os.path.join(OUT, "6_cell_click.png"))
        check("clicking a table cell does not crash", drive.u.IsWindow(app.frame))
        app.click(ed, 90, 106)
        app.shot(ed, os.path.join(OUT, "6_code_click.png"))
        app.key(VK_F9)
        time.sleep(0.4)
        check("F9 in the code editor survives", drive.u.IsWindow(app.frame))
    else:
        print("  (no editor window found)")

    app.close()
    print("FAILS: %s" % (fails or "none"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
