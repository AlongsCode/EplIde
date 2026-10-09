# -*- coding: utf-8 -*-
"""Scan a PE for GBK/ASCII strings and filter by regex. IDA has no .rsrc and
idautils.Strings() misses GBK, so raw byte scan is the only way.

usage: python scan_gbk.py <exe> <regex> [minlen]
"""
import re
import sys

import pefile


def main(exe, pat, minlen=4):
    rx = re.compile(pat.encode("gbk") if isinstance(pat, str) else pat)
    pe = pefile.PE(exe, fast_load=True)
    img = pe.get_memory_mapped_image()
    base = pe.OPTIONAL_HEADER.ImageBase
    # printable set: ASCII + GBK high bytes
    good = set(range(0x20, 0x7F)) | set(range(0x81, 0xFF))
    i, n = 0, len(img)
    while i < n:
        if img[i] in good and img[i] >= 0x20:
            j = i
            while j < n and img[j] in good and img[j] >= 0x20:
                j += 1
            if j - i >= minlen:
                raw = bytes(img[i:j])
                if rx.search(raw):
                    try:
                        s = raw.decode("gbk")
                    except UnicodeDecodeError:
                        s = raw.decode("gbk", "replace")
                    print("VA=%08X  %s" % (base + i, s))
            i = j
        i += 1


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 4)
