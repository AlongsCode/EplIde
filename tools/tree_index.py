# -*- coding: utf-8 -*-
"""Find a node's pre-order index inside one of the original IDE's trees.

usage: python tools/tree_index.py <trees.txt> <root text> <node text>
output: "<treeIdx> <itemIdx>"  (both as printed by treeinfo.exe; exit 2 if missing)

The item index is the `[%4d]` counter of treeinfo's dump, which is the same pre-order
walk that `treeinfo.exe <exe> sel <tree> <item>` uses -- so it can be fed straight in.
"""
import re
import sys

path, root_want, node_want = sys.argv[1], sys.argv[2], sys.argv[3]
txt = open(path, encoding="gbk", errors="replace").read()

blocks = re.findall(r"=== tree (\d+) hwnd=\S+ .*? items=(\d+) ===\n(.*?)(?==== tree |\Z)", txt, re.S)
for tidx, _items, body in blocks:
    lines = [l for l in body.split("\n") if l.strip()]
    if not lines:
        continue
    root = lines[0].split("|")[-2] if lines[0].count("|") >= 2 else ""
    if root != root_want:
        continue
    for l in lines:
        m = re.match(r"\s*\[\s*(\d+)\]", l)
        if not m:
            continue
        name = l.split("|")[-2] if l.count("|") >= 2 else ""
        if name == node_want:
            print("%s %s" % (tidx, m.group(1)))
            sys.exit(0)
sys.exit(2)
