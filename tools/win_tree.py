# -*- coding: utf-8 -*-
"""Dump a process's window tree (recursive), filtered by class-name substring.

usage: python tools/win_tree.py <pid> [class-substring] [max-depth]
"""
import ctypes
import ctypes.wintypes as wt
import sys

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetParent.argtypes = [wt.HWND]
u.GetParent.restype = wt.HWND
u.GetWindowLongW.argtypes = [wt.HWND, ctypes.c_int]

EnumProc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)

pid = int(sys.argv[1])
filt = sys.argv[2] if len(sys.argv) > 2 else ""
maxd = int(sys.argv[3]) if len(sys.argv) > 3 else 8
out = []


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


def title(h):
    b = ctypes.create_unicode_buffer(512)
    u.GetWindowTextW(h, b, 512)
    return b.value


def walk(h, d):
    if d > maxd:
        return
    res = []

    def cb(c, l):
        p = wt.DWORD()
        u.GetWindowThreadProcessId(c, ctypes.byref(p))
        if p.value == pid:
            res.append(c)
        return True

    u.EnumChildWindows(h, EnumProc(cb), 0)
    for c in res:
        line = "  " * d + "0x%08X %s '%s'" % (c, cls(c), title(c))
        out.append((line, cls(c), title(c), d))
        walk(c, d + 1)


tops = []


def cbt(h, l):
    p = wt.DWORD()
    u.GetWindowThreadProcessId(h, ctypes.byref(p))
    if p.value == pid:
        tops.append(h)
    return True


u.EnumWindows(EnumProc(cbt), 0)
for t in tops:
    out.append(("0x%08X %s '%s'" % (t, cls(t), title(t)), cls(t), title(t), 0))
    walk(t, 1)

for line, c, t, d in out:
    if not filt or filt.lower() in c.lower() or filt.lower() in t.lower():
        print(line)
print("total windows listed: %d" % len(out))
