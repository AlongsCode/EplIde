# -*- coding: utf-8 -*-
"""Decode the GUI resources of a PE file: menus (with their item strings) and dialog templates.

Written because tools/dump_res.py only lists resource types and dump_dialogs.py only handles
dialogs; the component box's group switcher lives in a MENU resource, which needs its own
decoder.

usage:
  python tools/pe_res.py menus  "C:\path\e.exe"
  python tools/pe_res.py menu   "C:\path\e.exe" <menu-id>
  python tools/pe_res.py find   "C:\path\e.exe" <substring>      (menus containing the text)
"""
import struct
import sys


def load_pe(path):
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    ddir = opt + (96 if magic == 0x10B else 112)
    rsrc_rva, rsrc_size = struct.unpack_from("<II", data, ddir + 2 * 8)
    secs = []
    for i in range(nsec):
        off = pe + 24 + opt_size + i * 40
        name, vsize, vaddr, rawsize, rawptr = struct.unpack_from("<8sIIII", data, off)
        secs.append((vaddr, vsize, rawptr, rawsize))
    return data, secs, rsrc_rva, rsrc_size


def rva2off(secs, rva):
    for vaddr, vsize, rawptr, rawsize in secs:
        if vaddr <= rva < vaddr + max(vsize, rawsize):
            return rawptr + (rva - vaddr)
    return None


DEBUG = False

def walk_res(data, secs, base_off, path=(), _depth=0, _seen=None):
    """yield (path_tuple, data_rva, size).

    Resource directory entries: name/id is either an integer id (high bit clear) or an offset
    to a UTF-16 name (high bit set); the second field is either a subdirectory offset (high bit
    set) or a data-entry offset (high bit clear). Both offsets are relative to the resource base
    and MUST be masked before use.
    """
    if _seen is None:
        _seen = set()
    if _depth > 8 or base_off in _seen or base_off + 16 > len(data):
        return
    _seen = _seen | {base_off}
    nname, nid = struct.unpack_from("<HH", data, base_off + 12)
    if nname + nid > 4096:
        return
    if DEBUG:
        print("dir @0x%X named=%d id=%d" % (base_off, nname, nid), file=sys.stderr)
    for i in range(nname + nid):
        off = base_off + 16 + i * 8
        if off + 8 > len(data):
            return
        name, offset = struct.unpack_from("<II", data, off)
        if name & 0x80000000:
            noff = base_off + (name & 0x7FFFFFFF)
            if noff + 2 > len(data):
                continue
            ln = struct.unpack_from("<H", data, noff)[0]
            if ln > 256:
                continue
            nm = data[noff + 2:noff + 2 + ln * 2].decode("utf-16-le", "replace")
        else:
            nm = name
        tgt = base_off + (offset & 0x7FFFFFFF)
        if DEBUG:
            print("   %s -> 0x%X %s" % (nm, tgt, "DIR" if offset & 0x80000000 else "DATA"),
                  file=sys.stderr)
        if offset & 0x80000000:
            if tgt + 16 > len(data):
                continue
            yield from walk_res(data, secs, tgt, path + (nm,), _depth + 1, _seen)
        else:
            if tgt + 8 > len(data):
                continue
            drva, dsize = struct.unpack_from("<II", data, tgt)
            yield (path + (nm,), drva, dsize)


def decode_menu(data, base):
    """MENU resource -> list of (level, id, text)."""
    ver, hsize = struct.unpack_from("<HH", data, base)
    out = []
    off = base + hsize

    def read_str(o):
        ln = struct.unpack_from("<H", data, o)[0]
        if ln == 0:
            return "", o + 2
        return data[o + 2:o + 2 + ln * 2].decode("utf-16-le", "replace"), o + 2 + ln * 2

    def items(o, level):
        flags = struct.unpack_from("<H", data, o)[0]
        o += 2
        while flags & 0x0010:      # MF_POPUP
            if flags & 0x8000:     # MF_END
                pass
            o += 4
            flags = struct.unpack_from("<H", data, o)[0]
            o += 2
        while True:
            flags = struct.unpack_from("<H", data, o)[0]
            if flags == 0:
                break
            o += 4                 # flags + id
            txt, o = read_str(o)
            out.append((level, flags, txt))
            if flags & 0x0010:     # popup -> recurse
                out[-1] = (level, flags | 0, txt)
                o = items(o, level + 1)
            if flags & 0x0080:     # MF_END
                break
        return o

    # outer loop over top-level items
    flags = struct.unpack_from("<H", data, off)[0]
    while flags != 0:
        f = struct.unpack_from("<H", data, off)[0]
        i = struct.unpack_from("<H", data, off + 2)[0]
        o2 = off + 4
        txt, o2 = read_str(o2)
        out.append((0, f, txt))
        if f & 0x0010:
            o2 = items(o2, 1)
        off = o2
        if f & 0x0080:
            break
        flags = struct.unpack_from("<H", data, off)[0]
    return out


def main():
    mode = sys.argv[1]
    path = sys.argv[2]
    data, secs, rsrc_rva, rsrc_size = load_pe(path)
    base = rva2off(secs, rsrc_rva)
    res = list(walk_res(data, secs, base))
    if mode == "menus":
        for p, rva, size in res:
            if len(p) == 2 and p[0] == 4:
                off = rva2off(secs, rva)
                items = decode_menu(data, off)
                texts = " | ".join(t for _, _, t in items if t)
                print("MENU %s (%d bytes): %s" % (p[1], size, texts[:200]))
    elif mode == "menu":
        want = int(sys.argv[3])
        for p, rva, size in res:
            if len(p) == 2 and p[0] == 4 and p[1] == want:
                off = rva2off(secs, rva)
                for lvl, f, t in decode_menu(data, off):
                    print("%s%04X %s%s" % ("  " * lvl, f, t, "  <POPUP>" if f & 0x10 else ""))
    elif mode == "find":
        sub = sys.argv[3]
        for p, rva, size in res:
            if len(p) == 2 and p[0] == 4:
                off = rva2off(secs, rva)
                try:
                    items = decode_menu(data, off)
                except Exception:
                    continue
                hit = [t for _, _, t in items if sub in t]
                if hit:
                    print("MENU %s: %s" % (p[1], [t for _, _, t in items if t]))
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
