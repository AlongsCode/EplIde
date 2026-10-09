# -*- coding: utf-8 -*-
"""诊断 RT_BITMAP 131 的真实布局（头部/调色板/像素），用于修 BMP 导出。"""
import struct
import sys

import pefile


def main(exe, rid_want):
    pe = pefile.PE(exe, fast_load=True)
    pe.parse_data_directories(directories=[
        pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
    img = pe.get_memory_mapped_image()
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tn = str(t.name) if t.name else pefile.RESOURCE_TYPE.get(t.struct.Id, t.struct.Id)
        if tn != "RT_BITMAP":
            continue
        for rid in t.directory.entries:
            name = rid.struct.Id if not rid.name else rid.name
            if str(name) != str(rid_want):
                continue
            for lang in rid.directory.entries:
                off = lang.data.struct.OffsetToData
                size = lang.data.struct.Size
                dib = img[off:off + size]
                biSize, w, h, planes, bpp = struct.unpack_from("<IiiHH", dib, 0)
                comp, sizeimg = struct.unpack_from("<II", dib, 16)
                clrused = struct.unpack_from("<I", dib, 32)[0]
                rowsize = ((w * bpp + 31) // 32) * 4
                img_bytes = rowsize * abs(h)
                pal_bytes = size - biSize - img_bytes
                print("id=%s size=%d biSize=%d w=%d h=%d bpp=%d clrUsed=%d comp=%d sizeImg=%d"
                      % (name, size, biSize, w, h, bpp, clrused, comp, sizeimg))
                print("  推算: rowsize=%d 像素=%d 调色板字节=%d (%.1f 项)"
                      % (rowsize, img_bytes, pal_bytes, pal_bytes / 4.0))
                print("  前 12 个调色板项 (B,G,R,0):")
                for i in range(12):
                    o = biSize + i * 4
                    if o + 4 > len(dib):
                        break
                    b, g, r, x = dib[o:o + 4]
                    print("    [%2d] %3d %3d %3d" % (i, b, g, r))
                # 像素前 16 字节
                po = biSize + pal_bytes
                print("  像素前 32 字节:", dib[po:po + 32].hex(" "))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
