# -*- coding: utf-8 -*-
"""Drive a window with REAL mouse/keyboard input and report what changed.

Why a library: every "does the original do X?" question needs the same three moves --
find a window, click/drag/key on it for real, then measure (window rects, dumped model,
screenshots). Hand-rolling that in each probe is how wrong conclusions creep in.

usage:
    import drive
    app = drive.App(r"C:\\Program Files (x86)\\e\\e.exe", args=[sample], wait=18)
    app.frame                    # ENewFrame hwnd
    app.find("WTWindow")         # first descendant whose class matches
    app.click(hwnd, cx, cy)      # real click at CLIENT coords
    app.drag(hwnd, x0, y0, x1, y1)
    app.key(0x78, ctrl=True, shift=False)
    app.shot(hwnd, "out.png")
    app.close()
"""
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import time

u = ctypes.windll.user32
g = ctypes.windll.gdi32
EnumProc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)

u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
u.GetClientRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
u.IsWindowVisible.argtypes = [wt.HWND]
u.IsWindowVisible.restype = wt.BOOL
u.GetParent.argtypes = [wt.HWND]
u.GetParent.restype = wt.HWND
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
u.PrintWindow.argtypes = [wt.HWND, ctypes.c_void_p, ctypes.c_uint]
u.ClientToScreen.argtypes = [wt.HWND, ctypes.POINTER(wt.POINT)]
u.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
u.GetWindowLongW.argtypes = [wt.HWND, ctypes.c_int]
u.GetWindowLongW.restype = ctypes.c_long
u.GetCursorPos.argtypes = [ctypes.POINTER(wt.POINT)]

MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
MOUSEEVENTF_RIGHTDOWN = 0x0008
MOUSEEVENTF_RIGHTUP = 0x0010


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


def client(h):
    r = wt.RECT()
    u.GetClientRect(h, ctypes.byref(r))
    return (r.right - r.left, r.bottom - r.top)


def children(h):
    """direct children only"""
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


def dump_tree(h, depth=0, maxdepth=14):
    out = []
    x, y, w, hh = rect(h)

    def visit(node, d):
        xx, yy, ww, hhh = rect(node)
        out.append("%s0x%08X %-30s vis=%-5s %5d,%-5d %4dx%-4d |%s|"
                   % ("  " * d, node, cls(node), bool(u.IsWindowVisible(node)),
                      xx, yy, ww, hhh, title(node)))
        if d < maxdepth:
            for c in children(node):
                visit(c, d + 1)

    visit(h, depth)
    return out


def screenshot(h, path):
    from PIL import Image
    x, y, w, hh = rect(h)
    if w <= 0 or hh <= 0:
        return None
    hdc = u.GetDC(0)
    mdc = g.CreateCompatibleDC(hdc)
    bmp = g.CreateCompatibleBitmap(hdc, w, hh)
    old = g.SelectObject(mdc, bmp)
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
    g.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bi), 0)
    img = Image.frombuffer("RGBA", (w, hh), buf, "raw", "BGRA", 0, 1).convert("RGB")
    g.SelectObject(mdc, old)
    g.DeleteObject(bmp)
    g.DeleteDC(mdc)
    u.ReleaseDC(0, hdc)
    if path:
        img.save(path)
    return img


class App(object):
    def __init__(self, exe, args=(), wait=6.0, kill_name=None, env=None):
        self.exe = exe
        self.name = kill_name or os.path.basename(exe)
        subprocess.run(["taskkill", "/f", "/im", self.name], capture_output=True)
        time.sleep(0.8)
        e = dict(os.environ)
        if env:
            e.update(env)
        self.p = subprocess.Popen([exe] + list(args), cwd=os.path.dirname(exe), env=e)
        time.sleep(wait)
        self.pid = self.p.pid
        self.frame = None
        for t in tops(self.pid):
            if cls(t) in ("ENewFrame", "ECMainFrame"):
                self.frame = t
        if self.frame is None and tops(self.pid):
            self.frame = tops(self.pid)[0]

    # ---------------------------------------------------------------- lookups
    def find(self, class_sub, want_visible=False, biggest=False):
        hits = [h for h in descendants(self.frame) if class_sub.lower() in cls(h).lower()]
        if want_visible:
            hits = [h for h in hits if u.IsWindowVisible(h)]
        if not hits:
            return None
        if biggest:
            return max(hits, key=lambda h: rect(h)[2] * rect(h)[3])
        return hits[0]

    def find_all(self, class_sub):
        return [h for h in descendants(self.frame) if class_sub.lower() in cls(h).lower()]

    def find_titled(self, class_sub, text):
        for h in descendants(self.frame):
            if class_sub.lower() in cls(h).lower() and text in title(h):
                return h
        return None

    # ---------------------------------------------------------------- input
    def _screen(self, hwnd, cx, cy):
        pt = wt.POINT(cx, cy)
        u.ClientToScreen(hwnd, ctypes.byref(pt))
        return pt.x, pt.y

    def focus_root(self):
        root = self.frame
        while u.GetParent(root):
            root = u.GetParent(root)
        u.SetForegroundWindow(root)
        time.sleep(0.4)

    def move(self, hwnd, cx, cy):
        x, y = self._screen(hwnd, cx, cy)
        u.SetCursorPos(x, y)
        time.sleep(0.3)
        return x, y

    def click(self, hwnd, cx, cy, right=False, dbl=False):
        self.focus_root()
        x, y = self.move(hwnd, cx, cy)
        down = MOUSEEVENTF_RIGHTDOWN if right else MOUSEEVENTF_LEFTDOWN
        up = MOUSEEVENTF_RIGHTUP if right else MOUSEEVENTF_LEFTUP
        u.mouse_event(down, 0, 0, 0, 0)
        time.sleep(0.1)
        u.mouse_event(up, 0, 0, 0, 0)
        if dbl:
            time.sleep(0.06)
            u.mouse_event(down, 0, 0, 0, 0)
            time.sleep(0.06)
            u.mouse_event(up, 0, 0, 0, 0)
        time.sleep(0.45)
        return x, y

    def drag(self, hwnd, x0, y0, x1, y1, steps=12, right=False):
        self.focus_root()
        self.move(hwnd, x0, y0)
        down = MOUSEEVENTF_RIGHTDOWN if right else MOUSEEVENTF_LEFTDOWN
        up = MOUSEEVENTF_RIGHTUP if right else MOUSEEVENTF_LEFTUP
        u.mouse_event(down, 0, 0, 0, 0)
        time.sleep(0.15)
        for i in range(1, steps + 1):
            self.move(hwnd, x0 + (x1 - x0) * i // steps, y0 + (y1 - y0) * i // steps)
            time.sleep(0.04)
        time.sleep(0.15)
        u.mouse_event(up, 0, 0, 0, 0)
        time.sleep(0.45)

    def key(self, vk, ctrl=False, shift=False, alt=False):
        self.focus_root()
        if ctrl:
            u.keybd_event(0x11, 0, 0, 0)
        if shift:
            u.keybd_event(0x10, 0, 0, 0)
        if alt:
            u.keybd_event(0x12, 0, 0, 0)
        time.sleep(0.08)
        u.keybd_event(vk, 0, 0, 0)
        time.sleep(0.06)
        u.keybd_event(vk, 0, 2, 0)
        if alt:
            u.keybd_event(0x12, 0, 2, 0)
        if shift:
            u.keybd_event(0x10, 0, 2, 0)
        if ctrl:
            u.keybd_event(0x11, 0, 2, 0)
        time.sleep(0.4)

    def cursor(self):
        pt = wt.POINT()
        u.GetCursorPos(ctypes.byref(pt))
        return (pt.x, pt.y)

    def cursor_kind(self):
        """the cursor handle under the mouse right now (changes when an app sets a cursor)"""
        pt = wt.POINT()
        u.GetCursorPos(ctypes.byref(pt))
        h = u.WindowFromPoint(pt)
        return (h, u.GetCursor())

    def shot(self, hwnd, path):
        img = screenshot(hwnd, path)
        return img

    def close(self):
        try:
            self.p.terminate()
            time.sleep(0.4)
            subprocess.run(["taskkill", "/f", "/pid", str(self.pid)], capture_output=True)
        except Exception:
            pass
