# -*- coding: utf-8 -*-
"""看核心支持库 krnln.fne 的实现（idalib + Hex-Rays headless）。

用法:
    python ida_krnln.py survey                 # 概况：架构/入口/导出/导入分类/段/函数数
    python ida_krnln.py base <outdir>           # 一次进程倒出基线：survey/allnames/big/exports/imports/strings
    python ida_krnln.py big [N]                # 按体积列前 N 个函数（找大子系统）
    python ida_krnln.py allnames <out.txt>     # 全量函数表（addr/size/name），离线 grep 用
    python ida_krnln.py gbk <文字>              # 原始字节搜 GBK 串（IDA 认不出中文串时用）
    python ida_krnln.py dw <地址> [count]       # 读静态映像里的 dword
    python ida_krnln.py imports [子串]          # 列导入（可按子串过滤）
    python ida_krnln.py calls <子串>            # 「引用了某导入函数」的函数清单
    python ida_krnln.py decimp <子串> [out]     # 上者的加强版：直接把引用函数反编译出来
    python ida_krnln.py decstr <子串> [out]     # 字符串 → 引用函数 → 反编译
    python ida_krnln.py decname <子串> [out]    # 按函数名子串反编译
    python ida_krnln.py str <子串>              # 搜字符串（大小写不敏感）
    python ida_krnln.py xref <名字或0x地址>      # 谁引用了它
    python ida_krnln.py dec 0x1234,0x5678 [out] # 反编译若干函数
    python ida_krnln.py report <out.txt> "0x地址,..."  # 一次进程里反编译 + 倒出基线
    python ida_krnln.py funcs <子串>            # 按名字列函数

⚠️ 第一次跑会做自动分析（1.6MB 的 DLL，约 1~3 分钟），之后的 .id0/.id1 会留在同目录。
"""
import os
import sys
import idapro          # noqa: F401  必须第一个 import
import ida_auto
import ida_bytes
import ida_idaapi
import ida_entry
import ida_funcs
import ida_hexrays
import ida_name
import ida_nalt
import ida_segment
import idautils
import idc

DB = os.environ.get("IDA_DB") or \
    r"D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\ida_work\krnln.fne"
# 换目标：set IDA_DB=...\ida_work\e_copy.exe（原版 IDE）。默认是核心支持库 krnln.fne。


def imports_all():
    out = []
    for i in range(ida_nalt.get_import_module_qty()):
        mod = ida_nalt.get_import_module_name(i) or "?"

        def cb(ea, name, ord_):
            out.append((mod, ea, name or ("ord#%d" % ord_)))
            return True
        ida_nalt.enum_import_names(i, cb)
    return out


def funcs_using(ea):
    res = {}
    for xr in idautils.XrefsTo(ea, 0):
        f = ida_funcs.get_func(xr.frm)
        if f:
            res[f.start_ea] = f
    return res


def decompile(f):
    if f is None:
        return "// no func"
    try:
        cf = ida_hexrays.decompile(f.start_ea)
        return str(cf) if cf else "// decompile returned none"
    except Exception as e:
        return "// decompile failed: %s" % e


def hdr(f):
    return "// ===== func %#x size=%d name=%s =====" % (
        f.start_ea, f.end_ea - f.start_ea, ida_name.get_name(f.start_ea) or "")


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "survey"
    idapro.open_database(DB, True)
    ida_auto.auto_wait()

    if mode == "survey":
        print("== 基本信息 ==")
        print("  input :", ida_nalt.get_input_file_path())
        print("  arch  :", "64bit" if ida_idaapi_is64() else "32bit")
        print("  funcs :", ida_funcs.get_func_qty())
        print("== 段 ==")
        for s in idautils.Segments():
            sg = ida_segment.getseg(s)
            nm = ida_segment.get_segm_name(sg)
            print("  %-10s %#010x..%#010x  %d bytes" % (nm, sg.start_ea, sg.end_ea, sg.end_ea - sg.start_ea))
        print("== 导出 ==")
        n = 0
        for i in range(ida_entry.get_entry_qty()):
            ord_ = ida_entry.get_entry_ordinal(i)
            ea = ida_entry.get_entry(ord_)
            nm = ida_entry.get_entry_name(ord_) or ""
            print("  [%4d] %#010x  %s" % (ord_, ea, nm))
            n += 1
            if n > 80:
                print("  ... 更多略")
                break
        print("  导出总数:", ida_entry.get_entry_qty())
        imps = imports_all()
        print("== 导入（%d 条）==" % len(imps))
        from collections import Counter
        cnt = Counter(m for m, _, _ in imps)
        for m, c in cnt.most_common():
            print("  %-24s %d" % (m, c))
        idapro.close_database(True)   # 存盘：不然下次打开 func_qty=0，分析白做
        return

    if mode == "imports":
        sub = sys.argv[2].lower() if len(sys.argv) > 2 else ""
        for mod, ea, nm in imports_all():
            if sub in nm.lower():
                print("  %#010x  %-20s %s" % (ea, mod, nm))
        idapro.close_database(True)   # 存盘：不然下次打开 func_qty=0，分析白做
        return

    if mode == "funcs":
        sub = sys.argv[2]
        for fe in idautils.Functions():
            nm = ida_name.get_name(fe) or ""
            if sub.lower() in nm.lower():
                f = ida_funcs.get_func(fe)
                print("  %#010x size=%-6d %s" % (fe, (f.end_ea - f.start_ea) if f else 0, nm))
        idapro.close_database(True)   # 存盘：不然下次打开 func_qty=0，分析白做
        return

    if mode == "base":
        # 一次进程里倒出整套基线（survey / 段 / 导入 / 导出 / 函数表 / 大函数 / 字符串表）。
        # base <outdir>
        import os as _os
        d = sys.argv[2]
        if not _os.path.isdir(d):
            _os.makedirs(d)

        def w(name, lines):
            p = _os.path.join(d, name)
            open(p, "w", encoding="utf-8", errors="replace").write("\n".join(lines))
            print("written ->", p, len(lines), "lines")

        info = ["input : %s" % ida_nalt.get_input_file_path(),
                "arch  : %s" % ("64bit" if ida_idaapi_is64() else "32bit"),
                "funcs : %d" % ida_funcs.get_func_qty(),
                "imagebase: %#x" % ida_nalt.get_imagebase()]
        for s in idautils.Segments():
            sg = ida_segment.getseg(s)
            info.append("seg %-10s %#010x..%#010x %d bytes" %
                        (ida_segment.get_segm_name(sg), sg.start_ea, sg.end_ea, sg.end_ea - sg.start_ea))
        w("survey.txt", info)

        rows = []
        for fe in idautils.Functions():
            f = ida_funcs.get_func(fe)
            rows.append(((f.end_ea - f.start_ea) if f else 0, fe, ida_name.get_name(fe) or ""))
        w("allnames.txt", ["%#010x\t%d\t%s" % (fe, sz, nm) for sz, fe, nm in rows])
        rows.sort(reverse=True)
        w("big.txt", ["%#010x size=%-7d %s" % (fe, sz, nm) for sz, fe, nm in rows])

        exps = []
        for i in range(ida_entry.get_entry_qty()):
            ord_ = ida_entry.get_entry_ordinal(i)
            exps.append("[%4d] %#010x %s" % (ord_, ida_entry.get_entry(ord_),
                                             ida_entry.get_entry_name(ord_) or ""))
        w("exports.txt", exps)

        imps = imports_all()
        w("imports.txt", ["%#010x %-16s %s" % (ea, mod, nm) for mod, ea, nm in
                          sorted(imps, key=lambda x: (x[0], x[2]))])

        ss = []
        for s in idautils.Strings():
            ss.append("%#010x\t%d\t%s" % (s.ea, s.length, str(s)))
        w("strings.txt", ss)
        idapro.close_database(True)
        return

    if mode == "decxref":
        # 若干地址 → 引用它们的函数 → 全部反编译。decxref "0xaddr,0xaddr" [out.txt]
        dst = sys.argv[3] if len(sys.argv) > 3 else None
        addrs = []
        for tok in sys.argv[2].split(","):
            tok = tok.strip()
            if not tok:
                continue
            addrs.append(int(tok, 16) if tok.lower().startswith("0x") else int(tok, 10))
        funcs = {}
        for ea in addrs:
            for xr in idautils.XrefsTo(ea, 0):
                f = ida_funcs.get_func(xr.frm)
                if f:
                    funcs.setdefault(f.start_ea, []).append((xr.frm, ea))
        out = ["== 目标 %d 个地址，引用函数 %d 个 ==" % (len(addrs), len(funcs))]
        for fe, sites in sorted(funcs.items()):
            f = ida_funcs.get_func(fe)
            out.append("")
            out.append("// 引用点 " + " | ".join("%#x->%#x" % (a, t) for a, t in sites[:6]))
            out.append(hdr(f))
            out.append(decompile(f) or "// no decompile")
        txt = "\n".join(out)
        if dst:
            open(dst, "w", encoding="utf-8", errors="replace").write(txt)
            print("written ->", dst, len(txt), "chars,", len(funcs), "funcs")
        else:
            print(txt)
        idapro.close_database(True)
        return

    if mode == "batch":
        # 一次进程里按 spec 文件批量倒库（省掉反复开关库的开销与随机 open rc=4）。
        # batch <spec.txt> <out.txt>
        # spec 每行一条：
        #   data <hexaddr> <count>      读 dword
        #   cstr <hexaddr>              读 GBK C 串
        #   func <hexaddr>              反编译该函数
        #   name <子串>                 反编译函数名含子串的函数
        #   imp  <子串>                 反编译引用了该导入的函数
        #   str  <子串>                 反编译引用了该字符串的函数
        #   gbk  <文字>                 按 GBK 原始字节找串，再反编译引用它的函数
        #   pat  <hexbytes>             搜字节模式（支持 ? 通配），再反编译命中处的函数
        #   xref <hexaddr>              反编译引用了该地址的函数
        dst = sys.argv[3]
        spec = open(sys.argv[2], "r", encoding="utf-8").read().splitlines()
        out = []
        todo = []          # (标注, 函数起始地址)
        seen = set()
        for line in spec:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(None, 2)
            kind = parts[0].lower()
            if kind == "data":
                ea = int(parts[1], 16)
                n = int(parts[2]) if len(parts) > 2 else 16
                out.append("== data %#x x%d ==" % (ea, n))
                for i in range(n):
                    a = ea + i * 4
                    v = ida_bytes.get_dword(a)
                    out.append("  %#010x : %#010x  %s" % (a, v, ida_name.get_name(v) or ""))
                continue
            if kind == "dis":
                # 反汇编一段（不看函数边界，能看没被定义成函数的区域）。dis <hexstart> <count>
                a = int(parts[1], 16)
                n = int(parts[2]) if len(parts) > 2 else 40
                out.append("== dis %#x x%d ==" % (a, n))
                for _ in range(n):
                    f = ida_funcs.get_func(a)
                    out.append("   %#010x  %-40s %s" % (
                        a, idc.generate_disasm_line(a, 0) or "?",
                        ("func=%#x" % f.start_ea) if f else ""))
                    a = idc.next_head(a, a + 16)
                    if a == ida_idaapi.BADADDR:
                        break
                continue
            if kind == "lx":
                a = int(parts[1], 16)
                out.append("== xrefs to %#x ==" % a)
                for xr in idautils.XrefsTo(a, 0):
                    f = ida_funcs.get_func(xr.frm)
                    out.append("   %#x  func=%s %s" % (xr.frm, hex(f.start_ea) if f else "?",
                                                       (ida_name.get_name(f.start_ea) or "") if f else ""))
                continue
            if kind == "cstr":
                a = int(parts[1], 16)
                raw = ida_bytes.get_bytes(a, 200).split(b"\x00")[0]
                try:
                    out.append("== cstr %#x = %s" % (a, raw.decode("gbk")))
                except Exception:
                    out.append("== cstr %#x = %r" % (a, raw))
                continue
            if kind == "pat":
                # ⚠️ pat 的模式串自己带空格，必须取整行剩余部分（用 split(None,2) 会只剩 "B8"）
                pattern = line[3:].strip()
                hits = []
                ea = 0
                while True:
                    ea = ida_bytes.find_bytes(pattern, ea)
                    if ea == ida_idaapi.BADADDR:
                        break
                    hits.append(ea)
                    ea += 1
                out.append("== pat %s 命中 %d ==" % (pattern, len(hits)))
                for h in hits[:40]:
                    f = ida_funcs.get_func(h)
                    out.append("   %#x  func=%s %s" % (h, hex(f.start_ea) if f else "?",
                                                       (ida_name.get_name(f.start_ea) or "") if f else ""))
                    if f:
                        todo.append(("pat %s @ %#x" % (pattern, h), f.start_ea))
                continue
            if kind == "gbk":
                try:
                    b = parts[1].encode("gbk")
                except Exception:
                    b = parts[1].encode("latin1")
                hexpat = " ".join("%02X" % x for x in b)
                hits = []
                ea = 0
                while True:
                    ea = ida_bytes.find_bytes(hexpat, ea)
                    if ea == ida_idaapi.BADADDR:
                        break
                    hits.append(ea)
                    ea += 1
                out.append("== gbk %s (%s) 命中 %d ==" % (parts[1], hexpat, len(hits)))
                for h in hits[:40]:
                    f = ida_funcs.get_func(h)
                    out.append("   %#x  func=%s" % (h, hex(f.start_ea) if f else "?"))
                    for xr in idautils.XrefsTo(h, 0):
                        ff = ida_funcs.get_func(xr.frm)
                        if ff:
                            todo.append(("gbk %s -> %#x" % (parts[1], xr.frm), ff.start_ea))
                    if f:
                        todo.append(("gbk(自含) %s" % parts[1], f.start_ea))
                continue
            if kind == "func":
                todo.append(("func " + parts[1], int(parts[1], 16)))
                continue
            sub = parts[1] if len(parts) > 1 else ""
            hits = []
            if kind == "name":
                for fe in idautils.Functions():
                    if sub.lower() in (ida_name.get_name(fe) or "").lower():
                        hits.append(fe)
            elif kind == "imp":
                for mod, ea, nm in imports_all():
                    if sub.lower() in nm.lower():
                        for xr in idautils.XrefsTo(ea, 0):
                            hits.append(xr.frm)
            elif kind == "str":
                for s in idautils.Strings():
                    if sub.lower() in str(s).lower():
                        for xr in idautils.XrefsTo(s.ea, 0):
                            hits.append(xr.frm)
            elif kind == "xref":
                for xr in idautils.XrefsTo(int(sub, 16), 0):
                    hits.append(xr.frm)
            else:
                out.append("!! 未知指令 %s" % line)
                continue
            for h in hits:
                f = ida_funcs.get_func(h)
                if f:
                    todo.append(("%s %s" % (kind, sub), f.start_ea))
        out.append("== 待反编译函数 %d 个 ==" % len(set(e for _, e in todo)))
        for label, fe in todo:
            if fe in seen:
                continue
            seen.add(fe)
            f = ida_funcs.get_func(fe)
            out.append("")
            out.append("// 来源: %s" % label)
            out.append(hdr(f) if f else "// no func %#x" % fe)
            out.append(decompile(f) if f else "")
        txt = "\n".join(out)
        open(dst, "w", encoding="utf-8", errors="replace").write(txt)
        print("written ->", dst, len(txt), "chars,", len(seen), "funcs")
        idapro.close_database(True)
        return

    if mode == "big":
        # 按体积列函数 —— 找"大子系统"的第一刀。big [N]
        n = int(sys.argv[2]) if len(sys.argv) > 2 else 40
        rows = []
        for fe in idautils.Functions():
            f = ida_funcs.get_func(fe)
            rows.append(((f.end_ea - f.start_ea) if f else 0, fe, ida_name.get_name(fe) or ""))
        rows.sort(reverse=True)
        for size, fe, nm in rows[:n]:
            print("  %#010x size=%-7d %s" % (fe, size, nm))
        print("  （函数总数 %d）" % len(rows))
        idapro.close_database(True)
        return

    if mode == "allnames":
        # 全量函数表落盘（离线 grep/统计用）。allnames <out.txt>
        dst = sys.argv[2]
        out = []
        for fe in idautils.Functions():
            f = ida_funcs.get_func(fe)
            out.append("%#010x\t%d\t%s" % (fe, (f.end_ea - f.start_ea) if f else 0,
                                           ida_name.get_name(fe) or ""))
        open(dst, "w", encoding="utf-8").write("\n".join(out))
        print("written ->", dst, len(out), "funcs")
        idapro.close_database(True)
        return

    if mode == "gbk":
        # 原始字节搜 GBK 串（IDA 的 Strings() 常常认不出中文串）。gbk <文字>
        txt = sys.argv[2]
        try:
            pat = txt.encode("gbk")
        except Exception:
            pat = txt.encode("latin1")
        hexpat = " ".join("%02X" % b for b in pat)
        ea = 0
        hits = []
        while True:
            ea = ida_bytes.find_bytes(hexpat, ea)
            if ea == ida_idaapi.BADADDR:
                break
            hits.append(ea)
            ea += 1
        print("== GBK %r (%s) 命中 %d 处 ==" % (txt, hexpat, len(hits)))
        for h in hits[:60]:
            f = ida_funcs.get_func(h)
            print("  %#010x func=%s %s" % (h, hex(f.start_ea) if f else "?",
                                           (ida_name.get_name(f.start_ea) or "") if f else ""))
        idapro.close_database(True)
        return

    if mode == "dw":
        # 读数据（静态映像里的 dword）。dw <addr> [count]
        ea = int(sys.argv[2], 16) if sys.argv[2].lower().startswith("0x") else int(sys.argv[2], 16)
        n = int(sys.argv[3]) if len(sys.argv) > 3 else 16
        for i in range(n):
            a = ea + i * 4
            v = ida_bytes.get_dword(a)
            nm = ida_name.get_name(v) or ""
            print("  %#010x : %#010x  %s" % (a, v, nm))
        idapro.close_database(True)
        return

    if mode == "decstr":
        # 字符串 → 引用它的函数 → 直接反编译（一个进程里全做完）。decstr <子串> [out.txt]
        sub = sys.argv[2]
        dst = sys.argv[3] if len(sys.argv) > 3 else None
        targets = []
        for s in idautils.Strings():
            t = str(s)
            if sub.lower() in t.lower():
                targets.append(("%s @ %#x" % (t[:70], s.ea), s.ea))
        funcs = {}
        for label, ea in targets:
            for xr in idautils.XrefsTo(ea, 0):
                f = ida_funcs.get_func(xr.frm)
                if f:
                    funcs.setdefault(f.start_ea, []).append((xr.frm, label))
        out = ["== 串命中 %d 个，引用函数 %d 个 ==" % (len(targets), len(funcs))]
        for label, ea in targets[:80]:
            out.append("  target %s" % label)
        for fe, sites in sorted(funcs.items()):
            f = ida_funcs.get_func(fe)
            out.append("")
            out.append("// 引用点 " + " | ".join("%#x(%s)" % (a, l[:50]) for a, l in sites[:6]))
            out.append(hdr(f))
            out.append(decompile(f) or "// no decompile")
        txt = "\n".join(out)
        if dst:
            open(dst, "w", encoding="utf-8", errors="replace").write(txt)
            print("written ->", dst, len(txt), "chars,", len(funcs), "funcs")
        else:
            print(txt)
        idapro.close_database(True)
        return

    if mode == "decimp":
        # 导入函数 → 引用它的函数 → 直接反编译。decimp <子串> [out.txt]
        sub = sys.argv[2]
        dst = sys.argv[3] if len(sys.argv) > 3 else None
        targets = [(nm, ea) for mod, ea, nm in imports_all() if sub.lower() in nm.lower()]
        funcs = {}
        for nm, ea in targets:
            for xr in idautils.XrefsTo(ea, 0):
                f = ida_funcs.get_func(xr.frm)
                if f:
                    funcs.setdefault(f.start_ea, []).append((xr.frm, nm))
        out = ["== 导入命中 %d 个，引用函数 %d 个 ==" % (len(targets), len(funcs))]
        for nm, ea in targets:
            out.append("  import %s @ %#x" % (nm, ea))
        for fe, sites in sorted(funcs.items()):
            f = ida_funcs.get_func(fe)
            out.append("")
            out.append("// 引用点 " + " | ".join("%#x(%s)" % (a, l) for a, l in sites[:6]))
            out.append(hdr(f))
            out.append(decompile(f) or "// no decompile")
        txt = "\n".join(out)
        if dst:
            open(dst, "w", encoding="utf-8", errors="replace").write(txt)
            print("written ->", dst, len(txt), "chars,", len(funcs), "funcs")
        else:
            print(txt)
        idapro.close_database(True)
        return

    if mode == "decname":
        # 按函数名子串反编译。decname <子串> [out.txt]
        sub = sys.argv[2].lower()
        dst = sys.argv[3] if len(sys.argv) > 3 else None
        out = []
        n = 0
        for fe in idautils.Functions():
            nm = ida_name.get_name(fe) or ""
            if sub in nm.lower():
                f = ida_funcs.get_func(fe)
                out.append(hdr(f))
                out.append(decompile(f) or "// no decompile")
                n += 1
        txt = "\n".join(out)
        if dst:
            open(dst, "w", encoding="utf-8", errors="replace").write(txt)
            print("written ->", dst, len(txt), "chars,", n, "funcs")
        else:
            print(txt)
        idapro.close_database(True)
        return

    if mode == "calls":
        sub = sys.argv[2]
        for mod, ea, nm in imports_all():
            if sub.lower() not in nm.lower():
                continue
            fs = funcs_using(ea)
            print("== %s @ %#x  被 %d 个函数引用" % (nm, ea, len(fs)))
            for fe, f in sorted(fs.items()):
                print("   func %#x size=%-6d %s" %
                      (fe, f.end_ea - f.start_ea, ida_name.get_name(fe) or ""))
        idapro.close_database(True)   # 存盘：不然下次打开 func_qty=0，分析白做
        return

    if mode == "calloff":
        # 扫「通过 [reg+off] 间接调用」的指令，用来找 LIB_INFO 里某个函数指针字段的**所有**调用点。
        # 例：m_pfnNotify 在 LIB_INFO 里的字节偏移是 120 = 0x78 → calloff 78
        # ⚠️ VC6 常把函数指针先 mov 进寄存器再 call：只搜 FF 5x/9x（直接 call [reg+off]）
        #    会一个都命中不了 —— 必须连 8B（mov r32,[reg+off]）一起搜。这条真踩过。
        off = int(sys.argv[2], 16)
        pats = []
        for modrm in range(0x40, 0x80):                     # mod=01, disp8
            pats.append("8B %02X %02X" % (modrm, off))      # mov r32,[reg+off]
        for reg in range(8):
            pats.append("FF %02X %02X" % (0x50 | reg, off))     # call [reg+off]
            pats.append("FF %02X %02X" % (0x10 | reg, off))     # call near [reg+off]
            pats.append("FF %02X %02X" % (0x20 | reg, off))     # jmp  [reg+off]
        for modrm in range(0x80, 0xC0):                     # mod=10, disp32
            pats.append("8B %02X %02X 00 00 00" % (modrm, off))
        for reg in range(8):
            pats.append("FF %02X %02X 00 00 00" % (0x90 | reg, off))
        groups = {}
        for p in pats:
            ea = 0
            while True:
                ea = ida_bytes.find_bytes(p, ea)   # 默认就是 BIN_SEARCH_FORWARD + 搜到 BADADDR
                if ea == ida_idaapi.BADADDR:
                    break
                f = ida_funcs.get_func(ea)
                key = f.start_ea if f else 0
                groups.setdefault(key, []).append((ea, p))
                ea += 1
        print("== [reg+%#x] 的使用点，命中 %d 个函数 ==" % (off, len(groups)))
        for fe in sorted(groups):
            nm = ida_name.get_name(fe) or ""
            sites = groups[fe]
            print("  func %#010x %-22s sites=%-3d %s" %
                  (fe, nm, len(sites), " ".join("%#x" % s for s, _ in sites[:5])))
        idapro.close_database(True)
        return

    if mode == "imm":
        # 扫「立即数」被 push/mov/cmp 的地方，用来找 NAS_*/NRS_*/NL_* 这些接口号的调用点。
        # imm <十进制值>...   例：imm 1002 1006 1014
        vals = [int(x, 0) for x in sys.argv[2].split(",")]
        pats = []
        for v in vals:
            b = "%02X %02X %02X %02X" % (v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF)
            pats.append(("push %d" % v, "68 " + b))          # push imm32
            pats.append(("mov  %d" % v, "B8 " + b))          # mov eax, imm32
            pats.append(("mov  %d" % v, "B9 " + b))          # mov ecx, imm32
            pats.append(("mov  %d" % v, "BA " + b))          # mov edx, imm32
            if v < 0x80:
                pats.append(("push %d" % v, "6A %02X" % v))  # push imm8
        groups = {}
        for label, p in pats:
            ea = 0
            while True:
                ea = ida_bytes.find_bytes(p, ea)
                if ea == ida_idaapi.BADADDR:
                    break
                f = ida_funcs.get_func(ea)
                key = f.start_ea if f else 0
                groups.setdefault(key, []).append((ea, label))
                ea += 1
        print("== 立即数使用点，命中 %d 个函数 ==" % len(groups))
        for fe in sorted(groups, key=lambda x: -len(groups[x])):
            nm = ida_name.get_name(fe) or ""
            sites = groups[fe]
            print("  func %#010x %-22s sites=%-3d %s" %
                  (fe, nm, len(sites),
                   " ".join("%#x=%s" % (s, t) for s, t in sites[:8])))
        idapro.close_database(True)
        return

    if mode == "pat":
        # 搜任意字节模式（支持 ? 通配），列所在函数 + 附近反汇编。
        #   pat "6A 01 FF"            找 push 1; call [reg+..]
        #   pat "68 ?? ?? ?? ?? 6A 01" 找 push <某个函数地址>; push 1
        p = sys.argv[2]
        ctx = int(sys.argv[3]) if len(sys.argv) > 3 else 12
        ea = 0
        n = 0
        while True:
            ea = ida_bytes.find_bytes(p, ea)
            if ea == ida_idaapi.BADADDR:
                break
            f = ida_funcs.get_func(ea)
            print("--- %#010x  func=%s size=%d" %
                  (ea, hex(f.start_ea) if f else "?", (f.end_ea - f.start_ea) if f else 0))
            a = ea - 8
            for _ in range(6):
                if a > ea + ctx:
                    break
                print("      %#010x  %s" % (a, idc.generate_disasm_line(a, 0)))
                a = idc.next_head(a, ea + ctx + 8)
            n += 1
            if n > 40:
                print("  ... 更多略")
                break
            ea += 1
        print("  (命中 %d 处)" % n)
        idapro.close_database(True)
        return

    if mode == "str":
        sub = sys.argv[2].lower()
        # ⚠️ 不要给 Strings.setup() 传 strtypes —— 传错参数会把列表清空，一条都搜不到
        #    （在 e_copy.exe 上就是这么踩的：连 "krnln" 都搜不出来）。默认构造 + 直接迭代就是全类型。
        sc = idautils.Strings()
        n = 0
        for s in sc:
            n += 1
            t = str(s)
            if sub in t.lower():
                print("  %#010x len=%-4d %s" % (s.ea, s.length, t[:160]))
        print("  (扫了 %d 条字符串)" % n)
        idapro.close_database(True)   # 存盘：不然下次打开 func_qty=0，分析白做
        return

    if mode == "xref":
        arg = sys.argv[2]
        if arg.startswith("0x"):
            ea = int(arg, 16)
        else:
            ea = idc.get_name_ea_simple(arg)
        print("target %#x" % ea)
        for xr in idautils.XrefsTo(ea, 0):
            f = ida_funcs.get_func(xr.frm)
            print("   from %#010x  func=%s" %
                  (xr.frm, hex(f.start_ea) if f else "?"))
        idapro.close_database(True)   # 存盘：不然下次打开 func_qty=0，分析白做
        return

    if mode == "report":
        # 一次进程里把 survey + 指定函数的反编译全倒出来。
        # ⚠️ 不要「跑一次 survey 存盘、再开一次 dec」—— idalib 反复开关 krnln.fne 会随机
        #    open rc=4（func_qty=0），看起来像"地址不存在"，其实是库根本没打开。
        dst = sys.argv[2]
        addrs = []
        if len(sys.argv) > 3:
            for tok in sys.argv[3].split(","):
                tok = tok.strip()
                if not tok:
                    continue
                # ⚠️ PowerShell 会把裸写的 0x100619e0 当数字字面量**转成十进制**再传进来，
                #    所以这里带 0x 前缀或有 a-f 的按十六进制，纯数字的按十进制（两种都认）。
                if tok.lower().startswith("0x") or any(c in "abcdefABCDEF" for c in tok):
                    addrs.append(int(tok, 16))
                else:
                    addrs.append(int(tok, 10))
        out = []
        # ⚠️ 先把要看的函数反编译掉，再做导出/导入的枚举 —— 实测枚举完再 get_func 会返回 None
        #    （同一个进程里 func_qty 仍是 5558）。
        for ea in addrs:
            f = ida_funcs.get_func(ea)
            out.append("")
            if not f:
                out.append("// no func at %#x (func_qty=%d)" % (ea, ida_funcs.get_func_qty()))
                continue
            out.append(hdr(f))
            out.append(decompile(f) or "// no decompile")
        out.append("== 基本信息 ==")
        out.append("  input : %s" % ida_nalt.get_input_file_path())
        out.append("  arch  : %s" % ("64bit" if ida_idaapi_is64() else "32bit"))
        out.append("  funcs : %d" % ida_funcs.get_func_qty())
        out.append("== 段 ==")
        for s in idautils.Segments():
            sg = ida_segment.getseg(s)
            out.append("  %-10s %#010x..%#010x  %d bytes" %
                       (ida_segment.get_segm_name(sg), sg.start_ea, sg.end_ea, sg.end_ea - sg.start_ea))
        out.append("== 导出（全部 %d）==" % ida_entry.get_entry_qty())
        for i in range(ida_entry.get_entry_qty()):
            ord_ = ida_entry.get_entry_ordinal(i)
            out.append("  [%4d] %#010x  %s" % (ord_, ida_entry.get_entry(ord_),
                                               ida_entry.get_entry_name(ord_) or ""))
        imps = imports_all()
        out.append("== 导入（%d 条）==" % len(imps))
        for mod, ea, nm in sorted(imps, key=lambda x: (x[0], x[2])):
            out.append("  %#010x  %-16s %s" % (ea, mod, nm))
        for ea in addrs:
            f = ida_funcs.get_func(ea)
            out.append("")
            if not f:
                out.append("// no func at %#x" % ea)
                continue
            out.append(hdr(f))
            out.append(decompile(f) or "// no decompile")
        txt = "\n".join(out)
        open(dst, "w", encoding="utf-8", errors="replace").write(txt)
        print("written ->", dst, len(txt), "chars, funcs =", ida_funcs.get_func_qty())
        idapro.close_database(True)
        return

    if mode == "dec":
        out = []
        for tok in sys.argv[2].split(","):
            tok = tok.strip()
            if not tok:
                continue
            # ⚠️ 同 report：PowerShell 会把裸写的 0x10064d30 转成十进制再传进来，两种都要认
            if tok.lower().startswith("0x") or any(c in "abcdefABCDEF" for c in tok):
                ea = int(tok, 16)
            else:
                ea = int(tok, 10)
            f = ida_funcs.get_func(ea)
            if not f:
                out.append("// no func at %#x" % ea)
                continue
            out.append(hdr(f))
            out.append(decompile(f) or "// no decompile")
        txt = "\n".join(out)
        if len(sys.argv) > 3:
            open(sys.argv[3], "w", encoding="utf-8", errors="replace").write(txt)
            print("written ->", sys.argv[3], len(txt), "chars")
        else:
            print(txt)
        idapro.close_database(True)   # 存盘：不然下次打开 func_qty=0，分析白做
        return

    print("unknown mode", mode)
    idapro.close_database(True)   # 存盘：不然下次打开 func_qty=0，分析白做


def ida_idaapi_is64():
    import ida_ida
    return ida_ida.inf_is_64bit()


if __name__ == "__main__":
    main()
