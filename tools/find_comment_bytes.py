# -*- coding: utf-8 -*-
"""find_comment_bytes.py -- locate a comment statement's bytes inside a .e file.

Question it answers: does the blank line the ORIGINAL renders before a standalone
comment (`' 演示"如果"命令`) live in the file, or is it IDE-side layout?

usage: python -B tools/find_comment_bytes.py <file.e> <gbk text>
"""
import io
import sys


def main():
    path, text = sys.argv[1], sys.argv[2]
    d = open(path, 'rb').read()
    pat = text.encode('gbk')
    out = []
    i = d.find(pat)
    while i >= 0:
        out.append('hit %06X  (len=%d)' % (i, len(pat)))
        start = max(0, i - 48) & ~15
        for off in range(start, i + len(pat) + 32, 16):
            row = d[off:off + 16]
            out.append('  %06X  %s  %s' % (
                off, ' '.join('%02X' % c for c in row),
                row.decode('gbk', 'replace').replace('\x00', '.')))
        i = d.find(pat, i + 1)
    io.open('temp/comment_bytes.txt', 'w', encoding='utf-8').write(
        '\n'.join(out) if out else 'no hit for %r' % text)
    print('hits=%d -> temp/comment_bytes.txt' % (len(out) // 4))


if __name__ == '__main__':
    main()
