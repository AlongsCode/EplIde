# -*- coding: utf-8 -*-
"""列出 e.exe 里的 RT_BITMAP 资源尺寸，并把适合做工具条的条带导出成 .bmp。

用法: python dump_bitmaps.py <e.exe> <out_dir> [长宽比过滤:height]
"""
import os
import struct
import sys

import pefile


def main(exe, outdir, want_h=None):
    os.makedirs(outdir, exist_ok=True)
    pe = pefile.PE(exe, fast_load=True)
    pe.parse_data_directories(directories=[
        pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
    img = pe.get_memory_mapped_image()
    rows = []
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tn = str(t.name) if t.name else pefile.RESOURCE_TYPE.get(t.struct.Id, t.struct.Id)
        if tn != "RT_BITMAP":
            continue
        for rid in t.directory.entries:
            name = rid.struct.Id if not rid.name else rid.name
            for lang in rid.directory.entries:
                off = lang.data.struct.OffsetToData
                size = lang.data.struct.Size
                dib = img[off:off + size]
                if len(dib) < 40:
                    continue
                hsz, w, h, planes, bpp = struct.unpack_from("<IiiHH", dib, 0)
                clr_used = struct.unpack_from("<I", dib, 32)[0]
                rows.append((name, size, w, h, bpp, clr_used, dib))

    rows.sort(key=lambda r: (r[3], r[2]))
    print("%-8s %9s %6s %6s %4s %6s" % ("id", "size", "width", "height", "bpp", "clrUsed"))
    for name, size, w, h, bpp, cu, _ in rows:
        flag = ""
        if h in (15, 16) and w >= 32 and w % 16 == 0:
            flag = "   <== 疑似工具条 %d 个按钮" % (w // 16)
        print("%-8s %9d %6d %6d %4d %6d%s" % (name, size, w, h, bpp, cu, flag))

    # 导出：默认导出 16/15 高且宽度是 16 整数倍的条带；给了 id 列表就按 id 导出
    ids = None
    if len(sys.argv) > 4:
        ids = set(int(x) for x in sys.argv[4].split(","))
    n = 0
    for name, size, w, h, bpp, cu, dib in rows:
        if ids is not None:
            if name not in ids:
                continue
        else:
            if h not in (15, 16) or w % 16 != 0 or w < 16:
                continue
            if want_h is not None and h != want_h:
                continue
        # DIB -> BMP 文件
        biSize, = struct.unpack_from("<I", dib, 0)
        sizeimg, = struct.unpack_from("<I", dib, 20)
        pal_bytes = len(dib) - biSize - sizeimg
        if pal_bytes < 0:
            pal_bytes = 0
        dib2 = bytearray(dib)
        # 原版把"透明色"写成了洋红 (255,0,255)：索引图里直接改成按钮面灰色，
        # 否则 comctl32 会把洋红原样画出来（实测就是这个现象）。
        for i in range(pal_bytes // 4):
            o = biSize + i * 4
            b, g, r = dib2[o], dib2[o + 1], dib2[o + 2]
            if b == 255 and g == 0 and r == 255:
                dib2[o], dib2[o + 1], dib2[o + 2] = 192, 192, 192
                print("   bitmap %s: palette[%d] magenta -> COLOR_BTNFACE" % (name, i))
        offbits = 14 + 40 + pal_bytes
        hdr = b"BM" + struct.pack("<IHHI", offbits + len(dib2), 0, 0, offbits)
        path = os.path.join(outdir, "bmp_%s_%dx%d.bmp" % (name, w, h))
        open(path, "wb").write(hdr + bytes(dib2))
        n += 1
    print("exported %d bitmap(s) to %s" % (n, outdir))


if __name__ == "__main__":
    h = int(sys.argv[3]) if len(sys.argv) > 3 else None
    main(sys.argv[1], sys.argv[2], h)
