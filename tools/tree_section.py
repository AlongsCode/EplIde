# -*- coding: utf-8 -*-
"""从 treeinfo.exe 的整档（所有 SysTreeView32）里挑出某一棵树并打印。

用法: python tree_section.py shots\\live_jy\\trees.txt 程序数据
"""
import re
import sys

path = sys.argv[1]
needle = sys.argv[2] if len(sys.argv) > 2 else None
txt = open(path, encoding="gbk", errors="replace").read()
blocks = re.findall(r"=== tree \d+ hwnd=\S+ \"[^\"]*\" (\d+x\d+) items=(\d+) ===\n(.*?)(?==== tree |\Z)",
                    txt, re.S)
for size, n, body in blocks:
    if needle and needle not in body:
        continue
    print("=== %s items=%s ===" % (size, n))
    print(body.rstrip())
    print()
