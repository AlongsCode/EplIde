# -*- coding: utf-8 -*-
"""caret_geom.py -- authoritative caret geometry probe (orig e.exe vs EClone).

For three click targets it does the same thing on both apps:
    1. click the target with a REAL mouse click,
    2. read the OS caret rectangle via GetGUIThreadInfo -- the same authoritative
       source tools/caret_check.ps1 uses (no screenshot flicker guessing),
    3. capture the client area and crop an 8x zoom around the click point,
    4. write shots/caret/<case>_{orig,mine,diff,sbs}.png and print the numbers.

The three targets are the two things the user actually clicks in a table row
(header cell / data cell) plus one code line, so "the caret in tables and in
code" is measured, not guessed.

usage: python tools/caret_geom.py
"""
import ctypes
import ctypes.wintypes as wt
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive                      # noqa: E402
import text_oracle as T           # noqa: E402
from drive import u               # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "shots", "caret")
PAGE_W, PAGE_H = 1400, 780

# (case, client x, client y) -- table rows live at the very top (see the ink
# bands of shots/cmp/01_top_*.png: 4..45 program table, 54..95 sub table,
# 100..129 variable table, code lines from ~147 down).
CASES = [
    ("tbl_head", 60, 65),     # subprogram-name table header cell
    ("tbl_data", 60, 86),     # subprogram-name table data cell
    ("code",     300, 200),   # a code line
]
CROP_W, CROP_H, ZOOM = 64, 40, 8


class GUITHREADINFO(ctypes.Structure):
    _fields_ = [("cbSize", wt.DWORD), ("flags", wt.DWORD),
                ("hwndActive", wt.HWND), ("hwndFocus", wt.HWND),
                ("hwndCapture", wt.HWND), ("hwndMenuOwner", wt.HWND),
                ("hwndMoveSize", wt.HWND), ("hwndCaret", wt.HWND),
                ("rcCaret", wt.RECT)]


class THREADENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD),
                ("th32ThreadID", wt.DWORD), ("th32OwnerProcessID", wt.DWORD),
                ("tpBasePri", ctypes.c_long), ("tpDeltaPri", ctypes.c_long),
                ("dwFlags", wt.DWORD)]


# 64-bit safe: without argtypes a Python int HWND is passed as a 32-bit C int.
u.ScreenToClient.argtypes = [wt.HWND, ctypes.POINTER(wt.POINT)]
u.ScreenToClient.restype = wt.BOOL
u.GetGUIThreadInfo.argtypes = [wt.DWORD, ctypes.POINTER(GUITHREADINFO)]
u.GetGUIThreadInfo.restype = wt.BOOL


def thread_ids(pid):
    k = ctypes.windll.kernel32
    k.CreateToolhelp32Snapshot.restype = ctypes.c_void_p
    snap = k.CreateToolhelp32Snapshot(0x00000004, 0)   # TH32CS_SNAPTHREAD
    out = []
    if not snap or snap == ctypes.c_void_p(-1).value:
        return out
    te = THREADENTRY32()
    te.dwSize = ctypes.sizeof(te)
    if k.Thread32First(snap, ctypes.byref(te)):
        while True:
            if te.th32OwnerProcessID == pid:
                out.append(te.th32ThreadID)
            if not k.Thread32Next(snap, ctypes.byref(te)):
                break
    k.CloseHandle(ctypes.c_void_p(snap))
    return out


def to_client(hwnd, x, y):
    pt = wt.POINT(int(x), int(y))
    u.ScreenToClient(hwnd, ctypes.byref(pt))
    return pt.x, pt.y


def read_caret(pid, view, tries=22, dt=0.09):
    """(x, y, w, h, owner) of the caret, expressed in *view* client coords.

    On this host GetGUIThreadInfo reports rcCaret in the caret window's CLIENT
    coordinates (verified: the rect lands on the clicked cell and matches the
    screen BitBlt of that cell), so the raw values are used as-is. Sampled over
    the blink cycle and the MOST FREQUENT rect wins -- the first sample right
    after a click can still be the previous position.
    """
    seen = {}
    raw = []
    for _ in range(tries):
        for tid in thread_ids(pid):
            g = GUITHREADINFO()
            g.cbSize = ctypes.sizeof(g)
            if not u.GetGUIThreadInfo(tid, ctypes.byref(g)):
                continue
            if not g.hwndCaret:
                continue
            raw.append((tid, int(g.hwndCaret), g.rcCaret.left, g.rcCaret.top,
                        g.rcCaret.right, g.rcCaret.bottom))
            cur = (g.rcCaret.left, g.rcCaret.top,
                   g.rcCaret.right - g.rcCaret.left,
                   g.rcCaret.bottom - g.rcCaret.top, int(g.hwndCaret))
            seen[cur] = seen.get(cur, 0) + 1
        time.sleep(dt)
    if not seen:
        return None, raw
    return max(seen.items(), key=lambda kv: kv[1])[0], raw


def pin_top(app, view):
    """Wheel to the very top (the original's Ctrl+Home stops at the element)."""
    w, hh = drive.client(view)
    app.click(view, max(200, w // 3), max(120, hh // 3))
    time.sleep(0.4)
    for _ in range(45):
        u.mouse_event(0x0800, 0, 0, 120, 0)
        time.sleep(0.02)
    time.sleep(1.2)


def count_bars(img, min_run=14):
    """Vertical dark bars inside the crop: columns with a dark run >= min_run."""
    px = img.load()
    W, H = img.size
    dark_cols = []
    for x in range(W):
        run = best = 0
        for y in range(H):
            r, g, b = px[x, y]
            if r < 140 and g < 140 and b < 140:
                run += 1
                best = max(best, run)
            else:
                run = 0
        if best >= min_run:
            dark_cols.append(x)
    bars = 0
    prev = -9
    for x in dark_cols:
        if x != prev + 1:
            bars += 1
        prev = x
    return bars


def screen_frames(x, y, w, h, n=8, dt=0.16):
    """Grab n frames of a screen region via BitBlt(GetDC(0)) -- real pixels,
    so the OS caret (which PrintWindow never paints) IS visible here."""
    from PIL import Image
    g = ctypes.windll.gdi32
    hdc = u.GetDC(0)
    mdc = g.CreateCompatibleDC(hdc)
    bmp = g.CreateCompatibleBitmap(hdc, w, h)
    old = g.SelectObject(mdc, bmp)

    class BMI(ctypes.Structure):
        _fields_ = [("biSize", wt.DWORD), ("biWidth", ctypes.c_long),
                    ("biHeight", ctypes.c_long), ("biPlanes", wt.WORD),
                    ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                    ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                    ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wt.DWORD),
                    ("biClrImportant", wt.DWORD), ("rgb", wt.BYTE * 12)]
    bi = BMI()
    bi.biSize, bi.biWidth, bi.biHeight = 40, w, -h
    bi.biPlanes, bi.biBitCount = 1, 32
    frames = []
    bufs = []
    try:
        for _ in range(n):
            g.BitBlt(mdc, 0, 0, w, h, hdc, x, y, 0x00CC0020)   # SRCCOPY
            buf = ctypes.create_string_buffer(w * h * 4)
            g.GetDIBits(mdc, bmp, 0, h, buf, ctypes.byref(bi), 0)
            bufs.append(buf)
            frames.append(Image.frombuffer("RGBA", (w, h), buf, "raw", "BGRA", 0, 1).convert("RGB"))
            time.sleep(dt)
    finally:
        g.SelectObject(mdc, old)
        g.DeleteObject(bmp)
        g.DeleteDC(mdc)
        u.ReleaseDC(0, hdc)
    return frames


def blink_map(frames, out_png=None):
    """Magenta where a pixel changes across frames (= the blinking caret).

    Returns (count, bbox) with bbox = (x0, y0, x1, y1) of the changed region.
    """
    from PIL import Image
    w, h = frames[0].size
    px = [f.load() for f in frames]
    out = Image.new("RGB", (w, h), (0, 0, 0))
    op = out.load()
    x0, y0, x1, y1, n = w, h, -1, -1, 0
    for y in range(h):
        for x in range(w):
            vals = [p[x, y] for p in px]
            rng = max(max(v) for v in vals) - min(min(v) for v in vals)
            if rng > 40:
                op[x, y] = (255, 0, 255)
                n += 1
                x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x), max(y1, y)
    if out_png:
        out.save(out_png)
    return n, (x0, y0, x1, y1) if n else None


def run(role, exe, sample, log):
    app = drive.App(exe, args=[sample], wait=3.0)
    res = {}
    try:
        app.frame = T.wait_frame(app.pid, timeout=150) or app.frame
        time.sleep(4.0)
        T.dismiss_dialogs(app.pid, log)
        time.sleep(1.5)
        page = T.page_window(app)
        if page is None:
            log.append("%s: no code page" % role)
            return res
        u.SetWindowPos(page, None, 0, 0, PAGE_W, PAGE_H, 0x0004 | 0x0010 | 0x0040)
        time.sleep(2.0)
        view = T.editor_view(page)
        if view is None:
            log.append("%s: no editor view" % role)
            return res
        w, hh = drive.client(view)
        log.append("%s: view=0x%08X client=%dx%d screen=%s"
                   % (role, view, w, hh, drive.rect(view)))
        app.focus_root()
        pin_top(app, view)
        for case, cx, cy in CASES:
            app.click(view, cx, cy)
            time.sleep(0.6)
            cr, raw = read_caret(app.pid, view)
            p = os.path.join(OUT, "%s_%s.png" % (case, role))
            T.capture_client(view, p)
            # real-screen frames around the click -> does a blinking caret exist,
            # and how many vertical bars does the painted row really show?
            org = wt.POINT(0, 0)
            u.ClientToScreen(view, ctypes.byref(org))
            sx, sy = org.x + cx, org.y + cy
            fw, fh = CROP_W, CROP_H
            frames = screen_frames(sx - fw // 2, sy - fh // 2, fw, fh)
            bm = os.path.join(OUT, "%s_%s_blink.png" % (case, role))
            npx, bbox = blink_map(frames, bm)
            res[case] = (cr, p, cx, cy)
            bars_screen = count_bars(frames[0], 14)
            log.append("%s %-9s click=(%d,%d) caret=%s blink=%dpx bbox=%s bars_on_screen=%d"
                       % (role, case, cx, cy, cr, npx, bbox, bars_screen))
            for r in raw[:3]:
                log.append("    raw tid=%d hwnd=0x%X rc=(%d,%d)-(%d,%d)" % r)
        return res
    finally:
        app.close()
        time.sleep(1.0)


def main():
    os.makedirs(OUT, exist_ok=True)
    log = []
    o = run("orig", T.ORIG, T.SAMPLE, log)
    m = run("mine", T.MINE, T.SAMPLE, log)
    for line in log:
        print(line)
    from PIL import Image
    for case, cx, cy in CASES:
        if case not in o or case not in m:
            continue
        cro, crm = o[case][1], m[case][1]
        if not (os.path.exists(cro) and os.path.exists(crm)):
            continue
        x0, y0 = max(0, cx - CROP_W // 2), max(0, cy - CROP_H // 2)
        box = (x0, y0, x0 + CROP_W, y0 + CROP_H)
        a = Image.open(cro).convert("RGB").crop(box)
        b = Image.open(crm).convert("RGB").crop(box)
        d = Image.new("RGB", a.size)
        ap, bp, dp = a.load(), b.load(), d.load()
        n = 0
        for y in range(a.size[1]):
            for x in range(a.size[0]):
                pa, pb = ap[x, y], bp[x, y]
                if abs(pa[0] - pb[0]) + abs(pa[1] - pb[1]) + abs(pa[2] - pb[2]) > 30:
                    dp[x, y] = (255, 0, 255)
                    n += 1
        sbs = Image.new("RGB", (a.size[0] * 2 + 8, a.size[1]), (0, 128, 0))
        sbs.paste(a, (0, 0))
        sbs.paste(b, (a.size[0] + 8, 0))
        a.resize((a.size[0] * ZOOM, a.size[1] * ZOOM), 0).save(
            os.path.join(OUT, "%s_orig_8x.png" % case))
        b.resize((b.size[0] * ZOOM, b.size[1] * ZOOM), 0).save(
            os.path.join(OUT, "%s_mine_8x.png" % case))
        d.resize((d.size[0] * ZOOM, d.size[1] * ZOOM), 0).save(
            os.path.join(OUT, "%s_diff.png" % case))
        sbs.resize((sbs.size[0] * ZOOM, sbs.size[1] * ZOOM), 0).save(
            os.path.join(OUT, "%s_sbs.png" % case))
        bars_o = count_bars(a, 14)
        bars_m = count_bars(b, 14)
        print("%-9s crop-diff=%4d px  bars orig=%d mine=%d  caret orig=%s mine=%s"
              % (case, n, bars_o, bars_m, o[case][0], m[case][0]))
    print("-> %s" % OUT)


if __name__ == "__main__":
    main()
