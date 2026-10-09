# -*- coding: utf-8 -*-
"""量「参数归属线」那一行的字符列位置，用来对齐原版 / 本工程。

用法: python tools\\param_cols.py <png> <y0> <y1> [x0] [x1]
打印每一列是否有墨迹（把所有颜色压成一个字符），再给出各字符块的起止 x。
"""
import sys
from PIL import Image

path = sys.argv[1]
y0 = int(sys.argv[2])
y1 = int(sys.argv[3])
x0 = int(sys.argv[4]) if len(sys.argv) > 4 else 60
x1 = int(sys.argv[5]) if len(sys.argv) > 5 else 330

im = Image.open(path).convert('RGB')
px = im.load()

cols = []
for x in range(x0, x1):
    n = 0
    for y in range(y0, y1):
        if px[x, y] != (255, 255, 255):
            n += 1
    cols.append((x, n))

# group consecutive columns that have ink
runs = []
cur = None
for x, n in cols:
    if n > 0:
        if cur is None:
            cur = [x, x]
        else:
            cur[1] = x
    else:
        if cur is not None:
            runs.append(tuple(cur))
            cur = None
if cur is not None:
    runs.append(tuple(cur))

print('%s y=%d..%d x=%d..%d' % (path, y0, y1, x0, x1))
print('  runs: ' + ' '.join('%d-%d' % r for r in runs))
# 每 12px 一格的落点（汉字步进 12）
print('  grid(12) from 99: ' + ' '.join(str(99 + 12 * k) for k in range(0, 16)))
