# -*- coding: utf-8 -*-
"""List the RT_BITMAP names in a module (pure PE resource-directory parse, no LoadLibrary, so it
works on a 32-bit .fne from 64-bit Python).

Purpose: check that a support library's LIB_DATA_TYPE_INFO.m_nBmpID values (see
tools/... ECLONE_DUMP_DTICON) are RT_BITMAP ids inside that same .fne -- which is what the
original component box loads them from.

usage: python tools/pe_bmp_ids.py <file> [filter]
"""
import struct
import sys

RT_BITMAP = 2


def rva_to_off(secs, rva):
    for va, vs, raw, rsize in secs:
        if va <= rva < va + max(vs, rsize):
            return raw + (rva - va)
    return None


def main():
    path = sys.argv[1]
    filt = sys.argv[2] if len(sys.argv) > 2 else None
    d = open(path, "rb").read()
    e_lfanew = struct.unpack_from("<I", d, 0x3C)[0]
    assert d[e_lfanew:e_lfanew + 4] == b"PE\0\0", "not a PE"
    coff = e_lfanew + 4
    nsec, = struct.unpack_from("<H", d, coff + 2)
    optsz, = struct.unpack_from("<H", d, coff + 16)
    opt = coff + 20
    magic, = struct.unpack_from("<H", d, opt)
    pe32plus = (magic == 0x20B)
    ddir = opt + (112 if pe32plus else 96)
    rsrc_rva, rsrc_size = struct.unpack_from("<II", d, ddir + 8 * 2)
    secs = []
    sec0 = opt + optsz
    for i in range(nsec):
        o = sec0 + 40 * i
        vs, va, rsize, raw = struct.unpack_from("<IIII", d, o + 8)
        secs.append((va, vs, raw, rsize))
    print("%s: %d sections, rsrc rva=0x%X size=%d" % (path, nsec, rsrc_rva, rsrc_size))

    base = rva_to_off(secs, rsrc_rva)
    if base is None:
        print("no resource directory")
        return

    def entries(off):
        nnamed, nid = struct.unpack_from("<HH", d, off + 12)
        out = []
        for i in range(nnamed + nid):
            e = off + 16 + 8 * i
            nameid, dataoff = struct.unpack_from("<II", d, e)
            out.append((nameid, dataoff))
        return out

    names = []
    for nameid, sub in entries(base):
        if (nameid & 0x80000000) == 0 and nameid == RT_BITMAP:
            for nid, sub2 in entries(base + (sub & 0x7FFFFFFF)):
                nm = "%d" % (nid & 0x7FFFFFFF) if not (nid & 0x80000000) else "<named>"
                names.append(nm)
    names = sorted(set(names), key=lambda s: int(s) if s.isdigit() else 0)
    print("RT_BITMAP count=%d" % len(names))
    print("ids:", ", ".join(names[:80]))
    if filt:
        print("contains %s: %s" % (filt, filt in names))


main()
