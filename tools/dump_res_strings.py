# -*- coding: utf-8 -*-
"""Dump e.exe's RT_STRING resource table as UTF-8 text.

usage: python tools\\dump_res_strings.py <exe> <out.txt>
"""
import struct
import sys

import pefile

exe, out = sys.argv[1], sys.argv[2]
pe = pefile.PE(exe, fast_load=True)
pe.parse_data_directories(directories=[
    pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
img = pe.get_memory_mapped_image()

lines = []
for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
    if t.struct.Id != 6:
        continue
    for rid in t.directory.entries:
        base = rid.struct.Id * 16
        for lang in rid.directory.entries:
            off, size = lang.data.struct.OffsetToData, lang.data.struct.Size
            blob = img[off:off + size]
            p = 0
            for k in range(16):
                if p + 2 > len(blob):
                    break
                n = struct.unpack_from("<H", blob, p)[0]
                p += 2
                if n:
                    s = blob[p:p + n * 2].decode("utf-16-le", "replace")
                    lines.append("%5d  %s" % (base + k, s))
                p += n * 2

open(out, "w", encoding="utf-8").write("\n".join(lines))
print("-> %s (%d strings)" % (out, len(lines)))
