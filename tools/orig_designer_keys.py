# -*- coding: utf-8 -*-
"""Switch the ORIGINAL IDE to the window-designer view by keyboard, then screenshot.

The document tabs are a custom control that only responds to real clicks at positions we cannot
map reliably, so try the IDE's own shortcuts (Shift+F12 / F12 / Ctrl+Tab) and check after each
whether a WTWindow became visible.

usage: python tools/orig_designer_keys.py
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

E = r"C:\Program Files (x86)\e\e.exe"
SAMPLE = r"D:\eclone\shots\e_samples\graphic.e"

u = ctypes.windll.user32
u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
u.EnumWindows.argtypes = [ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.EnumChildWindows.argtypes = [wt.HWND, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM), wt.LPARAM]
u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
u.SetForegroundWindow.argtypes = [wt.HWND]
KEYEVENTF_KEYUP = 0x0002
VK_F12, VK_TAB, VK_SHIFT, VK_F11, VK_F8 = 0x7B, 0x09, 0x10, 0x7A, 0x77


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u.GetClassNameW(h, b, 256)
    return b.value


def key(vk, shift=False):
    if shift:
        u.keybd_event(VK_SHIFT, 0, 0, 0)
    u.keybd_event(vk, 0, 0, 0)
    time.sleep(0.08)
    u.keybd_event(vk, 0, KEYEVENTF_KEYUP, 0)
    if shift:
        u.keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0)
    time.sleep(1.0)


def forms(pid, tops):
    out = []

    def cb(h, l):
        if cls(h) == "WTWindow":
            r = wt.RECT()
            u.GetWindowRect(h, ctypes.byref(r))
            out.append((r.left, r.top, r.right - r.left, r.bottom - r.top))
        return True

    for t in tops:
        u.EnumChildWindows(t, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cb), 0)
    return out


def main():
    p = subprocess.Popen([E, SAMPLE])
    pid = p.pid
    time.sleep(9)
    tops = []

    def cbt(h, l):
        q = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(q))
        if q.value == pid:
            tops.append(h)
        return True

    u.EnumWindows(ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cbt), 0)
    big = None
    for t in tops:
        r = wt.RECT()
        u.GetWindowRect(t, ctypes.byref(r))
        if r.right - r.left > 1200 and not (u.GetWindowLongW(t, -16) & 0x40000000):
            big = t
            break
    print("main frame 0x%08X" % (big or 0))
    if big:
        u.SetForegroundWindow(big)
        time.sleep(0.8)

    print("before: WTWindow=%s" % forms(pid, tops))
    for name, vk, sh in (("Shift+F12", VK_F12, True), ("F12", VK_F12, False),
                         ("Ctrl+Tab", VK_TAB, False), ("F11", VK_F11, False)):
        key(vk, sh)
        f = forms(pid, tops)
        print("after %-10s WTWindow=%s" % (name, f[:1]))
        if f:
            from PIL import ImageGrab
            r = wt.RECT()
            u.GetWindowRect(big, ctypes.byref(r))
            ImageGrab.grab(bbox=(r.left, r.top, r.right, r.bottom)).save(
                r"D:\eclone\shots\orig_designer_keys.png")
            print("saved shots\\orig_designer_keys.png")
            break

    p.terminate()


main()
