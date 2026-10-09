# -*- coding: utf-8 -*-
"""在 <e>\\lib\\*\\samples\\*.e 里找含指定流程块的样本（.e 是 CNWTEPRG 二进制，串按 UTF-16LE 存）。

用法: python tools\\find_block_samples.py [关键词...]
默认找 计次循环首 / 判断循环首 / 循环判断首 / 变量循环首 / 判断 / 否则
输出每种关键词命中的样本（按命中数排序，只列前几个）。
"""
import glob
import os
import sys

ROOT = r'C:\Program Files (x86)\e'
KW = sys.argv[1:] or ['计次循环首', '判断循环首', '循环判断首', '变量循环首', '判断', '否则', '默认']

files = glob.glob(os.path.join(ROOT, 'lib', '*', 'samples', '*.e'))
files += glob.glob(os.path.join(ROOT, 'samples', '**', '*.e'), )
files += glob.glob(os.path.join(ROOT, 'samples', '*.e'))

for kw in KW:
    b = kw.encode('utf-16-le')
    hits = []
    for f in files:
        try:
            d = open(f, 'rb').read()
        except Exception:
            continue
        n = d.count(b)
        if n:
            hits.append((n, f))
    hits.sort(reverse=True, key=lambda t: t[0])
    print('== %s : %d file(s)' % (kw, len(hits)))
    for n, f in hits[:5]:
        print('   %3d  %s' % (n, f))
