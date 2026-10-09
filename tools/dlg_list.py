# -*- coding: utf-8 -*-
"""List one dialog's controls (id / class / x,y / text) from shots\\e_dialogs.txt.

usage: python tools\\dlg_list.py <id> [out.txt]
"""
import io
import re
import sys

did = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else None
raw = io.open(r"D:\eclone\shots\e_dialogs.txt", encoding="utf-8").read()
i = raw.find("DIALOG id=%s " % did)
if i < 0:
    i = raw.find("DIALOGEX id=%s " % did)
if i < 0:
    print("not found")
    sys.exit(1)
j = raw.find("\nDIALOG", i + 10)
blk = raw[i:j if j > 0 else len(raw)]
lines = []
for m in re.finditer(
        r"\[(\d+)\] id=(-?\d+)\s+(\S+)\s+(-?\d+),(-?\d+)\s+(\d+)x(\d+).*?text='(.*?)'\r?$",
        blk, re.S | re.M):
    lines.append("%2s id=%-6s %-16s %4s,%-4s %sx%s  |%s|" % (
        m.group(1), m.group(2), m.group(3), m.group(4), m.group(5),
        m.group(6), m.group(7), m.group(8)))
txt = "\n".join(lines)
if out:
    io.open(out, "w", encoding="utf-8").write(txt)
    print("wrote %s (%d controls)" % (out, len(lines)))
else:
    print(txt)
