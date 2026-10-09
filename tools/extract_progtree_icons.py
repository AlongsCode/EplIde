# -*- coding: utf-8 -*-
"""从原版「程序」页树的 1:1 截图里抠出节点图标，拼成 src\\res\\progtree_icons.bmp。

两个来源（都是 1:1 像素，行距 16、首行 y=2、图标 x 随深度步进 19）：
  1) shots\\live_progtree\\tree0.png —— 自起原版 e.exe + 图形按钮.e，节点与图标号已知
     （treeinfo 倒档 shots\\live_progtree\\prog_tree.txt）：
       row0 img8 程序数据 / row1 img3 窗口程序集 / row2 img2 子程序 / row3 img6 窗口
       row4 img6 窗口 / row5 img9 常量表 / row6 img12 资源表 / row7 img10 声音
       row8 img11 图片或图片组 / row9 img14 模块引用表 / row10 img13 外部文件记录表
     这张图里图标 x：深度 0 = 2 / 1 = 21 / 2 = 40
  2) shots\\ref_prog_tree.png —— 用户给的截图（精易模块工程），用来补 tree0 里没有的
     节点类型：普通程序集(黄色文件夹) / 全局变量 / 自定义数据类型 / Dll命令
     这张图里图标 x：深度 0 = 2 / 1 = 20 / 2 = 39

用法: python extract_progtree_icons.py
"""
import os
from PIL import Image, ImageDraw

OUT_BMP = r"D:\eclone\src\res\progtree_icons.bmp"
OUT_DIR = r"D:\eclone\shots\zoom\progtree_icons"
TILE_PNG = r"D:\eclone\shots\live_progtree\tree0.png"
USER_PNG = r"D:\eclone\shots\ref_prog_tree.png"

X_TILE = {0: 2, 1: 21, 2: 40}
X_USER = {0: 2, 1: 20, 2: 39}
ROW_Y0, ROW_PITCH = 2, 16


def crop(im, xmap, row, depth):
    x = xmap[depth]
    y = ROW_Y0 + row * ROW_PITCH
    return im.crop((x, y, x + 16, y + 16))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    tile = Image.open(TILE_PNG).convert("RGB")
    user = Image.open(USER_PNG).convert("RGB")

    # 槽位号就是树控件里的 image index（代码里 EP_* 常量一一对应）
    # 槽位 2 的依据：tree0 里「启动窗口程序集」是 img=3 的菱形，而用户截图里普通
    # 的 集_* 程序集是黄色文件夹 —— 窗口程序集与普通程序集的图标不同。
    slots = [
        ("0_prog_data",  tile, X_TILE, 0,  0, 8),
        ("1_asm_win",    tile, X_TILE, 1,  1, 3),
        ("2_asm",        user, X_USER, 2,  1, None),   # 集_窗口句柄（第 1 行是选中态，别用）
        ("3_sub",        tile, X_TILE, 2,  2, 2),
        ("4_form",       tile, X_TILE, 3,  1, 6),
        ("5_window",     tile, X_TILE, 4,  2, 6),
        ("6_const",      tile, X_TILE, 5,  1, 9),
        ("7_resource",   tile, X_TILE, 6,  1, 12),
        ("8_res_sound",  tile, X_TILE, 7,  2, 10),
        ("9_res_image",  tile, X_TILE, 8,  2, 11),
        ("10_module_ref", tile, X_TILE, 9,  1, 14),
        ("11_ext_file",  tile, X_TILE, 10, 1, 13),
        ("12_global_var", user, X_USER, 21, 1, None),
        ("13_data_type", user, X_USER, 22, 1, None),
        ("14_dll_cmd",   user, X_USER, 23, 1, None),
    ]

    strip = Image.new("RGB", (16 * len(slots), 16), (255, 255, 255))
    for i, (name, im, xmap, row, depth, img) in enumerate(slots):
        t = crop(im, xmap, row, depth)
        t.save(os.path.join(OUT_DIR, name + ".png"))
        strip.paste(t, (16 * i, 0))
        print("%2d %-14s img=%-4s <- %s row %d" % (
            i, name.split("_", 1)[1], img, "tile" if im is tile else "user", row))
    strip.save(OUT_BMP)
    print("strip ->", OUT_BMP, strip.size)

    S = 6
    sheet = Image.new("RGB", (16 * S + 44, (18 * S) * len(slots)), (255, 255, 255))
    d = ImageDraw.Draw(sheet)
    for i, (name, im, xmap, row, depth, img) in enumerate(slots):
        sheet.paste(crop(im, xmap, row, depth).resize((16 * S, 16 * S), Image.NEAREST),
                    (44, i * 18 * S + 4))
        d.text((2, i * 18 * S + 44), "%d" % i, fill=(0, 0, 0))
    sheet.save(os.path.join(OUT_DIR, "sheet.png"))
    print("sheet ->", os.path.join(OUT_DIR, "sheet.png"))


if __name__ == "__main__":
    main()
