# -*- coding: utf-8 -*-
"""Screenshot the EClone designer page (starts with the `designer` cmdline switch).

usage: python tools/shot_designer.py [out.png] [exe]
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

OUT = sys.argv[1] if len(sys.argv) > 1 else r"D:\eclone\shots\designer_now.png"
EXE = sys.argv[2] if len(sys.argv) > 2 else r"D:\eclone\bin\EClone.exe"

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.EnumWindows.argtypes = [ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


p = subprocess.Popen([EXE, "designer"])
time.sleep(4.0)
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
    print("no ENewFrame")
    sys.exit(1)
r = wt.RECT()
u.GetWindowRect(frames[0], ctypes.byref(r))
from PIL import ImageGrab
img = ImageGrab.grab(bbox=(r.left, r.top, r.right, r.bottom))
img.save(OUT)
print("saved %s (%dx%d)" % (OUT, img.width, img.height))
p.terminate()
