# -*- coding: utf-8 -*-
"""Measure the ORIGINAL e.exe component box (窗口组件箱): activate the designer page, open the
box (#110), and dump its inner geometry + a 1:1 screenshot.

usage: python tools/probe_compbox2.py [--out shots/live_compbox] [--sample <file>] [--keep]
"""
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetParent.argtypes = [wt.HWND]
u.GetParent.restype = wt.HWND
u.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
u.IsWindowVisible.argtypes = [wt.HWND]
u.IsWindowVisible.restype = wt.BOOL
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
u.SendMessageW.argtypes = [wt.HWND, wt.UINT, ctypes.c_void_p, ctypes.c_void_p]
u.SendMessageW.restype = ctypes.c_void_p
EnumProc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
WM_COMMAND = 0x0111
WM_MDIACTIVATE = 0x0222


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


def title(h):
    b = ctypes.create_unicode_buffer(1024)
    u.GetWindowTextW(h, b, 1024)
    return b.value


def rect(h):
    r = wt.RECT()
    u.GetWindowRect(h, ctypes.byref(r))
    return (r.left, r.top, r.right - r.left, r.bottom - r.top)


def kids_seq(h):
    """direct children in z-order"""
    res = []

    def cb(c, l):
        if u.GetParent(c) == h:
            res.append(c)
        return True

    u.EnumChildWindows(h, EnumProc(cb), 0)
    return res


def descendants(h):
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


def click_screen(hwnd, cx, cy):
    """Real mouse click at the given CLIENT coordinates of hwnd."""
    pt = wt.POINT(cx, cy)
    u.ClientToScreen(hwnd, ctypes.byref(pt))
    u.SetForegroundWindow(hwnd)
    time.sleep(0.4)
    u.SetCursorPos(pt.x, pt.y)
    time.sleep(0.25)
    u.mouse_event(0x0002, 0, 0, 0, 0)
    time.sleep(0.08)
    u.mouse_event(0x0004, 0, 0, 0, 0)
    time.sleep(0.5)
    print("click client %d,%d -> screen %d,%d" % (cx, cy, pt.x, pt.y))


def shot(h, path):
    from PIL import Image
    x, y, w, hh = rect(h)
    if w <= 0 or hh <= 0:
        print("bad rect %s %s" % (path, (w, hh)))
        return
    hdc = u.GetDC(0)
    mdc = ctypes.windll.gdi32.CreateCompatibleDC(hdc)
    bmp = ctypes.windll.gdi32.CreateCompatibleBitmap(hdc, w, hh)
    sel = ctypes.windll.gdi32.SelectObject(mdc, bmp)
    u.PrintWindow(h, mdc, 0)

    class BMI(ctypes.Structure):
        _fields_ = [("biSize", wt.DWORD), ("biWidth", ctypes.c_long), ("biHeight", ctypes.c_long),
                    ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                    ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                    ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wt.DWORD),
                    ("biClrImportant", wt.DWORD), ("rgb", wt.BYTE * 12)]

    bi = BMI()
    bi.biSize = 40
    bi.biWidth = w
    bi.biHeight = -hh
    bi.biPlanes = 1
    bi.biBitCount = 32
    buf = ctypes.create_string_buffer(w * hh * 4)
    ctypes.windll.gdi32.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bi), 0)
    Image.frombuffer("RGBA", (w, hh), buf, "raw", "BGRA", 0, 1).convert("RGB").save(path)
    ctypes.windll.gdi32.SelectObject(mdc, sel)
    ctypes.windll.gdi32.DeleteObject(bmp)
    ctypes.windll.gdi32.DeleteDC(mdc)
    u.ReleaseDC(0, hdc)
    print("shot %s %dx%d" % (path, w, hh))


def main():
    out = r"D:\eclone\shots\live_compbox"
    sample = r"D:\eclone\shots\e_samples\graphic.e"
    keep = False
    argv = sys.argv[1:]
    i = 0
    while i < len(argv):
        if argv[i] == "--out":
            out = argv[i + 1]; i += 2
        elif argv[i] == "--sample":
            sample = argv[i + 1]; i += 2
        elif argv[i] == "--keep":
            keep = True; i += 1
        else:
            i += 1
    os.makedirs(out, exist_ok=True)
    subprocess.run(["taskkill", "/f", "/im", "e.exe"], capture_output=True)
    time.sleep(1.2)
    p = subprocess.Popen([r"C:\Program Files (x86)\e\e.exe", sample],
                         cwd=r"C:\Program Files (x86)\e")
    time.sleep(18)
    if p.poll() is not None:
        print("FAIL: original exited early")
        return 1
    pid = p.pid
    frame = None
    mdi = None
    for t in tops(pid):
        if cls(t) == "ENewFrame":
            frame = t
    if frame is None:
        print("FAIL: no ENewFrame")
        return 1
    for h in descendants(frame):
        if cls(h) == "MDIClient":
            mdi = h
    print("pid=%d frame=0x%X mdi=0x%X" % (pid, frame, mdi or 0))

    # activate the designer document (the MDI child that owns a WTWindow)
    designer = None
    for h in descendants(frame):
        if cls(h).startswith("Afx:400000:b") and "窗口" in title(h):
            designer = h
    print("designer child = %s |%s|" % (designer, title(designer) if designer else ""))
    if designer and mdi:
        u.SendMessageW(mdi, WM_MDIACTIVATE, designer, 0)
        time.sleep(2.0)

    # open the component box with a REAL mouse click on 窗口 -> 窗口组件箱 (#110).
    # A posted WM_COMMAND used to be enough, but the original now ignores it -- real input only.
    click_screen(frame, 440, 33)
    time.sleep(1.0)
    popup = None
    for t in tops(pid):
        if cls(t) == "#32768" and u.IsWindowVisible(t):
            popup = t
    if popup:
        px, py, pw, ph = rect(popup)
        print("window popup %s" % ((px, py, pw, ph),))
        shot(popup, os.path.join(out, "winmenu.png"))
        # item order in the original 窗口 menu (measured earlier): 层叠/平铺/排列图标/分隔/
        # 关闭/全部关闭/分隔/工具条开关...; the component box is the 4th switch. Click by
        # text instead: find the y of the "窗口组件箱" row by rendering is impossible here --
        # walk the popup rows and pick the one whose height puts it last-but-few.
        n = u.GetMenuItemCount(u.SendMessageW(popup, 0x01E1, 0, 0))
        print("popup item count=%d" % n)
        for i in range(n):
            click_screen(px + 40, py + 12 + i * 18)
            time.sleep(0.5)
            visible = False
            for h in [frame] + descendants(frame):
                if cls(h) == "AfxControlBar42s" and "组件" in title(h):
                    r = rect(h)
                    if r[2] > 10:
                        visible = True
            if visible:
                print("component box opened by popup row %d" % i)
                break
    else:
        print("FAIL: no popup menu")
        u.SendMessageW(frame, WM_COMMAND, 110, 0)
        time.sleep(1.5)

    lines = []
    for h in [frame] + descendants(frame):
        x, y, w, hh = rect(h)
        lines.append("0x%08X %-28s vis=%-5s %5d,%-5d %4dx%-4d |%s|"
                     % (h, cls(h), bool(u.IsWindowVisible(h)), x, y, w, hh, title(h)))
    open(os.path.join(out, "windows.txt"), "w", encoding="utf-8").write("\n".join(lines))
    print("windows -> %s/windows.txt" % out)

    # screenshot the component box bar if we can find it
    for h in [frame] + descendants(frame):
        if cls(h) == "AfxControlBar42s" and "组件" in title(h):
            shot(h, os.path.join(out, "compbar.png"))
            for c in descendants(h):
                if cls(c) == "#32770":
                    shot(c, os.path.join(out, "compdlg.png"))
                    break
    shot(frame, os.path.join(out, "frame.png"))
    if not keep:
        p.terminate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
