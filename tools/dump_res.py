# -*- coding: utf-8 -*-
"""把 e.exe 里的小资源（RT_BITMAP / RT_ICON）导出来拼成"联系表"，方便肉眼认图标。

用法: python dump_res.py <e.exe> <out_dir> [scale] [max_size]
产物: <out_dir>\\res_<type>_<id>.png   单张（放大 scale 倍）
      <out_dir>\\sheet_small.png       联系表（带 id 标注）

注意：e.exe 的 RT_BITMAP 里混着 BITMAPCOREHEADER(12 字节头) 和 BITMAPINFOHEADER(40 字节头)，
      且 4bpp/8bpp 都有，所以这里自己解码 DIB，不依赖 PIL 的 BMP 插件。
"""
import os
import struct
import sys

import pefile
from PIL import Image, ImageDraw


def decode_dib(dib):
    (biSize,) = struct.unpack_from("<I", dib, 0)
    if biSize == 12:  # BITMAPCOREHEADER
        w, h, planes, bpp = struct.unpack_from("<HHHH", dib, 4)
        clr_used = 0
        pal_entry = 3
        pal_off = 12
    else:
        w, h, planes, bpp = struct.unpack_from("<iiHH", dib, 4)
        (clr_used,) = struct.unpack_from("<I", dib, 32)
        pal_entry = 4
        pal_off = biSize
    (sizeimg,) = struct.unpack_from("<I", dib, 20)
    if bpp <= 8:
        n = clr_used or (1 << bpp)
        pal = []
        for i in range(n):
            o = pal_off + i * pal_entry
            b, g, r = dib[o], dib[o + 1], dib[o + 2]
            pal.append((r, g, b))
    else:
        pal = []
    pix_off = pal_off + (len(pal) * pal_entry if pal else 0)
    if sizeimg:
        pix_off = len(dib) - sizeimg
    bottom_up = h > 0
    h = abs(h)
    stride = ((w * bpp + 31) // 32) * 4
    im = Image.new("RGBA", (w, h))
    px = im.load()
    for y in range(h):
        sy = (h - 1 - y) if bottom_up else y
        row = dib[pix_off + sy * stride: pix_off + sy * stride + stride]
        for x in range(w):
            if bpp == 4:
                b = row[x // 2]
                idx = (b >> 4) if x % 2 == 0 else (b & 0xF)
                c = pal[idx] if idx < len(pal) else (255, 0, 255)
            elif bpp == 8:
                idx = row[x]
                c = pal[idx] if idx < len(pal) else (255, 0, 255)
            elif bpp == 24:
                c = (row[x * 3 + 2], row[x * 3 + 1], row[x * 3])
            elif bpp == 32:
                c = (row[x * 4 + 2], row[x * 4 + 1], row[x * 4])
            elif bpp == 1:
                c = pal[(row[x // 8] >> (7 - x % 8)) & 1]
            else:
                c = (255, 0, 255)
            if c == (255, 0, 255):
                px[x, y] = (192, 192, 192, 255)   # 洋红=透明色 -> 按钮面
            else:
                px[x, y] = (c[0], c[1], c[2], 255)
    return im


def ico_wrap(data):
    (w, h, ncol, res, planes, bpp, size) = (0, 0, 0, 0, 0, 32, len(data))
    hdr = struct.pack("<HHH", 0, 1, 1)
    hdr += struct.pack("<BBBBHHII", 0, 0, 0, 0, 1, 32, len(data), 22)
    return hdr + data


def main(exe, outdir, scale=5, max_size=40):
    os.makedirs(outdir, exist_ok=True)
    pe = pefile.PE(exe, fast_load=True)
    pe.parse_data_directories(directories=[
        pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
    img = pe.get_memory_mapped_image()

    by_type = {}
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tn = str(t.name) if t.name else pefile.RESOURCE_TYPE.get(t.struct.Id, "RT_%d" % t.struct.Id)
        lst = by_type.setdefault(tn, [])
        for rid in t.directory.entries:
            name = str(rid.struct.Id) if not rid.name else str(rid.name)
            for lang in rid.directory.entries:
                lst.append((name, lang.data.struct.OffsetToData, lang.data.struct.Size))

    tiles = []
    for tn in ("RT_BITMAP", "RT_ICON"):
        for name, off, size in by_type.get(tn, []):
            data = img[off:off + size]
            try:
                if tn == "RT_BITMAP":
                    im = decode_dib(data)
                else:
                    im = Image.open(__import__("io").BytesIO(ico_wrap(data))).convert("RGBA")
            except Exception as e:
                print("  skip %s %s: %r" % (tn, name, e))
                continue
            if im.width > max_size or im.height > max_size:
                print("  (big %s %s = %dx%d)" % (tn, name, im.width, im.height))
                continue
            big = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
            big.save(os.path.join(outdir, "res_%s_%s.png" % (tn.replace("RT_", ""), name)))
            tiles.append(("%s:%s %dx%d" % (tn.replace("RT_", ""), name, im.width, im.height), big))

    if tiles:
        cols = 8
        cell_w = max(t[1].width for t in tiles) + 10
        cell_h = max(t[1].height for t in tiles) + 20
        rows = (len(tiles) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * cell_w, rows * cell_h), (255, 255, 255))
        d = ImageDraw.Draw(sheet)
        for i, (label, big) in enumerate(tiles):
            cx = (i % cols) * cell_w
            cy = (i // cols) * cell_h
            sheet.paste(big, (cx + 5, cy + 16), big)
            d.text((cx + 3, cy + 3), label, fill=(0, 0, 0))
        path = os.path.join(outdir, "sheet_small.png")
        sheet.save(path)
        print("-> %s (%d tiles)" % (path, len(tiles)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2],
         int(sys.argv[3]) if len(sys.argv) > 3 else 5,
         int(sys.argv[4]) if len(sys.argv) > 4 else 40)
