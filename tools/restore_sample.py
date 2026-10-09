# -*- coding: utf-8 -*-
"""Restore EClone.cpp's g_szSampleCode from shots\\zoom\\sample_backup.txt.

Boundaries come from tools\\sampleblock.py's C tokenizer, so this is idempotent and works no
matter what patch_sample_*.py left behind.  A naive "first ';\\n' after the marker" search is
wrong (the sample contains `"";` inside string literals).

⚠️ ENCODING (round 41): src\\EClone.cpp is GBK.  This script used to read/write it as UTF-8,
which turned every CJK comment into mojibake and made the next build fail -- while the old
bin\\EClone.exe stayed on disk, so screenshots silently came from a stale binary.  Both the
target file and the backup are now decoded with sampleblock.load_text() and written back in
the encoding that decode reported.

⚠️ The backup must hold exactly one clean `static const TCHAR* g_szSampleCode =` line; if it
does not, the backup itself is corrupt and this script refuses to write (a silent bad restore
is what mangled EClone.cpp during round 22).

usage: python tools\\restore_sample.py
"""
import io
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sampleblock import bounds, load_text  # noqa: E402

P = r'C:\Users\Along\Desktop\eclone\src\EClone.cpp'
BAK = r'C:\Users\Along\Desktop\eclone\shots\zoom\sample_backup.txt'
MARK = 'static const TCHAR* g_szSampleCode ='

b, bak_enc = load_text(BAK)                  # the backup is UTF-8 in this repo
n_clean = sum(1 for l in b.splitlines() if l.strip() == MARK)
if n_clean != 1:
    raise SystemExit('backup is corrupt: %d clean declaration lines (expected 1) -- '
                     'rebuild it with tools\\_repair_sample_region.py' % n_clean)

d, enc = load_text(P)
i, j, d = bounds(P, text=d)                  # same decode as `enc`
res = d[:i] + b + d[j:]
if sum(1 for l in res.splitlines() if l.strip() == MARK) != 1:
    raise SystemExit('restore self-check FAILED')
io.open(P, 'w', encoding=enc, newline='').write(res)
print('restored %d chars (encoding=%s, backup encoding=%s)'
      % (len(b), enc, bak_enc))
