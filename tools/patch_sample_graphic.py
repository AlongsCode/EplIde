# -*- coding: utf-8 -*-
"""Patch EClone.cpp's g_szSampleCode with the exact content of graphic.e, in whatever
encoding the file is actually stored in.

WHY THIS FILE WAS REWRITTEN (round 41): the old code did
    io.open(P, encoding='utf-8').read() ... io.open(P, 'w', encoding='utf-8').write(...)
but <root>\\src\\EClone.cpp is **GBK**, not UTF-8.  So:
  * the read produced garbage, and
  * the write re-encoded that garbage as UTF-8,
leaving a mixed-encoding file.  cl.exe then failed -- and because the previous
bin\\EClone.exe was still on disk, every later screenshot silently came from a
STALE exe.  That is what made a whole round of "the fix did nothing" look real.
Now: detect the encoding, patch, and write back in the SAME encoding.

usage: python tools\\patch_sample_graphic.py
"""
import io
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sampleblock import bounds, load_text  # noqa: E402

P = r'C:\Users\Along\Desktop\eclone\src\EClone.cpp'
BAK = r'C:\Users\Along\Desktop\eclone\shots\zoom\sample_backup.txt'
MARK = 'static const TCHAR* g_szSampleCode ='
LOCK = r'C:\Users\Along\Desktop\eclone\shots\zoom\.sample_patch.lock'


def load():
    """Return (text, encoding) for P, via sampleblock's detector (single implementation).

    ⚠️ Do NOT reintroduce an ad-hoc "try utf-8 first" read here.  A GBK file's double-byte
    pairs are usually also legal UTF-8 codepoints, so `raw.decode('utf-8')` succeeds on GBK
    text; round 41 corrupted this file exactly that way and then diffed a stale exe for a
    whole cycle.  sampleblock.load_text() decides with a GBK well-formedness scan instead.
    """
    return load_text(P)


def save(text, enc):
    io.open(P, 'w', encoding=enc, newline='').write(text)


if not os.path.exists(LOCK):
    d0, _ = load()
    if sum(1 for l in d0.splitlines() if l.strip() == MARK) != 1:
        raise SystemExit('refusing to patch: EClone.cpp does not have exactly one clean '
                         'declaration line -- run tools\\_repair_sample_region.py first')
    io.open(LOCK, 'w').write('busy')
    del d0

try:
    d, enc = load()
    i, j, d = bounds(P, text=d)          # text MUST be the same decode as `enc`
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
    # ⚠️ 末尾这个 '\n' 必须去掉：bounds() 的 j 停在 ';' 那一行的**行尾换行符之前**，
    #    所以替换块自己不能再带一个行尾换行 —— 每跑一轮 patch/restore 都会多一行空行，
    #    跑几轮之后 EClone.cpp 里就堆出一片空行（git diff 里能看到 19 行新增空行）。
    out = out.rstrip('\n')

    res = d[:i] + out + d[j:]
    n_clean = sum(1 for l in res.splitlines() if l.strip() == MARK)
    if n_clean != 1:
        raise SystemExit('patch self-check FAILED: %d clean declaration lines (expected 1)'
                         % n_clean)
    save(res, enc)
    print('patched (graphic.e), encoding=%s, lines=%d' % (enc, len(lines)))
finally:
    if os.path.exists(LOCK):
        os.remove(LOCK)
