# -*- coding: utf-8 -*-
"""Dump the window tree of the ORIGINAL IDE while it shows the WINDOW DESIGNER, so the layout of
the designer page (canvas / base window / component box) can be compared with EClone.

usage: python tools/orig_win_dump.py [pid] [--shot out.png]
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

E = r"C:\Program Files (x86)\e\e.exe"
SAMPLE = r"D:\eclone\shots\e_samples\graphic.e"

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.EnumWindows.argtypes = [ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.EnumChildWindows.argtypes = [wt.HWND, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


def text(h):
    b = ctypes.create_unicode_buffer(512)
    u.GetWindowTextW(h, b, 512)
    return b.value


def main():
    pid = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else None
    p = None
    if pid is None:
        p = subprocess.Popen([E, SAMPLE])
        pid = p.pid
        print("started e.exe pid=%d" % pid)
        time.sleep(9)

    rows = []

    def walk(h, depth):
        if depth > 7:
            return
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value != pid:
            return
        r = wt.RECT()
        u.GetWindowRect(h, ctypes.byref(r))
        rows.append((depth, cls(h), (r.left, r.top, r.right - r.left, r.bottom - r.top),
                     int(u.GetWindowLongW(h, -16)) & 0xFFFFFFFF, text(h)[:40]))

        def cb(c, l):
            walk(c, depth + 1)
            return True

        u.EnumChildWindows(h, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cb), 0)

    tops = []

    def cbt(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            tops.append(h)
        return True

    u.EnumWindows(ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cbt), 0)
    for t in tops:
        if (u.GetWindowLongW(t, -16) & 0x40000000) == 0:      # skip child top-levels
            walk(t, 0)

    for depth, c, geo, style, t in rows:
        print("%s%-30s %5d,%-5d %4dx%-4d style=0x%08X %s"
              % ("  " * depth, c, geo[0], geo[1], geo[2], geo[3], style, t))

    if "--shot" in sys.argv:
        from PIL import ImageGrab
        out = sys.argv[sys.argv.index("--shot") + 1]
        for t in tops:
            if (u.GetWindowLongW(t, -16) & 0x40000000):
                continue
            r = wt.RECT()
            u.GetWindowRect(t, ctypes.byref(r))
            if r.right - r.left > 800:
                u.SetForegroundWindow(t)
                time.sleep(0.8)
                ImageGrab.grab(bbox=(r.left, r.top, r.right, r.bottom)).save(out)
                print("shot -> %s" % out)
                break

    if p is not None and "--keep" not in sys.argv:
        p.terminate()


main()
