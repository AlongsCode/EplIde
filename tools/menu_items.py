# -*- coding: utf-8 -*-
"""List the items of a popup menu (#32768) that is currently on screen in another process.

usage: python tools/menu_items.py <pid>          -> dump every visible popup menu's items
"""
import ctypes
import ctypes.wintypes as wt
import sys

sys.path.insert(0, r"D:\eclone\tools")
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


def main():
    pid = int(sys.argv[1])
    for h in drive.tops(pid):
        if drive.cls(h) != "#32768" or not u.IsWindowVisible(h):
            continue
        hm = u.SendMessageW(h, MN_GETHMENU, 0, 0)
        n = u.GetMenuItemCount(hm)
        print("popup 0x%08X rect=%s items=%d" % (h, drive.rect(h), n))
        for i in range(n):
            buf = ctypes.create_unicode_buffer(256)
            u.GetMenuStringW(hm, i, buf, 256, MF_BYPOSITION)
            flags = u.GetMenuState(hm, i, MF_BYPOSITION)
            kind = ""
            if flags & MF_SEPARATOR:
                kind = " <SEP>"
            elif flags & MF_GRAYED:
                kind = " <GRAY>"
            cmd = u.GetMenuItemID(hm, i)
            print("   [%2d] id=%-6d flags=0x%04X%s |%s|" % (i, cmd, flags & 0xFFFF, kind, buf.value))
    return 0


if __name__ == "__main__":
    sys.exit(main())
