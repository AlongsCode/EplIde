# -*- coding: utf-8 -*-
"""Find who paints the *dot grid* on the original window designer background.

Usage:
    python tools/ida_dots.py imports <sub,sub,...>   # list matching imports + xref funcs
    python tools/ida_dots.py xrefs <name_or_0xaddr>  # disasm around every xref
    python tools/ida_dots.py dec <0x,0x,..> [out]    # hexrays decompile funcs
    python tools/ida_dots.py gbk <text>              # search GBK bytes
"""
import sys
import idapro          # noqa: F401 must be first
import ida_auto
import ida_bytes
import ida_funcs
import ida_hexrays
import ida_name
import ida_nalt
import ida_segment
import idautils
import idc

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


def funcs_using(ea):
    res = {}
    for xr in idautils.XrefsTo(ea, 0):
        f = ida_funcs.get_func(xr.frm)
        if f:
            res.setdefault(f.start_ea, []).append(xr.frm)
    return res


def decompile(ea):
    f = ida_funcs.get_func(ea)
    if not f:
        return "// no func at %#x" % ea
    try:
        cf = ida_hexrays.decompile(f.start_ea)
        return str(cf) if cf else "// no decompile"
    except Exception as e:
        return "// decompile failed: %s" % e


def main():
    mode = sys.argv[1]
    idapro.open_database(DB, True)
    ida_auto.auto_wait()

    if mode == "imports":
        for sub in sys.argv[2].split(","):
            hits = imports_matching(sub)
            if not hits:
                print("== %-24s : no import" % sub)
            for ea, n in hits.items():
                fs = funcs_using(ea)
                print("== %-24s @ %#x  xref-funcs=%d" % (n, ea, len(fs)))
                for fe, sites in sorted(fs.items()):
                    print("     func %#x  sites=%s" % (fe, ",".join("%#x" % s for s in sites)))
    elif mode == "xrefs":
        t = sys.argv[2]
        ea = int(t, 16) if t.lower().startswith("0x") else ida_name.get_name_ea(ida_idaapi_BADADDR(), t)
        print("target %#x" % ea)
        for xr in idautils.XrefsTo(ea, 0):
            f = ida_funcs.get_func(xr.frm)
            print("--- xref @ %#x func=%s" % (xr.frm, hex(f.start_ea) if f else "?"))
            for h in idautils.Heads(max(0, xr.frm - 0x40), xr.frm + 0x10):
                print("      %#08x  %s" % (h, idc.generate_disasm_line(h, 0)))
    elif mode == "gbk":
        pat = sys.argv[2].encode("gbk")
        print("pattern %s" % pat.hex())
        for seg in idautils.Segments():
            s = ida_segment.getseg(seg)
            ea = s.start_ea
            while True:
                ea = ida_bytes.find_bytes(pat, ea, 0, s.end_ea)
                if ea == ida_idaapi_BADADDR or ea >= s.end_ea:
                    break
                f = ida_funcs.get_func(ea)
                print("hit %#x func=%s" % (ea, hex(f.start_ea) if f else "?"))
                ea += 1
    elif mode == "dec":
        out = []
        for tok in sys.argv[2].split(","):
            out.append("// ===== %s =====" % tok)
            out.append(decompile(int(tok, 16)))
        txt = "\n".join(out)
        dst = sys.argv[3] if len(sys.argv) > 3 else None
        if dst:
            open(dst, "w", encoding="utf-8").write(txt)
            print("written ->", dst, len(txt))
        else:
            print(txt)
    idapro.close_database(False)


def ida_idaapi_BADADDR():
    import ida_idaapi
    return ida_idaapi.BADADDR


if __name__ == "__main__":
    main()
