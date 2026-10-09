# -*- coding: utf-8 -*-
"""Swap EClone.cpp's g_szSampleCode for the EXACT content of graphic.e as the original renders it
(shots\\live_tabs\\view0.png, 1174x388), so the code page can be diffed 1:1 against the original
with tools\\diff_png.py.  Undo with tools\\restore_sample.py.

Block boundaries come from tools\\sampleblock.py (a C tokenizer).  A naive
"first ';\\n' after the marker" search is wrong: sample lines end with `"";` inside string
literals, so it either raises ValueError or truncates the block.

usage: python tools\\patch_sample_graphic.py
"""
import io
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sampleblock import bounds  # noqa: E402

P = r'C:\Users\Along\Desktop\eclone\src\EClone.cpp'
BAK = r'C:\Users\Along\Desktop\eclone\shots\zoom\sample_backup.txt'
MARK = 'static const TCHAR* g_szSampleCode ='

LOCK = r'C:\Users\Along\Desktop\eclone\shots\zoom\.sample_patch.lock'

# ---- guard: never run while a leftover half-written file is in place -------------------------
if not os.path.exists(LOCK):
    d0 = io.open(P, encoding='utf-8').read()
    if sum(1 for l in d0.splitlines() if l.strip() == MARK) != 1:
        raise SystemExit('refusing to patch: EClone.cpp does not have exactly one clean '
                         'declaration line -- run tools\\_repair_sample_region.py first')
    io.open(LOCK, 'w').write('busy')

try:
    d = io.open(P, encoding='utf-8').read()
    i, j = bounds(P)
    if not os.path.exists(BAK):
        io.open(BAK, 'w', encoding='utf-8').write(d[i:j])

    lines = [
        '.版本 2', '',
        '.程序集 启动窗口程序集', '',
        '.子程序 _图形按钮1_被单击',
        '    信息框（“易语言中国人之编程！”, 0, ）',
    ]

    out = MARK + '\n'
    for k, l in enumerate(lines):
        s = l.replace('\\', '\\\\').replace('"', '\\"')
        if k < len(lines) - 1:
            s += '\\r\\n'
        out += '    _T("%s")%s\n' % (s, ';' if k == len(lines) - 1 else '')

    res = d[:i] + out + d[j:]
    n_clean = sum(1 for l in res.splitlines() if l.strip() == MARK)
    if n_clean != 1:
        raise SystemExit('patch self-check FAILED: %d clean declaration lines (expected 1)' % n_clean)
    io.open(P, 'w', encoding='utf-8').write(res)
    print('patched (graphic.e), lines=%d' % len(lines))
finally:
    if os.path.exists(LOCK):
        os.remove(LOCK)
