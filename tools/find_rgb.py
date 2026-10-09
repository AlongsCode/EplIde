# -*- coding: utf-8 -*-
"""Search an executable for a colour DWORD (little-endian, high byte 0).

usage: python find_rgb.py <exe> <RRGGBB> ...
"""
import struct
import sys

import pefile


def main(exe, *vals):
    pe = pefile.PE(exe, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    for s in pe.sections:
        nm = s.Name.rstrip(b"\x00").decode("latin1")
        if nm not in (".text", ".data", ".rdata"):
            continue
        d = pe.get_memory_mapped_image()[s.VirtualAddress:s.VirtualAddress + s.Misc_VirtualSize]
        for v in vals:
            n = 0
            if v.lower().startswith("0x"):
                n = int(v, 16)
            else:
                n = int(v, 16)
            pat = struct.pack("<I", n)
            i = 0
            hits = []
            while True:
                i = d.find(pat, i)
                if i < 0:
                    break
                hits.append(base + s.VirtualAddress + i)
                i += 1
            print("%-8s %-6s %d hits %s" % (v, nm, len(hits),
                                            " ".join("%08X" % h for h in hits[:8])))


if __name__ == "__main__":
    main(sys.argv[1], *sys.argv[2:])
