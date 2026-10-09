# -*- coding: utf-8 -*-
"""Open the window-component box (MFC control-bar toggle id 110) in a running EClone and save a
screenshot of it, so the icon grid can be checked against the original.

usage: python tools/shot_compbox.py [out.png] [exe]
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

sys.path.insert(0, r"D:\eclone\tools")

OUT = sys.argv[1] if len(sys.argv) > 1 else r"D:\eclone\shots\compbox.png"
EXE = sys.argv[2] if len(sys.argv) > 2 else r"D:\eclone\bin\EClone.exe"

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.SendMessageW.argtypes = [wt.HWND, wt.UINT, ctypes.c_void_p, ctypes.c_void_p]
u.SendMessageW.restype = ctypes.c_void_p
u.EnumWindows.argtypes = [ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.EnumChildWindows.argtypes = [wt.HWND, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]

WM_COMMAND = 0x0111
ID_VIEW_COMP = 110


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


def main():
    p = subprocess.Popen([EXE])
    time.sleep(3.5)
    pid = p.pid
    frames = []

    def cb(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid and cls(h) == "ENewFrame":
            frames.append(h)
        return True

    u.EnumWindows(ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cb), 0)
    if not frames:
        print("no ENewFrame found")
        return 1
    fr = frames[0]
    u.SendMessageW(fr, WM_COMMAND, ID_VIEW_COMP, 0)
    time.sleep(1.0)

    kids = []

    def cb2(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            kids.append((h, cls(h)))
        return True

    u.EnumChildWindows(fr, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cb2), 0)
    for h, c in kids:
        if c in ("SysListView32", "SysTreeView32"):
            r = wt.RECT()
            u.GetWindowRect(h, ctypes.byref(r))
            n = int(u.SendMessageW(h, 0x1004, 0, 0) or 0)     # LVM_GETITEMCOUNT
            print("%-16s hwnd=0x%08X rect=(%d,%d,%d,%d) items=%d"
                  % (c, h, r.left, r.top, r.right, r.bottom, n))

    # screenshot the frame region
    from PIL import ImageGrab
    r = wt.RECT()
    u.GetWindowRect(fr, ctypes.byref(r))
    img = ImageGrab.grab(bbox=(r.left, r.top, r.right, r.bottom))
    img.save(OUT)
    print("screenshot -> %s (%dx%d)" % (OUT, img.width, img.height))
    p.terminate()
    return 0


sys.exit(main())
