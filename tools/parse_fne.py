# -*- coding: utf-8 -*-
"""正确的易语言支持库 LIB_INFO 离线解析器（对照 sdk/cpp/elib/lib2.h 的布局）。

用法: python parse_fne.py <lib.fne|fnr> [out.json]

与旧的 parse_libinfo.py 的差别（旧版的坑）:
  * CMD_INFO 大小是 **36** 字节（不是 40；结构尾没有成员函数，VC6 按 4 对齐）。
  * ARG_INFO 24 字节: name@0 explain@4 bmpIdx@8(2) bmpCnt@10(2) dtype@12 default@16 state@20。
  * LIB_CONST_INFO 32 字节（DOUBLE 8 字节对齐 -> 20 处有 4 字节填充）。
"""
import json
import struct
import sys

import capstone
import pefile

GBK = "gbk"


def rd(data, off, fmt):
    return struct.unpack_from(fmt, data, off)[0]


class Image:
    def __init__(self, path):
        self.path = path
        self.pe = pefile.PE(path, fast_load=True)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.sections = [(s.VirtualAddress, max(s.Misc_VirtualSize, s.SizeOfRawData),
                          s.PointerToRawData, s.SizeOfRawData) for s in self.pe.sections]
        self.blob = open(path, "rb").read()

    def va2off(self, va, size=4):
        rva = va - self.base
        for vaddr, vsize, raw, rawsize in self.sections:
            if vaddr <= rva < vaddr + vsize:
                off = raw + (rva - vaddr)
                return off if off + size <= len(self.blob) else None
        return None

    def u32(self, va):
        off = self.va2off(va)
        return rd(self.blob, off, "<I") if off is not None else 0

    def u16(self, va):
        off = self.va2off(va)
        return rd(self.blob, off, "<H") if off is not None else 0

    def cstr(self, va, limit=4096):
        if not va:
            return ""
        off = self.va2off(va)
        if off is None:
            return ""
        end = self.blob.find(b"\x00", off, off + limit)
        if end < 0:
            end = off + limit
        return self.blob[off:end].decode(GBK, "replace")

    def str_table(self, va, count, limit=4096):
        """szzXxx：count 个 \\0 结束的串，第 0 项通常为空串。"""
        if not va:
            return []
        off = self.va2off(va)
        if off is None:
            return []
        out, cur, i = [], b"", off
        while i < len(self.blob) and len(out) < count + 2 and i - off < limit * (count + 2):
            b = self.blob[i:i + 1]
            if b == b"\x00":
                out.append(cur.decode(GBK, "replace"))
                cur = b""
            else:
                cur += b
            i += 1
        return out

    def find_getnewinf(self):
        pe = self.pe
        pe.parse_data_directories(directories=[
            pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
        rva = None
        for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            if e.name and e.name.decode() in ("GetNewInf", "GetLibInf"):
                rva = e.address
        if rva is None:
            raise SystemExit("no GetNewInf export")
        off = self.va2off(self.base + rva)
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        for ins in md.disasm(self.blob[off:off + 64], self.base + rva):
            if ins.mnemonic in ("mov", "lea") and ins.op_str.startswith("eax, "):
                tgt = ins.op_str.split(", ", 1)[1]
                if tgt.startswith("0x"):
                    return int(tgt, 16)
            if ins.mnemonic == "ret":
                break
        raise SystemExit("could not resolve LIB_INFO address")


SDT_NAMES = {
    0x80000000: ("通用型", "all"),
    0x80000001 | (1 << 8): ("字节", "byte"),
    0x80000002 | (1 << 8): ("短整数", "short"),
    0x80000003 | (1 << 8): ("整数", "int"),
    0x80000004 | (1 << 8): ("长整数", "int64"),
    0x80000005 | (1 << 8): ("小数", "float"),
    0x80000006 | (1 << 8): ("双精度小数", "double"),
    0x80000000 | (2 << 8): ("逻辑型", "bool"),
    0x80000000 | (3 << 8): ("日期时间", "datetime"),
    0x80000000 | (4 << 8): ("文本型", "text"),
    0x80000000 | (5 << 8): ("字节集", "bin"),
    0x80000000 | (6 << 8): ("子程序指针", "subptr"),
    0x80000000 | (8 << 8): ("子语句型", "statment"),
}
# MAKELONG(MAKEWORD(a,b),0x8000) = (b<<24)|(a<<16)|0x8000  -> 错误
# 实际: MAKELONG(MAKEWORD(1,3),0x8000) = (0x8000<<16)|MAKEWORD(1,3) = 0x80000301
for _k in list(SDT_NAMES):
    pass


def dtype_raw(v):
    return v


def dtype_name(v, lib_dt=None, short=False):
    """把 DATA_TYPE 转成 '整数型（int）' 这种文本；lib_dt 为 {index: (中文,英文)}"""
    if v == 0:
        return ""
    if v & 0x20000000:
        return dtype_name(v & ~0x20000000, lib_dt, short)
    if v & 0x80000000:
        a = (v >> 16) & 0xFF
        b = (v >> 24) & 0xFF
        base = {
            (0, 0): ("通用型", "all"),
            (1, 1): ("字节", "byte"), (1, 2): ("短整数", "short"),
            (1, 3): ("整数", "int"), (1, 4): ("长整数", "int64"),
            (1, 5): ("小数", "float"), (1, 6): ("双精度小数", "double"),
            (2, 0): ("逻辑型", "bool"), (3, 0): ("日期时间", "datetime"),
            (4, 0): ("文本型", "text"), (5, 0): ("字节集", "bin"),
            (6, 0): ("子程序指针", "subptr"), (8, 0): ("子语句型", "statment"),
        }.get((b, a))
        if base is None:
            return "系统类型(%#x)" % v
    else:
        if lib_dt and v in lib_dt:
            base = lib_dt[v]
        else:
            return "库类型(%d)" % v
    if short:
        return base[0]
    return "%s（%s）" % (base[0], base[1])


def parse(path):
    img = Image(path)
    addr = img.find_getnewinf()
    f = lambda o: img.u32(addr + o)
    s = lambda o: img.cstr(f(o))
    info = {"file": path, "libinfo_va": hex(addr)}
    info["format"] = f(0)
    info["guid"] = s(4)
    info["ver"] = [f(8), f(12), f(16)]
    info["rq"] = [f(20), f(24), f(28), f(32)]
    info["name"] = s(36)
    info["language"] = f(40)
    info["explain"] = s(44)
    info["state"] = f(48)
    info["author"] = {"作者姓名": s(52), "邮政编码": s(56), "通信地址": s(60),
                      "电话号码": s(64), "传真": s(68), "电子信箱": s(72),
                      "主页地址": s(76), "其它信息": s(80)}
    n_dt, p_dt = f(84), f(88)
    n_cat, p_cat = f(92), f(96)
    n_cmd, p_cmd = f(100), f(104)
    n_const, p_const = f(132), f(136)
    info["counts"] = {"data_types": n_dt, "categories": n_cat, "commands": n_cmd,
                      "consts": n_const}
    info["categories"] = [c for c in img.str_table(p_cat, n_cat) if c]

    # ---- 数据类型 (56 字节) ----
    dts = []
    do = img.va2off(p_dt, 56 * max(n_dt, 1)) if p_dt else None
    for i in range(n_dt if do is not None else 0):
        o = do + i * 56
        dts.append({
            "i": i, "name": img.cstr(rd(img.blob, o + 0, "<I")),
            "eg": img.cstr(rd(img.blob, o + 4, "<I")),
            "explain": img.cstr(rd(img.blob, o + 8, "<I")),
            "ncmds": rd(img.blob, o + 12, "<i"),
            "pcmds": rd(img.blob, o + 16, "<I"),
            "state": rd(img.blob, o + 20, "<I"),
            "bmpid": rd(img.blob, o + 24, "<I"),
            "nevents": rd(img.blob, o + 28, "<i"),
            "pevents": rd(img.blob, o + 32, "<I"),
            "nprops": rd(img.blob, o + 36, "<i"),
            "pprops": rd(img.blob, o + 40, "<I"),
            "nelems": rd(img.blob, o + 48, "<i"),
            "pelems": rd(img.blob, o + 52, "<I"),
        })
    # 库自定义类型索引表：命令里 m_dtRetValType 的低位就是这个 i
    lib_dt = {d["i"]: (d["name"], d["eg"] or "") for d in dts}

    # ---- 命令 (36 字节) ----
    cmds = []
    co = img.va2off(p_cmd, 36 * max(n_cmd, 1)) if p_cmd else None
    for i in range(n_cmd if co is not None else 0):
        o = co + i * 36
        cat, wstate = struct.unpack_from("<hH", img.blob, o + 12)
        ret = rd(img.blob, o + 16, "<I")
        userlvl, bmpidx = struct.unpack_from("<hh", img.blob, o + 22)
        narg = rd(img.blob, o + 28, "<i")
        parg = rd(img.blob, o + 32, "<I")
        args = []
        ao = img.va2off(parg, 24 * max(narg, 1)) if parg else None
        for a in range(narg if ao is not None else 0):
            p = ao + a * 24
            args.append({
                "name": img.cstr(rd(img.blob, p + 0, "<I")),
                "explain": img.cstr(rd(img.blob, p + 4, "<I")),
                "bmps": list(struct.unpack_from("<hh", img.blob, p + 8)),
                "type": rd(img.blob, p + 12, "<I"),
                "default": rd(img.blob, p + 16, "<i"),
                "state": rd(img.blob, p + 20, "<I"),
            })
        cmds.append({
            "i": i, "name": img.cstr(rd(img.blob, o + 0, "<I")),
            "eg": img.cstr(rd(img.blob, o + 4, "<I")),
            "explain": img.cstr(rd(img.blob, o + 8, "<I")),
            "category": cat, "state": wstate, "ret": ret,
            "level": userlvl, "bmpidx": bmpidx, "nargs": narg, "args": args,
        })

    # ---- 常量 (32 字节) ----
    consts = []
    ko = img.va2off(p_const, 32 * max(n_const, 1)) if p_const else None
    for i in range(n_const if ko is not None else 0):
        o = ko + i * 32
        consts.append({
            "i": i, "name": img.cstr(rd(img.blob, o + 0, "<I")),
            "eg": img.cstr(rd(img.blob, o + 4, "<I")),
            "explain": img.cstr(rd(img.blob, o + 8, "<I")),
            "layout": rd(img.blob, o + 12, "<h"),
            "type": rd(img.blob, o + 14, "<h"),
            "text": img.cstr(rd(img.blob, o + 16, "<I")),
            "value": rd(img.blob, o + 24, "<d"),
        })

    info["data_types"] = dts
    info["commands"] = cmds
    info["consts"] = consts
    return info


if __name__ == "__main__":
    inf = parse(sys.argv[1])
    if len(sys.argv) > 2:
        json.dump(inf, open(sys.argv[2], "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print("库名:", inf["name"], "版本:", ".".join(map(str, inf["ver"])),
          "GUID:", inf["guid"])
    print("计数:", inf["counts"])
    print("类别(%d): %s" % (len(inf["categories"]), inf["categories"]))
    print("数据类型(%d): %s" % (len(inf["data_types"]),
                                [d["name"] for d in inf["data_types"]]))
    print("前 20 命令:", [c["name"] for c in inf["commands"][:20]])
    print("前 10 常量:", [(c["name"], c["type"], c["text"], c["value"])
                          for c in inf["consts"][:10]])
    print("末 5 命令:", [c["name"] for c in inf["commands"][-5:]])
