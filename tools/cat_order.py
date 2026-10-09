# -*- coding: utf-8 -*-
"""Compare the ORIGINAL support-library tree's category order (from an authoritative dump) with
the order this build produces (from the library's own category string table).

usage: python tools/cat_order.py
"""
import io
import re

DUMP = r"D:\eclone\shots\live_page2\trees.txt"
LINE = re.compile(r"^(\s*)\[\s*(\d+)\]\s+img=(\d+)\s+sel=\d+\s+st=0x[0-9A-Fa-f]+\s+ch=(\d)\s+\|(.*?)\|")

# the order this build walks the library's own category table (observed from the screenshot and
# from ec_lib.cpp's `for (cat = 1; cat <= L.nCategory; cat++)`)
MINE = ["流程控制", "算术运算", "逻辑比较", "位运算", "变量操作", "数组操作", "环境存取",
        "拼音处理", "文本操作", "字节集操作", "数值转换", "时间操作", "磁盘操作", "文件读写",
        "系统处理", "媒体播放", "程序调试", "其他", "数据库", "网络通信", "控制台操作",
        "易包", "数据类型", "常量"]


def main():
    t = io.open(DUMP, encoding="gbk", errors="replace").read()
    k = t.find("系统核心支持库")
    s = t.rfind("=== tree", 0, k)
    e = t.find("=== tree", k)
    blk = t[s:e if e > 0 else len(t)]
    orig = []
    for line in blk.split("\n"):
        m = LINE.match(line)
        if m and len(m.group(1)) == 2:          # depth 1 = the categories
            orig.append((int(m.group(3)), m.group(5)))
    print("original category order (depth 1), with its img=:")
    for i, (img, txt) in enumerate(orig):
        print("  %2d img=%-3d %s" % (i, img, txt))

    print("\nthis build's order:")
    for i, txt in enumerate(MINE):
        print("  %2d %s" % (i, txt))

    o = [x[1] for x in orig]
    print("\n--- diff (by name set) ---")
    print("only in original:", [x for x in o if x not in MINE])
    print("only in this build:", [x for x in MINE if x not in o])

    print("\n--- position differences (same name, different index) ---")
    for name in o:
        if name in MINE:
            a, b = o.index(name), MINE.index(name)
            if a != b:
                print("  %-10s original %2d  this build %2d" % (name, a, b))


main()
