# -*- coding: utf-8 -*-
"""Can the caret be placed inside a TABLE row of the code editor?

Before the round-27 fix, CECEditor::CaretFromPoint() special-cased table rows and jumped the
caret to the subprogram's first statement, so clicking a .子程序/.参数/程序集 header row could
never put the caret there ("光标都选不中表格中的地方").

This probe clicks each table row in turn (PostMessage path, same as tools/sel_smoke.ps1) and
reads back the caret line from the .fold.txt dump, then asserts it equals that row's src line.

usage: python tools/caret_table_check.py
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MINE = os.path.join(ROOT, "bin", "EClone.exe")
SAMPLE = os.path.join(ROOT, "shots", "e_samples", "flow_ref.e")
OUT = os.path.join(ROOT, "shots")
DUMP = os.path.join(OUT, "model_carettable.txt")
WM_REDUMP = 0x466


def read(path):
    if not os.path.exists(path):
        return ""
    with open(path, "rb") as f:
        return f.read().decode("gbk", "replace")


def model_rows(path):
    """[(row_index, kind, table, y, h, src)] from the model dump"""
    out = []
    for l in read(path).splitlines():
        m = re.match(r"row\s+(\d+) kind=(\d+) table=\s*(-?\d+) y=\s*(\d+) h=\s*(\d+) "
                     r"indent=(\d+) mark=(-?\d+) src=(\d+)", l)
        if m:
            g = [int(x) for x in m.groups()]
            out.append((g[0], g[1], g[2], g[3], g[4], g[7]))
    return out


def caret(foldpath):
    t = read(foldpath)
    m = re.search(r"anchor=(-?\d+),(-?\d+) caret=(-?\d+),(-?\d+)", t)
    if not m:
        return None
    return int(m.group(3)), int(m.group(4))


def main():
    env = {"ECLONE_DUMP_MODEL": DUMP}
    app = drive.App(MINE, args=["\"" + SAMPLE + "\""], wait=8, env=env)
    fold = DUMP[:-4] + ".fold.txt"
    try:
        ed = None
        for h in drive.descendants(app.frame):
            if drive.cls(h).startswith("Afx:400000:8:10005"):
                ed = h
        if ed is None:
            print("[FAIL] no editor window")
            return 1
        print("editor = 0x%X" % ed)

        rows = model_rows(DUMP)
        tbl = [r for r in rows if r[1] == 0 and r[4] > 0]
        if not tbl:
            print("[FAIL] no table rows in the model")
            return 1

        def click(x, y):
            lp = (x & 0xFFFF) | (y << 16)
            drive.u.PostMessageW(ed, 0x0200, 0, lp)
            drive.u.PostMessageW(ed, 0x0201, 1, lp)
            time.sleep(0.12)
            drive.u.PostMessageW(ed, 0x0202, 0, lp)
            time.sleep(0.45)

        # walk up the visible rows: for each table row, click inside its first cell and see
        # whether the caret line ends up on that row's src line
        ok = 0
        bad = []
        tried = 0
        for (idx, kind, table, y, h, src) in tbl[:6]:
            cy = y + h // 2 - 0          # model y already includes no scroll (scroll=0 at start)
            # first cell spans EC_TEXT_X .. +width; click a little way into it
            click(60, cy)
            drive.u.SendMessageW(ed, WM_REDUMP, 0, 0)
            time.sleep(0.5)
            c = caret(fold)
            tried += 1
            hit = (c is not None and c[0] == src)
            print("  table row %2d (src=%2d) click at y=%3d -> caret line %s  %s"
                  % (idx, src, cy, c[0] if c else "?", "OK" if hit else "MISS (row %d expected)" % src))
            if hit:
                ok += 1
            else:
                bad.append((idx, src, c))
        print("[%s] caret lands on the clicked table row (%d/%d)"
              % ("PASS" if ok == tried else "FAIL", ok, tried))
        return 0 if ok == tried else 1
    finally:
        app.close()


if __name__ == "__main__":
    sys.exit(main())
