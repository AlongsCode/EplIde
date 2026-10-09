# -*- coding: utf-8 -*-
"""Send WM_COMMAND to whichever window in a process owns that control id -- which is how the
original e.exe IDE ends up opening the 组件箱 on the designer page.

The original ignores posted *mouse* messages, but WM_COMMAND posted to the frame that owns the
control is processed normally, and MFC's CControlBar visibility update is synchronous inside
the handler. So: locate the frame, post the toggle, then poll the control bar's width.

usage: python tools/bar_toggle.py <cmdid> [--designer] [--wait 18]
"""
import ctypes
import ctypes.wintypes as wt
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
u.PostMessageW.argtypes = [wt.HWND, wt.UINT, ctypes.c_void_p, ctypes.c_void_p]
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


def bars(pid):
    found = []
    for t in tops(pid):
        for h in [t] + all_children(t):
            if cls(h) == "AfxControlBar42s":
                found.append(h)
    return found


def main():
    cmd = int(sys.argv[1])
    designer = "--designer" in sys.argv
    wait = 18
    if "--wait" in sys.argv:
        wait = int(sys.argv[sys.argv.index("--wait") + 1])
    if "--launch" in sys.argv:
        subprocess.run(["taskkill", "/f", "/im", "e.exe"], capture_output=True)
        time.sleep(1.2)
        p = subprocess.Popen([r"C:\Program Files (x86)\e\e.exe",
                              r"D:\eclone\shots\e_samples\graphic.e"],
                             cwd=r"C:\Program Files (x86)\e")
        time.sleep(wait)
        pid = p.pid
    else:
        pid = int(sys.argv[-1])
        p = None
    frame = None
    for t in tops(pid):
        if cls(t) == "ENewFrame":
            frame = t
    if frame is None:
        print("FAIL: no ENewFrame")
        return 1
    print("pid=%d frame=0x%X" % (pid, frame))

    before = {}
    for b in bars(pid):
        before[b] = rect(b)
    for b, r in before.items():
        print("before bar 0x%08X %-24s %s vis=%s" % (b, title(b), r, bool(u.IsWindowVisible(b))))

    if designer:
        mdi = None
        for h in all_children(frame):
            if cls(h) == "MDIClient":
                mdi = h
        d = None
        for h in all_children(frame):
            if cls(h).startswith("Afx:400000:b") and "窗口" in title(h):
                d = h
        if mdi and d:
            u.SendMessageW(mdi, WM_MDIACTIVATE, d, 0)
            time.sleep(2.0)
            print("activated designer 0x%X" % d)

    u.PostMessageW(frame, WM_COMMAND, cmd, 0)
    time.sleep(1.5)

    after = {}
    for b in bars(pid):
        after[b] = rect(b)
    for b, r in after.items():
        old = before.get(b)
        print("after  bar 0x%08X %-24s %s vis=%s %s"
              % (b, title(b), r, bool(u.IsWindowVisible(b)),
                 "CHANGED" if old != r else ""))
    if p:
        p.terminate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
