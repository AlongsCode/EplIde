# -*- coding: utf-8 -*-
"""text_oracle.py -- get the ORIGINAL e.exe's own rendering of a page as TEXT,
then get EClone's document text for the same file, and diff them.

Why: pixel diffing tells you *that* something differs; the clipboard tells you
*what* the original drew (component names, argument lists, block nesting, table
vs code order).  e.exe supports Ctrl+A / Ctrl+C in its code page, and the
clipboard is Unicode, so no OCR and no guessing.

usage:
  python tools/text_oracle.py [--sample <path>] [--keep]

Outputs (UTF-8):
  shots/text/orig.txt      - what the original copied (GBK decoded)
  shots/text/mine.txt      - what eclone copied
  shots/text/diff.txt      - unified-ish side-by-side of the first differences
  shots/text/orig_page.png / mine_page.png - same-geometry client shots
"""
import argparse
import ctypes
import io
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive
from drive import u, wt, g

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIG = r"C:\Program Files (x86)\e\e.exe"
MINE = os.path.join(ROOT, "bin", "EClone.exe")
SAMPLE = r"C:\Users\Along\AppData\Local\Temp\e_re\samples\flowcmd.e"
OUT = os.path.join(ROOT, "shots", "text")

PAGE_W, PAGE_H = 1400, 780

CF_UNICODETEXT = 13
CF_TEXT = 1

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
k32.GlobalLock.restype = ctypes.c_void_p
k32.GlobalLock.argtypes = [ctypes.c_void_p]
k32.GlobalUnlock.argtypes = [ctypes.c_void_p]
# user32 exports return handles/pointers: without an explicit restype ctypes
# truncates them to 32 bits on 64-bit Python.
u.GetClipboardData.restype = ctypes.c_void_p
u.GetClipboardData.argtypes = [ctypes.c_uint]
u.OpenClipboard.restype = ctypes.c_int
u.OpenClipboard.argtypes = [ctypes.c_void_p]
u.CloseClipboard.restype = ctypes.c_int


def read_clipboard_text():
    """Return (unicode_text, ansi_text) currently on the clipboard."""
    out = [None, None]
    for _ in range(20):
        if u.OpenClipboard(None):
            break
        time.sleep(0.1)
    else:
        return None, None
    try:
        for idx, fmt in ((0, CF_UNICODETEXT), (1, CF_TEXT)):
            h = u.GetClipboardData(fmt)
            if not h:
                continue
            p = k32.GlobalLock(ctypes.c_void_p(h))
            if not p:
                continue
            try:
                if fmt == CF_UNICODETEXT:
                    out[idx] = ctypes.wstring_at(p)
                else:
                    raw = ctypes.string_at(p)
                    out[idx] = raw.decode('gbk', 'replace')
            finally:
                k32.GlobalUnlock(ctypes.c_void_p(h))
    finally:
        u.CloseClipboard()
    return out[0], out[1]


def clear_clipboard():
    if u.OpenClipboard(None):
        u.EmptyClipboard()
        u.CloseClipboard()


def page_window(app):
    """The MDI child that holds the code page ('程序集: ...')."""
    for h in drive.descendants(app.frame):
        t = drive.title(h)
        if t.startswith(u'程序集') and drive.u.IsWindowVisible(h):
            return h
    return None


def pitch_profile(img, x0=60, x1=700):
    """Row profile of dark text pixels -> line pitch (font/line-height check)."""
    px = img.load()
    W, H = img.size
    x1 = min(x1, W)
    bands = []
    inb = False
    start = 0
    for y in range(H):
        dark = 0
        for x in range(x0, x1):
            r, g, b = px[x, y]
            if r < 150 and g < 150 and b < 150:
                dark += 1
        if dark > 4 and not inb:
            inb, start = True, y
        elif dark <= 4 and inb:
            inb = False
            bands.append((start, y - 1))
    starts = [b[0] for b in bands]
    pitches = [starts[i + 1] - starts[i] for i in range(len(starts) - 1)]
    pitch = max(set(pitches), key=pitches.count) if pitches else 0
    return bands, pitch


def pixel_diff(pa, pb, out_png):
    """Compare two same-geometry shots, return (diff_px, total_px, worst_rows)."""
    from PIL import Image, ImageChops
    a = Image.open(pa).convert("RGB")
    b = Image.open(pb).convert("RGB")
    W = min(a.size[0], b.size[0])
    H = min(a.size[1], b.size[1])
    a = a.crop((0, 0, W, H))
    b = b.crop((0, 0, W, H))
    d = ImageChops.difference(a, b).convert("L")
    px = d.load()
    rows = {}
    tot = 0
    out = Image.new("RGB", (W, H), "black")
    op = out.load()
    for y in range(H):
        c = 0
        for x in range(W):
            if px[x, y] > 16:
                c += 1
                op[x, y] = (255, 0, 255)
        rows[y] = c
        tot += c
    if out_png:
        out.save(out_png)
    return tot, W * H, sorted(rows.items(), key=lambda kv: -kv[1])[:12]


def editor_view(page):
    """The *innermost* big visible Afx window inside the page = the code view.

    (Both apps nest the text view inside an MDI child frame; matching the
    innermost view makes the two screenshots the same logical rectangle.)
    """
    cands = []
    for h in drive.descendants(page):
        if not drive.u.IsWindowVisible(h):
            continue
        if not drive.cls(h).startswith("Afx"):
            continue
        w, hh = drive.client(h)
        if w * hh <= 0:
            continue
        depth = 0
        p = drive.u.GetParent(h)
        while p:
            depth += 1
            p = drive.u.GetParent(p)
        cands.append((w * hh, depth, h))
    if not cands:
        return None
    biggest = max(c[0] for c in cands)
    deep = [c for c in cands if c[0] >= biggest * 0.5]
    return max(deep, key=lambda c: c[1])[2]


def capture_client(hwnd, path):
    from PIL import Image
    cr = wt.RECT()
    u.GetClientRect(hwnd, ctypes.byref(cr))
    w, hh = cr.right - cr.left, cr.bottom - cr.top
    if w <= 0 or hh <= 0:
        return None
    hdc = u.GetDC(0)
    mdc = g.CreateCompatibleDC(hdc)
    bmp = g.CreateCompatibleBitmap(hdc, w, hh)
    old = g.SelectObject(mdc, bmp)
    u.PrintWindow(hwnd, mdc, 1)          # 1 = PW_CLIENTONLY

    class BMI(ctypes.Structure):
        _fields_ = [("biSize", wt.DWORD), ("biWidth", ctypes.c_long),
                    ("biHeight", ctypes.c_long), ("biPlanes", wt.WORD),
                    ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                    ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                    ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wt.DWORD),
                    ("biClrImportant", wt.DWORD), ("rgb", wt.BYTE * 12)]
    bi = BMI()
    bi.biSize, bi.biWidth, bi.biHeight = 40, w, -hh
    bi.biPlanes, bi.biBitCount = 1, 32
    buf = ctypes.create_string_buffer(w * hh * 4)
    g.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bi), 0)
    img = Image.frombuffer("RGBA", (w, hh), buf, "raw", "BGRA", 0, 1).convert("RGB")
    g.SelectObject(mdc, old)
    g.DeleteObject(bmp)
    g.DeleteDC(mdc)
    u.ReleaseDC(0, hdc)
    if path:
        img.save(path)
    return img


def wait_frame(pid, timeout=120.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        for h in drive.tops(pid):
            if drive.cls(h) in ("ENewFrame", "ECMainFrame") and drive.u.IsWindowVisible(h):
                return h
        time.sleep(1.0)
    return None


def dismiss_dialogs(pid, log):
    for _ in range(6):
        found = False
        for h in drive.tops(pid):
            if drive.cls(h) != "#32770" or not drive.u.IsWindowVisible(h):
                continue
            log.append("dialog %r rect=%s" % (drive.title(h), drive.rect(h)))
            ok = drive.u.GetDlgItem(h, 1)
            if ok:
                drive.u.SendMessageW(ok, 0x00F5, 0, 0)
            else:
                drive.u.PostMessageW(h, 0x0010, 0, 0)
            found = True
            time.sleep(1.2)
        if not found:
            return


def harvest(role, exe, sample, wait):
    log = []
    app = drive.App(exe, args=[sample], wait=3.0)
    try:
        app.frame = wait_frame(app.pid, timeout=wait) or app.frame
        time.sleep(4.0)
        dismiss_dialogs(app.pid, log)
        time.sleep(1.5)
        page = page_window(app)
        if page is None:
            log.append("no code page window found")
            return None, None, log
        # identical geometry for both apps -> comparable pixels
        u.SetWindowPos(page, None, 0, 0, PAGE_W, PAGE_H,
                       0x0004 | 0x0010 | 0x0040)     # NOZORDER|NOACTIVATE|SHOWWINDOW
        time.sleep(2.0)
        view = editor_view(page) or page
        w, hh = drive.client(view)
        app.focus_root()
        app.click(view, max(40, w // 3), max(60, hh // 2))
        time.sleep(0.6)
        # Same logical position: the wheel is the only mechanism that pins the
        # ORIGINAL to the first document line (its Ctrl+Home stops at the
        # current element, and WM_VSCROLL SB_TOP does nothing).
        app.key(0x24, ctrl=True)          # Ctrl+Home (caret)
        time.sleep(0.4)
        for _ in range(45):
            u.mouse_event(0x0800, 0, 0, 120, 0)     # MOUSEEVENTF_WHEEL up
            time.sleep(0.02)
        time.sleep(1.5)
        img_top = capture_client(view, os.path.join(OUT, "%s_top.png" % role))
        clear_clipboard()
        app.key(0x41, ctrl=True)          # Ctrl+A
        app.key(0x43, ctrl=True)          # Ctrl+C
        time.sleep(1.0)
        uni, ansi = read_clipboard_text()
        img = capture_client(view, os.path.join(OUT, "%s_page.png" % role))
        txt = uni if uni else ansi
        log.append("page=0x%08X view=0x%08X client=%s clip=%s chars=%s"
                   % (page, view, (w, hh), "unicode" if uni else "ansi",
                      len(txt) if txt else 0))
        if img is not None:
            log.append("shot %dx%d" % (img.size[0], img.size[1]))
        if img_top is not None:
            bands, pitch = pitch_profile(img_top)
            log.append("lines=%d linePitch=%d firstBands=%s"
                       % (len(bands), pitch, [b[0] for b in bands[:6]]))
        return txt, img, log
    finally:
        app.close()
        time.sleep(1.0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", default=SAMPLE)
    ap.add_argument("--only", choices=["orig", "mine"], default=None)
    ap.add_argument("--wait", type=float, default=120.0)
    args = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)

    results = {}
    for role, exe in (("orig", ORIG), ("mine", MINE)):
        if args.only and args.only != role:
            continue
        txt, img, log = harvest(role, exe, args.sample, args.wait)
        for line in log:
            print("[%s] %s" % (role, line))
        if txt is not None:
            with io.open(os.path.join(OUT, "%s.txt" % role), "w",
                         encoding="utf-8", newline="\n") as f:
                f.write(txt.replace("\r\n", "\n"))
        results[role] = txt

    if args.only:
        return
    a = (results.get("orig") or "").replace("\r\n", "\n")
    b = (results.get("mine") or "").replace("\r\n", "\n")
    la, lb = a.split("\n"), b.split("\n")
    print("orig lines=%d  mine lines=%d" % (len(la), len(lb)))
    diffs = 0
    with io.open(os.path.join(OUT, "diff.txt"), "w", encoding="utf-8",
                 newline="\n") as f:
        for i in range(max(len(la), len(lb))):
            x = la[i] if i < len(la) else "<eof>"
            y = lb[i] if i < len(lb) else "<eof>"
            if x != y:
                diffs += 1
                if diffs <= 200:
                    f.write("%4d ORIG |%s|\n     MINE |%s|\n" % (i + 1, x, y))
    print("differing lines: %d  (details -> shots/text/diff.txt)" % diffs)

    for name in ("top", "page"):
        pa = os.path.join(OUT, "orig_%s.png" % name)
        pb = os.path.join(OUT, "mine_%s.png" % name)
        if os.path.exists(pa) and os.path.exists(pb):
            tot, npix, worst = pixel_diff(pa, pb, os.path.join(OUT, "diff_%s.png" % name))
            print("pixel diff [%s]: %d / %d = %.2f%%  worst rows %s"
                  % (name, tot, npix, 100.0 * tot / npix, worst[:6]))


if __name__ == "__main__":
    main()
