# -*- coding: utf-8 -*-
"""Dll 命令页回归检查：把一张 1:1 的编辑器客户区截图按「Dll命令块」的实测结构逐项断言。

断言的内容全部来自原版实测（shots\\ref_dll_page.png = 用户实例的 1:1 截图）：
  · 一个 Dll命令块 = 一张 5 列表，行序固定：
      0 表头   Dll命令名 | 返回值类型 | 公开    | 备注     -> 3 条内部竖线(共 5)，底色 #E6EDE4
      1 数据   命令名     | 返回值类型 | ✔公开   | 备注     -> 5 条竖线，白底
      2 库文件名:                                           -> 整行 1 格（共 2 条竖线），底色 #E6EDE4
      3 库文件名值                                          -> 2 条竖线，白底
      4 在库中对应命令名:                                    -> 2 条竖线，底色 #E6EDE4
      5 在库中对应命令名值                                   -> 2 条竖线，白底
      6 参数表头 参数名 | 类 型 | 传址 | 数组 | 备 注          -> 6 条竖线，底色 #E6EDE4
      7+ 参数行                                             -> 6 条竖线，白底
  · 行距 21px；块内最后一行 20px（末行的下边线画在 +20）；块与块之间空 8px（横线相距 9）
  · 流程槽 x=0..19 是 #ECECEC；正文区左边界 x=44 有一条 #888888 全高竖线
  · 表头「备注」列宽 = 参数表「数组」列宽 + 「备注」列宽（实测：两张表共用同一组 5 列宽度）
  · 文字颜色：命令名 #000080 / 返回值类型 #0000FF / 库文件名值 #800000 / 参数备注 #008000

块是从「表头行」（5 条竖线 + #E6EDE4 底）开始认的，所以截图上滚动过的残缺块会被跳过，
不影响判定。

用法: python check_dll.py shots\\dll_view.png [shots\\dll_view2.png ...]
"""
import sys
from collections import Counter
from PIL import Image

GRID = (0x88, 0x88, 0x88)
FILL_HDR = (0xE6, 0xED, 0xE4)
WHITE = (255, 255, 255)
GUTTER = (0xEC, 0xEC, 0xEC)

ROW_PITCH = 21
ROW_PITCH_LAST = 20
BLOCK_GAP = 9
TEXT_X = 44
GUTTER_W = 20

# 行类型 -> (竖线条数, 是否表头底色)
HDR, DAT, LBL, VAL, PHDR, PAR = "H", "D", "L", "V", "P", "p"
WANT = {HDR: (5, True), DAT: (5, False), LBL: (2, True), VAL: (2, False),
        PHDR: (6, True), PAR: (6, False)}
SEQ = [HDR, DAT, LBL, VAL, LBL, VAL, PHDR]

ok = fail = 0


def chk(cond, msg):
    global ok, fail
    if cond:
        ok += 1
    else:
        fail += 1
        print("  FAIL %s" % msg)


def hlines(px, w, h):
    return [y for y in range(h)
            if sum(1 for x in range(w) if px[x, y] == GRID) > 100]


def vlines(px, w, y):
    # 右边留 24px 给滚动条：PrintWindow 出来的滚动条里也可能有 #888888 的像素，
    # 不排除掉会时不时多出几条"竖线"（表格右边界最远也就 ~450px，不受影响）
    return [x for x in range(TEXT_X, w - 24) if px[x, y] == GRID]


def inkcolor(px, x0, x1, y0, y1):
    c = Counter()
    for y in range(y0, y1):
        for x in range(x0, x1):
            p = px[x, y]
            if p not in (GRID, WHITE, FILL_HDR, GUTTER):
                c[p] += 1
    return c.most_common(1)[0][0] if c else None


def fillof(px, v, y):
    """单元格底色：取 cell 左侧一小段的众数颜色（避开 1px 宽的插入点光标）"""
    c = Counter()
    for x in range(v[0] + 2, min(v[0] + 10, v[1] - 1)):
        c[px[x, y]] += 1
    return c.most_common(1)[0][0] if c else None


def build_rows(ys, w, px):
    """横线 -> [(y0, y1, 新块的顶行?, 竖线表, 底色)]"""
    rows = []
    for i in range(len(ys) - 1):
        y0, y1 = ys[i], ys[i + 1]
        d = y1 - y0
        if d == BLOCK_GAP:
            continue
        chk(d in (ROW_PITCH, ROW_PITCH_LAST), "横线间距 %d 非法（y=%d）" % (d, y1))
        v = vlines(px, w, y0 + 10)
        fill = fillof(px, v, y0 + 10) if len(v) >= 2 else None
        newblock = (i > 0 and ys[i] - ys[i - 1] == BLOCK_GAP)
        rows.append((y0, y1, newblock, v, fill))
    return rows


def check(path):
    global ok, fail
    ok = fail = 0                      # 每张图单独计数（% passed 只对本图）
    im = Image.open(path).convert("RGB")
    w, h = im.size
    px = im.load()
    print("== %s  %dx%d" % (path, w, h))

    chk(all(px[x, 100] == GUTTER for x in range(GUTTER_W)), "流程槽 x=0..19 不是 #ECECEC")
    chk(px[TEXT_X, 100] == GRID, "x=44 没有全高竖线")

    ys = hlines(px, w, h)
    chk(len(ys) > 8, "横线太少（%d）" % len(ys))
    rows = build_rows(ys, w, px)

    # 每行的类型
    kinds = []
    for (y0, y1, nb, v, fill) in rows:
        for k, (nv, f) in WANT.items():
            if len(v) == nv and fill == (FILL_HDR if f else WHITE):
                kinds.append(k)
                break
        else:
            kinds.append("?")

    nblocks = 0
    i = 0
    while i < len(rows):
        if kinds[i] != HDR:
            i += 1
            continue
        nblocks += 1
        bi = nblocks - 1
        # 从这个表头往下按固定行序走，直到块结束（下一个 newblock）或类型不符
        j = i
        step = 0
        while j < len(rows) and (j == i or not rows[j][2]):
            want = SEQ[step] if step < len(SEQ) else PAR
            got = kinds[j]
            chk(got == want, "块%d 第%d行类型 %s，应为 %s" % (bi, step, got, want))
            step += 1
            j += 1

        y0 = rows[i][0]
        hdr_v = rows[i][3]
        # 参数表头那一行
        if i + 6 < len(rows) and rows[i + 6][2] is False and len(rows[i + 6][3]) == 6:
            par_v = rows[i + 6][3]
            chk(hdr_v[4] - hdr_v[3] == par_v[5] - par_v[3],
                "块%d 表头备注列宽 %d != 参数表 数组+备注 列宽 %d"
                % (bi, hdr_v[4] - hdr_v[3], par_v[5] - par_v[3]))
        else:
            par_v = []

        # 文字颜色（空单元格允许 —— 比如没有返回值的 Dll命令）
        y0, y1 = rows[i + 1][0] + 1, rows[i + 1][1]
        c = inkcolor(px, hdr_v[0] + 1, hdr_v[1], y0, y1)
        chk(c in (None, (0, 0, 0x80)), "块%d 命令名颜色不对（%s）" % (bi, c))
        c = inkcolor(px, hdr_v[1] + 1, hdr_v[2], y0, y1)
        chk(c in (None, (0, 0, 0xFF)), "块%d 返回值类型颜色不对（%s）" % (bi, c))
        if i + 3 < len(rows) and len(rows[i + 3][3]) == 2:
            v3 = rows[i + 3][3]
            c = inkcolor(px, v3[0] + 1, v3[1], rows[i + 3][0] + 1, rows[i + 3][1])
            chk(c in (None, (0x80, 0, 0)), "块%d 库文件名值颜色不对（%s）" % (bi, c))
        if par_v and i + 7 < len(rows) and not rows[i + 7][2]:
            y0 = rows[i + 7][0] + 1
            y1 = rows[i + 8][0] if i + 8 < len(rows) else y0 + 20
            c = inkcolor(px, par_v[4] + 1, par_v[5], y0, y1)
            chk(c in (None, (0, 0x80, 0)), "块%d 参数备注颜色不对（%s）" % (bi, c))
        i = j

    print("   blocks=%d  行数=%d  类型=%s" % (nblocks, len(rows), "".join(kinds)))
    chk(nblocks >= 1, "一个 Dll命令块都没认出来")
    print("  %d passed, %d failed" % (ok, fail))
    return fail


if __name__ == "__main__":
    files = sys.argv[1:] or [r"D:\eclone\shots\dll_view.png"]
    bad = 0
    for f in files:
        bad += check(f)
    sys.exit(1 if bad else 0)
