# -*- coding: utf-8 -*-
"""检查 src\\*.cpp/.h 里的中文注释有没有会"关掉块注释"的危险字符。

背景（真踩过）：utf8togbk.ps1 把 UTF-8 转 GBK 时，某些全角标点（如「」）转出来的
字节序列里含 `*/`。如果这行是 `// 注释`，`*/` 本身无害；但只要文件后面还有
`/* ... */` 块注释，这个 `*/` 就会把那个块注释**提前关掉**，于是块注释后面第一条
真正的语句被吃进注释里，编译照样通过、行为却不对。

用法: python tools\\check_comments.py
退出码 1 = 发现危险行。
"""
import glob
import io
import os
import re
import sys

ROOT = r'D:\eclone'
BAD = re.compile(rb'\*/')


def gbk_bytes_of_comment(line):
    """把一行按 GBK 编码（转不了的字符跳过），用来找转换后才出现的 */ 。"""
    try:
        return line.encode('gbk', errors='replace')
    except Exception:
        return b''


def main():
    bad = 0
    for path in sorted(glob.glob(os.path.join(ROOT, 'src', '*.cpp')) +
                       glob.glob(os.path.join(ROOT, 'src', '*.h')) +
                       glob.glob(os.path.join(ROOT, 'tools', '*.cpp'))):
        if os.path.basename(path).startswith('sizecbar') or os.path.basename(path).startswith('scbar'):
            continue
        with io.open(path, 'r', encoding='utf-8', errors='replace') as f:
            for n, line in enumerate(f, 1):
                s = line.rstrip('\n')
                if not (s.lstrip().startswith('//') or s.lstrip().startswith('/*') or
                        s.lstrip().startswith('*')):
                    continue
                g = gbk_bytes_of_comment(s)
                # 行注释里出现 */ 是安全的；块注释或续行里的才危险
                if BAD.search(g):
                    print('%s:%d  DANGEROUS(GBK has */): %s' % (path, n, s.strip()[:90]))
                    bad += 1
    if bad:
        print('== %d dangerous comment line(s)' % bad)
        return 1
    print('== no dangerous comment lines')
    return 0


if __name__ == '__main__':
    sys.exit(main())
