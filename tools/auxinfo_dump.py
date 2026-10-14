# -*- coding: utf-8 -*-
"""auxinfo_dump.py -- dump the AuxInfo1 segment (0x0A007319) of a .e file.

Layout (SEGMENT_SPEC.md, 16-byte records, verified against Program method ids):
    [u32 formId][u32 组件ID][i32 事件槽][u32 子程序ID]

Why it matters: it is the ground truth for "which component id owns which event
handler", so a component-name decoder for the Resource segment can be validated
against a known answer (flowcmd.e: the only handler is `_按钮1_被单击`).

usage: python -B tools/auxinfo_dump.py <.e path>
"""
import io
import struct
import sys

MAGIC = 0x15117319
ESEG_AUXINFO1 = 0x0A007319


def u32(b, o):
    return struct.unpack_from('<I', b, o)[0]


def walk(data):
    pos, out = 8, []
    while pos + 100 <= len(data):
        if u32(data, pos) != MAGIC:
            break
        key, size = u32(data, pos + 8), u32(data, pos + 56)
        out.append((key, pos + 100, size))
        pos += 100 + size
        if key == 0x07007319:
            break
    return out


def main():
    data = open(sys.argv[1], 'rb').read()
    lines = []
    for key, off, size in walk(data):
        if key != ESEG_AUXINFO1:
            continue
        lines.append('auxinfo1 size=%d records=%d' % (size, size // 16))
        for i in range(size // 16):
            p = off + 16 * i
            lines.append('  rec%-3d form=%08X  comp=%08X  slot=%-4d method=%08X'
                         % (i, u32(data, p), u32(data, p + 4),
                            struct.unpack_from('<i', data, p + 8)[0],
                            u32(data, p + 12)))
    io.open('temp/auxinfo.txt', 'w', encoding='utf-8').write(
        '\n'.join(lines) if lines else 'no auxinfo1 segment')
    print('-> temp/auxinfo.txt')


if __name__ == '__main__':
    main()
