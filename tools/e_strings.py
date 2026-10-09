# -*- coding: utf-8 -*-
"""把 .e 里的 GBK 文本串抠出来（按连续可读段），用来看源码里到底有没有块尾关键字。

用法: python e_strings.py <file.e> [最少字符数]
"""
import re
import sys

path = sys.argv[1]
minlen = int(sys.argv[2]) if len(sys.argv) > 2 else 4
data = open(path, "rb").read()

# 连续的可读字节（GBK 双字节 + ASCII 可见字符），按段切开
runs = []
cur = bytearray()
i = 0
while i < len(data):
    b = data[i]
    if 0x20 <= b < 0x7F or b in (0x0D, 0x0A, 0x09):
        cur.append(b); i += 1
    elif 0x81 <= b <= 0xFE and i + 1 < len(data) and 0x40 <= data[i + 1] <= 0xFE:
        cur.append(b); cur.append(data[i + 1]); i += 2
    else:
        if len(cur) >= minlen:
            runs.append(bytes(cur))
        cur = bytearray(); i += 1
if len(cur) >= minlen:
    runs.append(bytes(cur))

for r in runs:
    s = r.decode("gbk", errors="replace").replace("\r", "").replace("\n", " / ")
    print(s)
