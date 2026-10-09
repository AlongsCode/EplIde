# -*- coding: utf-8 -*-
"""Launch the ORIGINAL e.exe, drive it with real input, and capture what each UI element
does -- the measurement half of "复刻原版行为".

Outputs (all under --out):
  step<N>_<tag>.png    full-frame PrintWindow after each step
  <tag>.txt            every visible child window's class / rect / text (WM_GETTEXT via a
                       32-bit helper, so Edit/Static/ComboBox text is readable)
  panes.txt            the full child-window dump right after the sample loads

usage:
  python tools/probe_ui.py --out shots/live_ui --steps "<spec>"
  spec: "name:click:x,y", "name:key:78" (vk hex), "name:tree:<tree>,<item>",
        "name:menu:x,y", "name:shot", "name:wait:1.5"
"""
import ctypes
import ctypes.wintypes as wt
import io
import os
import re
import subprocess
import sys
import time

TOOLS = r"D:\eclone\tools"
u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetParent.argtypes = [wt.HWND]
u.GetParent.restype = wt.HWND
u.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
u.IsWindowVisible.argtypes = [wt.HWND]
u.IsWindowVisible.restype = wt.BOOL
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
EnumProc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)


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


def find_tops(pid):
    res = []

    def cb(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            res.append(h)
        return True

    u.EnumWindows(EnumProc(cb), 0)
    return res


def find_by_class(root, prefix, want_max=False):
    hit = []
    for h in [root] + all_children(root):
        if cls(h).lower().startswith(prefix.lower()):
            hit.append(h)
    if not hit:
        return None
    if want_max:
        return max(hit, key=lambda x: rect(x)[2] * rect(x)[3])
    return hit[0]


def shot(h, path):
    from PIL import Image
    x, y, w, hh = rect(h)
    if w <= 0 or hh <= 0:
        print("bad rect %s" % path)
        return
    hdc = u.GetDC(0)
    mdc = ctypes.windll.gdi32.CreateCompatibleDC(hdc)
    bmp = ctypes.windll.gdi32.CreateCompatibleBitmap(hdc, w, hh)
    ctypes.windll.gdi32.SelectObject(mdc, bmp)
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
    ctypes.windll.gdi32.DeleteObject(bmp)
    ctypes.windll.gdi32.DeleteDC(mdc)
    u.ReleaseDC(0, hdc)
    print("shot %s %dx%d" % (path, w, hh))


def main():
    out = r"D:\eclone\shots\live_ui"
    exe = r"C:\Program Files (x86)\e\e.exe"
    sample = r"D:\eclone\shots\e_samples\graphic.e"
    wait = 18.0
    steps = []
    argv = sys.argv[1:]
    i = 0
    while i < len(argv):
        if argv[i] == "--out":
            out = argv[i + 1]; i += 2
        elif argv[i] == "--file":
            sample = argv[i + 1]; i += 2
        elif argv[i] == "--wait":
            wait = float(argv[i + 1]); i += 2
        elif argv[i] == "--steps":
            steps = argv[i + 1].split(";"); i += 2
        else:
            i += 1
    os.makedirs(out, exist_ok=True)
    subprocess.run(["taskkill", "/f", "/im", "e.exe"], capture_output=True)
    time.sleep(1.0)
    p = subprocess.Popen([exe, sample], cwd=os.path.dirname(exe))
    time.sleep(wait)
    if p.poll() is not None:
        print("FAIL: original exited early (the single-instance copy may still be running)")
        return 1
    pid = p.pid
    tops = find_tops(pid)
    frame = None
    for t in tops:
        if cls(t) == "ENewFrame":
            frame = t
    if frame is None:
        print("FAIL: no ENewFrame for pid %d" % pid)
        return 1
    print("pid=%d frame=0x%X |%s|" % (pid, frame, title(frame)))

    def dump(tag):
        r = subprocess.run([os.path.join(TOOLS, "wtext.exe"), str(pid)],
                           capture_output=True)
        io.open(os.path.join(out, tag + ".txt"), "wb").write(r.stdout)
        lines = []
        for h in [frame] + all_children(frame):
            x, y, w, hh = rect(h)
            lines.append("0x%08X %-28s vis=%-5s %5d,%-5d %4dx%-4d |%s|"
                         % (h, cls(h), bool(u.IsWindowVisible(h)), x, y, w, hh, title(h)))
        io.open(os.path.join(out, "panes.txt"), "w", encoding="utf-8").write("\n".join(lines))
        print("dump -> %s/%s.txt (%d windows)" % (out, tag, len(lines)))

    dump("step0")
    shot(frame, os.path.join(out, "step0.png"))

    n = 0
    for s in steps:
        if not s.strip():
            continue
        n += 1
        parts = s.split(":")
        tag = parts[0]
        kind = parts[1]
        arg = parts[2] if len(parts) > 2 else ""
        if kind == "click":
            x, y = arg.split(",")
            subprocess.run([os.path.join(TOOLS, "mouseact.exe"), "%X" % frame, "click", x, y],
                           capture_output=True)
        elif kind == "move":
            x, y = arg.split(",")
            subprocess.run([os.path.join(TOOLS, "mouseact.exe"), "%X" % frame, "move", x, y],
                           capture_output=True)
        elif kind == "key":
            subprocess.run([os.path.join(TOOLS, "mouseact.exe"), "%X" % frame, "key", arg],
                           capture_output=True)
        elif kind == "tree":
            tr, it = arg.split(",")
            subprocess.run([os.path.join(TOOLS, "treeinfo.exe"), str(pid), "mdclick", tr, it],
                           capture_output=True)
        elif kind == "wait":
            time.sleep(float(arg))
        elif kind == "dump":
            dump(tag)
            continue
        else:
            print("unknown step kind %s" % kind)
            continue
        time.sleep(1.0)
        dump(tag)
        shot(frame, os.path.join(out, "step%d_%s.png" % (n, tag)))

    p.terminate()
    time.sleep(0.5)
    subprocess.run(["taskkill", "/f", "/pid", str(pid)], capture_output=True)
    print("done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
