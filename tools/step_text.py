# -*- coding: utf-8 -*-
"""step_text.py -- dump BOTH editors' document text (Ctrl+A/Ctrl+C) after each
of the cmp_shots input steps, so the Enter/Backspace behaviour can be compared
line by line instead of guessed from pixels.

usage: python tools/step_text.py
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cmp_shots as C          # noqa: E402
import drive                   # noqa: E402
import text_oracle as T        # noqa: E402
from drive import u            # noqa: E402


def run(role, exe, sample, log):
    app = drive.App(exe, args=[sample], wait=3.0)
    out = {}
    try:
        app.frame = T.wait_frame(app.pid, timeout=150) or app.frame
        time.sleep(4.0)
        T.dismiss_dialogs(app.pid, log)
        time.sleep(1.5)
        page = T.page_window(app)
        u.SetWindowPos(page, None, 0, 0, C.PAGE_W, C.PAGE_H,
                       0x0004 | 0x0010 | 0x0040)
        time.sleep(2.0)
        view = T.editor_view(page)
        app.focus_root()
        C.pin_top(app, view)

        def dump(tag):
            t = C.copy_all(app, view)
            out[tag] = (t or u"").replace("\r\n", "\n")
            log.append("%s %s: %d chars" % (role, tag, len(out[tag])))

        app.click(view, 260, 200)
        dump("click")
        app.key(C.VK_SPACE)
        dump("space")
        app.key(C.VK_RETURN)
        dump("enter")
        app.key(C.VK_BACK)
        dump("back")
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
    for tag in ("click", "space", "enter", "back"):
        a = o.get(tag, u"").split("\n")
        b = m.get(tag, u"").split("\n")
        # find the first differing line index
        i = 0
        while i < min(len(a), len(b)) and a[i] == b[i]:
            i += 1
        print("== %-6s orig %d lines, mine %d lines, first diff at line %d"
              % (tag, len(a), len(b), i))
        for k in range(max(0, i - 1), min(len(a), i + 3)):
            print("   O %3d %r" % (k, a[k][:60]))
        for k in range(max(0, i - 1), min(len(b), i + 3)):
            print("   M %3d %r" % (k, b[k][:60]))


if __name__ == "__main__":
    main()
