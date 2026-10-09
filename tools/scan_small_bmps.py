# -*- coding: utf-8 -*-
"""在 e.exe 的 RT_BITMAP / RT_CURSOR / RT_ICON 资源里找「红色铅笔」那个小图，
以及所有小尺寸位图（编辑器自绘可能用它们）。纯离线，不需要 IDA。

用法: python scan_small_bmps.py "C:\\Program Files (x86)\\e\\e.exe"
"""
import sys
import pefile


def main(path):
    pe = pefile.PE(path)
    for kind in ("RT_BITMAP", "RT_CURSOR", "RT_ICON", "RT_GROUP_CURSOR", "RT_GROUP_ICON"):
        if not hasattr(pe, "DIRECTORY_ENTRY_RESOURCE"):
            continue
        for e in pe.DIRECTORY_ENTRY_RESOURCE.entries:
            name = pefile.RESOURCE_TYPE.get(e.struct.Id, str(e.struct.Id))
            if name != kind:
                continue
            for r in e.directory.entries:
                for l in r.directory.entries:
                    d = pe.get_data(l.data.struct.OffsetToData, l.data.struct.Size)
                    tag = "%s id=%s size=%d" % (kind, r.struct.Id, len(d))
                    if kind == "RT_BITMAP" and len(d) > 14:
                        # BITMAPINFOHEADER: w,h,bpp
                        import struct
                        w, h = struct.unpack_from("<ii", d, 4)
                        bpp = struct.unpack_from("<H", d, 14)[0]
                        if 8 <= abs(w) <= 32 and 8 <= abs(h) <= 32:
                            print("%-34s w=%d h=%d bpp=%d" % (tag, w, h, bpp))
                    else:
                        print(tag)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else r"C:\Program Files (x86)\e\e.exe")
