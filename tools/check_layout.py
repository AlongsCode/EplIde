# -*- coding: utf-8 -*-
"""EClone 布局回归检查：拿 tools\\shot.ps1 导出的窗口树 dump 对照原版实测值。

用法: python check_layout.py <shots\\mNN.windows.txt> ...
      （不传参数就自动取 shots 目录里最新的一份 m*.windows.txt）

检查的都是"改坏了会立刻看出来"的不变量，全部来自原版实测：
  工具条：标准/对齐 行高 25，三行间距 23
  工作夹：pane 宽 229；内层 #32770 在 pane 客户区原点；标签条在最下面 23px，内容在上方
  状态夹：pane 高 188；标签条在最上面 23px，内容在下方
  面板窗口类名 = sizecbar 的 Afx:400000:8:...
"""
import glob
import os
import re
import sys

LINE = re.compile(r"^\s*(\S+)\s+vis=(\w+)\s+(-?\d+),(-?\d+)\s+(\d+)x(\d+)\s*(.*)$")


def parse(path):
    wins = []
    # shot.ps1 用 -Encoding UTF8 写（带 BOM）
    for ln in open(path, encoding="utf-8-sig", errors="replace"):
        m = LINE.match(ln.rstrip("\n"))
        if m:
            wins.append(dict(cls=m.group(1), vis=m.group(2) == "True",
                             x=int(m.group(3)), y=int(m.group(4)),
                             w=int(m.group(5)), h=int(m.group(6)), text=m.group(7).strip()))
    return wins


def find(wins, cls=None, text=None, vis=None):
    for w in wins:
        if cls and w["cls"] != cls:
            continue
        if text and w["text"] != text:
            continue
        if vis is not None and w["vis"] != vis:
            continue
        return w
    return None


def pane(wins, text):
    """面板本体（sizecbar 窗口）：类名 Afx:400000:8:...；
    注意 dock bar 的窗口文本和面板同名，别抓错。"""
    for w in wins:
        if w["text"] == text and w["cls"].startswith("Afx:400000:8"):
            return w
    return None


def main(paths):
    ok = fail = 0

    def check(name, cond, detail):
        nonlocal ok, fail
        if cond:
            ok += 1
            print("  OK   %s (%s)" % (name, detail))
        else:
            fail += 1
            print("  FAIL %s (%s)" % (name, detail))

    for path in paths:
        wins = parse(path)
        print("== %s ==" % os.path.basename(path))

        std = find(wins, "ToolbarWindow32", "标准工具条")
        align = find(wins, "ToolbarWindow32", "对齐工具条")
        sel = find(wins, None, "选择器")
        check("标准工具条 25px 高", std and std["h"] == 25, std and "%dx%d" % (std["w"], std["h"]))
        check("对齐工具条 25px 高", align and align["h"] == 25, align and "%dx%d" % (align["w"], align["h"]))
        check("选择器条 25px 高", sel and sel["h"] == 25, sel and "%dx%d" % (sel["w"], sel["h"]))
        if std and align and sel:
            check("三行间距 23px", align["y"] - std["y"] == 23 and sel["y"] - align["y"] == 23,
                  "y=%d/%d/%d" % (std["y"], align["y"], sel["y"]))

        work = pane(wins, "工作夹")
        stat = pane(wins, "状态夹")
        check("工作夹 pane 宽 229", work and work["w"] == 229, work and str(work["w"]))
        check("状态夹 pane 高 188", stat and stat["h"] == 188, stat and str(stat["h"]))
        for nm, p in (("工作夹", work), ("状态夹", stat)):
            check("%s 窗口类=sizecbar" % nm, p and p["cls"].startswith("Afx:400000:8"),
                  p and p["cls"])

        # 内层 #32770：左右停靠 -> pane+(5,17)；上下停靠 -> pane+(17,5)
        if work and stat:
            hw = [w for w in wins if w["cls"] == "#32770" and w["text"] == "工作夹"]
            hs = [w for w in wins if w["cls"] == "#32770" and w["text"] == "状态夹"]
            if hw and hs:
                hw, hs = hw[0], hs[0]
                check("工作夹内层 #32770 偏移 (5,17)",
                      (hw["x"] - work["x"], hw["y"] - work["y"]) == (5, 17),
                      "(%d,%d)" % (hw["x"] - work["x"], hw["y"] - work["y"]))
                check("状态夹内层 #32770 偏移 (17,5)",
                      (hs["x"] - stat["x"], hs["y"] - stat["y"]) == (17, 5),
                      "(%d,%d)" % (hs["x"] - stat["x"], hs["y"] - stat["y"]))

                # 工作夹：标签条在最下面 23px，内容在它上方（不能压进标签行）
                tree = find(wins, "SysTreeView32", None, vis=True)
                tab = [w for w in wins if w["cls"] == "SysTabControl32" and w["y"] < stat["y"]]
                if tree and tab:
                    tab = tab[0]
                    check("工作夹标签条在 pane 底部 23px",
                          hw["y"] + hw["h"] - 3 - (tab["y"] + tab["h"]) <= 1,
                          "tab.bottom=%d host.bottom=%d" % (tab["y"] + tab["h"], hw["y"] + hw["h"] - 3))
                    check("工作夹内容在标签行上方",
                          tree["y"] >= tab["y"] + 3 and tree["y"] + tree["h"] <= tab["y"] + tab["h"] - 23,
                          "tree=%d..%d tab=%d..%d" % (tree["y"], tree["y"] + tree["h"],
                                                      tab["y"], tab["y"] + tab["h"]))

                # 状态夹：标签条在最上面 23px，内容在下方
                etabs = [w for w in wins if w["cls"] == "SysTabControl32" and w["y"] > stat["y"]]
                edits = [w for w in wins if w["cls"] == "Edit" and w["vis"] and w["y"] > stat["y"]]
                if etabs and edits:
                    check("状态夹标签条在 pane 顶部", etabs[0]["y"] - hs["y"] <= 4,
                          "tab.y=%d host.y=%d" % (etabs[0]["y"], hs["y"]))
                    check("状态夹内容在标签行下方",
                          edits[0]["y"] >= etabs[0]["y"] + 23 and
                          edits[0]["y"] + edits[0]["h"] <= etabs[0]["y"] + etabs[0]["h"],
                          "edit=%d..%d tab=%d..%d" % (edits[0]["y"], edits[0]["y"] + edits[0]["h"],
                                                      etabs[0]["y"], etabs[0]["y"] + etabs[0]["h"]))

    print("\n%d passed, %d failed" % (ok, fail))
    return 1 if fail else 0


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args:
        cands = sorted(glob.glob(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                              "..", "shots", "m*.windows.txt")),
                       key=os.path.getmtime)
        args = cands[-1:]
    sys.exit(main(args))
