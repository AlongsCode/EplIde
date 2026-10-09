# One-off: swap EClone.cpp's g_szSampleCode for the exact program visible in the
# user's reference screenshot (精易模块.e / 集_组件), so the editor's table geometry
# can be compared 1:1 against the original's pixel measurements.
# Backs up the original block to shots/zoom/sample_backup.txt.
import io

P = r'D:\eclone\src\EClone.cpp'
d = open(P, 'rb').read().decode('utf-8')
i = d.index('static const TCHAR* g_szSampleCode =')
j = d.index(';\n', i) + 2
if not __import__('os').path.exists(r'D:\eclone\shots\zoom\sample_backup.txt'):
    open(r'D:\eclone\shots\zoom\sample_backup.txt', 'w', encoding='utf-8').write(d[i:j])

lines = [
    '.版本 2', '',
    '.程序集 集_组件', '',
    '.子程序 外部超级列表框_保存数据_NEW, 逻辑型, 公开, BUG命令已弃用,如果要读入的话,直接 超级列表框.表项=读入文件(C:\\001.ebb) 即可',
    '.参数 超级列表框句柄, 整数型, , , , 超级列表框句柄',
    '.参数 要保存的位置, 文本型, , , , 如：C:\\001.ebb 即可',
    '.参数 列表类型, 整数型, , 可空, , 默认为“报表列表框” 0=报表列表框 1=大图标列表框',
    '.参数 不保存指定列的索引, 整数型, , 可空, , 输入不保存指定列的索引，如:我不保存第1列,直接输入0,因为索引是多0开始的',
    '.参数 超级列表框类名, 文本型, , 可空, , 留空默认为 SysListView32',
    '.局部变量 局_列数, 整数型',
    '.局部变量 局_表项数, 整数型',
    '.局部变量 局_字节集, 字节集, , , 超级列表框列数',
    '.局部变量 局_表项计次, 整数型',
    '.局部变量 局_列计次, 整数型',
    '.局部变量 局_文件号, 整数型',
    '.局部变量 外部超级列表框, 外部超级列表框',
    '.局部变量 局_文本, 文本型',
    '.局部变量 局_当前行计次, 整数型',
    '.局部变量 局_表项数值, 整数型', '',
    '如果真 (是否为空 (超级列表框类名))',
    '    超级列表框类名 ＝ “SysListView32”',
    '如果真结束',
    '返回 (假)',
]

out = 'static const TCHAR* g_szSampleCode =\n'
for k, l in enumerate(lines):
    s = l.replace('\\', '\\\\').replace('"', '\\"')
    if k < len(lines) - 1:
        s += '\\r\\n'
    out += '    _T("%s")%s\n' % (s, ';' if k == len(lines) - 1 else '')

open(P, 'wb').write((d[:i] + out + d[j:]).encode('utf-8'))
print('patched, lines=%d' % len(lines))
