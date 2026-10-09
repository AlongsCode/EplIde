# -*- coding: utf-8 -*-
"""Restore EClone.cpp's g_szSampleCode from shots\\zoom\\sample_backup.txt.

Boundaries come from tools\\sampleblock.py's C tokenizer, so this is idempotent and works no matter
what patch_sample_*.py left behind.  A naive "first ';\\n' after the marker" search is wrong (the
sample contains `"";` inside string literals).

⚠️ The backup must hold exactly one clean `static const TCHAR* g_szSampleCode =` line; if it does
not, the backup itself is corrupt and this script refuses to write (a silent bad restore is what
mangled EClone.cpp during round 22).

usage: python tools\\restore_sample.py
"""
import io
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sampleblock import bounds  # noqa: E402

p = r'C:\Users\Along\Desktop\eclone\src\EClone.cpp'
b = io.open(r'C:\Users\Along\Desktop\eclone\shots\zoom\sample_backup.txt', encoding='utf-8').read()
MARK = 'static const TCHAR* g_szSampleCode ='
n_clean = sum(1 for l in b.splitlines() if l.strip() == MARK)
if n_clean != 1:
    raise SystemExit('backup is corrupt: %d clean declaration lines (expected 1) -- '
                     'rebuild it with tools\\_repair_sample_region.py' % n_clean)

d = io.open(p, encoding='utf-8').read()
i, j = bounds(p)
res = d[:i] + b + d[j:]
if sum(1 for l in res.splitlines() if l.strip() == MARK) != 1:
    raise SystemExit('restore self-check FAILED')
io.open(p, 'w', encoding='utf-8').write(res)
print('restored %d bytes' % len(b.encode('utf-8')))
