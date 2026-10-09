# -*- coding: utf-8 -*-
"""Dump RT_MANIFEST (resource type 24) from a PE file -- pure parser, no LoadLibrary.

usage: python tools/dump_manifest.py <exe> [more.exe ...]

Why this exists: whether an app gets the "Common Controls 6.0" look is decided by an RT_MANIFEST
resource, NOT by any code, so the original e.exe's manifest is the only authority for "does the
original IDE use the v6 controls".  (The system-config "stub" page's `是否使用Windows通用组件库
6.0版` checkbox is for the program e.exe *compiles*, not for e.exe itself.)

Implementation note: the LoadLibraryEx + EnumResourceNamesW route hung (ctypes callback +
64-bit HMODULE marshalling), so this walks the PE resource directory directly.
"""
import struct
import sys


def rva_to_off(sections, rva):
    for va, vsz, raw, rsz in sections:
        if va <= rva < va + max(vsz, rsz):
            return raw + (rva - va)
    return None


def walk(path):
    data = open(path, "rb").read()
    if data[:2] != b"MZ":
        print("not a PE")
        return
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        print("no PE signature")
        return
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    ddir = opt + (96 if magic == 0x10B else 112)
    res_rva, res_sz = struct.unpack_from("<II", data, ddir + 2 * 8)   # entry 2 = resource

    sec = pe + 24 + optsz
    sections = []
    for i in range(nsec):
        s = sec + i * 40
        vsz, va, rsz, raw = struct.unpack_from("<IIII", data, s + 8)
        sections.append((va, vsz, raw, rsz))

    base = rva_to_off(sections, res_rva)
    if base is None:
        print("no resource directory")
        return
    print("== %s   (resource dir %d sections)" % (path, len(sections)))

    def entries(off):
        nnamed, nid = struct.unpack_from("<HH", data, off + 12)
        out = []
        p = off + 16
        for _ in range(nnamed + nid):
            name, offs = struct.unpack_from("<II", data, p)
            out.append((name, offs))
            p += 8
        return out

    found = False
    for name, offs in entries(base):
        if not (offs & 0x80000000):
            continue
        type_off = base + (offs & 0x7FFFFFFF)
        # the type entry's Name field: 24 == RT_MANIFEST
        if name != 24:
            continue
        found = True
        for n2, o2 in entries(type_off):
            if not (o2 & 0x80000000):
                continue
            name_off = base + (o2 & 0x7FFFFFFF)
            for n3, o3 in entries(name_off):
                lang_off = base + (o3 & 0x7FFFFFFF)
                data_rva, size = struct.unpack_from("<II", data, lang_off)
                off = rva_to_off(sections, data_rva)
                buf = data[off:off + size]
                print("   RT_MANIFEST name=%d lang=%d size=%d" % (n2, n3, size))
                print(buf.decode("utf-8", "replace"))
    if not found:
        print("   NO RT_MANIFEST (type 24)  ->  classic Common Controls 5.x look")


if __name__ == "__main__":
    for p in sys.argv[1:]:
        walk(p)
