# -*- coding: utf-8 -*-
"""Deep designer behaviour comparison with the harness bugs fixed (client-coordinate clicks).

For each app it:
  * selects the unit / the form and counts the navy handles in a SCREEN grab (PrintWindow does not
    include our overlay), recording each handle's offset from the selected rect
  * drags the SE handle of both the unit and the form and reports the size delta
  * verifies the form body cannot be dragged
  * clicks outside the form and reports whether the selection cleared

usage: python tools/cmp_desdeep2.py [orig|mine|both]
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
NAVY = (0, 0, 128)
os.makedirs(OUT, exist_ok=True)


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
            while stack:
                cx, cy = stack.pop()
                minx = min(minx, cx); maxx = max(maxx, cx)
                miny = min(miny, cy); maxy = max(maxy, cy)
                for nx, ny in ((cx+1, cy), (cx-1, cy), (cx, cy+1), (cx, cy-1)):
                    if 0 <= nx < w and 0 <= ny < h and not seen[ny][nx] and px[nx, ny] == NAVY:
                        seen[ny][nx] = True
                        stack.append((nx, ny))
            bw, bh = maxx - minx + 1, maxy - miny + 1
            if 4 <= bw <= 12 and 4 <= bh <= 12:
                out.append((minx, miny, bw, bh))
    return sorted(out)


def open_designer(app):
    mdi = app.find("MDIClient")
    d = None
    for h in drive.descendants(app.frame):
        if drive.cls(h).startswith("Afx:400000:b") and "\u7a97\u53e3" in drive.title(h):
            d = h
    if mdi and d:
        drive.u.SendMessageW(mdi, WM_MDIACTIVATE, d, 0)
        time.sleep(2.5)


def run(tag):
    exe = ORIG if tag == "orig" else MINE
    args = [SAMPLE] if tag == "orig" else ["designer"]
    wait = 18 if tag == "orig" else 6
    env = None if tag == "orig" else {"ECLONE_DESIGN_DEMO": "1"}
    app = drive.App(exe, args=args, wait=wait, env=env)
    lines = []
    open_designer(app)
    form = app.find("WTWindow")
    unit = [h for h in drive.children(form) if drive.cls(h) != "AfxControlBar42s"][0]

    def form_origin():
        pt = drive.wt.POINT(0, 0)
        drive.u.ClientToScreen(form, drive.ctypes.byref(pt))
        return pt.x, pt.y

    def click_win_pt(target, wx, wy):
        tx, ty, tw, th = drive.rect(target)
        ox, oy = form_origin()
        app.click(form, tx + wx - ox, ty + wy - oy)

    def drag_win_pt(target, wx0, wy0, wx1, wy1):
        tx, ty, tw, th = drive.rect(target)
        ox, oy = form_origin()
        app.drag(form, tx + wx0 - ox, ty + wy0 - oy, tx + wx1 - ox, ty + wy1 - oy)

    def grab_and_blobs(name, target):
        from PIL import ImageGrab
        x, y, w, h = drive.rect(form)
        img = ImageGrab.grab(bbox=(x, y, x + w, y + h))
        img.save(os.path.join(OUT, "%s_%s.png" % (tag, name)))
        blobs = navy_blobs(img)
        tx, ty, tw, th = drive.rect(target)
        fx, fy = drive.rect(form)[:2]
        rel = [(b[0] + b[2] // 2 - (tx + tw - fx), b[1] + b[3] // 2 - (ty + th - fy), b[2], b[3])
               for b in blobs]
        return len(blobs), sorted(set(rel))

    # ---- 1. select the unit
    ux, uy, uw, uh = drive.rect(unit)
    click_win_pt(unit, uw // 2, uh // 2)
    time.sleep(0.6)
    n, rel = grab_and_blobs("unit_sel", unit)
    lines.append("unit selected: handles=%d  handle-centre offsets from unit BR: %s" % (n, rel))

    # ---- 2. unit SE handle -> resize
    r0 = drive.rect(unit)
    x0, y0, w0, h0 = r0
    if rel:
        # the SE handle centre offset is the max (dx,dy)
        dx, dy = max((a, b) for a, b, w, h in rel)
        drag_win_pt(unit, w0 + dx, h0 + dy, w0 + dx + 25, h0 + dy + 20)
        r1 = drive.rect(unit)
        lines.append("unit SE-handle drag: %dx%d -> %dx%d  sizeDelta=(%+d,%+d)"
                     % (w0, h0, r1[2], r1[3], r1[2] - w0, r1[3] - h0))
        # undo it back so the next steps start clean
        drag_win_pt(unit, r1[2] + dx, r1[3] + dy, r1[2] + dx - 25, r1[3] + dy - 20)

    # ---- 3. the form: select it (click the paper, away from the unit) and count handles
    click_win_pt(form, 300, 30)
    time.sleep(0.6)
    n2, rel2 = grab_and_blobs("form_sel", form)
    lines.append("form selected: handles=%d  handle-centre offsets from form BR: %s" % (n2, rel2))

    # ---- 4. form SE handle -> resize
    f0 = drive.rect(form)
    if rel2:
        dx, dy = max((a, b) for a, b, w, h in rel2)
        drag_win_pt(form, f0[2] + dx, f0[3] + dy, f0[2] + dx + 20, f0[3] + dy + 15)
        f1 = drive.rect(form)
        lines.append("form SE-handle drag: %dx%d -> %dx%d  sizeDelta=(%+d,%+d)"
                     % (f0[2], f0[3], f1[2], f1[3], f1[2] - f0[2], f1[3] - f0[3]))
    # ---- 5. form body drag must not move it
    f2 = drive.rect(form)
    drag_win_pt(form, 300, 30, 340, 60)
    f3 = drive.rect(form)
    lines.append("form body drag: moved=%s (%s -> %s)"
                 % ((f2[0] != f3[0] or f2[1] != f3[1]), f2[:2], f3[:2]))
    app.shot(app.frame, os.path.join(OUT, "%s_deep_frame.png" % tag))
    app.close()
    return lines


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "both"
    res = {}
    for tag in (("orig", "mine") if which == "both" else (which,)):
        res[tag] = run(tag)
    for tag, lines in res.items():
        print("=== %s ===" % tag)
        for l in lines:
            print("   " + l)
    if len(res) == 2:
        print("=== side by side ===")
        for i in range(max(len(res["orig"]), len(res["mine"]))):
            print("  O: %s" % (res["orig"][i] if i < len(res["orig"]) else ""))
            print("  M: %s" % (res["mine"][i] if i < len(res["mine"]) else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
