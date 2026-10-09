# -*- coding: utf-8 -*-
"""Verify the component box follows the DESIGNER view: visible on the designer page, hidden on the
code (program-assembly) page -- matching the original (shots\\orig_designer_activated.png vs
shots\\orig_des_full.png).

usage: python tools/probe_compboxview.py [exe]
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
u.SendMessageW.argtypes = [wt.HWND, wt.UINT, ctypes.c_void_p, ctypes.c_void_p]
u.SendMessageW.restype = ctypes.c_void_p

WM_COMMAND = 0x0111
ID_COMPBOX = 110
EC_WM_TABSEL = 0x400 + 110      # WM_USER+110


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


def find(pid):
    res = {"frame": None, "child": None, "comp": None}

    def cbt(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value != pid:
            return True
        c = cls(h)
        if c == "ENewFrame":
            res["frame"] = h

        def cb(k, l2):
            ck = cls(k)
            if ck == "ECDesignOverlay" or ck.startswith("Afx:400000:b:10003:6"):
                return True
            if ck == "AfxMDIFrame42s":
                res["child"] = k
            # the component box list: a SysListView32 with ~31 items
            if ck == "SysListView32":
                n = int(u.SendMessageW(k, 0x1004, 0, 0) or 0)
                if n >= 25 and n < 60:
                    res["comp"] = k
            return True

        u.EnumChildWindows(h, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cb), 0)
        return True

    u.EnumWindows(ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cbt), 0)
    return res


def state(tag, pid):
    f = find(pid)
    vis = None
    if f["comp"]:
        vis = bool(u.IsWindowVisible(f["comp"]))
    print("%-22s compbox hwnd=%s visible=%s" % (tag, f["comp"], vis))
    return f, vis


def main():
    p = subprocess.Popen([EXE, "designer"])
    time.sleep(4.0)
    f, vis_des = state("designer page", p.pid)

    # switch to the program-assembly page via the child frame's tab message
    if f["child"]:
        u.SendMessageW(f["child"], EC_WM_TABSEL, 1, 0)
        time.sleep(1.0)
    _, vis_code = state("code page", p.pid)

    # back to the designer (the tab message index 0 = the form page)
    if f["child"]:
        u.SendMessageW(f["child"], EC_WM_TABSEL, 0, 0)
        time.sleep(1.0)
    _, vis_back = state("designer page again", p.pid)

    fails = 0
    if not vis_des:
        print("[FAIL] component box should be visible on the designer page")
        fails += 1
    if vis_code:
        print("[FAIL] component box should be hidden on the code page")
        fails += 1
    if not vis_back:
        print("[FAIL] component box should come back on the designer page")
        fails += 1
    print("probe_compboxview: %s" % ("all checks passed" if fails == 0 else "%d FAILED" % fails))
    p.terminate()
    return 0 if fails == 0 else 1


sys.exit(main())
