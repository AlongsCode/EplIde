# -*- coding: utf-8 -*-
"""比对 EClone 从截图抠的 tree_icons.bmp（24 个 16x16）与 krnln.fne 的 LIB_BITMAP 真值条带
（21 个 16x13），给出每个截图图标对应的真值格子号（最佳匹配 + 分数），
用来判断「支持库树的图标其实可以直接从 krnln.fne 资源取」。

用法: python match_tree_icons.py [截图条带] [LIB_BITMAP条带]
"""
import sys

from PIL import Image

SHOT = r"D:\eclone\src\res\tree_icons.bmp"
LIB = r"D:\eclone\shots\krnln_icons\lib_bitmap_strip.png"


def gray(img):
    return img.convert("L").load(), img.width, img.height


def main(argv):
    shot_p = argv[1] if len(argv) > 1 else SHOT
    lib_p = argv[2] if len(argv) > 2 else LIB
    s = Image.open(shot_p).convert("RGB")
    l = Image.open(lib_p).convert("RGB")
    n_shot = s.width // 16
    n_lib = l.width // 16
    sp, _, sh = gray(s)
    lp, _, lh = gray(l)

    def diff_at(sx, lx):
        # 截图格 16x16 对真值格 16x13，比 16x13 的重叠区（截图 icon 通常上边多 1px 空白）
        best = None
        for oy in range(0, sh - 13 + 1):
            d = 0
            for y in range(13):
                for x in range(16):
                    d += abs(sp[sx * 16 + x, oy + y] - lp[lx * 16 + x, y])
            if best is None or d < best[0]:
                best = (d, oy)
        return best

    print("截图 %d 格 / 真值 %d 格" % (n_shot, n_lib))
    print("idx  最佳格  分数   次佳格/分数   结论")
    for i in range(n_shot):
        scores = [diff_at(i, j) for j in range(n_lib)]
        order = sorted(range(n_lib), key=lambda j: scores[j][0])
        b, second = order[0], order[1]
        ratio = scores[second][0] / max(1, scores[b][0])
        print("%3d   %3d   %6d   %3d/%6d   %s"
              % (i, b, scores[b][0], second, scores[second][0],
                 "确定" if ratio > 1.35 else ("可疑" if ratio > 1.1 else "无法区分")))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
