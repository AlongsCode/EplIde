# -*- coding: utf-8 -*-
"""_probe_pages2.py -- enumerate the orig IDE's left-tree (工作夹) items + the visible
page control for each, to build the 1:1 page sweep (#11) on measured facts.

usage: python tools\\_probe_pages2.py
"""
import ctypes
import ctypes.wintypes as wt
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive  # noqa: E402

ORIG = r"C:\Program Files (x86)\e\e.exe"
SAMPLE = r"D:\EPL\samples\Basic Sample\Bubble Sort.e"

u = ctypes.windll.user32
TVM_GETCOUNT = 0x1100 + 5
TVM_GETNEXTITEM = 0x1100 + 10
TVGN_ROOT = 0
TVGN_NEXT = 1
TVGN_CHILD = 4
TVM_GETITEMW = 0x1100 + 62
TVIF_TEXT = 0x0001
TVIF_CHILDREN = 0x0040


class TVITEM(ctypes.Structure):
    _fields_ = [("mask", wt.UINT), ("hItem", wt.HANDLE), ("state", wt.UINT),
                ("stateMask", wt.UINT), ("pszText", wt.LPWSTR), ("cchTextMax", ctypes.c_int),
                ("iImage", ctypes.c_int), ("iSelectedImage", ctypes.c_int),
                ("cChildren", ctypes.c_int), ("lParam", wt.LPARAM)]


def send(h, msg, wp=0, lp=0):
    return u.SendMessageW(h, msg, wp, lp)


def item_text(h, hitem):
    buf = ctypes.create_unicode_buffer(512)
    it = TVITEM()
    it.mask = TVIF_TEXT | TVIF_CHILDREN
    it.hItem = hitem
    it.pszText = ctypes.cast(buf, wt.LPWSTR)
    it.cchTextMax = 512
    r = send(h, TVM_GETITEMW, 0, ctypes.byref(it))
    if not r:
        return None, 0
    return buf.value, it.cChildren


def walk(h, hitem, depth, want_child):
    while hitem:
        t, ch = item_text(h, hitem)
        if t is None:
            return
        print("%s%s%s" % ("  " * depth, t, "  [+]" if ch else ""))
        if ch and depth < 3:
            c = send(h, TVM_GETNEXTITEM, TVGN_CHILD, hitem)
            if c:
                walk(h, c, depth + 1, True)
        hitem = send(h, TVM_GETNEXTITEM, TVGN_NEXT, hitem)


def main():
    app = drive.App(ORIG, args=[SAMPLE], wait=20)
    try:
        trees = []
        for h in drive.descendants(app.frame):
            if drive.cls(h).lower() == "systreeview32" and u.IsWindowVisible(h):
                r = drive.rect(h)
                trees.append((h, r))
        print("visible SysTreeView32: %d" % len(trees))
        for h, r in trees:
            print()
            print("=== tree %s rect=%s items=%d ===" % (hex(h), r, send(h, TVM_GETCOUNT)))
            root = send(h, TVM_GETNEXTITEM, TVGN_ROOT, 0)
            walk(h, root, 0, False)
    finally:
        try:
            app.close()
        except Exception:
            pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
