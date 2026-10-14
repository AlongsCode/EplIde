# e_resource.py -- 倒一个 .e 的「程序资源段」(0x04007319) 结构。
#
# 段体布局（依据 D:\EPL\_ida\RES_FINDING.md §2，逐样本复核中）：
#   +0            u32 A = 表单描述符区字节数 = 8 × 窗口数
#   +4            A 字节：A/8 条 × [u32 form_id][u32 运行期值]
#   +4+A          窗口记录区，每条：
#                     [u32 hash][u32 tag][mfc 窗口名][mfc 注释]
#                     [u32 part_count][u32 part_data_len][part_data]
#                 part_data = [u32 id × n][u32 off × n][(u32 len)(元素) × n]，off 相对块区起点
#   尾             12 字节 00（空尾）
#   元素（非菜单）  = [I32 dataType][20 字节][名字 NUL 串] …
#   mfc_string     = [u32 len][len 字节 GBK]（e.exe 的 sub_4FEBA0）
#
# 本工具**不猜**元素内部的几何字段位置：它把每个元素块按 16 字节一行倒出来，
# 并列出所有满足 `x+w <= 窗体宽 && y+h <= 窗体高` 的「连续 4 个 u32」候选，
# 这样多个样本一比就能看出真正的 [left][top][width][height] 落在哪个偏移。
#
# 用法：python tools\e_resource.py <文件.e> [--dump <元素 id 或任意数字>]
import struct
import sys

SEC_RESOURCE = 0x04007319


def load_resource(path):
    d = open(path, "rb").read()
    assert d[:8] == b"CNWTEPRG", "not an .e"
    pos = 8
    while pos + 100 <= len(d):
        key = struct.unpack_from("<I", d, pos + 8)[0]
        size = struct.unpack_from("<I", d, pos + 56)[0]
        if key == SEC_RESOURCE:
            return d[pos + 100:pos + 100 + size]
        pos += 100 + size
    raise SystemExit("no resource section")


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def mstr(b, o):
    if o + 4 > len(b):
        raise ValueError("mfc_string 越界")
    n = u32(b, o)
    if n > 65535 or o + 4 + n > len(b):
        raise ValueError("mfc_string 长度不合理 (%d)" % n)
    return b[o + 4:o + 4 + n], o + 4 + n


def gbk(s):
    return s.decode("gbk", "replace")


def parse(res):
    A = u32(res, 0)
    print("payload=%d  A=%d  窗口数=%d" % (len(res), A, A // 8))
    i = 4 + A
    recs = []
    while i + 16 <= len(res):
        try:
            h, tag = struct.unpack_from("<II", res, i)
            if h == 0 and tag == 0:
                break
            q = i + 8
            wname, q = mstr(res, q)
            wcmt, q = mstr(res, q)
            n, blen = struct.unpack_from("<II", res, q)
            b = q + 8
            if n == 0 or n > 1024 or blen == 0 or b + blen > len(res) or 8 * n > blen:
                break
        except (ValueError, struct.error):
            break
        blocks = b + 8 * n
        recs.append((i, h, tag, gbk(wname), gbk(wcmt), n, blen, b, blocks))
        nxt = b + blen
        # 记录之间：12 字节 00（空尾）或直接接第二条记录；都不像就停
        if res[nxt:nxt + 12] == b"\x00" * 12:
            i = nxt + 12
        else:
            i = nxt
            if i + 16 > len(res):
                break
    tail = res[i:i + 32]
    if tail:
        print(" 尾部/第二数据块 @%d: %s%s" % (i, tail.hex(), " …" if len(res) - i > 32 else ""))
    return recs


def elems(res, rec):
    i, h, tag, wname, wcmt, n, blen, b, blocks = rec
    out = []
    for e in range(n):
        eid = u32(res, b + 4 * e)
        off = u32(res, b + 4 * n + 4 * e)
        blk = blocks + off
        if blk + 4 > b + blen:
            continue
        elen = u32(res, blk)
        el = blk + 4
        if el + elen > b + blen:
            continue
        out.append((eid, off, el, elen))
    return out


def cstr(blk, o):
    """NUL 结尾串（元素里的名字/注释都是这个形态）"""
    e = o
    while e < len(blk) and blk[e] != 0:
        e += 1
    return blk[o:e], e + 1


def parse_elem(blk):
    """元素（非菜单）顺序：dataType / 20 字节 / 名字 / 注释 / cWndAddress / left / top / w / h / …

    依据：e-packager `WriteFormElements` 的字段顺序 + 本项目在 flowcmd.e 上的逐字节复核
    （按钮1 312,352,80x32；画板1 8,8,272x376；窗体 50,50,404x423 三个互证）。
    ⚠️ 字段是**顺序**而非定长：名字/注释是变长 NUL 串，所以后面所有字段的偏移都不定。
    """
    d = {}
    d["dataType"] = struct.unpack_from("<I", blk, 0)[0]
    nm, o = cstr(blk, 24)
    d["name"] = nm
    cmt, o = cstr(blk, o)
    d["comment"] = cmt
    if o + 20 > len(blk):
        d["truncated"] = True
        return d
    d["cwnd"], d["left"], d["top"], d["width"], d["height"] = struct.unpack_from("<IIIII", blk, o)
    o += 20
    d["unknownBeforeParent"], d["parent"], nchild = struct.unpack_from("<III", blk, o)
    o += 12
    d["children"] = list(struct.unpack_from("<%dI" % nchild, blk, o)) if nchild and o + 4 * nchild <= len(blk) else []
    o += 4 * len(d["children"])
    d["tailOff"] = o
    d["tail"] = blk[o:]
    return d


def dump_block(res, el, elen, eid):
    print("---- dump id=%08X elen=%d" % (eid, elen))
    blk = res[el:el + elen]
    for r in range(0, len(blk), 16):
        ch = blk[r:r + 16]
        print("%4d  %-47s %s" % (r, " ".join("%02X" % c for c in ch),
                                 "".join(chr(c) if 32 <= c < 127 else "." for c in ch)))


def main():
    path = sys.argv[1]
    res = load_resource(path)
    recs = parse(res)
    print("%s  records=%d" % (path, len(recs)))
    formw = formh = 0
    for ri, rec in enumerate(recs):
        es = elems(res, rec)
        print(" rec%d name=%s comment=%s parts=%d blen=%d elems=%d"
              % (ri, rec[3], rec[4], rec[5], rec[6], len(es)))
        for eid, off, el, elen in es:
            d = parse_elem(res[el:el + elen])
            mark = ""
            if (d["dataType"] & 0xFFFF) == 1 and formw == 0:
                formw, formh = d["width"], d["height"]
                mark = "  ← 窗体"
            inside = ""
            if formw and (d["dataType"] & 0xFFFF) != 1:
                ok = d["left"] + d["width"] <= formw and d["top"] + d["height"] <= formh
                inside = " [窗体 %dx%d 内 %s]" % (formw, formh, "OK" if ok else "**越界**")
            print("   elem id=%08X dt=%08X elen=%3d name=%s rect=(%d,%d,%dx%d) cwnd=%08X parent=%08X%s%s"
                  % (eid, d["dataType"], elen, gbk(d["name"]),
                     d.get("left", -1), d.get("top", -1), d.get("width", -1), d.get("height", -1),
                     d.get("cwnd", 0), d.get("parent", 0), inside, mark))
            if d["tail"]:
                print("        tail(%d)=%s" % (len(d["tail"]), d["tail"][:32].hex()))
    if "--dump" in sys.argv:
        want = int(sys.argv[sys.argv.index("--dump") + 1], 0)
        for ri, rec in enumerate(recs):
            for eid, off, el, elen in elems(res, rec):
                if eid == want:
                    dump_block(res, el, elen, eid)


if __name__ == "__main__":
    main()
