# -*- coding: utf-8 -*-
"""Print the hwnd of the original-IDE tree whose ROOT text is <name>.

usage: python tools/tree_pick.py <trees.txt> 程序数据   ->  006001F0
Exits 2 when not found. See tools/tree_row.py for the dump format.
"""
import re
import sys

path, want = sys.argv[1], sys.argv[2]
txt = open(path, encoding="gbk", errors="replace").read()

for m in re.finditer(r"=== tree \d+ hwnd=(\S+) .*? items=\d+ ===\n(.*?)(?==== tree |\Z)", txt, re.S):
    lines = [l for l in m.group(2).split("\n") if l.strip()]
    if not lines:
        continue
    root = lines[0].split("|")[-2] if lines[0].count("|") >= 2 else ""
    if root == want:
        print(m.group(1))
        sys.exit(0)
sys.exit(2)
