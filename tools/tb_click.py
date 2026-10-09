# -*- coding: utf-8 -*-
"""Click a button on the ORIGINAL IDE's toolbar by its command id, via TB_GETBUTTON ->
TB_PRESSBUTTON (a real click on a toolbar is not needed for MFC: TB_PRESSBUTTON then
TB_SETSTATE would only change visuals, so we post the command the button sends).

Why: the component box toggle (#110) does not respond to a posted WM_COMMAND to the frame, but
the toolbar that carries the same command does -- this is the shortest path to open the box.

usage: python tools/tb_click.py <cmdid> [--launch] [--designer] [--pid N]
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
TB_BUTTONCOUNT = 0x0418
TB_GETBUTTON = 0x0417
TB_COMMANDTOINDEX = 0x0419
TB_GETSTATE = 0x0412
TB_PRESSBUTTON = 0x0403


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


class TBBUTTON(ctypes.Structure):
    _fields_ = [("iBitmap", ctypes.c_int), ("idCommand", ctypes.c_int),
                ("fsState", wt.BYTE), ("fsStyle", wt.BYTE),
                ("bReserved", wt.BYTE * 2), ("dwData", ctypes.c_void_p),
                ("iString", ctypes.c_void_p)]


def open_process(pid):
    k = ctypes.windll.kernel32
    k.OpenProcess.restype = wt.HANDLE
    k.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
    k.VirtualAllocEx.restype = ctypes.c_void_p
    k.VirtualAllocEx.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_size_t, wt.DWORD, wt.DWORD]
    k.WriteProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                     ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    k.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                    ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    h = k.OpenProcess(0x0008 | 0x0010 | 0x0020 | 0x0400, False, pid)
    remote = k.VirtualAllocEx(h, None, 4096, 0x1000, 0x04)
    return k, h, remote


def main():
    cmd = int(sys.argv[1])
    wait = 18
    if "--wait" in sys.argv:
        wait = int(sys.argv[sys.argv.index("--wait") + 1])
    pid = None
    p = None
    if "--pid" in sys.argv:
        pid = int(sys.argv[sys.argv.index("--pid") + 1])
    else:
        subprocess.run(["taskkill", "/f", "/im", "e.exe"], capture_output=True)
        time.sleep(1.2)
        p = subprocess.Popen([r"C:\Program Files (x86)\e\e.exe",
                              r"D:\eclone\shots\e_samples\graphic.e"],
                             cwd=r"C:\Program Files (x86)\e")
        time.sleep(wait)
        pid = p.pid
    frame = None
    for t in tops(pid):
        if cls(t) == "ENewFrame":
            frame = t
    if frame is None:
        print("FAIL: no frame")
        return 1
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
            print("designer activated")

    k, hproc, remote = open_process(pid)
    print("hproc=%s remote=0x%X" % (hproc, remote or 0))
    tbs = [x for x in all_children(frame) if cls(x) == "ToolbarWindow32"]
    hit = None
    for tb in tbs:
        n = u.SendMessageW(tb, TB_BUTTONCOUNT, 0, 0)
        for i in range(int(n)):
            buf = ctypes.create_string_buffer(ctypes.sizeof(TBBUTTON))
            ctypes.memmove(buf, ctypes.byref(TBBUTTON()), ctypes.sizeof(TBBUTTON))
            k.WriteProcessMemory(hproc, remote, buf, ctypes.sizeof(TBBUTTON), None)
            u.SendMessageW(tb, TB_GETBUTTON, i, remote)
            out = ctypes.create_string_buffer(ctypes.sizeof(TBBUTTON))
            k.ReadProcessMemory(hproc, remote, out, ctypes.sizeof(TBBUTTON), None)
            b = TBBUTTON.from_buffer_copy(out)
            if b.idCommand == cmd:
                hit = (tb, i)
                print("toolbar 0x%08X |%s| button %d id=%d state=0x%X style=0x%X"
                      % (tb, title(tb), i, b.idCommand, b.fsState, b.fsStyle))
                break
        if hit:
            break
    if not hit:
        print("command %d not on any toolbar" % cmd)
        # fall back: post the command anyway
        u.PostMessageW(frame, WM_COMMAND, cmd, 0)
    else:
        tb, i = hit
        r = wt.RECT()
        u.SendMessageW(tb, 0x0415, i, ctypes.byref(r))     # TB_GETRECT
        print("button rect on toolbar client = (%d,%d,%d,%d)" % (r.left, r.top, r.right, r.bottom))
        pt = wt.POINT((r.left + r.right) // 2, (r.top + r.bottom) // 2)
        u.ClientToScreen(tb, ctypes.byref(pt))
        root = tb
        while u.GetParent(root):
            root = u.GetParent(root)
        u.SetForegroundWindow(root)
        time.sleep(0.6)
        u.SetCursorPos(pt.x, pt.y)
        time.sleep(0.3)
        u.mouse_event(0x0002, 0, 0, 0, 0)
        time.sleep(0.08)
        u.mouse_event(0x0004, 0, 0, 0, 0)
        time.sleep(1.5)
        print("clicked toolbar button at screen %d,%d" % (pt.x, pt.y))
    for b in all_children(frame):
        if cls(b) == "AfxControlBar42s":
            print("bar 0x%08X %-20s %s vis=%s" % (b, title(b), rect(b), bool(u.IsWindowVisible(b))))
    if p:
        p.terminate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
