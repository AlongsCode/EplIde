# One-off: swap EClone.cpp's g_szSampleCode for a SHORT program whose code area (with the
# nested 计次循环/如果真 blocks) sits at the very top of the document, so the flow lines can be
# captured 1:1 without scrolling (WM_VSCROLL / VK_NEXT sent by the harness have no effect on
# this app's self-drawn editor).
# Undo with tools\restore_sample.py (the backup is refreshed by tools\save_sample_backup.py).
import os

P = r'D:\eclone\src\EClone.cpp'
d = open(P, 'rb').read().decode('utf-8')
i = d.index('static const TCHAR* g_szSampleCode =')
j = d.index(';\n', i) + 2
if not os.path.exists(r'D:\eclone\shots\zoom\sample_backup.txt'):
    open(r'D:\eclone\shots\zoom\sample_backup.txt', 'w', encoding='utf-8').write(d[i:j])

lines = [
    '.版本 2', '',
    '.程序集 启动窗口程序集', '',
    '.子程序 _图形按钮1_被单击',
    '    信息框（“易语言中国人之编程！”, 0 ）',
    '    .计次循环首 (10, i)',
    '        如果真 (i ＞ 5)',
    '            跳出循环 ()',
    '        如果真结束',
    '    .计次循环尾 ()',
    '    返回 ()',
]

out = 'static const TCHAR* g_szSampleCode =\n'
for k, l in enumerate(lines):
    s = l.replace('\\', '\\\\').replace('"', '\\"')
    if k < len(lines) - 1:
        s += '\\r\\n'
    out += '    _T("%s")%s\n' % (s, ';' if k == len(lines) - 1 else '')

open(P, 'wb').write((d[:i] + out + d[j:]).encode('utf-8'))
print('patched (flow fixture), lines=%d' % len(lines))
