# -*- coding: utf-8 -*-
"""Search the original e.exe GBK string dump for candidate strings.

usage: python tools\gbk_search.py <out.txt> <word> [<word> ...]
"""
import io
import sys

src = io.open(
    r"D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\e_exe_gbk.txt",
    encoding="gbk", errors="replace").read()
lines = src.splitlines()
out = []
for w in sys.argv[2:]:
    w = w.encode("utf-8").decode("unicode_escape")
    hits = [(i + 1, ln) for i, ln in enumerate(lines) if w in ln]
    out.append("== %s  (%d hits)" % (w, len(hits)))
    for n, ln in hits[:10]:
        out.append("   L%-7d %s" % (n, ln[:110]))
txt = "\n".join(out)
io.open(sys.argv[1], "w", encoding="utf-8").write(txt)
print("wrote " + sys.argv[1])
