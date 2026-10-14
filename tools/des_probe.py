# -*- coding: utf-8 -*-
"""Dump the design-form (WTWindow) details for the original and for EClone:
style / exstyle / parent chain / title / rects / class atoms."""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIG = r"C:\Program Files (x86)\e\e.exe"
MINE = os.path.join(ROOT, "bin", "EClone.exe")
SAMPLE = r"C:\Users\Along\AppData\Local\Temp\e_re\samples\flowcmd.e"
u = drive.u


def info(tag, exe, args, wait):
    app = drive.App(exe, args=args, wait=wait)
    if tag == "o":
        # activate the designer MDI doc
        frame = app.frame
        mdi = None
        for h in drive.descendants(frame):
            if drive.cls(h) == "MDIClient" and drive.rect(h)[2] > 1000:
                mdi = h
        for h in drive.descendants(mdi):
            t = drive.title(h)
            if "\u7a97\u53e3" in t and "\u7a0b\u5e8f\u96c6" not in t:
                u.SendMessageW(mdi, 0x0222, h, 0)
                time.sleep(2)
    print("=== %s (%s)" % (tag, exe))
    for h in drive.descendants(app.frame):
        if drive.cls(h) != "WTWindow":
            continue
        st = u.GetWindowLongW(h, -16)
        ex = u.GetWindowLongW(h, -20)
        par = u.GetParent(h)
        wx, wy, ww, wh = drive.rect(h)
        cw, ch = drive.client(h)
        pt = drive.wt.POINT(0, 0)
        u.ClientToScreen(h, drive.ctypes.byref(pt))
        print("  hwnd=0x%08X class=%s" % (h, drive.cls(h)))
        print("    style=0x%08X ex=0x%08X" % (st & 0xFFFFFFFF, ex & 0xFFFFFFFF))
        print("    title=|%s| parent=%s(%s) grandparent=%s(%s)" % (
            drive.title(h), par, drive.cls(par),
            u.GetParent(par), drive.cls(u.GetParent(par)) if u.GetParent(par) else None))
        print("    window=%d,%d %dx%d  client=%dx%d  client0=(%d,%d) -> nc=(%d,%d,%d,%d)"
              % (wx, wy, ww, wh, cw, ch, pt.x, pt.y, pt.x - wx, pt.y - wy,
                 ww - cw - (pt.x - wx), wh - ch - (pt.y - wy)))
        print("    foreground=%s active=%s" % (u.GetForegroundWindow() == h,
                                              u.GetActiveWindow() == h))
    fg = u.GetForegroundWindow()
    print("  foreground window = 0x%08X %s |%s|" % (fg, drive.cls(fg), drive.title(fg)))
    app.close()


def main():
    sample = sys.argv[1] if len(sys.argv) > 1 else SAMPLE
    info("o", ORIG, [sample], 14)
    info("m", MINE, [sample, "designer"], 8)
    return 0


if __name__ == "__main__":
    sys.exit(main())
