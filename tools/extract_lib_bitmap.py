# -*- coding: utf-8 -*-
r"""从 krnln.fne 里抠出 LIB_BITMAP 资源（支持库自带的图标条）。

lib2.h 的约定：
    #define LIB_BMP_RESOURCE "LIB_BITMAP"
    #define LIB_BMP_CX       16        // 每格宽
    #define LIB_BMP_CY       13        // 每格高
    #define LIB_BMP_BKCOLOR  RGB(255,255,255)   // 底色
    #define UNIT_BMP_BACK_COLOR RGB(192,192,192) // 组件标志位图的背景色
CMD_INFO.m_shtBitmapIndex / LIB_DATA_TYPE_INFO.m_dwUnitBmpID 都是「从 1 开始」的格子号。

RT_BITMAP 资源在 PE 里是 DIB（没有 14 字节文件头），这里补头后用 PIL 拆格。

用法: python extract_lib_bitmap.py [krnln路径] [输出目录]
产物: <out>\lib_bitmap_strip.png      原始条带
      <out>\lib_bitmap_cells.png      每格拼成网格（放大 4 倍，便于人看）
      <out>\lib_bitmap_cells\NN.png   单格
"""
import io
import os
import struct
import sys

import pefile
from PIL import Image

KRNLN = r"C:\Program Files (x86)\e\lib\krnln.fne"
OUT = r"D:\eclone\shots\krnln_icons"
CX, CY = 16, 13
BK = (255, 255, 255)       # LIB_BMP_BKCOLOR
BACK = (192, 192, 192)     # UNIT_BMP_BACK_COLOR


def find_lib_bitmap(path):
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories(
        directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if pefile.RESOURCE_TYPE.get(t.struct.Id) != "RT_BITMAP":
            continue
        for e in t.directory.entries:
            name = e.name.string.decode("ascii", "replace") if e.name else None
            if name and name.upper() == "LIB_BITMAP":
                for l in e.directory.entries:
                    return pe.get_data(l.data.struct.OffsetToData, l.data.struct.Size)
    return None


def main(argv):
    src = argv[1] if len(argv) > 1 else KRNLN
    out = argv[2] if len(argv) > 2 else OUT
    os.makedirs(out, exist_ok=True)
    dib = find_lib_bitmap(src)
    if dib is None:
        print("没找到 LIB_BITMAP 资源")
        return 1
    hdr = struct.pack("<2sIHHI", b"BM", 14 + len(dib), 0, 0, 14)
    img = Image.open(io.BytesIO(hdr + dib)).convert("RGB")
    print("LIB_BITMAP 资源 %d 字节 -> 位图 %dx%d" % (len(dib), img.width, img.height))
    cols = img.width // CX
    rows = img.height // CY
    print("按 %dx%d 切：%d 列 x %d 行 = %d 格" % (CX, CY, cols, rows, cols * rows))
    img.save(os.path.join(out, "lib_bitmap_strip.png"))

    cells = []
    for r in range(rows):
        for c in range(cols):
            cells.append(img.crop((c * CX, r * CY, c * CX + CX, r * CY + CY)))
    grid = Image.new("RGB", (CX * 8 * 4, CY * ((len(cells) + 7) // 8) * 4), BACK)
    for i, cell in enumerate(cells):
        px = cell.load()
        for y in range(CY):
            for x in range(CX):
                if px[x, y] == BK:
                    px[x, y] = BACK
        big = cell.resize((CX * 4, CY * 4), Image.NEAREST)
        grid.paste(big, ((i % 8) * CX * 4, (i // 8) * CY * 4))
    grid.save(os.path.join(out, "lib_bitmap_cells.png"))
    print("written ->", os.path.join(out, "lib_bitmap_cells.png"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
