# -*- coding: utf-8 -*-
"""Dump the ORIGINAL code editor's right-click menu (all items, with ids and flags) to a UTF-8
file and to the screen, so our menu can be rebuilt 1:1.

usage: python tools/dump_ctxmenu.py
"""
import ctypes
import ctypes.wintypes as wt
import io
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

u = ctypes.windll.user32
u.GetMenuStringW.argtypes = [wt.HMENU, ctypes.c_uint, wt.LPWSTR, ctypes.c_int, ctypes.c_uint]
u.GetMenuStringW.restype = ctypes.c_int
u.GetMenuItemCount.argtypes = [wt.HMENU]
u.GetMenuItemCount.restype = ctypes.c_int
u.SendMessageW.argtypes = [wt.HWND, wt.UINT, ctypes.c_void_p, ctypes.c_void_p]
u.SendMessageW.restype = wt.HMENU
MN_GETHMENU = 0x01E1
MF_BYPOSITION = 0x400
MF_GRAYED = 0x0001
MF_SEPARATOR = 0x0800
MF_POPUP = 0x0010

ORIG = r"C:\Program Files (x86)\e\e.exe"
SAMPLE = r"D:\eclone\shots\e_samples\graphic.e"
OUT = r"D:\eclone\shots\ctxmenu_orig.txt"


def dump_menu(hm, out, depth=0):
    n = u.GetMenuItemCount(hm)
    for i in range(n):
        buf = ctypes.create_unicode_buffer(256)
        u.GetMenuStringW(hm, i, buf, 256, MF_BYPOSITION)
        flags = u.GetMenuState(hm, i, MF_BYPOSITION)
        cmd = u.GetMenuItemID(hm, i)
        kind = "<SEP>" if flags & MF_SEPARATOR else ("<SUB>" if flags & MF_POPUP else "")
        if flags & MF_GRAYED and kind != "<SEP>":
            kind += "GRAY"
        out.append("%s[%2d] id=%-6d flags=0x%04X %-9s |%s|"
                   % ("  " * depth, i, cmd, flags & 0xFFFF, kind, buf.value))
        if flags & MF_POPUP:
            sub = u.GetSubMenu(hm, i)
            dump_menu(sub, out, depth + 1)


def main():
    app = drive.App(ORIG, args=[SAMPLE], wait=18)
    if not app.frame:
        print("FAIL start")
        return 1
    ed = None
    best = 0
    for h in drive.descendants(app.frame):
        if ":8:" in drive.cls(h):
            r = drive.rect(h)
            if r[2] * r[3] > best and r[2] > 600:
                best = r[2] * r[3]
                ed = h
    print("editor=%s %s" % (ed, drive.rect(ed)))
    lines = []
    for tag, cx, cy in (("code", 90, 106), ("table-cell", 70, 33), ("data-name", 70, 12)):
        app.click(ed, cx, cy, right=True)
        time.sleep(0.9)
        for h in drive.tops(app.pid):
            if drive.cls(h) == "#32768" and u.IsWindowVisible(h):
                lines.append("### right-click at (%d,%d) in the editor -> menu rect %s"
                             % (cx, cy, drive.rect(h)))
                dump_menu(u.SendMessageW(h, MN_GETHMENU, 0, 0), lines)
        # dismiss
        app.key(0x1B)
        time.sleep(0.4)
    io.open(OUT, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print("-> %s (%d lines)" % (OUT, len(lines)))
    app.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
