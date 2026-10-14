# -*- coding: utf-8 -*-
"""cmp_shots.py -- multi-step, same-geometry screenshot comparison
(original e.exe  vs  EClone), one *real* mouse/keyboard action per step.

Every step produces:
    shots/cmp/<NN>_<name>_orig.png     the original's code view
    shots/cmp/<NN>_<name>_mine.png     EClone's code view
    shots/cmp/<NN>_<name>_diff.png     magenta = differing pixels
    shots/cmp/<NN>_<name>_sbs.png      side-by-side (orig | mine)
and a printed table of per-step differing-pixel counts.

usage: python tools/cmp_shots.py [--only orig|mine] [--steps 1,2,3]
"""
import argparse
import io
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive
import text_oracle as T
from drive import u

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "shots", "cmp")

PAGE_W, PAGE_H = 1400, 780
VK_A, VK_RETURN, VK_BACK, VK_SPACE = 0x41, 0x0D, 0x08, 0x20


def write_clipboard_text(text):
    k32 = T.k32
    k32.GlobalAlloc.restype = T.ctypes.c_void_p
    k32.GlobalAlloc.argtypes = [T.ctypes.c_uint, T.ctypes.c_size_t]
    for _ in range(20):
        if u.OpenClipboard(None):
            break
        time.sleep(0.1)
    else:
        return False
    try:
        u.EmptyClipboard()
        data = text.encode('utf-16-le') + b'\x00\x00'
        h = k32.GlobalAlloc(0x0042, len(data))
        p = k32.GlobalLock(T.ctypes.c_void_p(h))
        T.ctypes.memmove(p, data, len(data))
        k32.GlobalUnlock(T.ctypes.c_void_p(h))
        if not u.SetClipboardData(13, T.ctypes.c_void_p(h)):
            return False
        return True
    finally:
        u.CloseClipboard()


def copy_all(app, view, retries=3):
    """Ctrl+A/Ctrl+C with a verified marker so stale clipboard data cannot fool us."""
    for _ in range(retries):
        if not write_clipboard_text(u"ECLONE_MARKER_STALE"):
            time.sleep(0.4)
            continue
        w, hh = drive.client(view)
        app.click(view, max(40, w // 3), max(80, hh // 3))
        time.sleep(0.5)
        app.key(0x41, ctrl=True)
        time.sleep(0.25)
        app.key(0x43, ctrl=True)
        time.sleep(0.9)
        uni, ansi = T.read_clipboard_text()
        txt = uni or ansi
        if txt and txt != u"ECLONE_MARKER_STALE":
            return txt
        time.sleep(0.6)
    return None


def sbs(a_path, b_path, out_path):
    from PIL import Image
    a = Image.open(a_path).convert("RGB")
    b = Image.open(b_path).convert("RGB")
    W = min(a.size[0], b.size[0])
    H = min(a.size[1], b.size[1])
    a, b = a.crop((0, 0, W, H)), b.crop((0, 0, W, H))
    out = Image.new("RGB", (W * 2 + 8, H), (255, 0, 0))
    out.paste(a, (0, 0))
    out.paste(b, (W + 8, 0))
    out.save(out_path)


def pin_top(app, view):
    """Scroll the code view to the very top of the document.

    The original's Ctrl+Home stops at the current element, so the wheel is the
    only mechanism that pins both apps to the same first line.
    """
    w, hh = drive.client(view)
    app.click(view, max(200, w // 3), max(120, hh // 3))
    time.sleep(0.4)
    for _ in range(45):
        u.mouse_event(0x0800, 0, 0, 120, 0)      # MOUSEEVENTF_WHEEL up
        time.sleep(0.02)
    time.sleep(1.2)


def run(role, exe, sample, steps, log):
    app = drive.App(exe, args=[sample], wait=3.0)
    shots = {}
    try:
        app.frame = T.wait_frame(app.pid, timeout=150) or app.frame
        time.sleep(4.0)
        T.dismiss_dialogs(app.pid, log)
        time.sleep(1.5)
        page = T.page_window(app)
        if page is None:
            log.append("%s: no code page" % role)
            return {}
        u.SetWindowPos(page, None, 0, 0, PAGE_W, PAGE_H,
                       0x0004 | 0x0010 | 0x0040)
        time.sleep(2.0)
        view = T.editor_view(page)
        if view is None:
            log.append("%s: no editor view" % role)
            return {}
        w, hh = drive.client(view)
        log.append("%s: page=0x%08X view=0x%08X client=%dx%d"
                   % (role, page, view, w, hh))

        def snap(tag):
            p = os.path.join(OUT, "%s_%s.png" % (tag, role))
            T.capture_client(view, p)
            shots[tag] = p

        app.focus_root()
        pin_top(app, view)
        if 1 in steps:
            snap("01_top")
        if 2 in steps:
            app.click(view, 260, 200)            # click a code line
            snap("02_click")
        if 3 in steps:
            app.key(VK_SPACE)                    # type a space (visible ink)
            snap("03_type_space")
        if 4 in steps:
            app.key(VK_RETURN)                   # Enter -> new line + flow line
            snap("04_enter")
        if 5 in steps:
            app.key(VK_BACK)                     # Backspace
            snap("05_backspace")
        if 6 in steps:
            app.click(view, 200, 120)            # click the flow-line gutter
            snap("06_gutter")
        if 7 in steps:
            pin_top(app, view)
            txt = copy_all(app, view)
            snap("07_ctrl_a")
            if txt:
                with io.open(os.path.join(OUT, "%s_sel.txt" % role), "w",
                             encoding="utf-8", newline="\n") as f:
                    f.write(txt.replace("\r\n", "\n"))
                log.append("%s: copied %d chars" % (role, len(txt)))
            else:
                log.append("%s: clipboard copy FAILED" % role)
        return shots
    finally:
        app.close()
        time.sleep(1.0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", default=T.SAMPLE)
    ap.add_argument("--steps", default="1,2,3,4,5,6,7")
    args = ap.parse_args()
    steps = set(int(s) for s in args.steps.split(",") if s)
    os.makedirs(OUT, exist_ok=True)
    log = []

    o = run("orig", T.ORIG, args.sample, steps, log)
    m = run("mine", T.MINE, args.sample, steps, log)
    for line in log:
        print(line)

    print("%-18s %8s %8s %7s" % ("step", "diff", "total", "percent"))
    for tag in sorted(set(o) & set(m)):
        name = tag.split("_", 1)[1]
        sbs(o[tag], m[tag], os.path.join(OUT, "%s_sbs.png" % tag))
        tot, npix, worst = T.pixel_diff(
            o[tag], m[tag], os.path.join(OUT, "%s_diff.png" % tag))
        print("%-18s %8d %8d %6.2f%%  worst=%s"
              % (name, tot, npix, 100.0 * tot / npix, worst[:3]))


if __name__ == "__main__":
    main()
