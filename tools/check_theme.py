# -*- coding: utf-8 -*-
"""Theme regression check.

Reads the dump written by EClone when ECLONE_DUMP_THEME is set and verifies:
  1. every DEF (built-in default palette) slot equals the value measured off the
     original e.exe (hard-coded table below, all values verified against
     shots\\live2\\view0.png and shots\\user_ref.png);
  2. layout-critical ACT slots are present and are legal COLORREFs;
  3. display options are within range.

usage: python check_theme.py shots/theme.txt
"""
import re
import sys

# 原版实测（shots\live2\view0.png 的 13 种纯色 + HANDOFF 颜色表）
EXPECT_DEF = {
    1:  0x000000,   # 通常前景（代码文字黑）
    17: 0x0000FF,   # 流程命令名前景（流程关键字 / 数据类型）蓝
    19: 0x800000,   # 成员方法名前景（支持库命令）紫红
    21: 0x008000,   # 备注或草稿前景 绿
    24: 0x000080,   # 操作符前景（括号运算符）藏青
    31: 0x008080,   # 字符串前景 青
    34: 0x808080,   # 连接线（流程控制虚线）
    35: 0x888888,   # 表格线（网格）
    38: 0x800000,   # 参数开启状态指示器（✔）
    40: 0xECECEC,   # 标志列背景（流程槽底）
    9:  0xE6EDE4,   # 子程序定义表头背景
    11: 0xD9E3F0,   # 子程序变量表头背景
    42: 0x0000FF,   # 数据类型
}

# ACT（当前生效配色）里必须存在的槽位 —— 布局关键，缺一个编辑器就画崩
MUST_HAVE_ACT = [1, 17, 19, 21, 24, 31, 34, 35, 38, 40, 9, 11, 42]


def main(path):
    act, dfn, opt, scheme = {}, {}, {}, ""
    # EClone 是 MBCS 编译，倒档文件是 GBK（SCHEME 行是中文配色名）
    for line in open(path, encoding="gbk", errors="replace"):
        line = line.strip()
        m = re.match(r"^(ACT|DEF) (\d+) ([0-9A-Fa-f]{6})$", line)
        if m:
            (act if m.group(1) == "ACT" else dfn)[int(m.group(2))] = int(m.group(3), 16)
            continue
        m = re.match(r"^OPT (\w+) (-?\d+)$", line)
        if m:
            opt[m.group(1)] = int(m.group(2))
            continue
        if line.startswith("SCHEME "):
            scheme = line[7:]
            continue

    ok = fail = 0

    def chk(cond, msg):
        nonlocal ok, fail
        if cond:
            ok += 1
        else:
            fail += 1
            print("  FAIL %s" % msg)

    chk(len(dfn) >= 40, "DEF 槽位数量 %d（应 >= 40）" % len(dfn))
    for slot, want in sorted(EXPECT_DEF.items()):
        got = dfn.get(slot)
        chk(got == want, "DEF slot %d = %s，期望 %06X" % (
            slot, "缺失" if got is None else "%06X" % got, want))

    for slot in MUST_HAVE_ACT:
        chk(slot in act, "ACT 缺槽位 %d" % slot)
    for slot, v in act.items():
        chk(0 <= v <= 0xFFFFFF, "ACT slot %d 颜色非法 %06X" % (slot, v))

    chk(8 <= opt.get("LINEPITCH", 0) <= 64, "LINEPITCH=%s" % opt.get("LINEPITCH"))
    chk(6 <= opt.get("FONTH", 0) <= 72, "FONTH=%s" % opt.get("FONTH"))
    for k in ("FLOWDOT", "GRIDDOT", "DRAWGRID", "DRAWSEP", "DRAWFLOW", "DRAWPARAM", "SHOWPARAM"):
        chk(opt.get(k) in (0, 1), "OPT %s=%s" % (k, opt.get(k)))

    print("%d passed, %d failed   (scheme=%r)" % (ok, fail, scheme))
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
