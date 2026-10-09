# -*- coding: utf-8 -*-
"""从原版 1:1 截图里抽出两个自绘小位图的精确点阵，打印成 C 数组。

  1) 参数表/Dll表 里的「✔」：shots\\ref_dll_page.png 的 (297,246)-(305,252)，9x7，单色
  2) 流程槽里的「红色铅笔」标记：同图 (3,11)-(16,24)，14x14，三色（黑轮廓/暗红/亮红）

用法: python gen_masks.py
"""
from PIL import Image

DLL = r"D:\eclone\shots\ref_dll_page.png"

TICK_BOX = (297, 246, 306, 253)      # 9x7
PEN_BOX = (3, 11, 17, 25)            # 14x14

PEN_COLORS = {
    (0x00, 0x00, 0x00): 1,   # 黑轮廓
    (0x80, 0x00, 0x00): 2,   # 暗红
    (0xFF, 0x00, 0x00): 3,   # 亮红
}


def dump_tick():
    im = Image.open(DLL).convert("RGB")
    px = im.load()
    x0, y0, x1, y1 = TICK_BOX
    print("// ---- tick %dx%d (from shots\\ref_dll_page.png (%d,%d)) ----"
          % (x1 - x0, y1 - y0, x0, y0))
    print("static const char* g_szTick[%d] = {" % (y1 - y0))
    for y in range(y0, y1):
        row = "".join("#" if px[x, y] == (0x80, 0x00, 0x00) else "." for x in range(x0, x1))
        print('    "%s",' % row)
    print("};")
    # 校验：和原版实测的墨迹范围一致
    xs = [x for y in range(y0, y1) for x in range(x0, x1) if px[x, y] == (0x80, 0x00, 0x00)]
    ys = [y for y in range(y0, y1) for x in range(x0, x1) if px[x, y] == (0x80, 0x00, 0x00)]
    print("// measured ink x=%d..%d y=%d..%d" % (min(xs), max(xs), min(ys), max(ys)))


def dump_pen():
    im = Image.open(DLL).convert("RGB")
    px = im.load()
    x0, y0, x1, y1 = PEN_BOX
    print()
    print("// ---- red pen %dx%d (from shots\\ref_dll_page.png (%d,%d)) ----"
          % (x1 - x0, y1 - y0, x0, y0))
    print("// 0=透明 1=黑轮廓 #000000  2=暗红 #800000  3=亮红 #FF0000")
    print("static const char* g_szPen[%d] = {" % (y1 - y0))
    for y in range(y0, y1):
        row = ""
        for x in range(x0, x1):
            row += str(PEN_COLORS.get(px[x, y], 0))
        print('    "%s",' % row)
    print("};")


if __name__ == "__main__":
    dump_tick()
    dump_pen()
