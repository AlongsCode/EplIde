# -*- coding: utf-8 -*-
"""检查 utf8togbk 转出来的 GBK 源码里块注释是否配平。

背景（第 16 轮真踩过）：src 里某行是 `// 注释：全角标点...`，中文标点按 GBK 编码后
字节序列里含块注释的结束符。它本身在行注释里无害，但会把**文件后面**某个
`/* ... */` 块注释提前关掉 —— 于是块注释之后第一条语句被吃进注释里，编译照样通过、
运行时行为却不对（现象：参数归属线一行都不画，查了很久）。

判据：把文件当字符流扫一遍，要按 C 的规则先跳过字符串/字符字面量与行注释，
再要求 `/*` 与 `*/` 一一配对、不能出现孤立的 `*/`。
只对**转换后的 gbk 目录**做检查，因为危险字节是转换后才产生的。

用法: python tools\\check_block_comments.py <dir-or-file> ...
退出码 1 = 有文件不配平。
"""
import glob
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def check(path):
    with open(path, 'rb') as f:
        d = f.read()
    depth = 0
    i = 0
    n = len(d)
    in_line_cmt = False
    in_str = False
    in_chr = False
    while i < n:
        c = d[i]
        nxt = d[i + 1] if i + 1 < n else 0
        if in_line_cmt:
            if c == 0x0A:
                in_line_cmt = False
            i += 1
            continue
        if in_str or in_chr:
            if c == 0x5C:                       # backslash: skip the escaped char
                i += 2
                continue
            if in_str and c == 0x22:
                in_str = False
            elif in_chr and c == 0x27:
                in_chr = False
            i += 1
            continue
        if c == 0x2F and nxt == 0x2F:           # //
            in_line_cmt = True
            i += 2
            continue
        if c == 0x2F and nxt == 0x2A:           # /*
            depth += 1
            i += 2
            continue
        if c == 0x2A and nxt == 0x2F:           # */
            if depth == 0:
                return 'stray block-comment terminator at byte %d' % i
            depth -= 1
            i += 2
            continue
        if c == 0x22:
            in_str = True
        elif c == 0x27:
            in_chr = True
        i += 1
    if depth != 0:
        return 'unclosed /* (depth=%d at EOF)' % depth
    return None


def main():
    targets = sys.argv[1:]
    if not targets:
        targets = [os.path.join(ROOT, 'gbk')]
    files = []
    for t in targets:
        if os.path.isdir(t):
            files += sorted(glob.glob(os.path.join(t, '*.cpp')) +
                            glob.glob(os.path.join(t, '*.h')))
        else:
            files.append(t)
    bad = 0
    for p in files:
        if os.path.basename(p).startswith(('sizecbar', 'scbar', 'rg')):
            continue
        try:
            err = check(p)
        except Exception as e:
            err = 'read error: %s' % e
        if err:
            sys.stdout.write('%s : %s\n' % (p, err))
            bad += 1
    if bad:
        sys.stdout.write('== %d file(s) with unbalanced block comments\n' % bad)
        return 1
    sys.stdout.write('== block comments balanced in %d file(s)\n' % len(files))
    return 0


if __name__ == '__main__':
    sys.exit(main())
