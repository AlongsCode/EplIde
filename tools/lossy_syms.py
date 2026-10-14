# -*- coding: utf-8 -*-
"""lossy_syms.py -- dump the "Optional / lossy" segment (0x05007319) object index
table (id -> name) from a real .e file.

Why: e.exe's code renderer (sub_4C1150) resolves every element reference
(method / variable / component / const ...) through ONE global {id -> element*}
map.  The per-file copy of that map lives in the lossy segment field 2
(SEGMENT_SPEC.md 5.0/5.2).  eclone currently builds its symbol table only from
the program segment, so component ids (family 0x1601) fall through to
`var_%08X`.  This tool prints what the file really carries.

usage: python -B tools/lossy_syms.py <.e path>
"""
import struct
import sys

MAGIC = 0x15117319
ESEG_LOSSY = 0x05007319


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


def parse_lossy(p):
    i = 0
    L = u32(p, i); i += 4
    path = p[i:i + L]; i += L
    c1 = u32(p, i); l1 = u32(p, i + 4); i += 8
    blob = p[i:i + l1]
    res = []
    if len(blob) >= 12 * c1:
        A = struct.unpack_from('<%dI' % c1, blob, 0)
        B = struct.unpack_from('<%dI' % c1, blob, 4 * c1)
        C = struct.unpack_from('<%dI' % c1, blob, 8 * c1)
        pool = blob[12 * c1:]
        for k in range(c1):
            off = C[k]
            end = pool.find(b'\x00', off)
            if end < 0:
                end = len(pool)
            res.append((A[k], off, pool[off:end]))
    return path, c1, l1, res


def main():
    path = sys.argv[1]
    data = open(path, 'rb').read()
    segs = walk(data)
    print('segments: ' + ', '.join('%08X(%d)' % (k, s) for k, _, s in segs))
    for key, off, size in segs:
        if key != ESEG_LOSSY:
            continue
        p = data[off:off + size]
        outpath, c1, l1, rows = parse_lossy(p)
        print('lossy size=%d  L=%d  c1=%d  l1=%d  path=%s'
              % (size, len(outpath), c1, l1, outpath.decode('gbk', 'replace')))
        for aid, coff, nm in rows:
            print('  %08X  fam=%04X  off=%-5d %s'
                  % (aid, aid >> 16, coff, nm.decode('gbk', 'replace')))


if __name__ == '__main__':
    main()
