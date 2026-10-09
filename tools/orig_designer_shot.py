# -*- coding: utf-8 -*-
"""Launch the ORIGINAL IDE (C:\\Program Files (x86)\\e\\e.exe) with a sample .e, switch to the
window-DESIGNER view, and save a few screenshots for comparison.

Read-only probing of the original is fine; we never write to its files or registry.

usage: python tools/orig_designer_shot.py [sample.e] [outdir]
"""
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time

E = r"C:\Program Files (x86)\e\e.exe"
SAMPLE = sys.argv[1] if len(sys.argv) > 1 else r"D:\eclone\shots\e_samples\graphic.e"
OUT = sys.argv[2] if len(sys.argv) > 2 else r"D:\eclone\shots\orig_designer"

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.EnumWindows.argtypes = [ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.EnumChildWindows.argtypes = [wt.HWND, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
u.SetForegroundWindow.argtypes = [wt.HWND]
u.PostMessageW.argtypes = [wt.HWND, wt.UINT, ctypes.c_void_p, ctypes.c_void_p]


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


def text(h):
    b = ctypes.create_unicode_buffer(512)
    u.GetWindowTextW(h, b, 512)
    return b.value


def main():
    os.makedirs(OUT, exist_ok=True)
    p = subprocess.Popen([E, SAMPLE])
    print("started e.exe pid=%d" % p.pid)
    time.sleep(9)

    pid = p.pid
    kids = []

    def cb(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            kids.append((h, cls(h), text(h)))
        return True

    tops = []

    def cbt(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            tops.append(h)
        return True

    u.EnumWindows(ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cbt), 0)
    for t in tops:
        u.EnumChildWindows(t, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cb), 0)

    print("top windows: %d, children: %d" % (len(tops), len(kids)))
    # find the tab control (CECTabStrip-alike) with the 启动窗口 tab and click it
    tabs = [h for h, c, t in kids if c == "CCustomTabCtrl"]
    print("CCustomTabCtrl: %d" % len(tabs))

    # the frame
    frames = [h for h in tops if "ENewFrame" in cls(h) or "Afx" in cls(h)]
    print("frame candidates: %s" % [cls(h) for h in frames[:4]])

    for h, c, t in kids:
        if c == "WTWindow":
            r = wt.RECT()
            u.GetWindowRect(h, ctypes.byref(r))
            print("WTWindow at (%d,%d) %dx%d" % (r.left, r.top, r.right - r.left, r.bottom - r.top))
            par = u.GetParent(h)
            print("   parent = %s" % cls(par))
            gp = u.GetParent(par)
            print("   grandparent = %s" % cls(gp))

    from PIL import ImageGrab
    for i, t in enumerate(tops):
        r = wt.RECT()
        u.GetWindowRect(t, ctypes.byref(r))
        if r.right - r.left < 400:
            continue
        u.SetForegroundWindow(t)
        time.sleep(0.6)
        img = ImageGrab.grab(bbox=(r.left, r.top, r.right, r.bottom))
        fn = os.path.join(OUT, "top%d.png" % i)
        img.save(fn)
        print("saved %s (%dx%d) title=%s" % (fn, img.width, img.height, text(t)))

    print("\n!! now switch the original to the WINDOW DESIGNER view by clicking the 启动窗口 tab")
    print("!! (window classes above); re-run with --keep to leave it open")
    if "--keep" not in sys.argv:
        time.sleep(1)
        p.terminate()


main()
