# -*- coding: utf-8 -*-
"""Print the ORIGINAL support-library tree's category rows with BOTH numbers, so the real
index rule for the tree image list can be read off:

    dump img=   the image index e.exe actually used
    prefix      the 4-digit icon-number prefix in krnln's category string (tools\\libprobe.exe)

usage: python tools/tree_img_rule.py
"""
import io
import re

DUMP = r"D:\eclone\shots\live_page2\trees.txt"
LINE = re.compile(r"^(\s*)\[\s*(\d+)\]\s+img=(\d+)\s+sel=\d+\s+st=0x[0-9A-Fa-f]+\s+ch=(\d)\s+\|(.*?)\|")

# krnln category string-table order with its 4-digit prefix, from
#   tools\libprobe.exe "C:\Program Files (x86)\e\lib\krnln.fne"   (-- categories --)
PREFIX = {
    "流程控制": 1, "算术运算": 14, "逻辑比较": 5, "位运算": 15, "变量操作": 2,
    "数组操作": 0, "环境存取": 0, "拼音处理": 0, "文本操作": 0, "字节集操作": 0,
    "数值转换": 0, "时间操作": 0, "磁盘操作": 0, "文件读写": 0, "系统处理": 0,
    "媒体播放": 0, "程序调试": 0, "其他": 0, "数据库": 21, "网络通信": 0,
    "控制台操作": 0, "易包": 0, "数据类型": 24, "常量": 25,
}


def main():
    t = io.open(DUMP, encoding="gbk", errors="replace").read()
    k = t.find("系统核心支持库")
    s = t.rfind("=== tree", 0, k)
    e = t.find("=== tree", k)
    blk = t[s:e if e > 0 else len(t)]

    rows = []
    for line in blk.split("\n"):
        m = LINE.match(line)
        if m and len(m.group(1)) == 2:
            rows.append((int(m.group(3)), m.group(5)))

    print("%-3s %-8s %-8s %s" % ("#", "dumpImg", "prefix", "name"))
    for i, (img, name) in enumerate(rows):
        print("%-3d %-8d %-8s %s" % (i, img, PREFIX.get(name, "?"), name))

    print("\n--- is dumpImg == prefix? ---")
    print("differ:", [(n, im, PREFIX.get(n)) for im, n in rows if PREFIX.get(n) not in (None, im)])

    print("\n--- order check ---")
    print("dump order  :", [n for _, n in rows])
    pref = sorted(PREFIX.items(), key=lambda kv: kv[1])
    print("prefix-asc  :", [n for n, _ in pref])


main()
