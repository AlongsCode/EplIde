# -*- coding: utf-8 -*-
"""One round of the designer-page 1:1 push.

Grabs the real design window (WTWindow) 1:1 from the ORIGINAL e.exe and from EClone,
both opened on the same real .e, then reports pixel agreement + a contact sheet.

usage: python tools/des_round.py <tag> [sample.e]
  writes shots/des_round/<tag>/{orig.png,mine.png,diff.png,cmp.png,numbers.txt}
"""
import os
import sys
import time
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIG = r"C:\Program Files (x86)\e\e.exe"
MINE = os.path.join(ROOT, "bin", "EClone.exe")
SAMPLE = r"C:\Users\Along\AppData\Local\Temp\e_re\samples\flowcmd.e"
OUTROOT = os.path.join(ROOT, "shots", "des_round")
WM_MDIACTIVATE = 0x0222


def frame_of(app):
    for t in drive.tops(app.pid):
        if drive.cls(t) in ("ENewFrame", "ECMainFrame"):
            return t
    return app.frame


def activate_design_doc(app):
    """The original opens TWO MDI docs; the designer one is titled '窗口: ...'.
    Activate it so the WTWindow is the visible page."""
    frame = frame_of(app)
    mdi = None
    for h in drive.descendants(frame):
        if drive.cls(h) == "MDIClient" and drive.rect(h)[2] > 1000:
            mdi = h
    if mdi is None:
        return None, frame
    doc = None
    for h in drive.descendants(mdi):
        t = drive.title(h)
        if t.startswith("\u7a97\u53e3") or t.startswith("_") or t.startswith("\u7a97\u53e3:"):
            doc = h
    if doc is None:
        for h in drive.descendants(mdi):
            t = drive.title(h)
            if "\u7a97\u53e3" in t and "\u7a0b\u5e8f\u96c6" not in t:
                doc = h
    if doc is not None:
        drive.u.SendMessageW(mdi, WM_MDIACTIVATE, doc, 0)
        time.sleep(2.5)
    return doc, frame


def capture(tag, exe, args, env=None, wait=14):
    out = os.path.join(OUTROOT, tag)
    os.makedirs(out, exist_ok=True)
    # Park the cursor far away: a hovering mouse puts the caption close button into its
    # "hot" state (red face), and the two windows sit at different screen positions.
    drive.u.SetCursorPos(2, 2)
    app = drive.App(exe, args=args, wait=wait, env=env)
    drive.u.SetCursorPos(2, 2)
    doc, frame = activate_design_doc(app)
    # The title-bar palette depends on whether the window is ACTIVE. Make sure the app we are
    # shooting is the foreground process, otherwise we compare active (orig) vs inactive (mine).
    root = frame
    while drive.u.GetParent(root):
        root = drive.u.GetParent(root)
    drive.u.SetForegroundWindow(root)
    time.sleep(1.5)
    drive.u.SetCursorPos(2, 2)
    time.sleep(0.5)
    form = app.find("WTWindow")
    info = []
    info.append("exe=%s" % exe)
    info.append("frame=%s %s" % (frame, drive.rect(frame) if frame else None))
    info.append("design-doc=%s %s |%s|" % (doc, drive.rect(doc) if doc else None,
                                           drive.title(doc) if doc else ""))
    info.append("WTWindow=%s %s vis=%s" % (form, drive.rect(form) if form else None,
                                           bool(drive.u.IsWindowVisible(form)) if form else None))
    if form is None:
        app.close()
        return None, info
    img = app.shot(form, os.path.join(out, "form.png"))
    # same-size 1:1 grab of the client area too
    kids = []
    for h in drive.descendants(form):
        kids.append("%-24s %s |%s|" % (drive.cls(h), drive.rect(h), drive.title(h)))
    info.append("children:")
    info += ["   " + k for k in kids]
    # window list of the whole app (for docs / panels)
    info.append("windows:")
    for h in drive.descendants(frame):
        if drive.u.IsWindowVisible(h):
            x, y, w, hh = drive.rect(h)
            info.append("   %-30s %5d,%-5d %4dx%-4d |%s|" % (drive.cls(h), x, y, w, hh,
                                                              drive.title(h)))
    app.close()
    return img, info


def compare(tag, a_path, b_path):
    from PIL import Image
    A = Image.open(a_path).convert("RGB")
    B = Image.open(b_path).convert("RGB")
    W = min(A.size[0], B.size[0])
    H = min(A.size[1], B.size[1])
    pa, pb = A.load(), B.load()
    diff = []
    pairs = Counter()
    for y in range(H):
        for x in range(W):
            ca, cb = pa[x, y], pb[x, y]
            if ca != cb:
                diff.append((x, y))
                pairs[(ca, cb)] += 1
    total = W * H
    same = total - len(diff)
    pct = 100.0 * same / total
    bbox = None
    if diff:
        xs = [p[0] for p in diff]
        ys = [p[1] for p in diff]
        bbox = (min(xs), min(ys), max(xs), max(ys))
    # column/row profile of the diff to find the hot rectangle
    lines = []
    lines.append("orig=%s  mine=%s  common=%dx%d" % (A.size, B.size, W, H))
    lines.append("same=%d/%d = %.2f%%   diff=%d   bbox=%s" % (same, total, pct, len(diff), bbox))
    lines.append("--- top colour pairs (orig -> mine) ---")
    for (ca, cb), n in pairs.most_common(12):
        lines.append("   o=#%02X%02X%02X m=#%02X%02X%02X  x%d" % (ca[0], ca[1], ca[2],
                                                                  cb[0], cb[1], cb[2], n))

    def hot(name, idx, n):
        row = [0] * n
        for p in diff:
            row[p[idx]] += 1
        runs = []
        s = None
        for i in range(n):
            if row[i] > 0 and s is None:
                s = i
            if row[i] == 0 and s is not None:
                runs.append((s, i - 1, sum(row[s:i])))
                s = None
        if s is not None:
            runs.append((s, n - 1, sum(row[s:])))
        runs.sort(key=lambda r: -r[2])
        lines.append("--- hot %s (top 8 spans, coord %s) ---" % (name, "x" if idx == 0 else "y"))
        for a, b, c in runs[:8]:
            lines.append("   %s %d..%d  %d px" % ("x" if idx == 0 else "y", a, b, c))

    hot("columns", 0, W)
    hot("rows", 1, H)
    # 3x3 block map of diff density
    lines.append("--- diff density (16x16 grid, % of block) ---")
    for gy in range(16):
        row = []
        for gx in range(16):
            x0 = gx * W // 16
            x1 = max(x0 + 1, (gx + 1) * W // 16)
            y0 = gy * H // 16
            y1 = max(y0 + 1, (gy + 1) * H // 16)
            n = 0
            for (x, y) in diff:
                if x0 <= x < x1 and y0 <= y < y1:
                    n += 1
            row.append("%3d" % (100 * n // ((x1 - x0) * (y1 - y0))))
        lines.append("   " + " ".join(row))
    # contact sheet: orig / mine / diff-heat
    sheet = Image.new("RGB", (W * 3 + 16, H), (40, 40, 40))
    sheet.paste(A.crop((0, 0, W, H)), (0, 0))
    sheet.paste(B.crop((0, 0, W, H)), (W + 8, 0))
    heat = Image.new("RGB", (W, H), (255, 255, 255))
    hp = heat.load()
    for (x, y) in diff:
        hp[x, y] = (255, 0, 255)
    sheet.paste(heat, (2 * W + 16, 0))
    lines.append("sheet=%s (left=orig mid=mine right=diff)" % os.path.join(OUTROOT, tag, "cmp.png"))
    return pct, lines, sheet, (W, H)


def main():
    tag = sys.argv[1] if len(sys.argv) > 1 else "r"
    sample = sys.argv[2] if len(sys.argv) > 2 else SAMPLE
    out = os.path.join(OUTROOT, tag)
    os.makedirs(out, exist_ok=True)
    io, ii = capture("o", ORIG, [sample], wait=14)
    # NOTE: do NOT set ECLONE_DESIGN_DEMO here -- that path (CreateNewForm demo) makes the
    # designer build its *own* empty form and then the .e units fail to create (measured).
    im, mi = capture("m", MINE, [sample, "designer"], env=None, wait=8)
    lines = []
    lines.append("===== ORIGINAL =====")
    lines += ii
    lines.append("===== MINE =====")
    lines += mi
    if io is None or im is None:
        lines.append("!! capture failed")
        print("\n".join(lines))
        return 1
    io.save(os.path.join(out, "orig.png"))
    im.save(os.path.join(out, "mine.png"))
    pct, cl, sheet, size = compare(tag, os.path.join(out, "orig.png"), os.path.join(out, "mine.png"))
    lines.append("===== COMPARE =====")
    lines += cl
    sheet.save(os.path.join(out, "cmp.png"))
    txt = "\n".join(lines)
    with open(os.path.join(out, "numbers.txt"), "w", encoding="utf-8") as f:
        f.write(txt + "\n")
    print(txt)
    print("\nROUND %s : %.2f%% SAME" % (tag, pct))
    return 0


if __name__ == "__main__":
    sys.exit(main())
