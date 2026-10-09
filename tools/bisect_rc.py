# -*- coding: utf-8 -*-
"""二分定位：菜单 rc 里哪一行让 VC6 的 RC.EXE 解析错乱。

做法:
  1) 把 menu_main.rc 的每条 MENUITEM 文本替换为 "X"，看是否还报错 -> 判断是否文本内容问题
  2) 若替换后正常，则用逐段保留原文的方式二分出具体行
"""
import os
import re
import subprocess
import sys

ROOT = r"D:\eclone"
SRC = os.path.join(ROOT, "src")
GBK = os.path.join(ROOT, "gbk")
RC = r"C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98\Bin\RC.EXE"
ENV_INCLUDE = (r"C:\Program Files (x86)\Microsoft Visual Studio\VC98\Include;"
               r"C:\Program Files (x86)\Microsoft Visual Studio\VC98\MFC\Include")
ENC = "gbk"


def build_rc(menu_text, out_name):
    head = open(os.path.join(SRC, "rc_head.rc"), encoding="utf-8-sig").read()
    res = open(os.path.join(SRC, "resource.h"), encoding="utf-8-sig").read()
    panes = open(os.path.join(SRC, "panes.rc"), encoding="utf-8-sig").read()
    txt = head + "\n" + res + "\n" + menu_text + "\n" + panes
    p = os.path.join(GBK, out_name)
    open(p, "w", encoding=ENC, errors="replace").write(txt)
    return p


def run_rc(path):
    env = dict(os.environ)
    env["INCLUDE"] = ENV_INCLUDE
    r = subprocess.run([RC, "/fo", path + ".res", path], capture_output=True,
                       env=env, cwd=GBK)
    out = (r.stdout + r.stderr).decode(ENC, "replace")
    ok = "error" not in out.lower()
    return ok, out.strip().replace("\n", " | ")[:300]


def main():
    menu = open(os.path.join(SRC, "menu_main.rc"), encoding="utf-8-sig").read()
    # 1) 文本全部替换成 X
    stripped = re.sub(r'"(?:[^"]*)"', '"X"', menu)
    p = build_rc(stripped, "b_stripped.rc")
    ok, msg = run_rc(p)
    print("全部文本替换为 X :", "OK" if ok else "FAIL", msg)

    if not ok:
        print("=> 与文本内容无关，问题在结构")
        return

    # 2) 逐行二分：逐步把原始行还原回去
    orig_lines = menu.split("\n")
    strip_lines = stripped.split("\n")
    lo, hi = 0, len(orig_lines)
    # 先找第一个出问题的前缀长度
    def test_k(n):
        mixed = "\n".join(orig_lines[:n] + strip_lines[n:])
        pp = build_rc(mixed, "b_mix.rc")
        o, m = run_rc(pp)
        return o, m

    if test_k(hi)[0]:
        print("原文整体也 OK ?!")
        return
    bad = hi
    step = hi
    while step > 1:
        step = max(1, step // 2)
        while bad - step >= 0 and not test_k(bad - step)[0]:
            bad -= step
    print("第一个触发错误的原文行号 ≈", bad)
    for i in range(max(0, bad - 3), min(len(orig_lines), bad + 3)):
        mark = ">>" if i == bad - 1 else "  "
        print("%s %4d: %s" % (mark, i + 1, orig_lines[i][:110]))


if __name__ == "__main__":
    main()
