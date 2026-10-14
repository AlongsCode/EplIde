# e_sections.py -- 按 e.exe 的读盘总控 (sub_45B9B0) 逐字段走一遍 .e 的段表。
#
# 判据（REA/IDA 伪 C）：
#   * 文件头 "CNWTEPRG"（8 字节）
#   * 每段：[u32 magic=0x15117319][u32 checksum][92 字节段体]
#       - checksum = sub_4FF040(段体, 92)  = 4 通道 lane-XOR（不是单字节 XOR 和）
#       - 段体 +36 = index，读盘时按 1,2,3... 递增校验
#       - 段体 +0  = 段 key（小端 4 字节）
#       - 段体 +4..33 = 段名 30 字节，解码 Name[i] ^= Key[(i+1)%4]（sub_4FF010）
#       - 段体 +48 = payload 长度
#   * payload 紧跟在 100 字节段头之后；读完 seek 到 当前位置 + size（不信任读者）
#
# 用法：python tools\e_sections.py <文件.e> [-s]
#   -s 时顺带把 01 用户信息段 / 02 系统信息段 的字段按 sub_4556F0 的布局打出来
import sys, struct

try:
    sys.stdout.reconfigure(errors="replace")   # GBK 控制台：不认识的字换成 ?
except Exception:
    pass


def ascii_safe(s):
    return "".join(ch if 32 <= ord(ch) < 127 else "." for ch in s)

SEC_KEYS = {
    0x01007319: "UserInfo",
    0x02007319: "SystemInfo",
    0x03007319: "Program",
    0x04007319: "Resource",
    0x05007319: "Optional",
    0x06007319: "EditInfo",
    0x07007319: "End",
    0x08007319: "InitModule",
    0x09007319: "EditInfo2",
    0x0A007319: "AuxInfo1",
    0x0B007319: "AuxInfo2",
    0x0C007319: "ModuleRecord",
    0x0D007319: "EPackageInfo1",
    0x0E007319: "EditFilter",
    0x0F007319: "CompileCond",
    0x10007319: "AuxInfo3",
    0x11007319: "ModuleRecord2",
}
SEG_MAGIC = 0x15117319
END_KEY   = 0x07007319


def lane_xor(b):
    v = [0, 0, 0, 0]
    for i, c in enumerate(b):
        v[i & 3] ^= c
    return v[0] | (v[1] << 8) | (v[2] << 16) | (v[3] << 24)


def byte_xor_sum(b):
    v = 0
    for c in b:
        v ^= c
    return v


def decode_name(body, key):
    k = struct.pack("<I", key & 0xFFFFFFFF)
    out = bytearray()
    for i in range(30):
        c = body[4 + i] ^ k[(i + 1) & 3]
        if c == 0:
            break
        out.append(c)
    return out.decode("gbk", "replace")


def rd_u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def mfc_str(b, o):
    n = rd_u32(b, o)
    return b[o + 4:o + 4 + n], o + 4 + n


def walk(data):
    """返回 [(idx, key, name, size, off, hdr_ck_ok, idx_ok)]"""
    out = []
    if data[:8] != b"CNWTEPRG":
        raise SystemExit("NOT an .e (bad magic)")
    pos, expect = 8, 1
    while pos + 100 <= len(data):
        magic = rd_u32(data, pos)
        if magic != SEG_MAGIC:
            out.append(("BAD_MAGIC@%d" % pos, magic, "", 0, pos, False, False))
            break
        ck = rd_u32(data, pos + 4)
        body = data[pos + 8:pos + 100]
        key = rd_u32(body, 0)
        size = rd_u32(body, 48)
        name = decode_name(body, key)
        out.append((expect, key, name, size, pos + 100,
                    lane_xor(body) == ck, rd_u32(body, 36) == expect))
        pos += 100 + size
        expect += 1
        if key == END_KEY:
            break
    return out


def show_userinfo(p):
    """sub_4556F0 读侧：2 个 CString + 8 个 CString + 5 个 u32 + 20 字节 + u32 + 4 字节"""
    o = 0
    strs = []
    for _ in range(10):
        s, o = mfc_str(p, o)
        strs.append(s)
    ints = [rd_u32(p, o + 4 * i) for i in range(5)]
    o += 20
    tail20 = p[o:o + 20]; o += 20
    tail_u32 = rd_u32(p, o); o += 4
    tail4 = p[o:o + 4]; o += 4
    for i, s in enumerate(strs):
        if s:
            print("   U%02d len=%3d %s" % (i, len(s), s.decode("gbk", "replace")))
    print("   ints=%s tail20=%s tail_u32=%d tail4=%s consumed=%d/%d"
          % (ints, tail20.hex(), tail_u32, tail4.hex(), o, len(p)))


def show_sysinfo(p):
    print("   lang(+8)=%d  w12=%d w14=%d d16=%d d20=%d d24=%d"
          % (rd_u32(p, 8), struct.unpack_from("<H", p, 12)[0],
             struct.unpack_from("<H", p, 14)[0],
             rd_u32(p, 16), rd_u32(p, 20), rd_u32(p, 24)))


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    path = sys.argv[1]
    data = open(path, "rb").read()
    segs = walk(data)
    print("%s  %d bytes" % (path, len(data)))
    print(" %-3s %-10s %-16s %-9s %-8s %-5s" %
          ("idx", "key", "name", "size", "ck/lane", "idx_ok"))
    for idx, key, name, size, off, ck_ok, idx_ok in segs:
        print(" %-3s %08X  %-16s %-9s %-8s %-5s"
              % (idx, key if isinstance(key, int) else 0,
                 name, size, "OK" if ck_ok else "MISMATCH", idx_ok))
    if "-s" in sys.argv:
        for idx, key, name, size, off, ck_ok, idx_ok in segs:
            if key == 0x01007319:
                print(" [UserInfo]"); show_userinfo(data[off:off + size])
            elif key == 0x02007319:
                print(" [SystemInfo]"); show_sysinfo(data[off:off + size])
    # 逐字节总断言（notes 里的 byte_coverage 六条）
    end = 8
    for _, _, _, size, _, _, _ in segs:
        end += 100 + size
    print(" consumed=%d  filesize=%d  %s" % (end, len(data), "OK" if end == len(data) else "MISMATCH"))


if __name__ == "__main__":
    main()
