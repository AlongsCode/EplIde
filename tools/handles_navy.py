# -*- coding: utf-8 -*-
"""Find the selection handles by COLOUR (the original paints them navy #000080 squares just outside
the red selection frame), then drag each one and report whether the unit/form reacted.

This is the measurement that settles "the handles work / don't work" honestly, because the handle
pixel positions come from the screenshot instead of from a coordinate guess.

usage: python tools/handles_navy.py [orig|mine]
"""
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
NAVY = (0, 0, 128)
os.makedirs(OUT, exist_ok=True)


def open_designer(app):
    mdi = app.find("MDIClient")
    d = None
    for h in drive.descendants(app.frame):
        if drive.cls(h).startswith("Afx:400000:b") and "\u7a97\u53e3" in drive.title(h):
            d = h
    if mdi and d:
        drive.u.SendMessageW(mdi, WM_MDIACTIVATE, d, 0)
        time.sleep(2.5)


def navy_blobs(img):
    px = img.load()
    w, h = img.size
    seen = [[False] * w for _ in range(h)]
    out = []
    for y in range(h):
        for x in range(w):
            if seen[y][x] or px[x, y] != NAVY:
                continue
            stack = [(x, y)]
            seen[y][x] = True
            minx = maxx = x
            miny = maxy = y
            n = 0
            while stack:
                cx, cy = stack.pop()
                n += 1
                minx = min(minx, cx); maxx = max(maxx, cx)
                miny = min(miny, cy); maxy = max(maxy, cy)
                for nx, ny in ((cx+1,cy),(cx-1,cy),(cx,cy+1),(cx,cy-1)):
                    if 0 <= nx < w and 0 <= ny < h and not seen[ny][nx] and px[nx, ny] == NAVY:
                        seen[ny][nx] = True
                        stack.append((nx, ny))
            bw, bh = maxx - minx + 1, maxy - miny + 1
            if 4 <= bw <= 12 and 4 <= bh <= 12:
                out.append((minx, miny, bw, bh))
    return out


def main():
    tag = sys.argv[1] if len(sys.argv) > 1 else "mine"
    exe = ORIG if tag == "orig" else MINE
    args = [SAMPLE] if tag == "orig" else ["designer"]
    wait = 18 if tag == "orig" else 6
    env = None if tag == "orig" else {"ECLONE_DESIGN_DEMO": "1"}
    app = drive.App(exe, args=args, wait=wait, env=env)
    open_designer(app)
    form = app.find("WTWindow")
    unit = [h for h in drive.children(form) if drive.cls(h) != "AfxControlBar42s"][0]
    ox, oy = drive.rect(form)[:2]
    ux, uy, uw, uh = drive.rect(unit)

    def click_target(target, wx, wy):
        """click at (wx,wy) relative to `target`'s WINDOW rect, delivered via form client coords"""
        tx, ty, tw, th = drive.rect(target)
        pt = drive.wt.POINT(0, 0)
        drive.u.ClientToScreen(form, drive.ctypes.byref(pt))
        app.click(form, tx + wx - pt.x, ty + wy - pt.y)

    def drag_target(target, wx0, wy0, wx1, wy1):
        tx, ty, tw, th = drive.rect(target)
        pt = drive.wt.POINT(0, 0)
        drive.u.ClientToScreen(form, drive.ctypes.byref(pt))
        app.drag(form, tx + wx0 - pt.x, ty + wy0 - pt.y,
                 tx + wx1 - pt.x, ty + wy1 - pt.y)

    # select the unit by clicking its middle
    click_target(unit, uw // 2, uh // 2)
    time.sleep(0.7)
    img = app.shot(form, os.path.join(OUT, "%s_navy_form.png" % tag))
    # ⚠️ PrintWindow of the form does NOT include our overlay's painting (the overlay is a sibling
    # child with WS_EX_TRANSPARENT; PrintWindow renders the asked window only). A screen grab does.
    from PIL import ImageGrab
    fx, fy, fw, fh = drive.rect(form)
    grab = ImageGrab.grab(bbox=(fx, fy, fx + fw, fy + fh))
    grab.save(os.path.join(OUT, "%s_navy_screen.png" % tag))
    blobs = navy_blobs(grab)
    print("screen grab %s ; navy blobs: %s" % (grab.size, blobs))
    usx, usy = ux - ox, uy - oy          # unit's window rect relative to the FORM WINDOW rect
    print("tag=%s form=%s unit=%s" % (tag, (ox, oy), (ux, uy, uw, uh)))
    print("unit relative to form window rect: (%d,%d) %dx%d" % (usx, usy, uw, uh))
    print("navy blobs (form-window coords): %s" % blobs)
    for bx, by, bw, bh in blobs:
        print("   blob at (%d,%d) %dx%d   -> unit-relative offset (%+d,%+d)"
              % (bx, by, bw, bh, bx + bw // 2 - (usx + uw), by + bh // 2 - (usy + uh)))

    # drag the SE-ish blob and see whether the unit resizes
    if blobs:
        # pick the blob closest to the unit's bottom-right
        bx, by, bw, bh = min(blobs, key=lambda b: abs((b[0] + b[2] // 2) - (usx + uw + 6)) +
                                                abs((b[1] + b[3] // 2) - (usy + uh + 6)))
        r0 = drive.rect(unit)
        drag_target(unit, (bx + bw // 2) - usx, (by + bh // 2) - usy,
                    (bx + bw // 2) - usx + 25, (by + bh // 2) - usy + 20)
        r1 = drive.rect(unit)
        print("drag SE blob: %s -> %s  resized=%s" % (r0, r1, r0[2:] != r1[2:]))
        app.shot(form, os.path.join(OUT, "%s_navy_after.png" % tag))
    app.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
