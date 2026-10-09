# Snapshot the CURRENT g_szSampleCode block of src\EClone.cpp into
# shots\zoom\sample_backup.txt — run this after deliberately enriching the fixture, so a
# later tools\restore_sample.py restores YOUR version instead of the old one.
import shutil

P = r'D:\eclone\src\EClone.cpp'
B = r'D:\eclone\shots\zoom\sample_backup.txt'
d = open(P, 'rb').read().decode('utf-8')
i = d.index('static const TCHAR* g_szSampleCode =')
j = d.index(';\n', i) + 2
open(B, 'wb').write(d[i:j].encode('utf-8'))
print('backup refreshed: %d bytes -> %s' % (j - i, B))
