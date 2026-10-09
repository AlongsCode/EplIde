# -*- coding: utf-8 -*-
"""从用户给的「程序集树」截图 (shots\\ref_prog_tree.png, 1:1) 抠出树节点图标。

用法: python extract_prog_icons.py <png> <out_prefix>
产物: <out_prefix>_NN.png 每个 16x16 图标 + <out_prefix>_sheet.png 4 倍放大对照表
"""
import sys
from PIL import Image

# (行序号, 深度, 节点文字) —— 文字来自用户截图，用来确认图标归属
NODES = [
    (0, 0, "程序数据"),
    (1, 1, "集_程序"),
    (2, 1, "集_窗口句柄"),
    (3, 1, "集_键盘鼠标"),
    (4, 1, "集_进程线程"),
    (5, 1, "集_配置项"),
    (6, 1, "集_图片"),
    (7, 2, "类_识图"),
    (8, 2, "类_位图操作"),
    (9, 2, "类_位图处理"),
    (10, 2, "类_图片组操作类"),
    (11, 2, "类_像素组"),
    (12, 2, "类_图像格式转换类"),
    (13, 1, "集_网页"),
    (14, 1, "集_文本字节"),
    (15, 1, "集_文件目录"),
    (16, 1, "集_系统"),
    (17, 1, "集_杂类"),
    (18, 1, "集_组件"),
    (19, 1, "调试"),
    (20, 1, "全局变量"),
    (21, 1, "自定义数据类型"),
    (22, 1, "Dll命令"),
    (23, 1, "常量表..."),
    (24, 1, "资源表"),
    (25, 1, "模块引用表"),
    (26, 1, "外部文件记录表"),
]

# 行首 y（截图里第一行从 y=2 起，行距 16；用墨迹带实测校正）
ROW_Y0 = 2
ROW_PITCH = 16
ICON_X = {0: 2, 1: 20, 2: 39}


def main():
    png = sys.argv[1] if len(sys.argv) > 1 else r"D:\eclone\shots\ref_prog_tree.png"
    pre = sys.argv[2] if len(sys.argv) > 2 else r"D:\eclone\shots\zoom\progicon"
    im = Image.open(png).convert("RGB")
    tiles = []
    for idx, depth, name in NODES:
        y = ROW_Y0 + idx * ROW_PITCH
        x = ICON_X[depth]
        tile = im.crop((x, y, x + 16, y + 16))
        p = "%s_%02d.png" % (pre, idx)
        tile.save(p)
        tiles.append((idx, depth, name, tile))
        print("row %2d depth %d %-18s -> %s" % (idx, depth, name, p))

    S = 4
    sheet = Image.new("RGB", (16 * S * 2 + 200, 18 * S * len(tiles)), (255, 255, 255))
    for i, (idx, depth, name, tile) in enumerate(tiles):
        sheet.paste(tile.resize((16 * S, 16 * S), Image.NEAREST), (4, i * 18 * S + 4))
    # 只做图，文字留给外部核对（PIL 画中文要字体，麻烦）
    out = pre + "_sheet.png"
    sheet.save(out)
    print("sheet ->", out)


if __name__ == "__main__":
    main()
