# -*- coding: utf-8 -*-
"""把 treeinfo.exe 的倒档解析成「目标节点在默认（仅根展开）状态下的可见行号」。

用法: python tree_row.py <trees.txt> <节点文字>
输出: 行号（0 = 根节点那一行，1 = 第一个直接子节点 …）；找不到就退出码 2。

原理：IDE 打开时的树只展开了根，所以可见行 = 根的直接子节点按顺序排（0 行是根自己）。
treeinfo 会把整棵树展开再倒档，但**行序不变**，所以只要数出「目标文字在根的直接
子节点里排第几」即可。节点文字用 `|名字|` 的最后一段取，别写复杂正则。
"""
import re
import sys

path, want = sys.argv[1], sys.argv[2]
txt = open(path, encoding="gbk", errors="replace").read()

blocks = re.findall(r"=== tree \d+ hwnd=\S+ .*? items=\d+ ===\n(.*?)(?==== tree |\Z)", txt, re.S)
for body in blocks:
    lines = [l for l in body.split("\n") if l.strip()]
    if not lines or "程序数据" not in lines[0]:
        continue
    idx = 0
    for l in lines[1:]:
        if not re.match(r"^  \[", l):          # 直接子节点 = 缩进正好 2 个空格
            continue
        name = l.split("|")[-2] if l.count("|") >= 2 else ""
        if name == want:
            print(idx + 1)                     # +1：第 0 行是根节点
            sys.exit(0)
        idx += 1
sys.exit(2)
