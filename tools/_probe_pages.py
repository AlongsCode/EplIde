# -*- coding: utf-8 -*-
"""_probe_pages.py -- enumerate the original IDE's tree controls and tab pages,
so the 1:1 page sweep (#11) can be built on measured facts instead of guesses.

usage: python tools\\_probe_pages.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive  # noqa: E402

ORIG = r"C:\Program Files (x86)\e\e.exe"
SAMPLE = r"D:\EPL\samples\Basic Sample\Bubble Sort.e"

app = drive.App(ORIG, args=[SAMPLE], wait=20)
try:
    print("frame =", hex(app.frame))
    print()
    print("--- descendants: class | visible | rect | title ---")
    for h in drive.descendants(app.frame):
        try:
            c = drive.cls(h)
            r = drive.rect(h)
            t = drive.title(h)
            v = drive.u.IsWindowVisible(h)
        except Exception as e:
            continue
        low = c.lower()
        if any(k in low for k in ("systreeview", "syslistview", "systabcontrol",
                                  "systab", "toolbar", "msctls", "edit", "combobox")):
            print("%-22s vis=%-2s %-24s %s"
                  % (c[:22], v, "%d,%d %dx%d" % (r[0], r[1], r[2] - r[0], r[3] - r[1]),
                     t[:40]))
finally:
    try:
        app.close()
    except Exception:
        pass
