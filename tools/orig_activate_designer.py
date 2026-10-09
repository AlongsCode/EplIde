# -*- coding: utf-8 -*-
"""Activate the ORIGINAL IDE's window-DESIGNER MDI child (WM_MDIACTIVATE) and screenshot it.

The IDE keeps the designer as its own MDI child (AfxMDIFrame42s) alongside the code editor, so it
can be brought to the front deterministically instead of hunting for a tab hit-test.

usage: python tools/orig_activate_designer.py
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import time

E = r"C:\Program Files (x86)\e\e.exe"
SAMPLE = r"D:\eclone\shots\e_samples\graphic.e"

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.EnumWindows.argtypes = [ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.EnumChildWindows.argtypes = [wt.HWND, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
u.SendMessageW.argtypes = [wt.HWND, wt.UINT, ctypes.c_void_p, ctypes.c_void_p]
u.SendMessageW.restype = ctypes.c_void_p
u.SetForegroundWindow.argtypes = [wt.HWND]

WM_MDIGETACTIVE = 0x0229
WM_MDIACTIVATE = 0x0222


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
    time.sleep(9)

    allw = []

    def cb(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            allw.append(h)
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

    mdi = None
    frame = None
    for h in allw:
        c = cls(h)
        if c == "MDIClient" and mdi is None:
            mdi = h
        if c.startswith("AfxMDIFrame") and frame is None:
            frame = h
    print("MDIClient=0x%08X  first AfxMDIFrame=0x%08X" % (mdi or 0, frame or 0))

    # list MDI children (they are children of MDIClient)
    mdikids = []
    if mdi:
        def cb3(h, l):
            mdikids.append(h)
            return True
        u.EnumChildWindows(mdi, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cb3), 0)
    print("MDI children: %d" % len(mdikids))
    for h in mdikids:
        r = wt.RECT()
        u.GetWindowRect(h, ctypes.byref(r))
        # does it contain a WTWindow?
        has = []

        def cb4(c, l):
            if cls(c) == "WTWindow":
                has.append(c)
            return True

        u.EnumChildWindows(h, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cb4), 0)
        print("  %-30s 0x%08X (%d,%d %dx%d) vis=%d WTWindow=%d"
              % (cls(h), h, r.left, r.top, r.right - r.left, r.bottom - r.top,
                 u.IsWindowVisible(h), len(has)))

    # activate the MDI child that owns a WTWindow
    for h in mdikids:
        has = []

        def cb5(c, l):
            if cls(c) == "WTWindow":
                has.append(c)
            return True

        u.EnumChildWindows(h, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cb5), 0)
        if has and mdi:
            u.SendMessageW(mdi, WM_MDIACTIVATE, h, 0)
            print("activated 0x%08X (designer)" % h)
            break
    time.sleep(1.2)

    for t in tops:
        r = wt.RECT()
        u.GetWindowRect(t, ctypes.byref(r))
        if r.right - r.left > 1200 and not (u.GetWindowLongW(t, -16) & 0x40000000):
            u.SetForegroundWindow(t)
            time.sleep(0.8)
            from PIL import ImageGrab
            ImageGrab.grab(bbox=(r.left, r.top, r.right, r.bottom)).save(
                r"D:\eclone\shots\orig_designer_activated.png")
            print("shot -> shots\\orig_designer_activated.png")
            break

    p.terminate()


main()
