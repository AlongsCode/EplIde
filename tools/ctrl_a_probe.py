# -*- coding: utf-8 -*-
"""Compare the ORIGINAL e.exe code editor before/after Ctrl+A and report exactly which pixels
changed, so "select all covers the whole page" can be judged from data instead of by eye.

usage: python tools/ctrl_a_probe.py [--out shots/live_ctrla] [--sample <file>]
"""
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time
from PIL import Image

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetParent.argtypes = [wt.HWND]
u.GetParent.restype = wt.HWND
u.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
u.PrintWindow.argtypes = [wt.HWND, ctypes.c_void_p, ctypes.c_uint]
EnumProc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


def title(h):
    b = ctypes.create_unicode_buffer(1024)
    u.GetWindowTextW(h, b, 1024)
    return b.value


def rect(h):
    r = wt.RECT()
    u.GetWindowRect(h, ctypes.byref(r))
    return (r.left, r.top, r.right - r.left, r.bottom - r.top)


def desc(h):
    res = []

    def cb(c, l):
        res.append(c)
        return True

    u.EnumChildWindows(h, EnumProc(cb), 0)
    return res


def tops(pid):
    res = []

    def cb(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            res.append(h)
        return True

    u.EnumWindows(EnumProc(cb), 0)
    return res


def grab(h):
    x, y, w, hh = rect(h)
    if w <= 0 or hh <= 0:
        return None
    hdc = u.GetDC(0)
    mdc = ctypes.windll.gdi32.CreateCompatibleDC(hdc)
    bmp = ctypes.windll.gdi32.CreateCompatibleBitmap(hdc, w, hh)
    sel = ctypes.windll.gdi32.SelectObject(mdc, bmp)
    u.PrintWindow(h, mdc, 0)

    class BMI(ctypes.Structure):
        _fields_ = [("biSize", wt.DWORD), ("biWidth", ctypes.c_long), ("biHeight", ctypes.c_long),
                    ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                    ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                    ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wt.DWORD),
                    ("biClrImportant", wt.DWORD), ("rgb", wt.BYTE * 12)]

    bi = BMI()
    bi.biSize = 40
    bi.biWidth = w
    bi.biHeight = -hh
    bi.biPlanes = 1
    bi.biBitCount = 32
    buf = ctypes.create_string_buffer(w * hh * 4)
    ctypes.windll.gdi32.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bi), 0)
    img = Image.frombuffer("RGBA", (w, hh), buf, "raw", "BGRA", 0, 1).convert("RGB")
    ctypes.windll.gdi32.SelectObject(mdc, sel)
    ctypes.windll.gdi32.DeleteObject(bmp)
    ctypes.windll.gdi32.DeleteDC(mdc)
    u.ReleaseDC(0, hdc)
    return img


def main():
    out = r"D:\eclone\shots\live_ctrla"
    sample = r"D:\eclone\shots\e_samples\graphic.e"
    argv = sys.argv[1:]
    i = 0
    while i < len(argv):
        if argv[i] == "--out":
            out = argv[i + 1]; i += 2
        elif argv[i] == "--sample":
            sample = argv[i + 1]; i += 2
        else:
            i += 1
    os.makedirs(out, exist_ok=True)
    subprocess.run(["taskkill", "/f", "/im", "e.exe"], capture_output=True)
    time.sleep(1.2)
    p = subprocess.Popen([r"C:\Program Files (x86)\e\e.exe", sample],
                         cwd=r"C:\Program Files (x86)\e")
    time.sleep(18)
    pid = p.pid
    frame = None
    for t in tops(pid):
        if cls(t) == "ENewFrame":
            frame = t
    if frame is None:
        print("FAIL: no frame")
        return 1
    ed = None
    best = 0
    for h in desc(frame):
        c = cls(h)
        if c.startswith("Afx:400000:8:10003:900015"):
            r = rect(h)
            if r[2] * r[3] > best:
                best = r[2] * r[3]
                ed = h
    print("pid=%d frame=0x%X editor=0x%X rect=%s" % (pid, frame, ed or 0, rect(ed) if ed else None))
    if ed is None:
        p.terminate()
        return 1
    before = grab(ed)
    before.save(os.path.join(out, "before.png"))

    # real Ctrl+A with the editor focused: click inside it first, then the key
    u.SetForegroundWindow(frame)
    time.sleep(0.4)
    fx, fy, fw, fh = rect(ed)
    u.SetCursorPos(fx + 60, fy + 80)
    time.sleep(0.3)
    u.mouse_event(0x0002, 0, 0, 0, 0)
    time.sleep(0.1)
    u.mouse_event(0x0004, 0, 0, 0, 0)
    time.sleep(0.6)
    u.keybd_event(0x11, 0, 0, 0)
    time.sleep(0.15)
    u.keybd_event(0x41, 0, 0, 0)
    time.sleep(0.15)
    u.keybd_event(0x41, 0, 0x0002, 0)
    time.sleep(0.15)
    u.keybd_event(0x11, 0, 0x0002, 0)
    time.sleep(1.2)
    after = grab(ed)
    after.save(os.path.join(out, "after_ctrla.png"))

    # analysis: where did #000080 pixels appear?
    w, h = after.size
    rows = {}
    total_before = total_after = 0
    for y in range(h):
        for x in range(w):
            a = after.getpixel((x, y))
            b = before.getpixel((x, y))
            if a == (0, 0, 128):
                rows[y] = rows.get(y, 0) + 1
                total_after += 1
            if b == (0, 0, 128):
                total_before += 1
    print("#000080 before=%d after=%d" % (total_before, total_after))
    ys = sorted(rows)
    if ys:
        # group consecutive rows into bands
        bands = []
        start = prev = ys[0]
        for y in ys[1:]:
            if y - prev > 2:
                bands.append((start, prev))
                start = y
            prev = y
        bands.append((start, prev))
        print("bands (y0,y1,maxwidth):")
        for b0, b1 in bands:
            mx = max(rows[y] for y in range(b0, b1 + 1) if y in rows)
            print("  %d..%d  maxw=%d" % (b0, b1, mx))
    p.terminate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
