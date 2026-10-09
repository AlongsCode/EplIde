# -*- coding: utf-8 -*-
"""把 libtest dump 出来的提示文本和原版（treeinfo.exe ... sel N 读出的状态栏文本）逐字节对比。

用法: python cmp_hints.py <mine_hints.txt> <orig_dir>
      orig_dir 里放 sel_<idx>.txt（treeinfo.exe 的输出）

对比结果：每行一个不同点；末尾给汇总。原版文本前面有 6 个空格的缩进和两边的 | 。
"""
import os
import re
import sys

INDICES = [int(x) for x in sys.argv[2:]] if len(sys.argv) > 2 else None


def load_mine(path):
    out = {}
    cur = None
    buf = []
    for line in open(path, encoding="gbk", errors="replace"):
        m = re.match(r"^===== (\d+) =====\s*$", line)
        if m:
            if cur is not None:
                out[cur] = "\n".join(buf).strip("\n")
            cur = int(m.group(1))
            buf = []
        elif cur is not None:
            buf.append(line.rstrip("\n"))
    if cur is not None:
        out[cur] = "\n".join(buf).strip("\n")
    return out


def load_orig(path):
    """treeinfo 的输出：状态栏那一段文本在被 | 包起来的一行里（可能跨行）。"""
    txt = open(path, encoding="gbk", errors="replace").read()
    i = txt.find("msctls_statusbar32")
    if i < 0:
        return None
    j = txt.find("|", i)
    if j < 0:
        return None
    # 从 | 后面开始，找到配对的结尾 |（treeinfo 打印的是 "      |text|"）
    k = txt.find("|", j + 1)
    if k < 0:
        return None
    return txt[j + 1:k]


def main():
    mine = load_mine(sys.argv[1])
    d = os.path.dirname(os.path.abspath(sys.argv[1]))
    idxs = INDICES or sorted(mine.keys())
    same = diff = miss = 0
    for i in idxs:
        f = os.path.join(d, "sel_%d.txt" % i)
        if not os.path.exists(f):
            miss += 1
            continue
        o = load_orig(f)
        m = mine.get(i)
        if o is None or m is None:
            miss += 1
            continue
        if o == m:
            same += 1
            print("OK   %4d" % i)
        else:
            diff += 1
            print("DIFF %4d  len orig=%d mine=%d" % (i, len(o), len(m)))
            n = min(len(o), len(m))
            for p in range(n):
                if o[p] != m[p]:
                    print("      first diff at %d:" % p)
                    print("      orig ...%s..." % o[max(0, p - 30):p + 30].replace("\n", "\\n"))
                    print("      mine ...%s..." % m[max(0, p - 30):p + 30].replace("\n", "\\n"))
                    break
            else:
                print("      orig tail: %r" % o[n:n + 60])
                print("      mine tail: %r" % m[n:n + 60])
    print("\nsame=%d diff=%d missing=%d" % (same, diff, miss))


if __name__ == "__main__":
    main()
