# -*- coding: utf-8 -*-
"""Full icon census of the ORIGINAL support-library tree (shots\\live_page2\\trees.txt):
for each depth-1 category, list the distinct img= numbers its children use and one example name.

This gives the COMPLETE mapping "EClone node -> original image number" in one place, which is what
the tree icons have to match.

usage: python tools/icon_census.py
"""
import io
import re
from collections import OrderedDict

DUMP = r"D:\eclone\shots\live_page2\trees.txt"
LINE = re.compile(r"^(\s*)\[\s*(\d+)\]\s+img=(\d+)\s+sel=\d+\s+st=0x[0-9A-Fa-f]+\s+ch=(\d)\s+\|(.*?)\|")


def main():
    t = io.open(DUMP, encoding="gbk", errors="replace").read()
    k = t.find("系统核心支持库")
    s = t.rfind("=== tree", 0, k)
    e = t.find("=== tree", k)
    blk = t[s:e if e > 0 else len(t)]

    rows = []
    for ln in blk.split("\n"):
        m = LINE.match(ln)
        if m:
            rows.append((len(m.group(1)), int(m.group(3)), m.group(4), m.group(5)))

    cat = "(root)"
    print("per-category distinct child icons:")
    for d, img, ch, name in rows:
        if d == 2:
            cat = name
            print("\n%-12s catImg=%d" % (cat, img))
        elif d == 4 and ch == "1":
            print("             child img=%d  e.g. %s" % (img, name))

    print("\n\n=== all distinct (depth, img) pairs with an example ===")
    seen = OrderedDict()
    for d, img, ch, name in rows:
        key = (d, img)
        if key not in seen:
            seen[key] = (name, ch)
    for (d, img), (name, ch) in seen.items():
        print("   depth=%d img=%-3d ch=%s  e.g. %s" % (d, img, ch, name))


main()
