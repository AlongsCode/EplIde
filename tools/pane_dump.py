# -*- coding: utf-8 -*-
"""Dump every top-level window + child-window tree of a process with class/text/rect/visible,
and optionally PrintWindow a control to a PNG.

The 易语言 IDE panels (工作夹 / 状态夹 / 组件箱 / 代码编辑器) are plain child windows, so this
gives their exact geometry without needing a screenshot for every question.

usage:
  python tools/pane_dump.py <pid> [out.txt]                 -> dump to file (utf-8) + print
  python tools/pane_dump.py <pid> --find <class-substr>     -> just the matches
  python tools/pane_dump.py --exe <path> [args...] [--wait N] [--dump out.txt] [--shot class=path ...]
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
u.IsWindowVisible.argtypes = [wt.HWND]
u.IsWindowVisible.restype = wt.BOOL
u.GetParent.argtypes = [wt.HWND]
u.GetParent.restype = wt.HWND
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
u.SendMessageW.argtypes = [wt.HWND, wt.UINT, ctypes.c_void_p, ctypes.c_void_p]
u.SendMessageW.restype = ctypes.c_void_p
u.PrintWindow.argtypes = [wt.HWND, ctypes.c_void_p, ctypes.c_uint]

EnumProc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


def txt(h):
    b = ctypes.create_unicode_buffer(1024)
    u.GetWindowTextW(h, b, 1024)
    return b.value


def rect(h):
    r = wt.RECT()
    u.GetWindowRect(h, ctypes.byref(r))
    return (r.left, r.top, r.right - r.left, r.bottom - r.top)


def children(h):
    """Direct children only. EnumChildWindows is recursive, so a parent check is required
    (without it the tree comes out with duplicated deep entries)."""
    res = []

    def cb(c, l):
        if u.GetParent(c) == h:
            res.append(c)
        return True

    u.EnumChildWindows(h, EnumProc(cb), 0)
    return res


def dump_tree(h, depth, out, maxdepth=12):
    x, y, w, hh = rect(h)
    out.append("%s0x%08X %-30s vis=%-5s %5d,%-5d %4dx%-4d |%s|"
               % ("  " * depth, h if isinstance(h, int) else h, cls(h),
                  bool(u.IsWindowVisible(h)), x, y, w, hh, txt(h)))
    if depth >= maxdepth:
        return
    for c in children(h):
        dump_tree(c, depth + 1, out, maxdepth)


def shot(h, path):
    from PIL import Image
    x, y, w, hh = rect(h)
    if w <= 0 or hh <= 0:
        print("bad rect for %s" % path)
        return
    hdc = ctypes.windll.user32.GetDC(0)
    mdc = ctypes.windll.gdi32.CreateCompatibleDC(hdc)
    bmp = ctypes.windll.gdi32.CreateCompatibleBitmap(hdc, w, hh)
    ctypes.windll.gdi32.SelectObject(mdc, bmp)

    class BMI(ctypes.Structure):
        _fields_ = [("biSize", wt.DWORD), ("biWidth", ctypes.c_long), ("biHeight", ctypes.c_long),
                    ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                    ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                    ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wt.DWORD),
                    ("biClrImportant", wt.DWORD), ("rgb", wt.BYTE * 12)]

    u.PrintWindow(h, mdc, 0)
    bi = BMI()
    bi.biSize = 40
    bi.biWidth = w
    bi.biHeight = -hh
    bi.biPlanes = 1
    bi.biBitCount = 32
    buf = ctypes.create_string_buffer(w * hh * 4)
    ctypes.windll.gdi32.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bi), 0)
    img = Image.frombuffer("RGBA", (w, hh), buf, "raw", "BGRA", 0, 1).convert("RGB")
    img.save(path)
    ctypes.windll.gdi32.DeleteObject(bmp)
    ctypes.windll.gdi32.DeleteDC(mdc)
    ctypes.windll.user32.ReleaseDC(0, hdc)
    print("shot -> %s %dx%d" % (path, w, hh))


def main():
    argv = sys.argv[1:]
    if not argv:
        print(__doc__)
        return 2
    p = None
    if argv[0] == "--exe":
        exe = argv[1]
        rest = argv[2:]
        wait = 16.0
        args = []
        i = 0
        while i < len(rest):
            if rest[i] == "--wait":
                wait = float(rest[i + 1]); i += 2
            else:
                args.append(rest[i]); i += 1
        subprocess.run(["taskkill", "/f", "/im", exe.split("\\")[-1]],
                       capture_output=True)
        time.sleep(0.6)
        p = subprocess.Popen([exe] + args, cwd=exe.rsplit("\\", 1)[0])
        time.sleep(wait)
        pid = p.pid
    else:
        pid = int(argv[0])
        rest = argv[1:]
    if pid == 0:      # --exe given but the real window belongs to a leftover instance
        cands = []

        def cb0(h, l):
            q = wt.DWORD()
            u.GetWindowThreadProcessId(h, ctypes.byref(q))
            if cls(h) in ("ENewFrame", "ECMainFrame", "Afx:400000:b"):
                cands.append((q.value, h))
            return True

        u.EnumWindows(EnumProc(cb0), 0)
        for cpid, h in cands:
            print("candidate pid=%d hwnd=0x%X %s |%s|" % (cpid, h, cls(h), txt(h)))
        if cands:
            pid = cands[0][0]
            print("using pid=%d" % pid)
    filt = None
    outfile = None
    shots = []
    i = 0
    while i < len(rest):
        if rest[i] == "--find":
            filt = rest[i + 1]; i += 2
        elif rest[i] == "--dump":
            outfile = rest[i + 1]; i += 2
        elif rest[i] == "--shot":
            shots.append(rest[i + 1]); i += 2
        else:
            outfile = rest[i]; i += 1

    tops = []

    def cb(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            tops.append(h)
        return True

    u.EnumWindows(EnumProc(cb), 0)
    out = []
    for t in tops:
        dump_tree(t, 0, out)
    text = "\n".join(out)
    if outfile:
        import io
        io.open(outfile, "w", encoding="utf-8").write(text + "\n")
        print("dump -> %s (%d windows)" % (outfile, len(out)))
    if filt:
        low = filt.lower()
        for line in out:
            if low in line.lower():
                print(line)
    elif not outfile:
        print(text)

    for spec in shots:
        key, path = spec.split("=", 1)
        hit = None
        if key.lower().startswith("0x"):
            hit = int(key, 16)
        else:
            low = key.lower()
            for t in tops:
                for h in [t] + children(t):
                    if low in cls(h).lower():
                        hit = h
                        break
                if hit:
                    break
        if hit:
            shot(hit, path)
        else:
            print("no window matching %s" % key)
    return 0


if __name__ == "__main__":
    sys.exit(main())
