# -*- coding: utf-8 -*-
"""Launch my EClone in designer mode, find the component box (#32770 inside the 窗口组件箱 bar),
screenshot the whole frame AND that dialog 1:1, and report each 28x28 button's ink pixel count.

usage: python tools/shot_compbox_mine.py [--out shots] [--tag compbox_mine]
"""
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time

EXE = r"D:\eclone\bin\EClone.exe"
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


def title(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetWindowTextW(h, b, 256)
    return b.value


def rect(h):
    r = wt.RECT()
    u.GetWindowRect(h, ctypes.byref(r))
    return (r.left, r.top, r.right - r.left, r.bottom - r.top)


def grab(h):
    from PIL import Image
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
    out = r"D:\eclone\shots"
    tag = "compbox_mine"
    argv = sys.argv[1:]
    i = 0
    while i < len(argv):
        if argv[i] == "--out":
            out = argv[i + 1]; i += 2
        elif argv[i] == "--tag":
            tag = argv[i + 1]; i += 2
        else:
            i += 1
    env = dict(os.environ)
    env["ECLONE_DESIGN_DEMO"] = "1"
    subprocess.run(["taskkill", "/f", "/im", "EClone.exe"], capture_output=True)
    time.sleep(0.6)
    p = subprocess.Popen([EXE, "designer"], env=env, cwd=os.path.dirname(EXE))
    time.sleep(6)
    wins = []

    def cb(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == p.pid:
            wins.append(h)
        return True

    u.EnumWindows(E(cb), 0)
    frame = dlg = None
    kids = []
    for t in wins:
        if cls(t) == "ENewFrame":
            frame = t
        u.EnumChildWindows(t, E(lambda h, l: (kids.append(h), True)[1]), 0)
    for h in kids:
        if cls(h) == "#32770" and u.GetParent(h) and "组件" in title(u.GetParent(h)):
            dlg = h
    print("frame=%s dlg=%s rect=%s" % (frame, dlg, rect(dlg) if dlg else None))
    if frame:
        img = grab(frame)
        if img:
            img.save(os.path.join(out, tag + "_frame.png"))
            print("frame shot -> %s_frame.png" % tag)
    if dlg:
        img = grab(dlg)
        if img:
            img.save(os.path.join(out, tag + ".png"))
            print("dlg shot -> %s.png %s" % (tag, img.size))
            from collections import Counter
            ink_total = 0
            for row in range(10):
                for col in range(4):
                    x0, y0 = 5 + col * 29, 34 + row * 29
                    if x0 + 28 > img.width or y0 + 28 > img.height:
                        continue
                    cc = Counter(img.crop((x0, y0, x0 + 28, y0 + 28)).getdata())
                    ink = sum(n for c, n in cc.items() if sum(c) < 600)
                    ink_total += ink
            print("total ink pixels in the 4x10 grid = %d" % ink_total)
    p.terminate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
