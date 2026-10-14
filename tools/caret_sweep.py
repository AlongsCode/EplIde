# -*- coding: utf-8 -*-
"""caret_sweep.py -- click a row at many x and read back the caret x on both apps.

Derives the table-row caret x mapping empirically instead of guessing. Row y=86
is the subprogram DATA row (_button1_), y=65 is the HEADER row.

usage: python tools/caret_sweep.py
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import caret_geom as CG          # noqa: E402
import drive                     # noqa: E402
import text_oracle as T          # noqa: E402
from drive import u              # noqa: E402

XS = (46, 50, 54, 58, 62, 66, 72, 80, 92)
ROWS = [("data(y86)", 86), ("head(y65)", 65)]


def run(role, exe, sample, log):
    app = drive.App(exe, args=[sample], wait=3.0)
    out = {}
    try:
        app.frame = T.wait_frame(app.pid, timeout=150) or app.frame
        time.sleep(4.0)
        T.dismiss_dialogs(app.pid, log)
        time.sleep(1.5)
        page = T.page_window(app)
        u.SetWindowPos(page, None, 0, 0, CG.PAGE_W, CG.PAGE_H,
                       0x0004 | 0x0010 | 0x0040)
        time.sleep(2.0)
        view = T.editor_view(page)
        app.focus_root()
        CG.pin_top(app, view)
        for label, cy in ROWS:
            for cx in XS:
                app.click(view, cx, cy)
                time.sleep(0.55)
                cr, _ = CG.read_caret(app.pid, view)
                out[(label, cx)] = cr
        return out
    finally:
        app.close()
        time.sleep(1.0)


def main():
    log = []
    o = run("orig", T.ORIG, T.SAMPLE, log)
    m = run("mine", T.MINE, T.SAMPLE, log)
    for line in log:
        print(line)
    print("%-10s %5s | %-18s | %-18s" % ("row", "click", "orig caret", "mine caret"))
    for label, cy in ROWS:
        for cx in XS:
            co = o.get((label, cx))
            cm = m.get((label, cx))
            print("%-10s %5d | %-18s | %-18s"
                  % (label, cx, str(co), str(cm)))


if __name__ == "__main__":
    main()
