# -*- coding: utf-8 -*-
"""Dump the NUL-separated GBK string block around a VA in a PE image.

IDA's database for e.exe has no .rsrc and idautils.Strings() misses GBK text,
so raw byte walking is the only reliable way to read these tables.

usage: python dump_rdata.py <exe> <hexVA> <count> [wstr]
"""
import sys

import pefile


def main(exe, va, count, wstr=False):
    va = int(va, 16)
    count = int(count)
    pe = pefile.PE(exe, fast_load=True)
    img = pe.get_memory_mapped_image()
    off = va - pe.OPTIONAL_HEADER.ImageBase
    if wstr:
        end = img.index(b"\x00\x00", off) + 2
        print(repr(img[off:off + count * 2].decode("utf-16-le", "replace")))
        return
    p, n = off, 0
    while n < count and p < len(img):
        q = img.index(b"\x00", p)
        raw = bytes(img[p:q])
        if raw:
            print("%08X  %s" % (va + (p - off), raw.decode("gbk", "replace")))
        p = q + 1
        n += 1


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3], len(sys.argv) > 4)
