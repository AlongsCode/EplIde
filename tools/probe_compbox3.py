# -*- coding: utf-8 -*-
"""Dump the ORIGINAL e.exe component box (#110 窗口组件箱) geometry after switching the IDE to
the designer document, and screenshot the box 1:1 plus one PNG per component button.

Why the designer must be active: the component box is only shown on the designer page
(HANDOFF §2.6/§3.7).

usage: python tools/probe_compbox3.py [--out shots/live_compbox] [--keep]
"""
import ctypes
import ctypes.wintypes as wt
import io
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
u.PrintWindow.argtypes = [wt.HWND, ctypes.c_void_p, ctypes.c_uint]
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


def rect(h):
    r = wt.RECT()
    u.GetWindowRect(h, ctypes.byref(r))
    return (r.left, r.top, r.right - r.left, r.bottom - r.top)


def kids(h):
    res = []

    def cb(c, l):
        if u.GetParent(c) == h:
            res.append(c)
        return True

    u.EnumChildWindows(h, EnumProc(cb), 0)
    return res


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


def grab(h):
    """Return a PIL image of h's window rect via PrintWindow."""
    from PIL import Image
    x, y, w, hh = rect(h)
    if w <= 0 or hh <= 0:
        return None
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
    img = Image.frombuffer("RGBA", (w, hh), buf, "raw", "BGRA", 0, 1).convert("RGB")
    ctypes.windll.gdi32.SelectObject(mdc, sel)
    ctypes.windll.gdi32.DeleteObject(bmp)
    ctypes.windll.gdi32.DeleteDC(mdc)
    u.ReleaseDC(0, hdc)
    return img


def main():
    out = r"D:\eclone\shots\live_compbox"
    keep = "--keep" in sys.argv
    os.makedirs(out, exist_ok=True)
    subprocess.run(["taskkill", "/f", "/im", "e.exe"], capture_output=True)
    time.sleep(1.2)
    p = subprocess.Popen([r"C:\Program Files (x86)\e\e.exe",
                          r"D:\eclone\shots\e_samples\graphic.e"],
                         cwd=r"C:\Program Files (x86)\e")
    time.sleep(18)
    if p.poll() is not None:
        print("FAIL: original exited early")
        return 1
    pid = p.pid
    frame = None
    for t in tops(pid):
        if cls(t) == "ENewFrame":
            frame = t
    mdi = None
    for h in all_children(frame):
        if cls(h) == "MDIClient":
            mdi = h
    designer = None
    for h in all_children(frame):
        if cls(h).startswith("Afx:400000:b") and "窗口" in title(h):
            designer = h
    print("pid=%d frame=0x%X mdi=0x%X designer=0x%X" % (pid, frame, mdi or 0, designer or 0))
    if mdi and designer:
        u.SendMessageW(mdi, WM_MDIACTIVATE, designer, 0)
        time.sleep(2.5)

    lines = []
    for h in [frame] + all_children(frame):
        x, y, w, hh = rect(h)
        lines.append("0x%08X %-28s vis=%-5s %5d,%-5d %4dx%-4d |%s|"
                     % (h, cls(h), bool(u.IsWindowVisible(h)), x, y, w, hh, title(h)))
    io.open(os.path.join(out, "des_windows.txt"), "w", encoding="utf-8").write("\n".join(lines))

    bar = dlg = None
    for h in all_children(frame):
        if cls(h) == "AfxControlBar42s" and "组件" in title(h):
            bar = h
    if bar:
        print("comp bar rect=%s vis=%s" % (rect(bar), bool(u.IsWindowVisible(bar))))
        for h in all_children(bar):
            if cls(h) == "#32770":
                dlg = h
    if not dlg:
        print("FAIL: the component box has no #32770 (designer page did not show it)")
        if not keep:
            p.terminate()
        return 1
    dx, dy, dw, dh = rect(dlg)
    print("comp dialog rect=%s vis=%s" % ((dx, dy, dw, dh), bool(u.IsWindowVisible(dlg))))
    img = grab(dlg)
    if img:
        img.save(os.path.join(out, "dlg.png"))
        print("dlg.png %s" % (img.size,))

    entries = []
    for c in kids(dlg):
        ck = cls(c)
        cx, cy, cw, ch = rect(c)
        entries.append((ck, cx - dx, cy - dy, cw, ch, title(c)))
    io.open(os.path.join(out, "dlg_children.txt"), "w", encoding="utf-8").write(
        "\n".join("%-14s %4d,%-4d %3dx%-3d |%s|" % e for e in entries))
    print("dlg children -> dlg_children.txt (%d)" % len(entries))

    if img:
        for i, (ck, cx, cy, cw, ch, tx) in enumerate(entries):
            if ck == "Button" and cw >= 20 and ch >= 20:
                cell = img.crop((cx, cy, cx + cw, cy + ch))
                cell.save(os.path.join(out, "cell%02d.png" % i))
    if not keep:
        p.terminate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
