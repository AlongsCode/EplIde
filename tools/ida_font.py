# -*- coding: utf-8 -*-
"""用 idalib + Hex-Rays 看原版 e.exe 里跟「字体 / 自绘标记」有关的实现。

用法:
    python ida_font.py fonts      # 找 CreateFont* 的调用点，打印所在函数的伪代码
    python ida_font.py <地址>     # 反编译某个函数（十六进制，如 0x5d4e62）
"""
import sys
import idapro          # noqa: F401  必须第一个 import
import ida_auto
import ida_bytes
import ida_funcs
import ida_hexrays
import ida_name
import idautils
import ida_nalt

DB = r"D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\ida_work\e_copy.exe"


def imports_matching(sub):
    out = {}
    for i in range(ida_nalt.get_import_module_qty()):
        def cb(ea, name, ord_):
            if name and sub.lower() in name.lower():
                out[ea] = name
            return True
        ida_nalt.enum_import_names(i, cb)
    return out


def decompile(f):
    try:
        cf = ida_hexrays.decompile(f.start_ea)
        return str(cf) if cf else None
    except Exception as e:
        return "// decompile failed: %s" % e


def funcs_using(ea):
    res = {}
    for xr in idautils.XrefsTo(ea, 0):
        f = ida_funcs.get_func(xr.frm)
        if f:
            res[f.start_ea] = f
    return res


def find_bytes(pat):
    """在数据库里搜字节串，返回命中地址"""
    hits = []
    import ida_segment, ida_idaapi
    for seg in idautils.Segments():
        s = ida_segment.getseg(seg)
        start, end = s.start_ea, s.end_ea
        ea = start
        while True:
            ea = ida_bytes.find_bytes(pat, ea, 0, end)
            if ea == ida_idaapi.BADADDR or ea >= end:
                break
            hits.append(ea)
            ea += 1
    return hits


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "fonts"
    idapro.open_database(DB, True)
    ida_auto.auto_wait()

    if mode == "find":
        pat = bytes.fromhex(sys.argv[2])
        dst = sys.argv[3] if len(sys.argv) > 3 else None
        out = []
        for h in find_bytes(pat):
            nm = ida_name.get_name(h) or ""
            out.append("--- hit @ %#x %s" % (h, nm))
            for xr in idautils.XrefsTo(h, 0):
                f = ida_funcs.get_func(xr.frm)
                out.append("    xref from %#x  func=%s" %
                           (xr.frm, hex(f.start_ea) if f else "?"))
                for a in range(max(0, xr.frm - 0x30), xr.frm + 0x10):
                    out.append("      %#08x %s" % (a, idc_generate(a)))
        txt = "\n".join(out)
        if dst:
            open(dst, "w", encoding="utf-8").write(txt)
            print("written ->", dst, len(txt))
        else:
            print(txt[:4000])
        idapro.close_database(False)
        return

    if mode == "calls":
        # 列出「引用了某个导入函数」的所有函数（找自绘流程线/三角的实现时用）
        sub = sys.argv[2]
        for ea, n in imports_matching(sub).items():
            fs = funcs_using(ea)
            print("== %s @ %#x  被 %d 个函数引用" % (n, ea, len(fs)))
            for fe, f in sorted(fs.items()):
                print("   func %#x size=%-6d refs=%d" %
                      (fe, f.end_ea - f.start_ea, len(list(idautils.XrefsTo(ea, 0)))))
        idapro.close_database(False)
        return

    if mode == "fonts":
        for sub in ("CreateFont", "CreateFontIndirect"):
            for ea, n in imports_matching(sub).items():
                fs = funcs_using(ea)
                print("== %s @ %#x  被 %d 个函数引用" % (n, ea, len(fs)))
                for fe, f in fs.items():
                    nm = ida_name.get_name(fe) or ""
                    print("   func %#x %-24s size=%d" % (fe, nm, f.end_ea - f.end_ea))
        # 打印引用点附近的指令
        print()
        for sub in ("CreateFontA", "CreateFontIndirectA"):
            for ea, n in imports_matching(sub).items():
                for xr in idautils.XrefsTo(ea, 0):
                    f = ida_funcs.get_func(xr.frm)
                    print("--- %s xref @ %#x (func %s)" % (n, xr.frm, hex(f.start_ea) if f else "?"))
                    start = xr.frm - 0x40
                    for h in idautils.Heads(start, xr.frm + 8):
                        print("      %#08x  %s" % (h, idc_generate(h)))
    else:
        out = []
        for tok in mode.split(","):
            ea = int(tok, 16) if tok.startswith("0x") else int(tok, 16)
            f = ida_funcs.get_func(ea)
            if not f:
                out.append("// no func at %#x" % ea)
                continue
            out.append("// ===== func %#x size=%d name=%s =====" %
                       (f.start_ea, f.end_ea - f.start_ea, ida_name.get_name(f.start_ea)))
            out.append(decompile(f) or "// no decompile")
        txt = "\n".join(out)
        dst = sys.argv[2] if len(sys.argv) > 2 else None
        if dst:
            open(dst, "w", encoding="utf-8").write(txt)
            print("written ->", dst, len(txt), "chars")
        else:
            print(txt)
    idapro.close_database(False)


def idc_generate(ea):
    import idc
    return idc.generate_disasm_line(ea, 0)


if __name__ == "__main__":
    main()
