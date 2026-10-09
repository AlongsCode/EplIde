# -*- coding: utf-8 -*-
"""Pixel-locate the designer's selection handles, then drive them.

Why: a first attempt reported "the handles do nothing" in BOTH apps -- which was a bug in the probe
(it mixed GetWindowRect coordinates with client coordinates). So the harness now derives the handle
pixel positions from the screenshot itself.

usage: python tools/handles_px.py [orig|mine]
"""
import os
import sys
import time
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

ORIG = r"C:\Program Files (x86)\e\e.exe"
MINE = r"D:\eclone\bin\EClone.exe"
SAMPLE = r"D:\eclone\shots\e_samples\graphic.e"
OUT = r"D:\eclone\shots\desdeep"
WM_MDIACTIVATE = 0x0222
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


def white_blobs(img, x0, y0, x1, y1):
    """find small solid-white rectangles (the handles) in the given image region"""
    px = img.load()
    seen = set()
    blobs = []
    for y in range(y0, y1):
        for x in range(x0, x1):
            if (x, y) in seen:
                continue
            if px[x, y] != (255, 255, 255):
                continue
            # flood fill this white blob
            stack = [(x, y)]
            minx = maxx = x
            miny = maxy = y
            n = 0
            while stack:
                cx, cy = stack.pop()
                if (cx, cy) in seen:
                    continue
                if cx < x0 or cx >= x1 or cy < y0 or cy >= y1:
                    continue
                if px[cx, cy] != (255, 255, 255):
                    continue
                seen.add((cx, cy))
                n += 1
                minx = min(minx, cx); maxx = max(maxx, cx)
                miny = min(miny, cy); maxy = max(maxy, cy)
                stack.extend([(cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)])
            w = maxx - minx + 1
            h = maxy - miny + 1
            if 3 <= w <= 12 and 3 <= h <= 12 and n >= w * h * 0.5:
                blobs.append((minx, miny, w, h))
    return blobs


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
    print("form=%s %s" % (form, drive.rect(form)))
    print("unit=%s %s" % (unit, drive.rect(unit)))

    # select the unit: click at the WINDOW-rect-relative offset via the overlay/form client
    def click_window_pt(host, target, ox, oy):
        """click at (ox,oy) measured from the target's WINDOW rect, delivered through `host`'s client
        coordinates (so the host's client origin is what matters, not its window origin)."""
        tx, ty, tw, th = drive.rect(target)
        hx, hy, hw, hh = drive.rect(host)
        # client origin of the host in screen coords
        pt = drive.wt.POINT(0, 0)
        drive.u.ClientToScreen(host, drive.ctypes.byref(pt))
        sx = tx + ox
        sy = ty + oy
        return app.click(host, sx - pt.x, sy - pt.y)

    def drag_window_pt(host, target, ox0, oy0, ox1, oy1):
        tx, ty, tw, th = drive.rect(target)
        pt = drive.wt.POINT(0, 0)
        drive.u.ClientToScreen(host, drive.ctypes.byref(pt))
        app.drag(host, tx + ox0 - pt.x, ty + oy0 - pt.y, tx + ox1 - pt.x, ty + oy1 - pt.y)

    click_window_pt(form, unit, 52, 56)          # unit's middle
    time.sleep(0.6)
    img = app.shot(form, os.path.join(OUT, "%s_sel_form.png" % tag))
    print("shot of the form with the unit selected: %s" % (img.size,))
    blobs = white_blobs(img, 0, 0, img.width, img.height)
    print("white blobs (candidate handles) in form-client coords: %s" % blobs[:20])

    # the unit's rect in form client coords
    ux, uy, uw, uh = drive.rect(unit)
    fx, fy, fw, fh = drive.rect(form)
    cpt = drive.wt.POINT(0, 0)
    drive.u.ClientToScreen(form, drive.ctypes.byref(cpt))
    ulx, uly = ux - cpt.x, uy - cpt.y
    print("unit in form client coords: (%d,%d) %dx%d" % (ulx, uly, uw, uh))

    # try the 8 handle positions at the pixel-measured offsets
    if blobs:
        for bx, by, bw, bh in blobs[:8]:
            r0 = drive.rect(unit)
            app.drag(form, bx + bw // 2, by + bh // 2, bx + bw // 2 + 20, by + bh // 2 + 20)
            r1 = drive.rect(unit)
            print("  drag from blob (%d,%d)+%dx%d: %dx%d -> %dx%d changed=%s"
                  % (bx, by, bw, bh, r0[2], r0[3], r1[2], r1[3], r0 != r1))
    else:
        # fall back: try offsets around the unit's rect, in form-client coords
        for dx in (-4, -3, -2, 0, 2, 3, 4):
            for dy in (-4, -3, -2, 0, 2, 3, 4):
                r0 = drive.rect(unit)
                app.drag(form, ulx + uw + dx, uly + uh + dy,
                         ulx + uw + dx + 20, uly + uh + dy + 20)
                r1 = drive.rect(unit)
                if r0 != r1:
                    print("  SE-ish offset (%+d,%+d) resized/moved: %s -> %s" % (dx, dy, r0, r1))
    app.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
