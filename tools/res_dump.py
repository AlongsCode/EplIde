# -*- coding: utf-8 -*-
"""res_dump.py -- dump the Resource segment (0x04007319) window records of a .e file.

Layout (SEGMENT_SPEC.md 13.2):
  [u32 win_area_len = 8*n]
  n * { [u32 form_id][u32 val] }
  per record: [u32 hash][u32 tag][mfc_string name][mfc_string comment]
              [u32 part_count][u32 part_data_len][part_data bytes] 00*12

Goal: find where component (id, name) pairs live, so the code renderer can print
`画板1.滚动写行` instead of `var_16010002`.

usage: python -B tools/res_dump.py <.e path> [--hex]
"""
import struct
import sys

MAGIC = 0x15117319
ESEG_RESOURCE = 0x04007319


def u32(b, o):
    return struct.unpack_from('<I', b, o)[0]


def walk(data):
    pos = 8
    out = []
    while pos + 100 <= len(data):
        if u32(data, pos) != MAGIC:
            break
        key = u32(data, pos + 8)
        size = u32(data, pos + 56)
        out.append((key, pos + 100, size))
        pos += 100 + size
        if key == 0x07007319:
            break
    return out


def mfcstr(p, i):
    n = u32(p, i)
    return p[i + 4:i + 4 + n], i + 4 + n


def annotate(blob, base=0):
    """Print a hex+ascii walk that also labels GBK runs and u32 values."""
    i = 0
    while i < len(blob):
        chunk = blob[i:i + 16]
        hx = ' '.join('%02X' % c for c in chunk)
        asc = ''
        for c in chunk:
            asc += chr(c) if 32 <= c < 127 else '.'
        print('  %04X  %-47s  %s' % (base + i, hx, asc))
        i += 16


def main():
    path = sys.argv[1]
    show_hex = '--hex' in sys.argv
    data = open(path, 'rb').read()
    for key, off, size in walk(data):
        if key != ESEG_RESOURCE:
            continue
        p = data[off:off + size]
        A = u32(p, 0)
        n = A // 8
        print('resource size=%d  win_area_len=%d  forms=%d' % (size, A, n))
        for f in range(n):
            print('  form[%d] id=%08X val=%08X'
                  % (f, u32(p, 4 + 8 * f), u32(p, 8 + 8 * f)))
        i = 4 + A
        rno = 0
        while i + 8 <= len(p):
            h = u32(p, i); tag = u32(p, i + 4)
            if h == 0 and tag == 0:
                break
            nm, i2 = mfcstr(p, i + 8)
            cm, i3 = mfcstr(p, i2)
            pc = u32(p, i3); pl = u32(p, i3 + 4)
            body = p[i3 + 8:i3 + 8 + pl]
            print('rec[%d] off=%d hash=%08X tag=%08X name=%r comment=%r part_count=%d part_len=%d'
                  % (rno, i, h, tag, nm.decode('gbk', 'replace'),
                     cm.decode('gbk', 'replace'), pc, pl))
            if show_hex:
                annotate(body)
            i = i3 + 8 + pl + 12
            rno += 1
        print('records done at %d / size %d' % (i, size))


if __name__ == '__main__':
    main()
