# -*- coding: utf-8 -*-
"""一次性：把 e.exe 的 10 个 RT_ICON 按真实尺寸解码，拼成联系表看哪个是"易"主图标。
图标 DIB 的高度是真实高度的 2 倍（XOR 图 + AND 掩码），掩码 stride 与颜色图不同。
用法: python _icons_sheet.py <e.exe> <out.png>
"""
import struct
import sys

import pefile
from PIL import Image, ImageDraw


def decode_icon(d):
    biSize, w, h, planes, bpp = struct.unpack_from("<IiiHH", d, 0)
    (sizeimg,) = struct.unpack_from("<I", d, 20)
    clr = struct.unpack_from("<I", d, 32)[0] or (1 << bpp if bpp <= 8 else 0)
    pal_off = biSize
    pal = []
    for i in range(clr):
        o = pal_off + i * 4
        pal.append((d[o + 2], d[o + 1], d[o]))
    rows = abs(h) // 2
    stride = ((w * bpp + 31) // 32) * 4
    mask_stride = ((w + 31) // 32) * 4
    xor_off = pal_off + clr * 4
    and_off = xor_off + stride * rows
    im = Image.new("RGBA", (w, rows))
    px = im.load()
    for y in range(rows):
        sy = rows - 1 - y                      # 自下而上
        row = d[xor_off + sy * stride: xor_off + (sy + 1) * stride]
        mrow = d[and_off + sy * mask_stride: and_off + (sy + 1) * mask_stride]
        for x in range(w):
            if bpp == 4:
                v = row[x // 2]
                idx = (v >> 4) if x % 2 == 0 else (v & 0xF)
            elif bpp == 8:
                idx = row[x]
            elif bpp == 1:
                idx = (row[x // 8] >> (7 - x % 8)) & 1
            else:
                idx = 0
            c = pal[idx] if idx < len(pal) else (255, 0, 255)
            transparent = (mrow[x // 8] >> (7 - x % 8)) & 1 if mrow else 0
            px[x, y] = (0, 0, 0, 0) if transparent else (c[0], c[1], c[2], 255)
    return im


def main(exe, out):
    pe = pefile.PE(exe, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
    img = pe.get_memory_mapped_image()
    icons = {}
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tn = str(t.name) if t.name else pefile.RESOURCE_TYPE.get(t.struct.Id, t.struct.Id)
        if tn != "RT_ICON":
            continue
        for rid in t.directory.entries:
            for lang in rid.directory.entries:
                off, size = lang.data.struct.OffsetToData, lang.data.struct.Size
                icons[rid.struct.Id] = img[off:off + size]
    tiles = []
    for i in sorted(icons):
        try:
            im = decode_icon(icons[i])
        except Exception as e:
            print("skip", i, e)
            continue
        print("icon %-3d %dx%d" % (i, im.width, im.height))
        tiles.append((i, im.resize((im.width * 5, im.height * 5), Image.NEAREST)))
    W = sum(t[1].width + 10 for t in tiles) + 10
    H = max(t[1].height for t in tiles) + 26
    sheet = Image.new("RGB", (W, H), (255, 255, 255))
    d = ImageDraw.Draw(sheet)
    x = 5
    for i, im in tiles:
        d.text((x, 4), "RT_ICON %d" % i, fill=(0, 0, 0))
        sheet.paste(im, (x, 20), im)
        x += im.width + 10
    sheet.save(out)
    print("->", out)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
