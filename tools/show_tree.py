# -*- coding: utf-8 -*-
"""打印 probe_live.ps1 倒出的窗口树（GBK），只留控件类名/可见性/坐标，避免控制台编码问题。
用法: python tools\\show_tree.py <tree.txt>
"""
import sys

p = sys.argv[1]
d = open(p, 'rb').read().decode('gbk', errors='replace')
for ln in d.split('\n'):
    t = ln.rstrip()
    if not t.strip():
        continue
    # 只保留 ASCII 部分（类名/vis/坐标/style），中文标题丢掉
    out = ''.join(ch if 32 <= ord(ch) < 127 else '.' for ch in t)
    print(out[:130])
