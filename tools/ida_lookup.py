# -*- coding: utf-8 -*-
"""用 IDA 数据库查 e.exe 里工具条/面板控件的初始化代码。

用法: <py312> ida_lookup.py <关键字...>
例:   ida_lookup.py 标准工具条            # 找工具条初始化函数并打印其反汇编
      ida_lookup.py SysTreeView32        # 找树控件的 CreateWindowEx 样式
输出里重点看：CreateWindowExA 前面 push 的样式立即数、以及 SendMessageA 的 TB_*/TCM_* 消息号。
"""
import re
import sys

import idapro
import idc
import ida_auto
import ida_bytes
import ida_funcs
import ida_lines
import ida_ua
import idautils

DB = r"D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\ida_work\e_copy.exe"
# 想重点看的消息号（comctl32 工具条/标签控件）
WATCH = {
    0x400: "WM_USER", 0x411: "TB_SETSTATE", 0x416: "TB_SETIMAGELIST",
    0x41B: "TB_ADDBITMAP", 0x41D: "TB_GETITEMRECT", 0x421: "TB_SETBUTTONSIZE",
    0x42B: "TB_CHANGEBITMAP", 0x43C: "TB_BUTTONSTRUCTSIZE", 0x442: "TB_SETBUTTONINFO",
    0x400 + 14: "TB_ADDBUTTONS", 0x400 + 17: "TB_SETSTATE", 0x400 + 43: "TB_CHANGEBITMAP",
    0x1300 + 19: "TCM_SETIMAGELIST", 0x1300 + 22: "TCM_SETITEMSIZE",
}


def disasm(frm, back=120, fwd=200):
    out = []
    ea = frm
    for _ in range(back):
        p = ida_bytes.prev_head(ea, 0)
        if p == ida_bytes.BADADDR:
            break
        ea = p
        f = ida_funcs.get_func(ea)
        if f and ea < f.start_ea:
            break
    for _ in range(back + fwd):
        if ea == ida_bytes.BADADDR:
            break
        line = ida_lines.generate_disasm_line(ea, 0)
        line = ida_lines.tag_remove(line)
        mark = " <== HERE" if ea == frm else ""
        out.append("  %08X  %s%s" % (ea, line, mark))
        ea = ida_bytes.next_head(ea, ida_bytes.BADADDR)
    return out


def find_bytes(text):
    """在数据库里按字节找（GBK 中文用 idautils.Strings() 找不到，它只认 ASCII/拉丁）"""
    try:
        pat = text.encode("gbk")
    except Exception:
        pat = text.encode("ascii", "ignore")
    res = []
    for seg in idautils.Segments():
        s = idc.get_segm_start(seg)
        e = idc.get_segm_end(seg)
        data = ida_bytes.get_bytes(s, e - s)
        if not data:
            continue
        i = data.find(pat)
        while i >= 0:
            res.append(s + i)
            i = data.find(pat, i + 1)
    return res


def main(keys):
    idapro.open_database(DB, True)
    ida_auto.auto_wait()
    for seg in idautils.Segments():
        print('SEG %-8s %08X-%08X %d bytes' % (idc.get_segm_name(seg), idc.get_segm_start(seg), idc.get_segm_end(seg), idc.get_segm_end(seg)-idc.get_segm_start(seg)))
    for k in keys:
        print("=" * 70)
        eas = find_bytes(k)
        print("%s : %d 处字节匹配 %s" % (k, len(eas), [hex(e) for e in eas[:4]]))
        for ea in eas[:2]:
            xr = [x.frm for x in idautils.XrefsTo(ea)]
            print("   %08X xrefs=%s" % (ea, [hex(x) for x in xr[:6]]))
            for frm in xr[:1]:
                f = ida_funcs.get_func(frm)
                print("   --- %s 附近 ---" % (ida_funcs.get_func_name(f.start_ea) if f else "?"))
                for ln in disasm(frm, back=60, fwd=90):
                    print(ln)
    idapro.close_database(False)


if __name__ == "__main__":
    main(sys.argv[1:] or ["标准工具条"])
