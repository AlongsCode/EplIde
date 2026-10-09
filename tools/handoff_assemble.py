# -*- coding: utf-8 -*-
"""Assemble the HANDOFF recovery: take the head fragment (correct structure, lines 1..627) and
append the rest of the union-recovered content that is not already present, merging overlapping
regions line by line.

usage: python tools/handoff_assemble.py
"""
import io
import os

HEAD = r"D:\eclone\shots\handoff_best.md"      # lines 1..627, correct structure
UNION = r"D:\eclone\shots\handoff_base_backup.md"   # union of many reads (may mix line numbers)
OUT = r"D:\eclone\HANDOFF.md"

head = io.open(HEAD, encoding="utf-8", errors="replace").read().split("\n")
union = io.open(UNION, encoding="utf-8", errors="replace").read().split("\n")

# 1) where does the union's content cease to overlap the head?  find the longest head suffix that
#    appears in the union, then append everything after it.
tail_probe = head[-20:]
joined_union = "\n".join(union)
pos = joined_union.find(tail_probe[-1])          # last line of the head
print("head lines=%d union lines=%d  last-head-line found in union at %s"
      % (len(head), len(union), pos))
out = list(head)
seen = set(head)
for ln in union:
    s = ln.rstrip()
    if not s.strip():
        continue
    if s in seen:
        continue
    out.append(s)
    seen.add(s)
io.open(OUT, "w", encoding="utf-8", newline="").write("\n".join(out))
print("assembled %d lines -> %s" % (len(out), OUT))
