# -*- coding: utf-8 -*-
"""Report which window of a process owns the (visible) menu bar, and how many items it has.

The original e.exe draws its menu bar on the MDI frame; GetMenu(frame) can return 0 while the
bar is still visible, so every window in the process has to be checked.

usage: python tools/menu_owner.py [pid]
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
u.GetMenu.argtypes = [wt.HWND]
u.GetMenu.restype = wt.HMENU
u.GetMenuItemCount.argtypes = [wt.HMENU]
u.GetMenuItemCount.restype = ctypes.c_int
E = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)


def cls(h):
    b = ctypes.create_unicode_buffer(128)
    u.GetClassNameW(h, b, 128)
    return b.value


def pid_of(exename="e.exe"):
    out = subprocess.run(["tasklist", "/fi", "imagename eq " + exename, "/fo", "csv", "/nh"],
                         capture_output=True, text=True).stdout
    for line in out.splitlines():
        parts = line.split(",")
        if len(parts) > 1 and parts[1].strip('"').isdigit():
            return int(parts[1].strip('"'))
    return None


def main():
    pid = int(sys.argv[1]) if len(sys.argv) > 1 else pid_of()
    if not pid:
        print("no e.exe running")
        return 1
    res = []

    def cb(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            res.append(h)
        return True

    u.EnumWindows(E(cb), 0)

    def desc(h):
        out = []

        def cb(c, l):
            out.append(c)
            return True

        u.EnumChildWindows(h, E(cb), 0)
        return out

    for h in res:
        for w in [h] + desc(h):
            m = u.GetMenu(w)
            if m:
                n = u.GetMenuItemCount(m)
                texts = []
                for i in range(n):
                    buf = ctypes.create_unicode_buffer(128)
                    u.GetMenuStringW(m, i, buf, 128, 0x400)
                    texts.append(buf.value)
                print("0x%08X %-24s menu=0x%X items=%d %s" % (w, cls(w), m, n, texts))
    return 0


if __name__ == "__main__":
    sys.exit(main())
