# -*- coding: utf-8 -*-
"""Why do real keystrokes not reach the EClone editor?

Reuses tools/drive.py (real mouse click) and then inspects, for the APP's UI thread:
  * GetForegroundWindow / GetFocus (thread-scoped) / GetGUIThreadInfo.hwndFocus
  * which window the OS thinks has focus, and its class
  * whether the editor window is in fact the focused window

usage: python tools/focus_check.py
"""
import ctypes  # noqa: E402
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MINE = os.path.join(ROOT, "bin", "EClone.exe")
SAMPLE = os.path.join(ROOT, "shots", "e_samples", "flow_ref.e")
OUT = os.path.join(ROOT, "shots")
DUMP = os.path.join(OUT, "model_focus.txt")

u = drive.u
wt = drive.wt
u2 = ctypes.windll.user32


class GTI(ctypes.Structure):
    _fields_ = [("cbSize", ctypes.c_uint32), ("flags", ctypes.c_uint32),
                ("hwndActive", ctypes.c_void_p), ("hwndFocus", ctypes.c_void_p),
                ("hwndCapture", ctypes.c_void_p), ("hwndMenuOwner", ctypes.c_void_p),
                ("hwndMoveSize", ctypes.c_void_p), ("hwndCaret", ctypes.c_void_p),
                ("rcCaret", ctypes.c_int * 4)]


def cls(h):
    buf = ctypes.create_unicode_buffer(256)
    u2.GetClassNameW(ctypes.c_void_p(h), buf, 256)
    return buf.value


def focus_info_for(tid):
    g = GTI()
    g.cbSize = ctypes.sizeof(GTI)
    if not u2.GetGUIThreadInfo(tid, ctypes.byref(g)):
        return None
    return g


def walk(h, depth=0, maxdepth=3):
    if depth > maxdepth:
        return
    print("  " * depth + "0x%X %s" % (h, cls(h)))
    for k in drive.children(h):
        walk(k, depth + 1, maxdepth)


def main():
    env = {"ECLONE_DUMP_MODEL": DUMP}
    app = drive.App(MINE, args=["\"" + SAMPLE + "\""], wait=8, env=env)
    try:
        ed = None
        for h in drive.descendants(app.frame):
            if drive.cls(h).startswith("Afx:400000:8:10005"):
                ed = h
        if ed is None:
            print("[FAIL] no editor window")
            return 1
        print("frame  = 0x%X (%s)" % (app.frame, drive.cls(app.frame)))
        print("editor = 0x%X (%s)" % (ed, drive.cls(ed)))

        print("\n-- window tree above the editor --")
        walk(app.frame)

        pid = ctypes.c_uint32()
        tid = u2.GetWindowThreadProcessId(ctypes.c_void_p(ed), ctypes.byref(pid))
        print("\napp pid=%d  ui tid=%d" % (pid.value, tid))

        print("\n-- before click --")
        print("  GetForegroundWindow = 0x%X (%s)" % (u2.GetForegroundWindow(), cls(u2.GetForegroundWindow())))
        g = focus_info_for(tid)
        if g:
            print("  GTInfo hwndFocus = 0x%X (%s)" % (g.hwndFocus or 0, cls(g.hwndFocus or 0)))
            print("  GTInfo hwndCaret = 0x%X" % (g.hwndCaret or 0))

        # real mouse click into a code row
        import re
        rws = []
        if os.path.exists(DUMP):
            txt = open(DUMP, "rb").read().decode("gbk", "replace")
            for l in txt.splitlines():
                m = re.match(r"row\s+(\d+) kind=(\d+).*?y=\s*(\d+) h=\s*(\d+)", l)
                if m:
                    rws.append(tuple(int(x) for x in m.groups()))
        cy = (rws[0][2] + rws[0][3] // 2) if rws else 150
        print("\nclick at client (150, %d)" % cy)
        app.click(ed, 150, cy)
        time.sleep(0.6)

        print("\n-- after click --")
        fg = u2.GetForegroundWindow()
        print("  GetForegroundWindow = 0x%X (%s)" % (fg, cls(fg)))
        print("  editor==foreground? %s" % (fg == ed))
        print("  is editor a descendant of foreground? %s" % (fg in drive.descendants(fg) or True))
        g = focus_info_for(tid)
        if g:
            print("  GTInfo hwndFocus = 0x%X (%s)" % (g.hwndFocus or 0, cls(g.hwndFocus or 0)))
            print("  GTInfo hwndCaret = 0x%X" % (g.hwndCaret or 0))
            print("  focus==editor? %s" % (g.hwndFocus == ed))
        print("  GetFocus() (this thread) = 0x%X" % u2.GetFocus())
        return 0
    finally:
        app.close()


if __name__ == "__main__":
    sys.exit(main())
