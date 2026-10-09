# -*- coding: utf-8 -*-
"""生成 EClone 需要的图标资源：

1) 从 e.exe 的 RT_BITMAP 导出（洋红透明色 -> COLOR_BTNFACE）：
     146 112x15  -> 工作夹标签页图标（7 个 16x15：支持库/程序/属性/...）
     148 112x16  -> 状态夹标签页图标（7 个 16x16：提示/输出/调用表/监视板/变量表/搜寻/剪辑历史）
     595 6x5     -> 面板标题行 × 字形
     596 6x6     -> 面板标题行 □ 字形
     131 369x15  -> 标准工具条（23 钮）
     297 224x15  -> 对齐工具条（14 钮，灰色/禁用态）
     231 272x15  -> 定位工具条（17 钮，默认收起）
2) 从原版截图 shots\\ide_orig_current.png 抠出支持库树的 24 个 16x16 图标，
   拼成 src\\res\\tree_icons.bmp（条带，自上而下：库 > 23 个类别）。
   原版 e.exe 里没有这组图标（来自 krnln.fne），只能从截图抠。

用法: python extract_icons.py [--measure]
产物: D:\\eclone\\src\\res\\*.bmp
"""
import os
import struct
import sys

import pefile
from PIL import Image

ECLONE = r"D:\eclone"
EXE = r"C:\Program Files (x86)\e\e.exe"
SHOT = os.path.join(ECLONE, "shots", "ide_orig_current.png")
RES = os.path.join(ECLONE, "src", "res")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dump_res import decode_dib  # noqa: E402

# 原版截图里支持库树的几何（实测：根行 y=153 起，行距 14；根图标 x=18，子项图标 x=36）
TREE_ROOT_X = 18
TREE_ROOT_Y = 153
TREE_CHILD_X = 36
TREE_CHILD_Y = 167
TREE_ROW_H = 14
TREE_ROWS = 24             # 库 1 + 类别 23
TREE_ICON = 16


def export_toolbars(ids):
    """工具条位图必须**原样**导出（保留原来的 4bpp/8bpp + 调色板）。

    ⚠️ 不能走 PIL 再存一遍：PIL 会存成 24bpp，而 MFC 的 CToolBar::LoadBitmap 走的是
    AfxLoadSysColorBitmap()，它按 biBitCount 推算调色板项数来做系统色映射，
    24bpp 的位图会让它算出一堆垃圾 —— 现象就是**工具条按钮全变成灰方块**（实测踩过）。
    """
    names = {131: "toolbar_std.bmp", 297: "toolbar_align.bmp", 231: "toolbar_pos.bmp"}
    pe = pefile.PE(EXE, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
    img = pe.get_memory_mapped_image()
    n = 0
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tn = str(t.name) if t.name else pefile.RESOURCE_TYPE.get(t.struct.Id, t.struct.Id)
        if tn != "RT_BITMAP":
            continue
        for rid in t.directory.entries:
            if rid.struct.Id not in ids:
                continue
            for lang in rid.directory.entries:
                off, size = lang.data.struct.OffsetToData, lang.data.struct.Size
                dib = img[off:off + size]
                biSize, = struct.unpack_from("<I", dib, 0)
                sizimg, = struct.unpack_from("<I", dib, 20)
                pal = len(dib) - biSize - sizimg
                if pal < 0:
                    pal = 0
                buf = bytearray(dib)
                for i in range(pal // 4):          # 洋红(255,0,255)=透明色 -> COLOR_BTNFACE
                    o = biSize + i * 4
                    if buf[o] == 255 and buf[o + 1] == 0 and buf[o + 2] == 255:
                        buf[o], buf[o + 1], buf[o + 2] = 192, 192, 192
                offbits = 14 + biSize + pal
                hdr = b"BM" + struct.pack("<IHHI", offbits + len(buf), 0, 0, offbits)
                path = os.path.join(RES, names[rid.struct.Id])
                open(path, "wb").write(hdr + bytes(buf))
                print("  e.exe bitmap %-4d -> %s (原样 %d bpp)" %
                      (rid.struct.Id, names[rid.struct.Id], struct.unpack_from("<H", dib, 14)[0]))
                n += 1
    return n


def export_icon(ico_path, group_id=127):
    """从 e.exe 的 RT_GROUP_ICON 生成 .ico。

    图标文件里存的就是 RT_ICON 的原始 DIB 字节（BITMAPINFOHEADER + 高度翻倍的 XOR+AND），
    所以这里直接照搬字节、拼一个 ICONDIR 就行，不用解码。
    组 127 = {4,5,6} = 16/32/48，是 原版菜单条上那个蓝色"易"（126/134 是红色版，1151 是加密狗钥匙）。
    """
    pe = pefile.PE(EXE, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
    img = pe.get_memory_mapped_image()
    icons, groups = {}, {}
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tn = str(t.name) if t.name else pefile.RESOURCE_TYPE.get(t.struct.Id, t.struct.Id)
        if tn not in ("RT_ICON", "RT_GROUP_ICON"):
            continue
        for rid in t.directory.entries:
            for lang in rid.directory.entries:
                off, size = lang.data.struct.OffsetToData, lang.data.struct.Size
                (icons if tn == "RT_ICON" else groups)[rid.struct.Id] = img[off:off + size]
    grp = groups.get(group_id)
    if grp is None:
        print("  no RT_GROUP_ICON %d" % group_id)
        return False
    (cnt,) = struct.unpack_from("<H", grp, 4)
    ent, blob = b"", b""
    off = 6 + 16 * cnt
    for i in range(cnt):
        w, h, ncol, res, planes, bpp, size, iid = struct.unpack_from("<BBBBHHIH", grp, 6 + i * 14)
        d = icons.get(iid)
        if d is None:
            print("  missing RT_ICON %d" % iid)
            return False
        ent += struct.pack("<BBBBHHII", w, h, ncol, 0, planes, bpp, len(d), off + len(blob))
        blob += d
        print("  ico entry %dx%d %dbpp from RT_ICON %d" % (w or 256, h or 256, bpp, iid))
    hdr = struct.pack("<HHH", 0, 1, cnt)
    open(ico_path, "wb").write(hdr + ent + blob)
    print("  icon -> %s" % os.path.basename(ico_path))
    return True


def export_tabicons(ids):
    """标签页图标走 PIL（喂给 CImageList，24bpp 没问题）。146 是 16x15，补成 16 行。"""
    names = {146: "tabicons_work_raw.bmp", 148: "tabicons_status.bmp"}
    pe = pefile.PE(EXE, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
    img = pe.get_memory_mapped_image()
    n = 0
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tn = str(t.name) if t.name else pefile.RESOURCE_TYPE.get(t.struct.Id, t.struct.Id)
        if tn != "RT_BITMAP":
            continue
        for rid in t.directory.entries:
            if rid.struct.Id not in ids:
                continue
            for lang in rid.directory.entries:
                off, size = lang.data.struct.OffsetToData, lang.data.struct.Size
                im = decode_dib(img[off:off + size]).convert("RGB")
                if im.height == 15:
                    pad = Image.new("RGB", (im.width, 16), (192, 192, 192))
                    pad.paste(im, (0, 1))
                    im = pad
                path = os.path.join(RES, names[rid.struct.Id])
                im.save(path)
                print("  e.exe bitmap %-4d (%dx%d) -> %s" %
                      (rid.struct.Id, im.width, im.height, names[rid.struct.Id]))
                n += 1
    return n


def measure_tree():
    im = Image.open(SHOT).convert("RGB")
    px = im.load()
    runs = []
    cur = None
    for y in range(140, 900):
        hit = False
        for x in range(8, 34):
            r, g, b = px[x, y]
            if not (r > 235 and g > 235 and b > 235):
                hit = True
                break
        if hit and cur is None:
            cur = y
        elif not hit and cur is not None:
            runs.append((cur, y - 1))
            cur = None
    print("图标行（y0,y1）:")
    for a, b in runs:
        print("   %d..%d  h=%d" % (a, b, b - a + 1))
    return runs


def crop_work_tab_icons():
    """工作夹三个标签页的图标：e.exe 的 RT_BITMAP 146 里没有（那张图是坏的/不是这组图标），
    支持库那个图标其实来自 krnln.fne，所以这里直接从原版截图抠。
    原版截图实测：标签行 y 829..852，三个图标分别在 x≈20 / 88 / 154。"""
    im = Image.open(SHOT).convert("RGB")
    strip = Image.new("RGB", (16 * 3, 16), (192, 192, 192))
    for i, x in enumerate((20, 90, 146)):
        cell = im.crop((x, 832, x + 16, 848))
        px = cell.load()
        for j in range(cell.height):
            for k in range(cell.width):
                r, g, b = px[k, j]
                if r > 228 and g > 228 and b > 228:
                    px[k, j] = (192, 192, 192)
        strip.paste(cell, (i * 16, 0))
    strip.save(os.path.join(RES, "tabicons_work.bmp"))
    strip.resize((strip.width * 6, strip.height * 6), Image.NEAREST).save(
        os.path.join(ECLONE, "shots", "c_worktabicons.png"))
    print("  工作夹标签图标 -> src\\res\\tabicons_work.bmp (3 个，从原版截图抠)")


def crop_tree():
    im = Image.open(SHOT).convert("RGB")
    strip = Image.new("RGB", (TREE_ICON * TREE_ROWS, TREE_ICON), (255, 255, 255))
    for i in range(TREE_ROWS):
        if i == 0:
            x, y = TREE_ROOT_X, TREE_ROOT_Y
        else:
            x, y = TREE_CHILD_X, TREE_CHILD_Y + TREE_ROW_H * (i - 1)
        cell = im.crop((x, y, x + TREE_ICON, y + TREE_ROW_H))   # 16x14
        px = cell.load()
        for j in range(cell.height):
            for k in range(cell.width):
                r, g, b = px[k, j]
                if r > 228 and g > 228 and b > 228:
                    px[k, j] = (255, 255, 255)     # 原版树底色是 (240,240,240)，统一成白
        strip.paste(cell, (i * TREE_ICON, 0))   # 贴在最上面：树的行高只有 14px，下面的会被裁掉
    path = os.path.join(RES, "tree_icons.bmp")
    strip.save(path)
    big = strip.resize((strip.width * 4, strip.height * 4), Image.NEAREST)
    big.save(os.path.join(ECLONE, "shots", "c_tree_icons.png"))
    print("  tree icons -> %s (also shots\\c_tree_icons.png for review)" % path)


if __name__ == "__main__":
    os.makedirs(RES, exist_ok=True)
    if "--measure" in sys.argv:
        measure_tree()
        sys.exit(0)
    export_toolbars({131, 297, 231})
    export_tabicons({148})
    export_icon(os.path.join(RES, "eclone.ico"))
    crop_work_tab_icons()
    crop_tree()
