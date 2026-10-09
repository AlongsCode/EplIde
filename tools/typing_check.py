# -*- coding: utf-8 -*-
"""Does the code editor accept REAL keyboard input (not synthetic WM_CHAR)?

Reuses tools/drive.py, which brings the frame to the foreground and clicks with the real
mouse cursor (the click itself gives the editor focus, so no SetForegroundWindow games).

Sequence:
  1) start EClone on a sample .e with ECLONE_DUMP_MODEL set
  2) real-mouse click inside a code row  -> caret lands there
  3) read the dumped source lines, then type an ASCII char with keybd_event
  4) force a redump (EC_WM_REDUMP = WM_USER+102 = 0x466) and diff the source lines

usage: python tools/typing_check.py [--mine]
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
DUMP = os.path.join(OUT, "model_typing.txt")
WM_REDUMP = 0x466


def src_block(path):
    """Return the RAW bytes of the SRC section (no line-splitting!).

    splitlines() and friends strip the trailing \\r of each line, which would hide a
    character typed at end-of-line -- that bit me once, so compare raw bytes instead.
    """
    if not os.path.exists(path):
        return b""
    with open(path, "rb") as f:
        d = f.read()
    i = d.find(b"\nSRC ")
    if i < 0:
        return b""
    return d[i:]


def fold_line(path):
    p = path[:-4] + ".fold.txt"
    if not os.path.exists(p):
        return "(no fold dump)"
    with open(p, "rb") as f:
        txt = f.read().decode("gbk", "replace")
    for l in txt.splitlines():
        if l.startswith("scroll="):
            return l
    return "(no scroll line)"


def rows(path):
    out = []
    if not os.path.exists(path):
        return out
    with open(path, "rb") as f:
        txt = f.read().decode("gbk", "replace")
    for l in txt.splitlines():
        m = re.match(r"row\s+(\d+) kind=(\d+).*?y=\s*(\d+) h=\s*(\d+)", l)
        if m:
            out.append(tuple(int(g) for g in m.groups()))
    return out


def main():
    os.makedirs(OUT, exist_ok=True)
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
        print("editor hwnd = 0x%X" % ed)

        # first code row from the model
        rws = [r for r in rows(DUMP) if r[1] == 1]
        if not rws:
            print("[FAIL] no code row in model")
            return 1
        _, _, y, h = rws[0]
        cy = y + h // 2
        print("clicking into editor client (150, %d)" % cy)
        app.click(ed, 150, cy)          # real mouse -> editor gets focus + caret
        time.sleep(0.5)

        before = src_block(DUMP)
        print("before: SRC block = %d bytes; %s" % (len(before), fold_line(DUMP)))

        # type 'a' then 'b' with the real keyboard
        app.key(0x41)                    # VK 'A' (no ctrl/shift) -> WM_CHAR 'a'
        time.sleep(0.6)
        app.key(0x42)                    # VK 'B'
        time.sleep(0.6)

        drive.u.SendMessageW(ed, WM_REDUMP, 0, 0)
        time.sleep(0.7)
        after = src_block(DUMP)
        print("after : SRC block = %d bytes" % len(after))

        changed = after != before
        print("[%s] real keyboard types into the editor" % ("PASS" if changed else "FAIL"))
        if not changed:
            print("     (source byte-identical -> the editor is NOT accepting real keystrokes)")
            print("     first 200 bytes: " + repr(before[:200]))
        else:
            # show the first differing line (raw, keep the \r)
            bl = before.split(b"\n")
            al = after.split(b"\n")
            for i in range(max(len(bl), len(al))):
                x = bl[i] if i < len(bl) else b"<none>"
                y = al[i] if i < len(al) else b"<none>"
                if x != y:
                    print("     SRC line %d:" % i)
                    print("       - " + repr(x))
                    print("       + " + repr(y))
                    break
        return 0 if changed else 1
    finally:
        app.close()


if __name__ == "__main__":
    sys.exit(main())
