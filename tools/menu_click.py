# -*- coding: utf-8 -*-
"""Open a top-level menu of the ORIGINAL e.exe by measuring where its items actually are.

GetMenuBarInfo(OBJID_MENU) gives only the whole bar, so the item x-offsets are computed the way
Windows does: accumulate GetTextExtentPoint32 widths of the item texts (with the '&' removed)
plus the standard padding, honouring MF_POPUP separators.

usage: python tools/menu_click.py "窗口" [--item "窗口组件箱"] [--launch] [--designer]
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

u = ctypes.windll.user32
g = ctypes.windll.gdi32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetParent.argtypes = [wt.HWND]
u.GetParent.restype = wt.HWND
u.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
u.SendMessageW.argtypes = [wt.HWND, wt.UINT, ctypes.c_void_p, ctypes.c_void_p]
u.SendMessageW.restype = ctypes.c_void_p
EnumProc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
WM_MDIACTIVATE = 0x0222


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


def title(h):
    b = ctypes.create_unicode_buffer(1024)
    u.GetWindowTextW(h, b, 1024)
    return b.value


def all_children(h):
    res = []

    def cb(c, l):
        res.append(c)
        return True

    u.EnumChildWindows(h, EnumProc(cb), 0)
    return res


def tops(pid):
    res = []

    def cb(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            res.append(h)
        return True

    u.EnumWindows(EnumProc(cb), 0)
    return res


def text_width(font, s):
    dc = u.GetDC(0)
    old = g.SelectObject(dc, font)
    sz = wt.SIZE()
    g.GetTextExtentPoint32W(dc, s, len(s), ctypes.byref(sz))
    g.SelectObject(dc, old)
    u.ReleaseDC(0, dc)
    return sz.cx


def menu_items(hmenu):
    n = u.GetMenuItemCount(hmenu)
    out = []
    for i in range(n):
        buf = ctypes.create_unicode_buffer(256)
        u.GetMenuStringW(hmenu, i, buf, 256, 0x400)       # MF_BYPOSITION
        mid = u.GetMenuItemID(hmenu, i)
        out.append((i, mid, buf.value))
    return out


def find_popup(pid):
    for t in tops(pid):
        if cls(t) == "#32768" and u.IsWindowVisible(t):
            return t
    return None


def main():
    want = sys.argv[1]
    item = None
    if "--item" in sys.argv:
        item = sys.argv[sys.argv.index("--item") + 1]
    if "--launch" in sys.argv:
        subprocess.run(["taskkill", "/f", "/im", "e.exe"], capture_output=True)
        time.sleep(1.2)
        p = subprocess.Popen([r"C:\Program Files (x86)\e\e.exe",
                              r"D:\eclone\shots\e_samples\graphic.e"],
                             cwd=r"C:\Program Files (x86)\e")
        time.sleep(18)
        pid = p.pid
    else:
        p = None
        pid = int(sys.argv[-1])
    frame = None
    for t in tops(pid):
        if cls(t) == "ENewFrame":
            frame = t
    print("pid=%d frame=0x%X" % (pid, frame))
    if "--designer" in sys.argv:
        mdi = d = None
        for h in all_children(frame):
            if cls(h) == "MDIClient":
                mdi = h
            if cls(h).startswith("Afx:400000:b") and "窗口" in title(h):
                d = h
        if mdi and d:
            u.SendMessageW(mdi, WM_MDIACTIVATE, d, 0)
            time.sleep(2.0)

    hmenu = u.GetMenu(frame)
    items = menu_items(hmenu)
    print("top items: %s" % [(i, t) for i, m, t in items])
    # measure: the menu font is the system menu font
    ncm = ctypes.c_int(0)
    u.SystemParametersInfoW(0x002A, 0, ctypes.byref(ncm), 0)      # SPI_GETNONCLIENTMETRICS? no
    class NONCLIENTMETRICSW(ctypes.Structure):
        _fields_ = [("cbSize", wt.UINT), ("iBorderWidth", ctypes.c_int),
                    ("iScrollWidth", ctypes.c_int), ("iScrollHeight", ctypes.c_int),
                    ("iCaptionWidth", ctypes.c_int), ("iCaptionHeight", ctypes.c_int),
                    ("lfCaptionFont", ctypes.c_byte * 92),
                    ("iSmCaptionWidth", ctypes.c_int), ("iSmCaptionHeight", ctypes.c_int),
                    ("lfSmCaptionFont", ctypes.c_byte * 92),
                    ("iMenuWidth", ctypes.c_int), ("iMenuHeight", ctypes.c_int),
                    ("lfMenuFont", ctypes.c_byte * 92),
                    ("lfStatusFont", ctypes.c_byte * 92),
                    ("lfMessageFont", ctypes.c_byte * 92)]
    ncm2 = NONCLIENTMETRICSW()
    ncm2.cbSize = ctypes.sizeof(NONCLIENTMETRICSW)
    u.SystemParametersInfoW(0x0029, ctypes.sizeof(NONCLIENTMETRICSW), ctypes.byref(ncm2), 0)  # SPI_GETNONCLIENTMETRICS
    font = g.CreateFontIndirectW(ctypes.byref(ncm2.lfMenuFont))
    x = 0
    bar_top = None
    for i, mid, tx in items:
        plain = tx.replace("&", "")
        w = text_width(font, plain) + 12
        if plain == want or plain.endswith(want) or want in plain:
            bar_top = (x, x + w)
        print("  item %d |%s| x=%d..%d" % (i, tx, x, x + w))
        x += w
    g.DeleteObject(font)
    if bar_top is None:
        print("FAIL: menu item %s not found" % want)
        if p:
            p.terminate()
        return 1
    r = wt.RECT()
    u.GetWindowRect(frame, ctypes.byref(r))
    cx = (bar_top[0] + bar_top[1]) // 2
    cy = 20
    pt = wt.POINT(cx, cy)
    u.ClientToScreen(frame, ctypes.byref(pt))
    u.SetForegroundWindow(frame)
    time.sleep(0.6)
    u.SetCursorPos(pt.x, pt.y)
    time.sleep(0.3)
    u.mouse_event(0x0002, 0, 0, 0, 0)
    time.sleep(0.1)
    u.mouse_event(0x0004, 0, 0, 0, 0)
    time.sleep(1.2)
    pop = find_popup(pid)
    print("popup = %s" % (pop,))
    if pop and item:
        pm = u.SendMessageW(pop, 0x01E1, 0, 0)
        sub = menu_items(pm)
        print("sub items: %s" % [(i, t) for i, m, t in sub])
        idx = None
        for i, mid, tx in sub:
            if item in tx.replace("&", ""):
                idx = i
        if idx is not None:
            pr = wt.RECT()
            u.GetWindowRect(pop, ctypes.byref(pr))
            # walk rows: use GetMenuItemRect if available
            mi = wt.RECT()
            if u.GetMenuItemRect(frame, pm, idx, ctypes.byref(mi)):
                u.SetCursorPos(mi.left + 10, mi.top + 8)
                time.sleep(0.3)
                u.mouse_event(0x0002, 0, 0, 0, 0)
                time.sleep(0.1)
                u.mouse_event(0x0004, 0, 0, 0, 0)
                time.sleep(1.5)
                print("clicked |%s| at %d,%d" % (item, mi.left + 10, mi.top + 8))
            else:
                print("GetMenuItemRect failed")
    for b in all_children(frame):
        if cls(b) == "AfxControlBar42s":
            rr = wt.RECT()
            u.GetWindowRect(b, ctypes.byref(rr))
            print("bar 0x%08X %-20s %dx%d vis=%s" % (b, title(b),
                  rr.right - rr.left, rr.bottom - rr.top, bool(u.IsWindowVisible(b))))
    if p:
        p.terminate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
