# -*- coding: utf-8 -*-
"""对比原版 IDE 与 EClone 的窗口类层级（控件级一致性证据）。

输入：两份由 shot.ps1 / ide_probe*.ps1 生成的窗口树 dump。
"""
import re
import sys
from collections import Counter

CLASS_RE = re.compile(r"^\s*([A-Za-z_#][A-Za-z0-9_:.#]*)\s")
# 归一化：MFC 运行时窗口类带实例哈希，统一成通配
def norm(c):
    c = re.sub(r"^Afx:[0-9a-f]+:", "Afx:", c)
    c = re.sub(r":1000[0-9a-f]+:", ":...:", c)
    return c


def classes(path):
    out = Counter()
    for line in open(path, encoding="utf-8", errors="replace"):
        if line.startswith("#") or not line.strip():
            continue
        m = CLASS_RE.match(line)
        if not m:
            continue
        c = norm(m.group(1))
        if c in ("vis", "top-level"):
            continue
        out[c] += 1
    return out


def main(a, b, la="原版", lb="EClone"):
    ca, cb = classes(a), classes(b)
    keys = sorted(set(ca) | set(cb))
    print("%-30s %8s %8s" % ("窗口类", la, lb))
    print("-" * 50)
    for k in keys:
        print("%-30s %8d %8d" % (k, ca.get(k, 0), cb.get(k, 0)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
