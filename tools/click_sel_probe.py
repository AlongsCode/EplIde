# -*- coding: utf-8 -*-
"""What does a single click select? Launch the original with graphic.e, click at several places
inside the code editor (data cell / middle of a cell / the empty area right of a cell / a code
line), grab the editor after each click and report how many #000080 pixels appeared and where.

This answers "does one click select the whole row / the whole cell / just the text?" from data.

usage: python tools/click_sel_probe.py [--out shots/live_click]
"""
import ctypes
import ctypes.wintypes as wt
import io
import os
import subprocess
import sys
import time

u = ctypes.windll.user32
E = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
u.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
u.PrintWindow.argtypes = [wt.HWND, ctypes.c_void_p, ctypes.c_uint]


def cls(h):
    b = ctypes.create_unicode_buffer(128)
    u.GetClassNameW(h, b, 128)
    return b.value


def rect(h):
    r = wt.RECT()
    u.GetWindowRect(h, ctypes.byref(r))
    return (r.left, r.top, r.right - r.left, r.bottom - r.top)


def grab(h):
    from PIL import Image
    x, y, w, hh = rect(h)
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


def navy_info(img):
    px = img.load()
    tot = 0
    rows = {}
    for y in range(img.height):
        n = 0
        xs = []
        for x in range(img.width):
            if px[x, y] == (0, 0, 128):
                n += 1
                xs.append(x)
        if n:
            rows[y] = (n, min(xs), max(xs))
            tot += n
    return tot, rows


def main():
    out = r"D:\eclone\shots\live_click"
    os.makedirs(out, exist_ok=True)
    subprocess.run(["taskkill", "/f", "/im", "e.exe"], capture_output=True)
    time.sleep(1.2)
    p = subprocess.Popen([r"C:\Program Files (x86)\e\e.exe",
                          r"D:\eclone\shots\e_samples\graphic.e"],
                         cwd=r"C:\Program Files (x86)\e")
    time.sleep(18)
    if p.poll() is not None:
        print("FAIL: original exited early")
        return 1
    pid = p.pid
    frame = ed = None
    wins = []

    def cb(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            wins.append(h)
        return True

    u.EnumWindows(E(cb), 0)
    for t in wins:
        if cls(t) == "ENewFrame":
            frame = t
        u.EnumChildWindows(t, E(lambda h, l: (wins.append(h), True)[1]), 0)
    best = 0
    for h in wins:
        if cls(h).startswith("Afx:400000:8:10003:900015"):
            r = rect(h)
            if r[2] * r[3] > best:
                best = r[2] * r[3]
                ed = h
    print("pid=%d frame=%s editor=%s rect=%s" % (pid, frame, ed, rect(ed) if ed else None))
    if not ed:
        return 1
    ex, ey, ew, eh = rect(ed)
    u.SetForegroundWindow(frame)
    time.sleep(0.6)

    # client coordinates inside the editor (the editor's client == its window here)
    probes = [
        ("row_asm_name", 60, 12),      # assembly table, name cell (row 0)
        ("row_asm_mid", 200, 12),      # assembly table, middle column (row 0)
        ("row_asm_data", 70, 33),      # assembly table data row
        ("row_sub_name", 60, 65),      # subroutine table header
        ("row_sub_data", 70, 85),      # subroutine table data row
        ("code_line", 90, 106),        # the code line
    ]
    io.open(os.path.join(out, "report.txt"), "w", encoding="utf-8").write("")
    for tag, cx, cy in probes:
        u.SetCursorPos(ex + cx, ey + cy)
        time.sleep(0.3)
        u.mouse_event(2, 0, 0, 0, 0)
        time.sleep(0.1)
        u.mouse_event(4, 0, 0, 0, 0)
        time.sleep(0.6)
        img = grab(ed)
        img.save(os.path.join(out, tag + ".png"))
        tot, rows = navy_info(img)
        ys = sorted(rows)
        desc = "none"
        if ys:
            desc = "y=%d..%d" % (ys[0], ys[-1])
            xs = [rows[y][1] for y in ys]
            xe = [rows[y][2] for y in ys]
            desc += " x=%d..%d" % (min(xs), max(xe))
        line = "%-14s click(%3d,%3d) navy=%5d %s" % (tag, cx, cy, tot, desc)
        print(line)
        with io.open(os.path.join(out, "report.txt"), "a", encoding="utf-8") as f:
            f.write(line + "\n")
    p.terminate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
