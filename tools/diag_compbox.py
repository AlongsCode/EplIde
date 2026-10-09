# -*- coding: utf-8 -*-
"""Diagnose the component box: is the WM_DRAWITEM handler called, and what style do the buttons
have? Uses the ECLONE_DUMP_MODEL trace channel (ec_editor.cpp EcTrace).

usage: python tools/diag_compbox.py
"""
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time

EXE = r"D:\eclone\bin\EClone.exe"
TRACE = r"D:\eclone\shots\diagcomp.txt"
u = ctypes.windll.user32
E = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
u.GetWindowLongW.argtypes = [wt.HWND, ctypes.c_int]
u.GetWindowLongW.restype = ctypes.c_long
u.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]


def cls(h):
    b = ctypes.create_unicode_buffer(128)
    u.GetClassNameW(h, b, 128)
    return b.value


def rect(h):
    r = wt.RECT()
    u.GetWindowRect(h, ctypes.byref(r))
    return (r.left, r.top, r.right - r.left, r.bottom - r.top)


def main():
    env = dict(os.environ)
    env["ECLONE_DESIGN_DEMO"] = "1"
    env["ECLONE_DUMP_MODEL"] = TRACE
    if os.path.exists(TRACE + ".trace.txt"):
        os.remove(TRACE + ".trace.txt")
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
    kids = []
    for t in wins:
        u.EnumChildWindows(t, E(lambda h, l: (kids.append(h), True)[1]), 0)
    btns = [h for h in kids if cls(h) == "Button" and rect(h)[2] == 28]
    print("28x28 buttons: %d" % len(btns))
    for h in btns[:3]:
        style = u.GetWindowLongW(h, -16)      # GWL_STYLE
        ex = u.GetWindowLongW(h, -20)         # GWL_EXSTYLE
        print("  hwnd=0x%08X style=0x%08X (owner-draw=%s visible=%s) rect=%s"
              % (h, style & 0xFFFFFFFF,
                 bool(style & 0x0B), bool(style & 0x10000000), rect(h)))
    for h in btns[:3]:
        u.RedrawWindow(h, None, None, 0x0001 | 0x0080 | 0x0100)   # INVALIDATE|UPDATENOW|ALLCHILDREN
    time.sleep(1.0)
    try:
        txt = open(TRACE + ".trace.txt", encoding="gbk", errors="replace").read()
    except Exception as e:
        txt = "(no trace: %s)" % e
    lines = [l for l in txt.splitlines() if "drawitem" in l]
    print("drawitem trace lines: %d" % len(lines))
    for l in lines[:5]:
        print("  " + l)
    p.terminate()


if __name__ == "__main__":
    sys.exit(main())
