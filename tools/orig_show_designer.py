# -*- coding: utf-8 -*-
"""Switch the ORIGINAL IDE to the WINDOW DESIGNER view and capture it.

The IDE is an MDI app: the code editor and the window designer are two MDI children, switched by
the document tab strip (CCustomTabCtrl at the bottom).  This finds the tab whose text contains
"启动窗口" and clicks it with a real mouse click (the same trick tools\\syscfg_smoke.ps1 uses for
TCM tabs).

usage: python tools/orig_show_designer.py [outdir]
"""
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time

E = r"C:\Program Files (x86)\e\e.exe"
SAMPLE = r"D:\eclone\shots\e_samples\graphic.e"
OUT = sys.argv[1] if len(sys.argv) > 1 else r"D:\eclone\shots"

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.EnumWindows.argtypes = [ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.EnumChildWindows.argtypes = [wt.HWND, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
u.SendMessageW.argtypes = [wt.HWND, wt.UINT, ctypes.c_void_p, ctypes.c_void_p]
u.SendMessageW.restype = ctypes.c_void_p
u.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
u.ClientToScreen.argtypes = [wt.HWND, ctypes.POINTER(wt.POINT)]

TCM_GETITEMCOUNT = 0x1304
TCM_GETITEMRECT = 0x130A
TCM_GETCURSEL = 0x130B
TCM_SETCURSEL = 0x130C
MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP = 0x0002, 0x0004


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


def text(h):
    b = ctypes.create_unicode_buffer(512)
    u.GetWindowTextW(h, b, 512)
    return b.value


def main():
    p = subprocess.Popen([E, SAMPLE])
    pid = p.pid
    print("e.exe pid=%d" % pid)
    time.sleep(9)

    kids = []

    def cb(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            kids.append(h)
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

    # the document tab strip: CCustomTabCtrl, tall 22, near the bottom of the MDI client
    tabs = [h for h in kids if cls(h) == "CCustomTabCtrl"]
    print("CCustomTabCtrl: %d" % len(tabs))
    for h in tabs:
        n = int(u.SendMessageW(h, TCM_GETITEMCOUNT, 0, 0) or 0)
        sel = int(u.SendMessageW(h, TCM_GETCURSEL, 0, 0) or 0)
        r = wt.RECT()
        u.GetWindowRect(h, ctypes.byref(r))
        print("  tab hwnd=0x%08X items=%d sel=%d rect=(%d,%d,%d,%d)"
              % (h, n, sel, r.left, r.top, r.right, r.bottom))

    if not tabs:
        print("no tab strip")
        p.terminate()
        return 1

    tab = tabs[0]
    n = int(u.SendMessageW(tab, TCM_GETITEMCOUNT, 0, 0) or 0)

    class RECT(ctypes.Structure):
        _fields_ = [("l", ctypes.c_long), ("t", ctypes.c_long), ("r", ctypes.c_long), ("b", ctypes.c_long)]

    # try each tab index: click it and see whether a WTWindow becomes visible
    for idx in range(n):
        rb = RECT()
        u.SendMessageW(tab, TCM_GETITEMRECT, idx, ctypes.byref(rb))
        o = wt.POINT(0, 0)
        u.ClientToScreen(tab, ctypes.byref(o))
        cx = o.x + (rb.l + rb.r) // 2
        cy = o.y + (rb.t + rb.b) // 2
        u.SetCursorPos(cx, cy)
        time.sleep(0.2)
        u.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
        u.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
        time.sleep(1.2)

        # is a designer form present now?
        vis = []

        def cb2(h, l):
            if cls(h) == "WTWindow":
                r2 = wt.RECT()
                u.GetWindowRect(h, ctypes.byref(r2))
                vis.append((h, r2.left, r2.top, r2.right - r2.left, r2.bottom - r2.top))
            return True

        for t in tops:
            u.EnumChildWindows(t, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cb2), 0)
        sel = int(u.SendMessageW(tab, TCM_GETCURSEL, 0, 0) or 0)
        print("clicked tab %d -> sel=%d  WTWindow=%s" % (idx, sel, vis[:1]))
        if vis:
            break

    from PIL import ImageGrab
    os.makedirs(OUT, exist_ok=True)
    for t in tops:
        r = wt.RECT()
        u.GetWindowRect(t, ctypes.byref(r))
        if r.right - r.left > 1200:
            u.SetForegroundWindow(t)
            time.sleep(0.8)
            fn = os.path.join(OUT, "orig_designer_full.png")
            ImageGrab.grab(bbox=(r.left, r.top, r.right, r.bottom)).save(fn)
            print("shot -> %s (%dx%d)" % (fn, r.right - r.left, r.bottom - r.top))
            break

    p.terminate()
    return 0


sys.exit(main())
