# -*- coding: utf-8 -*-
"""Measure a running app's tree views: window rect, client rect, background/text colour and the
scrollbar geometry, so "the background looks inconsistent" can be attributed to a real control.

usage: python tools/probe_treegeom.py <pid>
"""
import ctypes
import ctypes.wintypes as wt
import sys

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowRect.argtypes = [wt.HWND, ctypes.c_void_p]
u.GetClientRect.argtypes = [wt.HWND, ctypes.c_void_p]
u.GetWindowLongW.argtypes = [wt.HWND, ctypes.c_int]
u.GetWindowLongW.restype = ctypes.c_long
u.SendMessageW.argtypes = [wt.HWND, wt.UINT, ctypes.c_void_p, ctypes.c_void_p]
u.SendMessageW.restype = ctypes.c_void_p
u.EnumWindows.argtypes = [ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.EnumChildWindows.argtypes = [wt.HWND, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]

pid = int(sys.argv[1])


class RECT(ctypes.Structure):
    _fields_ = [("l", ctypes.c_long), ("t", ctypes.c_long), ("r", ctypes.c_long), ("b", ctypes.c_long)]


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


trees = []


def hunt(h, d=0):
    if d > 8:
        return
    res = []

    def cb(c, l):
        p = wt.DWORD()
        u.GetWindowThreadProcessId(c, ctypes.byref(p))
        if p.value == pid:
            res.append(c)
        return True

    u.EnumChildWindows(h, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cb), 0)
    for c in res:
        if cls(c) == "SysTreeView32" and c not in trees:
            trees.append(c)
        hunt(c, d + 1)


tops = []


def cbt(h, l):
    p = wt.DWORD()
    u.GetWindowThreadProcessId(h, ctypes.byref(p))
    if p.value == pid:
        tops.append(h)
    return True


u.EnumWindows(ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cbt), 0)
for t in tops:
    hunt(t)

WS_HSCROLL, WS_VSCROLL = 0x00100000, 0x00200000
TVM_GETBKCOLOR = 0x111F
TVM_GETTEXTCOLOR = 0x1120
TVM_GETCOUNT = 0x1105
TVM_GETVISIBLECOUNT = 0x1110

print("found %d SysTreeView32" % len(trees))
for h in trees:
    wr, cr = RECT(), RECT()
    u.GetWindowRect(h, ctypes.byref(wr))
    u.GetClientRect(h, ctypes.byref(cr))
    st = u.GetWindowLongW(h, -16) & 0xFFFFFFFF
    bk = int(u.SendMessageW(h, TVM_GETBKCOLOR, 0, 0) or 0)
    tc = int(u.SendMessageW(h, TVM_GETTEXTCOLOR, 0, 0) or 0)
    cnt = int(u.SendMessageW(h, TVM_GETCOUNT, 0, 0) or 0)
    print("  hwnd=0x%08X  win=%dx%d at %d,%d  client=%dx%d  style=0x%08X(vscroll=%d hscroll=%d)"
          % (h, wr.r - wr.l, wr.b - wr.t, wr.l, wr.t, cr.r - cr.l, cr.b - cr.t, st,
             (st & WS_VSCROLL) != 0, (st & WS_HSCROLL) != 0))
    print("       items=%d  bkcolor=0x%08X(RGB %d,%d,%d)  textcolor=0x%08X  border=%d"
          % (cnt, bk, bk & 0xFF, (bk >> 8) & 0xFF, (bk >> 16) & 0xFF, tc,
             (st & 0x00800000) != 0))
