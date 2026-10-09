# Calibration: does keybd_event + AttachThreadInput actually deliver keystrokes to a
# normal Win32 app? Uses tools/drive.py to find the window (Get-Process MainWindowHandle
# is unreliable on this machine), so no guessing about window handles.
#
# MUST STAY PURE ASCII.
# usage: python tools/kbd_calibrate.py
import ctypes
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive  # noqa: E402

u2 = ctypes.windll.user32


class POINT(ctypes.Structure):
    _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]


def cls(h):
    b = ctypes.create_unicode_buffer(256)
    u2.GetClassNameW(ctypes.c_void_p(h), b, 256)
    return b.value


def steal_foreground(h):
    me = ctypes.windll.kernel32.GetCurrentThreadId()
    pid = ctypes.c_uint32()
    them = u2.GetWindowThreadProcessId(ctypes.c_void_p(h), ctypes.byref(pid))
    u2.AttachThreadInput(me, them, True)
    u2.ShowWindow(ctypes.c_void_p(h), 9)
    u2.BringWindowToTop(ctypes.c_void_p(h))
    u2.SetForegroundWindow(ctypes.c_void_p(h))
    time.sleep(0.3)
    u2.AttachThreadInput(me, them, False)


def key(vk):
    u2.keybd_event(vk, 0, 0, None)
    time.sleep(0.12)
    u2.keybd_event(vk, 0, 2, None)
    time.sleep(0.2)


def win_text(h):
    b = ctypes.create_unicode_buffer(512)
    u2.GetWindowTextW(ctypes.c_void_p(h), b, 512)
    return b.value


def main():
    # MSPaint is a plain Win32 app; its window class is "MSPaintApp" and it holds focus.
    app = drive.App(r"C:\Windows\System32\mspaint.exe", args=[], wait=4)
    try:
        root = app.frame
        if not root:
            # drive.App may not find a frame for mspaint; fall back to its top window
            for h in drive.top_level():
                if drive.proc_name(h).lower().startswith("mspaint"):
                    root = h
        print("paint top window = 0x%X class=%s title=%r" % (root or 0, cls(root or 0), win_text(root or 0)))
        if not root:
            print("[SKIP] could not locate the paint window")
            return 2

        steal_foreground(root)
        print("foreground now = 0x%X" % u2.GetForegroundWindow())
        time.sleep(0.4)
        key(0x41)
        key(0x42)
        time.sleep(0.6)
        print("[INFO] typed A and B into paint")
        print("       look at the machine: if Paint shows a text tool entry, injection works")
        print("       (Paint has no text readback, so this is an eyeball check)")
        return 0
    finally:
        app.close()


if __name__ == "__main__":
    sys.exit(main())
