# -*- coding: utf-8 -*-
"""Verify the design form is NOT draggable but IS resizable, and that its components ARE draggable.

Original behaviour (user 2026-10 + shots\\live_des_orig\\windows.txt):
  * the base window never moves; only its size changes (from the surrounding handles)
  * components move and resize from their handles

usage: python tools/probe_dragform.py [exe]
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
u.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
u.ScreenToClient.argtypes = [wt.HWND, ctypes.POINTER(wt.POINT)]

MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP = 0x0002, 0x0004


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


def rect(h):
    r = wt.RECT()
    u.GetWindowRect(h, ctypes.byref(r))
    return r


def drag(x0, y0, x1, y1, steps=12):
    u.SetCursorPos(x0, y0)
    time.sleep(0.15)
    u.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
    time.sleep(0.1)
    for i in range(1, steps + 1):
        u.SetCursorPos(x0 + (x1 - x0) * i // steps, y0 + (y1 - y0) * i // steps)
        time.sleep(0.03)
    time.sleep(0.1)
    u.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
    time.sleep(0.3)


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
    if not forms:
        print("no WTWindow")
        p.terminate()
        return 1
    f = forms[0]
    r0 = rect(f)
    print("form before: (%d,%d) %dx%d" % (r0.left, r0.top, r0.right - r0.left, r0.bottom - r0.top))

    # 1) drag on a spot INSIDE the form but away from the middle (no unit there in a fresh session)
    cx = r0.left + (r0.right - r0.left) // 2
    cy = r0.top + (r0.bottom - r0.top) - 40
    drag(cx, cy, cx + 90, cy + 60)
    r1 = rect(f)
    print("form after drag inside it: (%d,%d) %dx%d   -> moved=%s"
          % (r1.left, r1.top, r1.right - r1.left, r1.bottom - r1.top,
             (r1.left != r0.left or r1.top != r0.top)))

    # 2) drag the form's bottom-right handle: the size should change, the top-left should not
    bx = r1.right
    by = r1.bottom
    drag(bx, by, bx + 60, by + 40)
    r2 = rect(f)
    print("form after handle drag:    (%d,%d) %dx%d   -> resized=%s, topLeftMoved=%s"
          % (r2.left, r2.top, r2.right - r2.left, r2.bottom - r2.top,
             (r2.right - r2.left != r1.right - r1.left) or (r2.bottom - r2.top != r1.bottom - r1.top),
             (r2.left != r1.left or r2.top != r1.top)))

    p.terminate()
    return 0


sys.exit(main())
