# -*- coding: utf-8 -*-
"""从 ECLONE_DUMP_MODEL 的模型里算出某个表格单元格的像素中心（给 cell_smoke.ps1 点击用）。

用法: python cellpos.py <model.txt> <列标题> <行首格文字> [scroll]
输出: "x y"（编辑器客户区坐标，y 已减去 scroll —— 不传 scroll 就是未滚动的坐标）；找不到退码 2。
⚠️ cell_smoke 必须先滚动再点：采样源码变长以后目标行可能落在可视区之外，
   而点一下会让编辑器 EnsureVisible 自动滚一下，第二次点的坐标就错位了（真踩过）。
模型行格式见 ec_editor.cpp DumpModel：
    TABLE 1 cols=6 kind=1 w=174,72,36,36,36,426,
    row   7 kind=0 table= 1 y= 100 h=21 indent=0 mark=2 src=12 | [名][类型][]*[] span=...
"""
import re
import sys

path, colhead, rowfirst = sys.argv[1], sys.argv[2], sys.argv[3]
scroll = int(sys.argv[4]) if len(sys.argv) > 4 else 0
txt = open(path, encoding="gbk", errors="replace").read()

tables = {}
for m in re.finditer(r"^TABLE (\d+) cols=(\d+) kind=(\d+) w=([\d,]*)$", txt, re.M):
    tables[int(m.group(1))] = [int(v) for v in m.group(4).rstrip(",").split(",") if v != ""]

rows = []
# ⚠️ 尾部字段会增（第 21 轮加了 bp=）—— 别把 src= 后面的东西写进模式，
#    只吃到 src=，剩下的原样放过，否则整个模型一行都解析不出来（cell_smoke 就是这么假失败的）。
for m in re.finditer(r"^row\s+(\d+) kind=(\d+) table=\s*(-?\d+) y=\s*(\d+) h=\s*(\d+) indent=(\d+) mark=(\d+) src=(-?\d+).*? \| (.*)$", txt, re.M):
    rows.append({
        "idx": int(m.group(1)), "kind": int(m.group(2)), "table": int(m.group(3)),
        "y": int(m.group(4)), "h": int(m.group(5)), "src": int(m.group(8)),
        "cells": re.findall(r"\[([^\]]*)\](\*?)", m.group(9)),
    })

# 目标行：第一个格子的文字 = rowfirst
target = None
for r in rows:
    if r["cells"] and r["cells"][0][0] == rowfirst:
        target = r
        break
if target is None:
    sys.exit(2)

# 该行的表头行 = 同一张表、格数相同的最近一条前面的行
head = None
for r in rows:
    if r["idx"] >= target["idx"]:
        break
    if r["kind"] == 0 and r["table"] == target["table"] and len(r["cells"]) == len(target["cells"]):
        head = r
if head is None:
    sys.exit(2)

col = -1
for i, (name, _chk) in enumerate(head["cells"]):
    if name.replace(" ", "") == colhead:
        col = i
        break
if col < 0:
    sys.exit(2)

# 列宽：模型里的 TABLE w= 是「按格号」的宽度表
w = tables.get(target["table"])
if not w:
    sys.exit(2)
x = 44
for i in range(col):
    x += w[i] if i < len(w) else 0
x += (w[col] if col < len(w) else 40) // 2
y = target["y"] + target["h"] // 2 - scroll
print("%d %d" % (x, y))
