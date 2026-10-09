# -*- coding: utf-8 -*-
"""Verify the designer's base-window geometry: the design form (WTWindow) must sit at a FIXED
client offset (EC_DESIGN_FORM_X/Y = 10,10) inside its design view, and must not be draggable.

The original was measured from shots\\live_des_orig\\windows.txt: WTWindow at screen (275,189),
its design view at (265,179) -> offset (10,10).

usage: python tools/probe_formgeom.py [exe]
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

EXE = sys.argv[1] if len(sys.argv) > 1 else r"D:\eclone\bin\EClone.exe"

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.EnumWindows.argtypes = [ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.EnumChildWindows.argtypes = [wt.HWND, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
u.ScreenToClient.argtypes = [wt.HWND, ctypes.POINTER(wt.POINT)]
u.ClientToScreen.argtypes = [wt.HWND, ctypes.POINTER(wt.POINT)]


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


def main():
    p = subprocess.Popen([EXE, "designer"])
    time.sleep(4.0)
    pid = p.pid
    found = []

    def cb(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            found.append((h, cls(h)))
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

    forms = [h for h, c in found if c == "WTWindow"]
    views = [h for h, c in found if c == "Afx:400000:3:10003:900015:0"]
    print("WTWindow=%d  design view candidates=%d" % (len(forms), len(views)))
    if not forms:
        for h, c in found[:20]:
            print("   %-34s 0x%08X" % (c, h))
        p.terminate()
        return 1

    f = forms[0]
    r = wt.RECT()
    u.GetWindowRect(f, ctypes.byref(r))
    print("form  WTWindow  screen=(%d,%d) size=%dx%d" % (r.left, r.top, r.right - r.left, r.bottom - r.top))

    par = u.GetParent(f)
    while par:
        c = cls(par)
        if c.startswith("Afx:400000:3") or c.startswith("Afx:400000:b"):
            o = wt.POINT(0, 0)
            u.ScreenToClient(par, ctypes.byref(o))     # client origin in screen coords
            print("view  %-30s screen=(%d,%d)  ->  form offset in view client = (%d,%d)"
                  % (c, o.x, o.y, r.left - o.x, r.top - o.y))
            break
        par = u.GetParent(par)

    p.terminate()
    return 0


sys.exit(main())
