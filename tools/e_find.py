# -*- coding: utf-8 -*-
"""在 .e 二进制里按 GBK 找一个关键词，打印它周围的字节（解码成 GBK）——用来确认源码里
到底有没有「如果真结束」这种块尾关键字，从而判断原版是**不显示**还是**没有**。

用法: python e_find.py <file.e> <关键词> [关键词2 ...]
"""
import sys

path = sys.argv[1]
data = open(path, "rb").read()

for kw in sys.argv[2:]:
    pat = kw.encode("gbk")
    hits = []
    i = data.find(pat)
    while i >= 0 and len(hits) < 8:
        ctx = data[max(0, i - 40):i + 60]
        hits.append((i, ctx.decode("gbk", errors="replace").replace("\r", "\\r").replace("\n", "\\n")))
        i = data.find(pat, i + 1)
    print("== %s : %d hit(s)" % (kw, len(hits)))
    for off, ctx in hits:
        print("   @%#x  ...%s..." % (off, ctx))
