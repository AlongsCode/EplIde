# -*- coding: utf-8 -*-
"""Dump the top-level control bars (class + title + rect) of a running app, to compare the
window skeleton with the original.

usage: python tools/bars_probe.py <pid>   |   --exe <path> [args...] --wait N
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive


def bars(app, tag):
    print("--- %s frame=%s %s" % (tag, app.frame, drive.rect(app.frame)))
    seen = set()
    for h in drive.descendants(app.frame):
        c = drive.cls(h)
        if c != "AfxControlBar42s" and not c.startswith("ToolbarWindow32"):
            continue
        key = (c, drive.title(h))
        r = drive.rect(h)
        print("  %-22s %-16s vis=%-5s x=%-5d y=%-5d %4dx%-4d %s"
              % (c, drive.title(h), bool(drive.u.IsWindowVisible(h)), r[0], r[1], r[2], r[3],
                 "(dup)" if key in seen else ""))
        seen.add(key)


def main():
    if "--exe" in sys.argv:
        i = sys.argv.index("--exe")
        exe = sys.argv[i + 1]
        rest = sys.argv[i + 2:]
        wait = 18 if "e.exe" in exe else 6
        args = [a for a in rest if not a.startswith("--")]
        app = drive.App(exe, args=args, wait=wait)
        bars(app, "app")
        app.shot(app.frame, os.path.join(r"D:\eclone\shots", "bars_" +
                                         ("orig" if "e.exe" in exe else "mine") + ".png"))
        app.close()
    else:
        pid = int(sys.argv[1])
        tops = drive.tops(pid)
        frame = None
        for t in tops:
            if drive.cls(t) in ("ENewFrame", "ECMainFrame"):
                frame = t
        print("pid=%d frame=%s" % (pid, frame))
        ht = drive.EnumProc  # noqa
        for h in drive.descendants(frame):
            c = drive.cls(h)
            if c == "AfxControlBar42s" or c.startswith("ToolbarWindow32"):
                print("  %-22s %-16s vis=%-5s %s" % (c, drive.title(h),
                                                     bool(drive.u.IsWindowVisible(h)), drive.rect(h)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
