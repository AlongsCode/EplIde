# -*- coding: utf-8 -*-
"""Print one dialog template's control inventory from tools\\dump_dialogs.py's output.

usage: python tools\\dialog_inv.py shots\\e_dialogs.txt 282

Why: the original e.exe's dialog templates are the authority for every dialog EClone draws.
This prints the class histogram (so "is it a tab control or group boxes?" is answered by data)
plus every control's index / id / class / position / size / text.
"""
import io
import re
import sys


def main():
    path, want = sys.argv[1], sys.argv[2]
    # ⚠️ dump_dialogs.py 写出来的是 **UTF-8**（就用这个读）。本会话第 16 批先误判成 GBK 改过一次，
    #    结果读出来是"二次乱码"（瀵绘壘鏇挎崲瀵硅瘽妗�）——**中文显示成乱码是 GBK 控制台的锅，不是文件**。
    #    要核对中国字：把结果写成 UTF-8 文件，再用能按 UTF-8 显示的工具看（别信控制台）。
    s = io.open(path, encoding="utf-8", errors="replace").read()
    for b in re.split(r"(?=^DIALOG id=)", s, flags=re.M):
        m = re.match(r"DIALOG id=(\d+)", b)
        if not m or m.group(1) != want:
            continue
        head = [l for l in b.split("\n") if l.strip()][:2]
        print("=== " + " | ".join(h.strip() for h in head))
        items = re.findall(r"\[(\d+)\] id=(\d+)\s+(\S+)\s+(\S+)\s+(\S+)\s+style=(\S+)(.*?)(?=\n\s*\[|\Z)",
                           b, re.S)
        hist = {}
        for idx, cid, cls, pos, size, style, rest in items:
            t = re.search(r"text='([^']*)'", rest)
            txt = t.group(1) if t else ""
            hist[cls] = hist.get(cls, 0) + 1
            tags = " ".join(sorted(set(re.findall(
                r"(BS_GROUPBOX|BS_AUTOCHECKBOX|BS_AUTORADIOBUTTON|BS_DEFPUSHBUTTON|"
                r"CBS_DROPDOWN|CBS_DROPDOWNLIST|LBS_OWNERDRAWFIXED|TVS_HASBUTTONS|LVS_SMALLICON)",
                cls + " " + rest))))
            print("  [%s] id=%-6s %-16s %-10s %-9s %-42s %s" % (idx, cid, cls, pos, size, txt[:42], tags))
        print("--- class histogram:", hist)
        return
    print("dialog id=%s not found" % want)
    sys.exit(2)


main()
